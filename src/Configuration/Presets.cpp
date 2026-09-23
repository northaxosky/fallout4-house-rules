#include "Configuration/Presets.h"

#include <SimpleIni.h>
#undef ERROR

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <system_error>
#include <unordered_map>

namespace HouseRules::Configuration
{
	namespace
	{
		constexpr auto kPresetSection = "Preset";
		constexpr std::string_view kGeneralPage = "general";
		constexpr std::size_t kMaximumNameLength = 48;

		[[nodiscard]] std::string Trim(std::string_view a_value)
		{
			const auto first = a_value.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos)
			{
				return {};
			}
			const auto last = a_value.find_last_not_of(" \t\r\n");
			return std::string { a_value.substr(first, last - first + 1) };
		}

		[[nodiscard]] std::string Lower(std::string_view a_value)
		{
			std::string result { a_value };
			std::ranges::transform(result, result.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::tolower(a_char));
			});
			return result;
		}

		[[nodiscard]] std::string SettingKey(std::string_view a_section, std::string_view a_key)
		{
			return Lower(a_section) + '|' + Lower(a_key);
		}

		[[nodiscard]] std::optional<std::string> LoadPresetFile(
		    const std::filesystem::path& a_path,
		    CSimpleIniA& a_file)
		{
			a_file.SetUnicode(true);
			a_file.SetQuotes(true);
			const auto result = a_file.LoadFile(a_path.string().c_str());
			if (result < 0)
			{
				return std::format("Could not read preset '{}' (SimpleIni error {}).", a_path.string(), result);
			}
			return std::nullopt;
		}

		void DiscoverDirectory(
		    const std::filesystem::path& a_directory,
		    PresetOrigin a_origin,
		    std::string_view a_idPrefix,
		    PresetDiscovery& a_result)
		{
			if (a_directory.empty())
			{
				return;
			}
			std::error_code error;
			if (!std::filesystem::is_directory(a_directory, error))
			{
				if (error && error != std::errc::no_such_file_or_directory)
				{
					a_result.warnings.push_back(std::format("Could not inspect preset folder '{}': {}.", a_directory.string(), error.message()));
				}
				return;
			}

			std::vector<Preset> found;
			for (std::filesystem::directory_iterator it { a_directory, error }, end; !error && it != end; it.increment(error))
			{
				const auto& path = it->path();
				std::error_code fileError;
				if (!it->is_regular_file(fileError) || Lower(path.extension().string()) != ".ini")
				{
					continue;
				}
				CSimpleIniA file;
				if (const auto loadError = LoadPresetFile(path, file))
				{
					a_result.warnings.push_back(*loadError);
					continue;
				}
				const auto stem = path.stem().string();
				auto name = Trim(file.GetValue(kPresetSection, "Name", ""));
				found.push_back({ std::format("{}:{}", a_idPrefix, Lower(stem)),
				                  name.empty() ? stem : std::move(name),
				                  Trim(file.GetValue(kPresetSection, "Description", "")),
				                  a_origin,
				                  path });
			}
			if (error)
			{
				a_result.warnings.push_back(std::format("Could not list preset folder '{}': {}.", a_directory.string(), error.message()));
			}
			std::ranges::sort(found, {}, [](const Preset& a_preset) { return Lower(a_preset.name); });
			std::ranges::move(found, std::back_inserter(a_result.presets));
		}

		[[nodiscard]] bool IsValidNameCharacter(unsigned char a_char) noexcept
		{
			return std::isalnum(a_char) || a_char == ' ' || a_char == '-' || a_char == '_';
		}

		[[nodiscard]] bool IsReservedWindowsName(std::string_view a_stem)
		{
			static constexpr std::array<std::string_view, 22> kReserved {
				"con", "prn", "aux", "nul",
				"com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
				"lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
			};
			const auto lowered = Lower(a_stem);
			return std::ranges::any_of(kReserved, [&](std::string_view a_reserved) { return a_reserved == lowered; });
		}
	}  // namespace

	bool IsPresetScoped(const SettingsCatalog::Descriptor& a_descriptor) noexcept
	{
		return a_descriptor.exposed && a_descriptor.pageId != kGeneralPage;
	}

	PresetDiscovery DiscoverPresets(const PresetDirectories& a_directories)
	{
		PresetDiscovery result;
		result.presets.push_back({ "builtin:vanilla",
		                           "Vanilla",
		                           "Every gameplay setting at its vanilla default.",
		                           PresetOrigin::kBuiltIn,
		                           {} });
		DiscoverDirectory(a_directories.shipped, PresetOrigin::kShipped, "shipped", result);
		DiscoverDirectory(a_directories.user, PresetOrigin::kUser, "user", result);
		return result;
	}

	PresetResolution ResolvePreset(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    const Preset& a_preset)
	{
		std::unordered_map<std::string, std::size_t> scoped;
		for (std::size_t index = 0; index < a_descriptors.size(); ++index)
		{
			if (IsPresetScoped(a_descriptors[index]))
			{
				scoped.emplace(SettingKey(a_descriptors[index].section, a_descriptors[index].key), index);
			}
		}

		PresetResolution result { true, std::format("Applied preset '{}'.", a_preset.name) };
		std::vector<std::optional<SettingsCatalog::Value>> overrides(a_descriptors.size());
		if (a_preset.origin != PresetOrigin::kBuiltIn)
		{
			CSimpleIniA file;
			if (const auto error = LoadPresetFile(a_preset.path, file))
			{
				return { false, *error };
			}
			CSimpleIniA::TNamesDepend sections;
			file.GetAllSections(sections);
			for (const auto& section : sections)
			{
				if (Lower(section.pItem) == Lower(kPresetSection))
				{
					continue;
				}
				CSimpleIniA::TNamesDepend keys;
				file.GetAllKeys(section.pItem, keys);
				for (const auto& key : keys)
				{
					const auto found = scoped.find(SettingKey(section.pItem, key.pItem));
					if (found == scoped.end())
					{
						result.warnings.push_back(std::format(
						    "Preset '{}' ignores [{}]{}: not a preset-managed setting.",
						    a_preset.name, section.pItem, key.pItem));
						continue;
					}
					const auto& descriptor = a_descriptors[found->second];
					const auto parsed = ParseIniValue(descriptor, file.GetValue(section.pItem, key.pItem, ""));
					std::string error;
					if (!parsed || !ValidateRequestedValue(descriptor, *parsed, error))
					{
						return { false, std::format(
						                    "Preset '{}' has an invalid or out-of-range value for [{}]{}.",
						                    a_preset.name, section.pItem, key.pItem) };
					}
					overrides[found->second] = *parsed;
				}
			}
		}

		for (std::size_t index = 0; index < a_descriptors.size(); ++index)
		{
			if (IsPresetScoped(a_descriptors[index]))
			{
				result.changes.push_back({ index, overrides[index].value_or(a_descriptors[index].defaultValue) });
			}
		}
		return result;
	}

	PresetSaveResult SaveUserPreset(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_values,
	    std::string_view a_name,
	    const std::filesystem::path& a_userDirectory)
	{
		if (a_values.size() != a_descriptors.size())
		{
			return { false, "Settings are not loaded yet." };
		}
		const auto name = Trim(a_name);
		if (name.empty() || !std::ranges::all_of(name, [](unsigned char a_char) { return IsValidNameCharacter(a_char); }))
		{
			return { false, "Use letters, numbers, spaces, hyphens, or underscores for the preset name." };
		}
		if (name.size() > kMaximumNameLength)
		{
			return { false, std::format("Preset names are limited to {} characters.", kMaximumNameLength) };
		}
		if (IsReservedWindowsName(name))
		{
			return { false, std::format("'{}' is reserved by Windows; choose another name.", name) };
		}

		std::error_code error;
		std::filesystem::create_directories(a_userDirectory, error);
		if (error)
		{
			return { false, std::format("Could not create '{}': {}.", a_userDirectory.string(), error.message()) };
		}
		const auto target = a_userDirectory / (name + ".ini");
		const auto exists = std::filesystem::exists(target, error);
		if (error)
		{
			return { false, std::format("Could not inspect '{}': {}.", target.string(), error.message()) };
		}
		if (exists)
		{
			return { false, std::format("A preset file named '{}' already exists.", target.filename().string()) };
		}

		CSimpleIniA file;
		file.SetUnicode(true);
		file.SetValue(kPresetSection, "Name", name.c_str());
		file.SetValue(kPresetSection, "Description", "Saved from House Rules settings.");
		for (std::size_t index = 0; index < a_descriptors.size(); ++index)
		{
			const auto& descriptor = a_descriptors[index];
			if (!IsPresetScoped(descriptor) || a_values[index] == descriptor.defaultValue)
			{
				continue;
			}
			std::string validationError;
			if (!ValidateRequestedValue(descriptor, a_values[index], validationError))
			{
				return { false, std::move(validationError) };
			}
			const std::string section { descriptor.section };
			const std::string key { descriptor.key };
			if (file.SetValue(section.c_str(), key.c_str(), FormatIniValue(a_values[index]).c_str()) < 0)
			{
				return { false, std::format("Could not stage [{}]{} for the preset.", section, key) };
			}
		}

		auto temporary = target;
		temporary += ".tmp";
		std::filesystem::remove(temporary, error);
		if (error)
		{
			return { false, std::format("Could not remove stale '{}': {}.", temporary.string(), error.message()) };
		}
		if (const auto saved = file.SaveFile(temporary.string().c_str()); saved < 0)
		{
			std::filesystem::remove(temporary, error);
			return { false, std::format("Could not write '{}' (SimpleIni error {}).", temporary.string(), saved) };
		}
		std::filesystem::rename(temporary, target, error);
		if (error)
		{
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return { false, std::format("Could not save '{}': {}.", target.string(), error.message()) };
		}

		const auto stem = target.stem().string();
		return { true,
		         std::format("Saved preset '{}'.", name),
		         { std::format("user:{}", Lower(stem)), name, "Saved from House Rules settings.", PresetOrigin::kUser, target } };
	}
}  // namespace HouseRules::Configuration
