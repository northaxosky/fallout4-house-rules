#include "PCH.h"

#include "Settings.h"

#include "Configuration/SettingsRepository.h"

namespace HouseRules
{
	void Settings::Update()
	{
		const auto result =
			Configuration::SettingsRepository::GetSingleton().ReloadFromDisk();
		if (!result.success) {
			REX::ERROR("Settings: {}", result.message);
		}

		// Form-touching Apply paths (Magnitudes, SurvivalCarryWeight, etc.)
		// run from Main.cpp's PauseMenu / LoadingMenu close sinks, not here:
		// kGameDataReady fires on a worker thread mid-init and touching forms
		// there triggers a lazy mesh preload that null-derefs on OG.
	}
}
