#include "PCH.h"

#include "Tweaks/Movement.h"

#include "Settings.h"
#include "Tweaks/GameSettings.h"

#include <array>

namespace Tweaks::Movement
{
	namespace
	{
		using GameSettings::FloatTarget;
		using GameSettings::Mode;

		// Player-only fall constants; the NPC variants stay vanilla.
		const std::array<FloatTarget, 4> kTargets = { {
			{ "fJumpFallHeightMin",      &MCM::Settings::Character::fJumpFallHeightMin,      Mode::Direct, 600.0f },
			{ "fJumpFallHeightMult",     &MCM::Settings::Character::fJumpFallHeightMult,     Mode::Direct, 0.1f   },
			{ "fJumpFallHeightExponent", &MCM::Settings::Character::fJumpFallHeightExponent, Mode::Direct, 1.45f  },
			{ "fJumpHeightMin",          &MCM::Settings::Character::fJumpHeightMin,          Mode::Direct, 90.0f  },
		} };
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}

		GameSettings::Apply("Movement", std::span<const FloatTarget>{ kTargets });
	}
}
