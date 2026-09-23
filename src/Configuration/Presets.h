#pragma once

#include "Configuration/SettingsPersistence.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HouseRules::Configuration
{
	enum class PresetOrigin : std::uint8_t
	{
		kBuiltIn,
		kShipped,
		kUser
	};

	struct Preset
	{
		std::string id;
		std::string name;
		std::string description;
		PresetOrigin origin {};
		std::filesystem::path path;
	};

	struct PresetDirectories
	{
		std::filesystem::path shipped;
		std::filesystem::path user;
	};

	struct PresetDiscovery
	{
		std::vector<Preset> presets;
		std::vector<std::string> warnings;
	};

	struct PresetResolution
	{
		bool success {};
		std::string message;
		std::vector<SettingChange> changes;
		std::vector<std::string> warnings;
	};

	struct PresetSaveResult
	{
		bool success {};
		std::string message;
		Preset preset;
	};

	// Presets cover exposed gameplay settings; the General page (plugin switch, logging) is left alone.
	[[nodiscard]] bool IsPresetScoped(const SettingsCatalog::Descriptor& a_descriptor) noexcept;

	// Built-in Vanilla first, then shipped and user preset files, each sorted by name.
	[[nodiscard]] PresetDiscovery DiscoverPresets(const PresetDirectories& a_directories);

	// Presets are deltas from vanilla: scoped settings a preset omits resolve to their defaults.
	[[nodiscard]] PresetResolution ResolvePreset(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    const Preset& a_preset);

	// Writes the scoped values that differ from their defaults to a new user preset file.
	[[nodiscard]] PresetSaveResult SaveUserPreset(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_values,
	    std::string_view a_name,
	    const std::filesystem::path& a_userDirectory);
}  // namespace HouseRules::Configuration
