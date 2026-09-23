#include "PCH.h"

#include "Tweaks/World.h"

#include "Globals/Globals.h"
#include "Settings.h"
#include "Tweaks/GameSettings.h"

#include <array>
#include <cstdint>

namespace Tweaks::World
{
	namespace
	{
		using GameSettings::IntTarget;
		using GameSettings::Mode;

		constexpr std::uint32_t kTimeScaleFormID = 0x3Au;
		constexpr float         kVanillaTimeScale = 20.0f;

		// Lets turning the override off (manually or via a preset) put vanilla time back into the save.
		bool g_timeScaleOverridden = false;

		const std::array<IntTarget, 3> kTargets = { {
			{ "iHoursToRespawnCell",        &MCM::Settings::World::iHoursToRespawnCell,        Mode::Direct, std::int32_t{ 168 } },
			{ "iHoursToRespawnCellCleared", &MCM::Settings::World::iHoursToRespawnCellCleared, Mode::Direct, std::int32_t{ 480 } },
			{ "iDaysToRespawnVendor",       &MCM::Settings::World::iDaysToRespawnVendor,       Mode::Direct, std::int32_t{ 2 }   },
		} };
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}

		GameSettings::Apply("World", std::span<const IntTarget>{ kTargets });

		if (MCM::Settings::World::bTimeScaleOverride.GetValue()) {
			Globals::WriteByFormID(kTimeScaleFormID, MCM::Settings::World::fTimeScale.GetValue());
			g_timeScaleOverridden = true;
		} else if (g_timeScaleOverridden) {
			Globals::WriteByFormID(kTimeScaleFormID, kVanillaTimeScale);
			g_timeScaleOverridden = false;
		}
	}
}
