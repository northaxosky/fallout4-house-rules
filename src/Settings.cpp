#include "PCH.h"

#include "Settings.h"

#include "Diagnostics/Logging.h"
#include "Hooks/GodMode.h"
#include "Hooks/Unlocks.h"

namespace MCM
{
	void Settings::Update()
	{
		const auto ini = REX::FIniSettingStore::GetSingleton();
		ini->Init(
			"Data/MCM/Config/HouseRules/settings.ini",
			"Data/MCM/Settings/HouseRules.ini");
		ini->Load();

		Diagnostics::Logging::ApplyLogLevel();

		Hooks::Unlocks::RefreshRuntimePatches();
		Hooks::GodMode::RefreshRuntimePatches();

		// Form-touching Apply paths (Magnitudes, SurvivalCarryWeight, etc.)
		// run from Main.cpp's PauseMenu / LoadingMenu close sinks, not here:
		// kGameDataReady fires on a worker thread mid-init and touching forms
		// there triggers a lazy mesh preload that null-derefs on OG.
	}
}
