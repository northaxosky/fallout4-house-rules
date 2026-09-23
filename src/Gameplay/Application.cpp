#include "PCH.h"

#include "Gameplay/Application.h"
#include "Gameplay/Lifecycle.h"

#include "Diagnostics/ActorValueProbe.h"
#include "Diagnostics/HCManagerProbe.h"
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
#include "Tweaks/Movement.h"
#include "Tweaks/PlayerRefresh.h"
#include "Tweaks/PowerArmor.h"
#include "Tweaks/Progression.h"
#include "Tweaks/Settlements.h"
#include "Tweaks/Skills.h"
#include "Tweaks/Sneak.h"
#include "Tweaks/Survival.h"
#include "Tweaks/SurvivalCarryWeight.h"
#include "Tweaks/VATS.h"
#include "Tweaks/World.h"

#include <utility>

namespace HouseRules::Gameplay
{
	using namespace std::literals;

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
			&Tweaks::Movement::Apply,
			&Tweaks::World::Apply,
			&Tweaks::Skills::Apply,
			&Tweaks::Sneak::Apply,
			&Tweaks::CompanionsAffinity::Apply,
			&Tweaks::CombatPerks::Apply,
			&Tweaks::Settlements::Apply,
			&Tweaks::SurvivalCarryWeight::Apply,
			&Tweaks::Survival::Apply
		};

		LifecycleState g_lifecycle;

		void ApplyTweaks(const char* a_probeLabel)
		{
			for (const auto apply : kTweakApplyFunctions)
			{
				apply();
			}
			// Bust derived AV cache last so the engine sees all GMST writes.
			Tweaks::PlayerRefresh::ResetDerivedActorValues();
			Diagnostics::ActorValueProbe::MaybeRun(a_probeLabel);
			Diagnostics::HCManagerProbe::MaybeRun(a_probeLabel);
		}

		void ApplySnapshot(
		    const Configuration::Snapshot& a_snapshot,
		    const char* a_probeLabel)
		{
			auto& repository =
			    Configuration::SettingsRepository::GetSingleton();
			switch (EvaluateApply(
			    repository.IsCurrent(a_snapshot.revision),
			    g_lifecycle.IsReady()))
			{
				case ApplyDisposition::kStale:
					return;
				case ApplyDisposition::kDeferred:
					repository.PublishStatus(
					    a_snapshot.revision,
					    false,
					    "Settings saved. Gameplay changes will apply after the next loading screen.");
					return;
				case ApplyDisposition::kApply:
					break;
			}

			std::string error;
			if (!repository.ApplyToRuntime(a_snapshot, error))
			{
				repository.PublishStatus(a_snapshot.revision, true, error);
				REX::ERROR("Settings: {}"sv, error);
				return;
			}
			ApplyTweaks(a_probeLabel);
			repository.PublishStatus(
			    a_snapshot.revision,
			    false,
			    "Settings saved and applied.");
		}
	}  // namespace

	void MarkNotReady() noexcept
	{
		g_lifecycle.MarkNotReady();
	}

	bool IsReady() noexcept
	{
		return g_lifecycle.IsReady();
	}

	void OnMainMenu(bool a_opening) noexcept
	{
		g_lifecycle.OnMainMenu(a_opening);
	}

	void OnLoadingMenuOpened() noexcept
	{
		g_lifecycle.OnLoadingMenu(true);
	}

	void ApplyCurrent(const char* a_probeLabel, bool a_syncSettings)
	{
		auto& repository =
		    Configuration::SettingsRepository::GetSingleton();
		if (!g_lifecycle.IsReady())
		{
			const auto snapshot = repository.GetSnapshot();
			if (!snapshot.values.empty())
			{
				repository.PublishStatus(
				    snapshot.revision,
				    false,
				    "Settings saved. Gameplay changes will apply after the next loading screen.");
			}
			return;
		}
		if (!a_syncSettings)
		{
			ApplyTweaks(a_probeLabel);
			return;
		}
		const auto snapshot = repository.GetSnapshot();
		if (snapshot.values.empty())
		{
			REX::WARN("Settings: no loaded snapshot is available to apply."sv);
			return;
		}
		std::string error;
		if (!repository.ApplyToRuntime(snapshot, error))
		{
			repository.PublishStatus(snapshot.revision, true, error);
			REX::ERROR("Settings: {}"sv, error);
			return;
		}
		ApplyTweaks(a_probeLabel);
		repository.PublishStatus(snapshot.revision, false, "Settings applied.");
	}

	void OnLoadingMenuClosed()
	{
		g_lifecycle.OnLoadingMenu(false);
		const auto tasks = F4SE::GetTaskInterface();
		if (!tasks)
		{
			REX::ERROR(
			    "Settings: F4SE game-thread task interface unavailable; loading completion cannot be confirmed safely."sv);
			return;
		}
		tasks->AddTask([] {
			const auto ui = RE::UI::GetSingleton();
			if (!ui)
			{
				g_lifecycle.MarkNotReady();
				REX::WARN(
				    "Settings: UI singleton unavailable; gameplay application remains deferred."sv);
				return;
			}
			const auto mainMenuOpen =
			    ui->GetMenuOpen(RE::MainMenu::MENU_NAME);
			if (!g_lifecycle.ConfirmLoadingComplete(mainMenuOpen))
			{
				return;
			}
			ApplyCurrent(
			    "LoadingMenu",
			    Configuration::SelectedFrontend() ==
			        Configuration::Frontend::kDearModdingUI);
		});
	}

	bool QueueApply(Configuration::Snapshot a_snapshot)
	{
		const auto tasks = F4SE::GetTaskInterface();
		if (!tasks)
		{
			Configuration::SettingsRepository::GetSingleton().PublishStatus(
			    a_snapshot.revision,
			    true,
			    "Settings were saved, but the F4SE game-thread task interface is unavailable.");
			REX::ERROR(
			    "Settings: F4SE game-thread task interface unavailable; changes remain saved but unapplied."sv);
			return false;
		}
		tasks->AddTask([snapshot = std::move(a_snapshot)] {
			ApplySnapshot(snapshot, "DearModdingUI");
		});
		return true;
	}
}  // namespace HouseRules::Gameplay
