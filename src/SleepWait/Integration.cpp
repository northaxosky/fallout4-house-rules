#include "PCH.h"

#include "SleepWait/Integration.h"

#include "Settings.h"

namespace SleepWait::Integration
{
	namespace
	{
		bool ObserverEnabled()
		{
			return MCM::Settings::Diagnostic::bSleepWaitObserver.GetValue();
		}

		bool LifecycleLoggingEnabled()
		{
			return ObserverEnabled() &&
			       MCM::Settings::Diagnostic::bSleepWaitObserverLifecycle.GetValue();
		}

		bool SnapshotLoggingEnabled()
		{
			return ObserverEnabled() &&
			       MCM::Settings::Diagnostic::bSleepWaitObserverSnapshots.GetValue();
		}

		const char* SafeEditorID(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return "";
			}
			const auto edid = a_form->GetFormEditorID();
			return edid ? edid : "";
		}

		const char* SafeName(const RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) {
				return "";
			}
			const auto* name = const_cast<RE::TESObjectREFR*>(a_ref)->GetDisplayFullName();
			return name ? name : "";
		}

		const char* FurnitureEventName(const RE::TESFurnitureEvent& a_event)
		{
			if (a_event.IsEnter()) {
				return "enter";
			}
			if (a_event.IsExit()) {
				return "exit";
			}
			return "unknown";
		}

		class Observer :
			public RE::BSTEventSink<RE::TESFurnitureEvent>
		{
		public:
			static Observer* GetSingleton()
			{
				static Observer instance;
				return std::addressof(instance);
			}

			void Install()
			{
				if (installed) {
					return;
				}
				installed = true;
			}

			void OnF4SEMessage(std::uint32_t a_messageType)
			{
				if (!ObserverEnabled()) {
					return;
				}

				LogChosenSurfacesOnce();

				switch (a_messageType) {
					case F4SE::MessagingInterface::kGameLoaded:
						EnsureFurnitureSinkRegistered();
						LogLifecycle("kGameLoaded", "furniture sink registration attempted");
						break;
					default:
						break;
				}
			}

			void OnMenuOpenClose(const RE::MenuOpenCloseEvent& a_event)
			{
				if (!ObserverEnabled()) {
					return;
				}

				if (a_event.menuName != RE::SitWaitMenu::MENU_NAME &&
				    a_event.menuName != "LoadingMenu" &&
				    a_event.menuName != "PauseMenu") {
					return;
				}

				EnsureFurnitureSinkRegistered();
				LogChosenSurfacesOnce();

				if (LifecycleLoggingEnabled()) {
					REX::INFO("SleepWait: menu='{}' opening={}",
						a_event.menuName,
						a_event.opening);
				}

				if (!SnapshotLoggingEnabled()) {
					return;
				}

				if (a_event.menuName == RE::SitWaitMenu::MENU_NAME) {
					SnapshotPlayerState(a_event.opening ? "SitWaitMenu open" : "SitWaitMenu close");
					return;
				}

				if (!a_event.opening) {
					SnapshotPlayerState(a_event.menuName == "LoadingMenu" ?
						"LoadingMenu close" :
						"PauseMenu close");
				}
			}

			RE::BSEventNotifyControl ProcessEvent(
				const RE::TESFurnitureEvent& a_event,
				RE::BSTEventSource<RE::TESFurnitureEvent>*) override
			{
				if (!ObserverEnabled()) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto actor = a_event.actor.get();
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || actor != player) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto target = a_event.targetFurniture.get();
				if (LifecycleLoggingEnabled()) {
					REX::INFO("SleepWait: furnitureEvent='{}' target='{}' targetName='{}'",
						FurnitureEventName(a_event),
						SafeEditorID(target),
						SafeName(target));
				}

				if (SnapshotLoggingEnabled()) {
					SnapshotPlayerState(a_event.IsEnter() ?
						"TESFurnitureEvent enter" :
						"TESFurnitureEvent exit",
						target);
				}

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			void LogLifecycle(const char* a_phase, const char* a_detail) const
			{
				if (!LifecycleLoggingEnabled()) {
					return;
				}
				REX::INFO("SleepWait: phase='{}' {}", a_phase, a_detail);
			}

			void LogChosenSurfacesOnce()
			{
				if (loggedChosenSurfaces) {
					return;
				}
				loggedChosenSurfaces = true;

				// The public/native surfaces here already cover the boundaries we need
				// for future rest work. Leave direct DoWait interception for the first
				// gameplay change that truly needs to alter time-skip behavior.
				REX::INFO("SleepWait: observing TESFurnitureEvent + SitWaitMenu lifecycle + PlayerCharacter sleep fields + TESObjectCELL::GetCantWaitHere; deferring direct DoWait hook for now");
			}

			void EnsureFurnitureSinkRegistered()
			{
				if (!ObserverEnabled() || registeredFurnitureSink) {
					return;
				}

				if (auto* source = RE::TESFurnitureEvent::GetEventSource()) {
					source->RegisterSink(static_cast<RE::BSTEventSink<RE::TESFurnitureEvent>*>(this));
					registeredFurnitureSink = true;
					LogLifecycle("context-sink", "registered TESFurnitureEvent sink");
				}
			}

			void SnapshotPlayerState(const char* a_reason, RE::TESObjectREFR* a_furnitureHint = nullptr) const
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					REX::WARN("SleepWait: {} skipped; PlayerCharacter singleton unavailable", a_reason);
					return;
				}

				RE::TESObjectREFR* occupiedFurniture = a_furnitureHint;
				if (!occupiedFurniture && player->currentProcess) {
					auto handle = player->currentProcess->GetOccupiedFurniture();
					if (handle) {
						occupiedFurniture = handle.get().get();
					}
				}

				auto* occupiedBase = occupiedFurniture ? occupiedFurniture->GetObjectReference() : nullptr;
				// Gate on the TESForm formType enum rather than
				// RE::fallout_cast<TESFurniture*> — the cast costs an RTTI address
				// lookup this doesn't need.
				auto* furniture = (occupiedBase && occupiedBase->GetFormType() == RE::ENUM_FORM_ID::kFURN)
					? static_cast<RE::TESFurniture*>(occupiedBase)
					: nullptr;
				auto* cell = player->GetParentCell();
				auto* location = player->GetCurrentLocation();
				auto* calendar = RE::Calendar::GetSingleton();

				const float gameHour = calendar && calendar->gameHour ? calendar->gameHour->GetValue() : 0.0f;
				const float daysPassed = calendar && calendar->gameDaysPassed ? calendar->gameDaysPassed->GetValue() : 0.0f;
				const float timeScale = calendar && calendar->timeScale ? calendar->timeScale->GetValue() : 0.0f;
				const bool inCombat = player->IsInCombat();
				const bool isSleeping = player->isSleeping;
				const auto requestedHours = player->hourstosleep;
				const auto sleepSeconds = player->sleepSeconds;
				const auto secondsPerUpdate = player->secondsToSleepPerUpdate;
				const bool cantWaitHere = cell ? cell->GetCantWaitHere() : false;
				const auto entryPointCount = furniture ?
					static_cast<std::uint32_t>(furniture->entryPointDataArray.size()) :
					0u;
				const auto markerCount = furniture ?
					static_cast<std::uint32_t>(furniture->markersArray.size()) :
					0u;

				REX::INFO("SleepWait: snapshot reason='{}' difficulty={} inCombat={} isSleeping={} requestedHours={} sleepSeconds={} secondsPerUpdate={} gameHour={} daysPassed={} timeScale={} cantWaitHere={} cell='{}' location='{}' furnitureRef='{}' furnitureBase='{}' furnitureName='{}' furnitureEntryPoints={} furnitureMarkers={}",
					a_reason,
					static_cast<std::uint32_t>(player->GetDifficultyLevel()),
					inCombat,
					isSleeping,
					requestedHours,
					sleepSeconds,
					secondsPerUpdate,
					gameHour,
					daysPassed,
					timeScale,
					cantWaitHere,
					SafeEditorID(cell),
					SafeEditorID(location),
					SafeEditorID(occupiedFurniture),
					SafeEditorID(occupiedBase),
					SafeName(occupiedFurniture),
					entryPointCount,
					markerCount);
			}

			bool installed{ false };
			bool loggedChosenSurfaces{ false };
			bool registeredFurnitureSink{ false };
		};
	}

	void Install()
	{
		Observer::GetSingleton()->Install();
	}

	void OnF4SEMessage(std::uint32_t a_messageType)
	{
		Observer::GetSingleton()->OnF4SEMessage(a_messageType);
	}

	void OnMenuOpenClose(const RE::MenuOpenCloseEvent& a_event)
	{
		Observer::GetSingleton()->OnMenuOpenClose(a_event);
	}
}
