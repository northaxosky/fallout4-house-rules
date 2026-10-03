#pragma once

#include "Configuration/Presets.h"
#include "Configuration/SettingsPersistence.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace HouseRules::Configuration
{
	enum class Frontend : std::uint8_t
	{
		kMCM,
		kDearModdingUI
	};

	struct Snapshot
	{
		std::uint64_t revision {};
		std::vector<SettingsCatalog::Value> values;
	};

	struct OperationResult
	{
		bool success {};
		std::string message;
		Snapshot snapshot;
		bool changed {};
	};

	struct SettingsNotification
	{
		bool error {};
		std::string message;
	};

	class SettingsRepository
	{
	public:
		static SettingsRepository& GetSingleton() noexcept;

		[[nodiscard]] OperationResult ReloadFromDisk();
		[[nodiscard]] OperationResult SaveUserOverrides(
		    std::span<const SettingChange> a_changes);
		[[nodiscard]] bool ApplyToRuntime(
		    const Snapshot& a_snapshot,
		    std::string& a_error);

		[[nodiscard]] Snapshot GetSnapshot() const;
		[[nodiscard]] std::optional<SettingsCatalog::Value> GetValue(std::size_t a_index) const;
		[[nodiscard]] bool IsCurrent(std::uint64_t a_revision) const;
		[[nodiscard]] bool IsLoaded() const noexcept;

		void PublishStatus(
		    std::uint64_t a_revision,
		    bool a_error,
		    std::string a_message);
		[[nodiscard]] std::string StatusMessage() const;
		[[nodiscard]] bool StatusIsError() const;
		void EnableNotifications();
		void PublishNotification(
		    std::uint64_t a_revision,
		    bool a_error,
		    std::string a_message);
		[[nodiscard]] std::vector<SettingsNotification> TakeNotifications();

	private:
		SettingsRepository() = default;

		mutable std::mutex _lock;
		Snapshot _committed;
		std::string _status { "Waiting for settings to load." };
		bool _statusError {};
		std::vector<SettingsNotification> _notifications;
		bool _notificationsEnabled {};
		std::atomic_bool _loaded { false };
	};

	[[nodiscard]] Frontend SelectedFrontend();
	[[nodiscard]] const char* FrontendName(Frontend a_frontend) noexcept;
	[[nodiscard]] PresetDirectories InstalledPresetDirectories();
}  // namespace HouseRules::Configuration
