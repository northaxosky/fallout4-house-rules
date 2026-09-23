#pragma once

#include "SettingsCatalog.generated.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HouseRules::Configuration
{
	struct PersistencePaths
	{
		std::filesystem::path defaults;
		std::filesystem::path userOverrides;
	};

	struct PersistenceResult
	{
		bool success {};
		std::string message;
		std::vector<SettingsCatalog::Value> values;
		std::vector<std::string> warnings;
	};

	struct SettingChange
	{
		std::size_t index;
		SettingsCatalog::Value value;
	};

	[[nodiscard]] PersistenceResult LoadPersistedValues(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    const PersistencePaths& a_paths);

	[[nodiscard]] PersistenceResult SavePersistedOverrides(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_committed,
	    std::vector<SettingsCatalog::Value> a_requested,
	    const std::filesystem::path& a_userPath);

	[[nodiscard]] PersistenceResult SavePersistedOverrides(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_committed,
	    std::span<const SettingChange> a_changes,
	    const std::filesystem::path& a_userPath);

	[[nodiscard]] bool ValidateRequestedValue(
	    const SettingsCatalog::Descriptor& a_descriptor,
	    const SettingsCatalog::Value& a_value,
	    std::string& a_error);

	[[nodiscard]] SettingsCatalog::Value QuantizeUiValue(
	    const SettingsCatalog::Descriptor& a_descriptor,
	    SettingsCatalog::Value a_value);

	[[nodiscard]] bool CanUseNativeControlQuantization(
	    const SettingsCatalog::Descriptor& a_descriptor) noexcept;

	// Parses one INI value with the same rules as persisted settings.
	[[nodiscard]] std::optional<SettingsCatalog::Value> ParseIniValue(
	    const SettingsCatalog::Descriptor& a_descriptor,
	    std::string_view a_raw);

	// Round-trip text form accepted by ParseIniValue.
	[[nodiscard]] std::string FormatIniValue(const SettingsCatalog::Value& a_value);
}  // namespace HouseRules::Configuration
