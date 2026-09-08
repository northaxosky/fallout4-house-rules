#pragma once

#include "SettingsCatalog.generated.h"

#include <filesystem>
#include <span>
#include <string>
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

	[[nodiscard]] PersistenceResult LoadPersistedValues(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    const PersistencePaths& a_paths);

	[[nodiscard]] PersistenceResult SavePersistedOverrides(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_committed,
	    std::vector<SettingsCatalog::Value> a_requested,
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
}  // namespace HouseRules::Configuration
