#include "PCH.h"

#include "Tweaks/VATS.h"

#include "Settings.h"
#include "Tweaks/GameSettings.h"

#include <array>

namespace Tweaks::VATS
{
	namespace
	{
		using GameSettings::FloatTarget;
		using GameSettings::Mode;

		const std::array<FloatTarget, 5> kTargets = { {
			{ "fVATSMaxEngageDistance",    &MCM::Settings::VATS::fVATSMaxEngageDistance,    Mode::Direct, 5000.0f },
			{ "fVATSTimeMultTargetSelect", &MCM::Settings::VATS::fVATSTimeMultTargetSelect, Mode::Direct, 0.04f   },
			{ "fVATSPlayerDamageMult",     &MCM::Settings::VATS::fVATSPlayerDamageMult,     Mode::Direct, 0.10f   },
			{ "fVATSCriticalChargeBase",   &MCM::Settings::VATS::fVATSCriticalChargeBase,   Mode::Direct, 5.0f    },
			{ "fVATSCriticalChargeMult",   &MCM::Settings::VATS::fVATSCriticalChargeMult,   Mode::Direct, 1.5f    },
		} };
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}

		GameSettings::Apply("VATS", std::span<const FloatTarget>{ kTargets });
	}
}
