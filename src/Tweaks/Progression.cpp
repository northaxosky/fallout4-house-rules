#include "PCH.h"

#include "Tweaks/Progression.h"

#include "Settings.h"
#include "Tweaks/GameSettings.h"

#include <array>
#include <cstdint>

namespace Tweaks::Progression
{
	namespace
	{
		using GameSettings::FloatTarget;
		using GameSettings::IntTarget;
		using GameSettings::Mode;

		const std::array<FloatTarget, 14> kFloatTargets = { {
			// Cooking
			{ "fCookingExpBase", &MCM::Settings::Progression::fCookingExpBase, Mode::Direct, 1.0f  },
			{ "fCookingExpMax",  &MCM::Settings::Progression::fCookingExpMax,  Mode::Direct, 10.0f },
			{ "fCookingExpMult", &MCM::Settings::Progression::fCookingExpMult, Mode::Direct, 0.15f },

			// Weapon/Armor Workbench
			{ "fWorkbenchExperienceBase", &MCM::Settings::Progression::fWorkbenchExpBase, Mode::Direct, 2.0f  },
			{ "fWorkbenchExperienceMax",  &MCM::Settings::Progression::fWorkbenchExpMax,  Mode::Direct, 50.0f },
			{ "fWorkbenchExperienceMult", &MCM::Settings::Progression::fWorkbenchExpMult, Mode::Direct, 0.03f },

			// Settlement Workshop
			{ "fWorkshopExperienceBase", &MCM::Settings::Progression::fWorkshopExpBase, Mode::Direct, 2.0f  },
			{ "fWorkshopExperienceMax",  &MCM::Settings::Progression::fWorkshopExpMax,  Mode::Direct, 25.0f },
			{ "fWorkshopExperienceMult", &MCM::Settings::Progression::fWorkshopExpMult, Mode::Direct, 0.10f },

			// Lockpick rewards
			{ "fLockpickXPRewardEasy",     &MCM::Settings::Progression::fLockpickXPApprentice, Mode::Direct,  5.0f },
			{ "fLockpickXPRewardAverage",  &MCM::Settings::Progression::fLockpickXPAdept,      Mode::Direct, 10.0f },
			{ "fLockpickXPRewardHard",     &MCM::Settings::Progression::fLockpickXPExpert,     Mode::Direct, 15.0f },
			{ "fLockpickXPRewardVeryHard", &MCM::Settings::Progression::fLockpickXPMaster,     Mode::Direct, 20.0f },

			// fXPModBase/fXPModMult are owned by Tweaks::Difficulty.
			{ "fXPDeathRewardHealthThreshold", &MCM::Settings::Progression::fXPDeathRewardHealthThreshold, Mode::Direct, 0.25f },
		} };

		const std::array<IntTarget, 6> kIntTargets = { {
			{ "iMineDisarmExperience", &MCM::Settings::Progression::iMineDisarmXP, Mode::Direct, std::int32_t{ 5 } },

			// Kill / discovery XP rewards
			{ "iXPRewardKillOpponent",      &MCM::Settings::Progression::iXPRewardKillOpponent,      Mode::Direct, std::int32_t{ 20 } },
			{ "iXPRewardDiscoverMapMarker", &MCM::Settings::Progression::iXPRewardDiscoverMapMarker, Mode::Direct, std::int32_t{ 20 } },
			{ "iXPRewardDiscoverSecretArea", &MCM::Settings::Progression::iXPRewardDiscoverSecretArea, Mode::Direct, std::int32_t{ 20 } },

			// XP base scalar + level-up bump
			{ "iXPBase",     &MCM::Settings::Progression::iXPBase,     Mode::Direct, std::int32_t{ 200 } },
			{ "iXPBumpBase", &MCM::Settings::Progression::iXPBumpBase, Mode::Direct, std::int32_t{ 75  } },
		} };

		void WarnIfInverted(const char* a_label,
			REX::INI::F32<>& a_base,
			REX::INI::F32<>& a_max,
			const char*       a_key)
		{
			if (a_base.GetValue() > a_max.GetValue()) {
				GameSettings::WarnOnce(
					"Progression",
					a_key,
					std::string{ a_label } + " Base > Max; engine clamps XP between Base and Max");
			}
		}
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}

		GameSettings::Apply("Progression", std::span<const FloatTarget>{ kFloatTargets });
		GameSettings::Apply("Progression", std::span<const IntTarget>{ kIntTargets });

		WarnIfInverted("Cooking",
			MCM::Settings::Progression::fCookingExpBase,
			MCM::Settings::Progression::fCookingExpMax,
			"fCookingExpBase");
		WarnIfInverted("Workbench",
			MCM::Settings::Progression::fWorkbenchExpBase,
			MCM::Settings::Progression::fWorkbenchExpMax,
			"fWorkbenchExpBase");
		WarnIfInverted("Workshop",
			MCM::Settings::Progression::fWorkshopExpBase,
			MCM::Settings::Progression::fWorkshopExpMax,
			"fWorkshopExpBase");
	}
}
