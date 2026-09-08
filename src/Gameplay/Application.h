#pragma once

#include "Configuration/SettingsRepository.h"

namespace HouseRules::Gameplay
{
	void MarkNotReady() noexcept;
	[[nodiscard]] bool IsReady() noexcept;
	void OnMainMenu(bool a_opening) noexcept;
	void OnLoadingMenuOpened() noexcept;

	void ApplyCurrent(const char* a_probeLabel, bool a_syncSettings = false);
	void OnLoadingMenuClosed();
	[[nodiscard]] bool QueueApply(Configuration::Snapshot a_snapshot);
}  // namespace HouseRules::Gameplay
