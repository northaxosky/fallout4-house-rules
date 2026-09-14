#include "Configuration/SettingsPersistence.h"

#include <SimpleIni.h>
#undef ERROR

#include <Windows.h>
#undef ERROR

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string_view>

namespace HouseRules::Configuration
{
	namespace
	{
		struct IniDocument
		{
			CSimpleIniA file;
			bool present {};
		};

		[[nodiscard]] std::size_t OptionCount(std::string_view a_options) noexcept
		{
			return a_options.empty() ? 0 : 1 + static_cast<std::size_t>(std::ranges::count(a_options, '|'));
		}

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

		[[nodiscard]] std::string Lower(std::string a_value)
		{
			std::ranges::transform(a_value, a_value.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::tolower(a_char));
			});
			return a_value;
		}

		[[nodiscard]] std::string IniError(
		    std::string_view a_action,
		    const std::filesystem::path& a_path,
		    SI_Error a_error)
		{
			return std::format(
			    "{} '{}' failed (SimpleIni error {}).",
			    a_action,
			    a_path.string(),
			    a_error);
		}

		[[nodiscard]] std::optional<std::string> LoadIni(
		    const std::filesystem::path& a_path,
		    bool a_required,
		    IniDocument& a_document)
		{
			std::error_code error;
			a_document.present = std::filesystem::exists(a_path, error);
			if (error)
			{
				return std::format(
				    "Could not inspect '{}': {}.",
				    a_path.string(),
				    error.message());
			}
			if (!a_document.present)
			{
				if (a_required)
				{
					return std::format(
					    "Required settings defaults '{}' are missing.",
					    a_path.string());
				}
				return std::nullopt;
			}

			a_document.file.SetUnicode(true);
			a_document.file.SetQuotes(true);
			const auto result =
			    a_document.file.LoadFile(a_path.string().c_str());
			if (result < 0)
			{
				return IniError("Parsing", a_path, result);
			}
			return std::nullopt;
		}

		[[nodiscard]] const char* FindRawValue(
		    const IniDocument& a_user,
		    const IniDocument& a_defaults,
		    const SettingsCatalog::Descriptor& a_descriptor)
		{
			if (a_user.present)
			{
				if (const auto value = a_user.file.GetValue(
				        a_descriptor.section.data(),
				        a_descriptor.key.data()))
				{
					return value;
				}
			}
			return a_defaults.file.GetValue(
			    a_descriptor.section.data(),
			    a_descriptor.key.data());
		}

		[[nodiscard]] std::optional<SettingsCatalog::Value> ParseValue(
		    const SettingsCatalog::Descriptor& a_descriptor,
		    std::string_view a_raw)
		{
			using SettingsCatalog::ValueType;
			const auto text = Trim(a_raw);
			switch (a_descriptor.type)
			{
				case ValueType::kBool:
				{
					const auto value = Lower(text);
					if (value == "1" || value == "true" ||
					    value == "yes" || value == "on")
					{
						return true;
					}
					if (value == "0" || value == "false" ||
					    value == "no" || value == "off")
					{
						return false;
					}
					return std::nullopt;
				}
				case ValueType::kFloat:
				{
					double value {};
					const auto* first = text.data();
					const auto* last = first + text.size();
					const auto result =
					    std::from_chars(first, last, value);
					if (result.ec != std::errc {} ||
					    result.ptr != last ||
					    !std::isfinite(value))
					{
						return std::nullopt;
					}
					return value;
				}
				case ValueType::kInt:
				{
					std::int64_t value {};
					const auto* first = text.data();
					const auto* last = first + text.size();
					const auto result =
					    std::from_chars(first, last, value);
					if (result.ec != std::errc {} ||
					    result.ptr != last)
					{
						return std::nullopt;
					}
					return value;
				}
				case ValueType::kString:
					return text;
			}
			return std::nullopt;
		}

		[[nodiscard]] SettingsCatalog::Value NormalizeLoadedValue(
		    const SettingsCatalog::Descriptor& a_descriptor,
		    SettingsCatalog::Value a_value)
		{
			using SettingsCatalog::ValueType;
			switch (a_descriptor.type)
			{
				case ValueType::kBool:
					if (!std::holds_alternative<bool>(a_value))
					{
						return a_descriptor.defaultValue;
					}
					break;
				case ValueType::kString:
					if (!std::holds_alternative<std::string>(a_value))
					{
						return a_descriptor.defaultValue;
					}
					break;
				case ValueType::kFloat:
				{
					auto* value = std::get_if<double>(&a_value);
					if (!value || !std::isfinite(*value))
					{
						return a_descriptor.defaultValue;
					}
					if (a_descriptor.hasRange)
					{
						*value = std::clamp(
						    *value,
						    a_descriptor.minimum,
						    a_descriptor.maximum);
					}
					break;
				}
				case ValueType::kInt:
				{
					auto* value = std::get_if<std::int64_t>(&a_value);
					if (!value)
					{
						return a_descriptor.defaultValue;
					}
					if (!a_descriptor.options.empty())
					{
						const auto count = OptionCount(a_descriptor.options);
						if (count == 0)
						{
							return a_descriptor.defaultValue;
						}
						*value = std::clamp<std::int64_t>(
						    *value,
						    0,
						    static_cast<std::int64_t>(count - 1));
					}
					else if (a_descriptor.hasRange)
					{
						*value = std::clamp(
						    *value,
						    static_cast<std::int64_t>(
						        a_descriptor.minimum),
						    static_cast<std::int64_t>(
						        a_descriptor.maximum));
					}
					break;
				}
			}
			return a_value;
		}

		[[nodiscard]] std::optional<std::string> SetValue(
		    CSimpleIniA& a_file,
		    const SettingsCatalog::Descriptor& a_descriptor,
		    const SettingsCatalog::Value& a_value)
		{
			SI_Error result = SI_FAIL;
			switch (a_descriptor.type)
			{
				case SettingsCatalog::ValueType::kBool:
					result = a_file.SetBoolValue(
					    a_descriptor.section.data(),
					    a_descriptor.key.data(),
					    std::get<bool>(a_value));
					break;
				case SettingsCatalog::ValueType::kFloat:
					result = a_file.SetDoubleValue(
					    a_descriptor.section.data(),
					    a_descriptor.key.data(),
					    std::get<double>(a_value));
					break;
				case SettingsCatalog::ValueType::kInt:
					result = a_file.SetLongValue(
					    a_descriptor.section.data(),
					    a_descriptor.key.data(),
					    static_cast<long>(std::get<std::int64_t>(a_value)));
					break;
				case SettingsCatalog::ValueType::kString:
					result = a_file.SetValue(
					    a_descriptor.section.data(),
					    a_descriptor.key.data(),
					    std::get<std::string>(a_value).c_str());
					break;
			}
			if (result < 0)
			{
				return std::format(
				    "Could not stage [{}]{} for persistence (SimpleIni error {}).",
				    a_descriptor.section,
				    a_descriptor.key,
				    result);
			}
			return std::nullopt;
		}

		[[nodiscard]] std::string WindowsError(
		    std::string_view a_action,
		    const std::filesystem::path& a_path,
		    DWORD a_error)
		{
			return std::format(
			    "{} '{}' failed (Windows error {}).",
			    a_action,
			    a_path.string(),
			    a_error);
		}
	}  // namespace

	bool ValidateRequestedValue(
	    const SettingsCatalog::Descriptor& a_descriptor,
	    const SettingsCatalog::Value& a_value,
	    std::string& a_error)
	{
		using SettingsCatalog::ValueType;
		const auto invalid = [&] {
			a_error = std::format(
			    "Invalid value requested for [{}]{}.",
			    a_descriptor.section,
			    a_descriptor.key);
			return false;
		};
		switch (a_descriptor.type)
		{
			case ValueType::kBool:
				return std::holds_alternative<bool>(a_value) || invalid();
			case ValueType::kString:
				return std::holds_alternative<std::string>(a_value) || invalid();
			case ValueType::kFloat:
			{
				const auto* value = std::get_if<double>(&a_value);
				if (!value || !std::isfinite(*value))
				{
					return invalid();
				}
				if (a_descriptor.hasRange &&
				    (*value < a_descriptor.minimum ||
				     *value > a_descriptor.maximum))
				{
					return invalid();
				}
				return true;
			}
			case ValueType::kInt:
			{
				const auto* value = std::get_if<std::int64_t>(&a_value);
				if (!value)
				{
					return invalid();
				}
				if (!a_descriptor.options.empty())
				{
					const auto count = OptionCount(a_descriptor.options);
					if (count == 0 || *value < 0 ||
					    *value >= static_cast<std::int64_t>(count))
					{
						return invalid();
					}
				}
				else if (a_descriptor.hasRange &&
				         (*value <
				              static_cast<std::int64_t>(
				                  a_descriptor.minimum) ||
				          *value >
				              static_cast<std::int64_t>(
				                  a_descriptor.maximum)))
				{
					return invalid();
				}
				return true;
			}
		}
		return invalid();
	}

	SettingsCatalog::Value QuantizeUiValue(
	    const SettingsCatalog::Descriptor& a_descriptor,
	    SettingsCatalog::Value a_value)
	{
		if (a_value == a_descriptor.defaultValue)
		{
			return a_value;
		}
		if (!a_descriptor.hasRange || a_descriptor.step <= 0.0)
		{
			return a_value;
		}
		if (auto* value = std::get_if<double>(&a_value))
		{
			if (!std::isfinite(*value))
			{
				return a_value;
			}
			*value = a_descriptor.minimum +
			         std::round(
			             (*value - a_descriptor.minimum) /
			             a_descriptor.step) *
			             a_descriptor.step;
			*value = std::clamp(
			    *value,
			    a_descriptor.minimum,
			    a_descriptor.maximum);
		}
		else if (auto* integer = std::get_if<std::int64_t>(&a_value))
		{
			const auto minimum =
			    static_cast<std::int64_t>(a_descriptor.minimum);
			const auto maximum =
			    static_cast<std::int64_t>(a_descriptor.maximum);
			const auto step =
			    static_cast<std::int64_t>(a_descriptor.step);
			if (step > 0)
			{
				const auto distance = *integer - minimum;
				*integer = minimum +
				           static_cast<std::int64_t>(
				               std::llround(
				                   static_cast<double>(distance) /
				                   static_cast<double>(step))) *
				               step;
				*integer = std::clamp(*integer, minimum, maximum);
			}
		}
		return a_value;
	}

	bool CanUseNativeControlQuantization(
	    const SettingsCatalog::Descriptor& a_descriptor) noexcept
	{
		if (!a_descriptor.hasRange || a_descriptor.step <= 0.0)
		{
			return false;
		}
		if (const auto* value =
		        std::get_if<double>(&a_descriptor.defaultValue))
		{
			if (!std::isfinite(*value))
			{
				return false;
			}
			const auto steps =
			    (*value - a_descriptor.minimum) / a_descriptor.step;
			return std::abs(steps - std::round(steps)) <= 1e-9;
		}
		if (const auto* value =
		        std::get_if<std::int64_t>(&a_descriptor.defaultValue))
		{
			const auto step =
			    static_cast<std::int64_t>(a_descriptor.step);
			if (step <= 0)
			{
				return false;
			}
			const auto minimum =
			    static_cast<std::int64_t>(a_descriptor.minimum);
			return (*value - minimum) % step == 0;
		}
		return false;
	}

	PersistenceResult LoadPersistedValues(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    const PersistencePaths& a_paths)
	{
		IniDocument defaults;
		if (const auto error = LoadIni(a_paths.defaults, true, defaults))
		{
			return { false, *error };
		}
		IniDocument user;
		if (const auto error = LoadIni(a_paths.userOverrides, false, user))
		{
			return { false, *error };
		}

		PersistenceResult result { true, "Settings loaded." };
		result.values.reserve(a_descriptors.size());
		for (const auto& descriptor : a_descriptors)
		{
			const auto raw = FindRawValue(user, defaults, descriptor);
			if (!raw)
			{
				result.values.push_back(descriptor.defaultValue);
				result.warnings.push_back(std::format(
				    "Missing [{}]{}; using the declared default.",
				    descriptor.section,
				    descriptor.key));
				continue;
			}
			const auto parsed = ParseValue(descriptor, raw);
			if (!parsed)
			{
				result.values.push_back(descriptor.defaultValue);
				result.warnings.push_back(std::format(
				    "Invalid [{}]{}; using the declared default.",
				    descriptor.section,
				    descriptor.key));
				continue;
			}
			auto normalized = NormalizeLoadedValue(descriptor, *parsed);
			if (normalized != *parsed)
			{
				result.warnings.push_back(std::format(
				    "Out-of-range [{}]{} was clamped.",
				    descriptor.section,
				    descriptor.key));
			}
			result.values.push_back(std::move(normalized));
		}
		return result;
	}

	PersistenceResult SavePersistedOverrides(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_committed,
	    std::span<const SettingChange> a_changes,
	    const std::filesystem::path& a_userPath)
	{
		if (a_committed.size() != a_descriptors.size())
		{
			return { false, "Settings are not loaded yet." };
		}
		std::vector<SettingsCatalog::Value> requested(a_committed.begin(), a_committed.end());
		for (const auto& change : a_changes)
		{
			if (change.index >= a_descriptors.size() || !a_descriptors[change.index].exposed)
			{
				return { false, "The requested setting is not editable." };
			}
			std::string error;
			if (!ValidateRequestedValue(a_descriptors[change.index], change.value, error))
			{
				return { false, std::move(error) };
			}
			requested[change.index] = change.value;
		}
		if (std::ranges::equal(requested, a_committed))
		{
			return { true, "Settings unchanged.", std::move(requested) };
		}
		return SavePersistedOverrides(a_descriptors, a_committed, std::move(requested), a_userPath);
	}

	PersistenceResult SavePersistedOverrides(
	    std::span<const SettingsCatalog::Descriptor> a_descriptors,
	    std::span<const SettingsCatalog::Value> a_committed,
	    std::vector<SettingsCatalog::Value> a_requested,
	    const std::filesystem::path& a_userPath)
	{
		if (a_requested.size() != a_descriptors.size() ||
		    a_committed.size() != a_descriptors.size())
		{
			return {
				false,
				std::format(
				    "Settings snapshot has {} requested and {} committed values; expected {}.",
				    a_requested.size(),
				    a_committed.size(),
				    a_descriptors.size())
			};
		}
		for (std::size_t index = 0; index < a_descriptors.size(); ++index)
		{
			const auto& descriptor = a_descriptors[index];
			if (!descriptor.exposed)
			{
				a_requested[index] = a_committed[index];
				continue;
			}
			std::string error;
			if (!ValidateRequestedValue(
			        descriptor,
			        a_requested[index],
			        error))
			{
				return { false, std::move(error) };
			}
		}

		std::error_code error;
		std::filesystem::create_directories(a_userPath.parent_path(), error);
		if (error)
		{
			return {
				false,
				std::format(
				    "Could not create '{}': {}.",
				    a_userPath.parent_path().string(),
				    error.message())
			};
		}

		CSimpleIniA file;
		file.SetUnicode(true);
		file.SetQuotes(true);
		const auto exists = std::filesystem::exists(a_userPath, error);
		if (error)
		{
			return {
				false,
				std::format(
				    "Could not inspect '{}': {}.",
				    a_userPath.string(),
				    error.message())
			};
		}
		if (exists)
		{
			const auto loadResult =
			    file.LoadFile(a_userPath.string().c_str());
			if (loadResult < 0)
			{
				return {
					false,
					IniError("Parsing", a_userPath, loadResult)
				};
			}
		}

		for (std::size_t index = 0; index < a_descriptors.size(); ++index)
		{
			if (!a_descriptors[index].exposed)
			{
				continue;
			}
			if (const auto setError =
			        SetValue(file, a_descriptors[index], a_requested[index]))
			{
				return { false, *setError };
			}
		}

		auto temporary = a_userPath;
		temporary += ".tmp";
		const auto temporaryExists =
		    std::filesystem::exists(temporary, error);
		if (error)
		{
			return {
				false,
				std::format(
				    "Could not inspect temporary settings file '{}': {}.",
				    temporary.string(),
				    error.message())
			};
		}
		if (temporaryExists)
		{
			const auto removed = std::filesystem::remove(temporary, error);
			if (error || !removed)
			{
				return {
					false,
					std::format(
					    "Could not remove stale temporary settings file '{}': {}.",
					    temporary.string(),
					    error ? error.message() : "path was not removed")
				};
			}
		}

		const auto saveResult =
		    file.SaveFile(temporary.string().c_str());
		if (saveResult < 0)
		{
			return {
				false,
				IniError("Saving temporary settings", temporary, saveResult)
			};
		}

		if (!::MoveFileExW(
		        temporary.c_str(),
		        a_userPath.c_str(),
		        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			const auto moveError = ::GetLastError();
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return {
				false,
				WindowsError(
				    "Replacing settings file",
				    a_userPath,
				    moveError)
			};
		}

		return {
			true,
			"Settings saved; gameplay application is pending.",
			std::move(a_requested)
		};
	}
}  // namespace HouseRules::Configuration
