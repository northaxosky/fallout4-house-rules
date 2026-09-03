#include "PCH.h"

#include "Diagnostics/ActorValueProbe.h"
#include "Diagnostics/DumpDefaultObjects.h"
#include "Diagnostics/HCManagerProbe.h"
#include "Diagnostics/SurvivalObserver.h"
#include "Hooks/ExitSave.h"
#include "Hooks/GodMode.h"
#include "Hooks/SafeTravel.h"
#include "Hooks/Unlocks.h"
#include "Settings.h"
#include "SleepWait/Integration.h"
#include "Survival/HCManagerScript.h"
#include "Tweaks/ActionPoints.h"
#include "Tweaks/ActorValues.h"
#include "Tweaks/CharacterStats.h"
#include "Tweaks/CombatPerks.h"
#include "Tweaks/CompanionsAffinity.h"
#include "Tweaks/DamageFormulas.h"
#include "Tweaks/Difficulty.h"
#include "Tweaks/DifficultyEffects.h"
#include "Tweaks/Economy.h"
#include "Tweaks/Magnitudes.h"
#include "Tweaks/PlayerRefresh.h"
#include "Tweaks/PowerArmor.h"
#include "Tweaks/Progression.h"
#include "Tweaks/Settlements.h"
#include "Tweaks/Skills.h"
#include "Tweaks/Sneak.h"
#include "Tweaks/Survival.h"
#include "Tweaks/SurvivalCarryWeight.h"
#include "Tweaks/VATS.h"

namespace
{
	constexpr void (*kTweakApplyFunctions[])() = {
		&Tweaks::Magnitudes::Apply,
		&Tweaks::Difficulty::Apply,
		&Tweaks::DifficultyEffects::Apply,
		&Tweaks::ActionPoints::Apply,
		&Tweaks::CharacterStats::Apply,
		&Tweaks::ActorValues::Apply,
		&Tweaks::DamageFormulas::Apply,
		&Tweaks::PowerArmor::Apply,
		&Tweaks::Economy::Apply,
		&Tweaks::Progression::Apply,
		&Tweaks::VATS::Apply,
		&Tweaks::Skills::Apply,
		&Tweaks::Sneak::Apply,
		&Tweaks::CompanionsAffinity::Apply,
		&Tweaks::CombatPerks::Apply,
		&Tweaks::Settlements::Apply,
		&Tweaks::SurvivalCarryWeight::Apply,
		&Tweaks::Survival::Apply
	};

	void ApplyTweaks(const char* a_probeLabel)
	{
		for (const auto apply : kTweakApplyFunctions) {
			apply();
		}
		// Bust derived AV cache last so the engine sees all GMST writes from this pass.
		Tweaks::PlayerRefresh::ResetDerivedActorValues();
		Diagnostics::ActorValueProbe::MaybeRun(a_probeLabel);
		Diagnostics::HCManagerProbe::MaybeRun(a_probeLabel);
	}

	class MenuSink :
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuSink* GetSingleton()
		{
			static MenuSink instance;
			return std::addressof(instance);
		}

		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent&                 a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			// SafeTravel needs both edges of LoadingMenu (snapshot on open,
			// clamp on close) so route it before the opening/closing split.
			Hooks::SafeTravel::OnMenuOpenClose(a_event);

			if (a_event.opening) {
				Diagnostics::SurvivalObserver::OnMenuOpenClose(a_event);
				SleepWait::Integration::OnMenuOpenClose(a_event);
				return RE::BSEventNotifyControl::kContinue;
			}
			// PauseMenu close: the player just changed MCM settings, re-apply
			// all toggles / byte-patches / magnitudes.
			if (a_event.menuName == "PauseMenu") {
				MCM::Settings::Update();
				ApplyTweaks("PauseMenu");
				Diagnostics::SurvivalObserver::OnMenuOpenClose(a_event);
				SleepWait::Integration::OnMenuOpenClose(a_event);
				return RE::BSEventNotifyControl::kContinue;
			}
			// LoadingMenu close: the player has fully spawned into a save /
			// new game, meshes and the form DB are warm. This is the first
			// reliable moment to touch forms on OG — the F4SE kPostLoadGame
			// / kNewGame messages fire on worker threads mid-init and crash.
			if (a_event.menuName == "LoadingMenu") {
				ApplyTweaks("LoadingMenu");
				Diagnostics::SurvivalObserver::OnMenuOpenClose(a_event);
				SleepWait::Integration::OnMenuOpenClose(a_event);
				return RE::BSEventNotifyControl::kContinue;
			}
			Diagnostics::SurvivalObserver::OnMenuOpenClose(a_event);
			SleepWait::Integration::OnMenuOpenClose(a_event);
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void RegisterMenuSink()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			REX::WARN("UI singleton unavailable; MCM live-reload disabled");
			return;
		}
		ui->RegisterSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
	}

	void MessageHandler(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->type) {
			case F4SE::MessagingInterface::kPreLoadGame:
			case F4SE::MessagingInterface::kPostLoadGame:
			case F4SE::MessagingInterface::kNewGame:
				// Reset per-save caches; do not touch forms here on OG.
				Tweaks::ActorValues::ResetSnapshots();
				Tweaks::SurvivalCarryWeight::ResetSnapshots();
				Diagnostics::HCManagerProbe::Reset();
				Survival::HCManagerScript::Reset();
				break;
			case F4SE::MessagingInterface::kGameDataReady:
				MCM::Settings::Update();
				Diagnostics::DumpDefaultObjects::MaybeRun();
				break;
			case F4SE::MessagingInterface::kGameLoaded:
				RegisterMenuSink();
				Diagnostics::SurvivalObserver::OnF4SEMessage(a_msg->type);
				SleepWait::Integration::OnF4SEMessage(a_msg->type);
				break;
			default:
				break;
		}
	}
}

F4SE_EXPORT bool F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, {
		.logName        = "HouseRules",
		.trampoline     = true,
		.trampolineSize = 4 * 1024,  // enough for current call hooks without exhausting F4SE's branch pool
	});

	REX::INFO("House Rules loading");

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		REX::ERROR("failed to register message listener");
		return false;
	}

	Diagnostics::SurvivalObserver::Install();
	Hooks::Unlocks::Install();
	Hooks::GodMode::Install();
	Hooks::ExitSave::Install();
	SleepWait::Integration::Install();

	REX::INFO("House Rules loaded");
	return true;
}
