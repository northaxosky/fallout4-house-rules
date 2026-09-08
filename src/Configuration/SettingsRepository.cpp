#include "PCH.h"

#include "Configuration/SettingsPersistence.h"
#include "Configuration/SettingsRepository.h"

#include "Diagnostics/Logging.h"
#include "Hooks/GodMode.h"
#include "Hooks/Unlocks.h"

#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>

namespace HouseRules::Configuration
{
	using namespace std::literals;

	namespace
	{
		constexpr auto kDefaultsPath = "Data/MCM/Config/HouseRules/settings.ini";
		constexpr auto kUserPath = "Data/MCM/Settings/HouseRules.ini";
		constexpr auto kFrontendPath = "Data/F4SE/Plugins/HouseRules.frontend.ini";

		[[nodiscard]] std::string Trim(std::string a_value)
		{
			const auto first = a_value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
			{
				return {};
			}
			const auto last = a_value.find_last_not_of(" \t\r\n");
			return a_value.substr(first, last - first + 1);
		}

		[[nodiscard]] std::string Lower(std::string a_value)
		{
			std::ranges::transform(a_value, a_value.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::tolower(a_char));
			});
			return a_value;
		}
	}  // namespace

	SettingsRepository& SettingsRepository::GetSingleton() noexcept
	{
		static SettingsRepository singleton;
		return singleton;
	}

	OperationResult SettingsRepository::ReloadFromDisk()
	{
		const auto descriptors = SettingsCatalog::All();
		auto persisted = LoadPersistedValues(
		    descriptors,
		    { kDefaultsPath, kUserPath });
		if (!persisted.success)
		{
			PublishStatus(0, true, persisted.message);
			return { false, persisted.message, GetSnapshot() };
		}
		for (const auto& warning : persisted.warnings)
		{
			REX::WARN("Settings: {}"sv, warning);
		}
		Snapshot next;
		next.values = std::move(persisted.values);
		for (std::size_t index = 0; index < descriptors.size(); ++index)
		{
			if (!descriptors[index].write(next.values[index]))
			{
				const auto error = std::format(
				    "Could not load [{}]{} because its value type is invalid.",
				    descriptors[index].section,
				    descriptors[index].key);
				PublishStatus(0, true, error);
				return { false, error, GetSnapshot() };
			}
		}

		Diagnostics::Logging::ApplyLogLevel();
		Hooks::Unlocks::RefreshRuntimePatches();
		Hooks::GodMode::RefreshRuntimePatches();

		{
			std::scoped_lock lock { _lock };
			next.revision = _committed.revision + 1;
			_committed = next;
			_status = "Settings loaded.";
			_statusError = false;
		}
		_loaded.store(true, std::memory_order_release);
		return { true, "Settings loaded.", std::move(next) };
	}

	OperationResult SettingsRepository::SaveUserOverrides(
	    std::vector<SettingsCatalog::Value> a_values)
	{
		const auto descriptors = SettingsCatalog::All();
		const auto committed = GetSnapshot();
		auto persisted = SavePersistedOverrides(
		    descriptors,
		    committed.values,
		    std::move(a_values),
		    kUserPath);
		if (!persisted.success)
		{
			PublishStatus(0, true, persisted.message);
			return { false, persisted.message, GetSnapshot() };
		}

		Snapshot next;
		{
			std::scoped_lock lock { _lock };
			next.revision = _committed.revision + 1;
			next.values = std::move(persisted.values);
			_committed = next;
			_status = "Settings saved; gameplay application is pending.";
			_statusError = false;
		}
		return {
			true,
			"Settings saved; gameplay application is pending.",
			std::move(next)
		};
	}

	bool SettingsRepository::ApplyToRuntime(
	    const Snapshot& a_snapshot,
	    std::string& a_error)
	{
		const auto descriptors = SettingsCatalog::All();
		if (a_snapshot.values.size() != descriptors.size())
		{
			a_error = "Cannot apply an incomplete settings snapshot.";
			return false;
		}
		for (std::size_t index = 0; index < descriptors.size(); ++index)
		{
			std::string validationError;
			if (!ValidateRequestedValue(
			        descriptors[index],
			        a_snapshot.values[index],
			        validationError))
			{
				a_error = std::move(validationError);
				return false;
			}
			if (!descriptors[index].write(a_snapshot.values[index]))
			{
				a_error = std::format(
				    "Cannot apply [{}]{} because its value type is invalid.",
				    descriptors[index].section,
				    descriptors[index].key);
				return false;
			}
		}
		Diagnostics::Logging::ApplyLogLevel();
		Hooks::Unlocks::RefreshRuntimePatches();
		Hooks::GodMode::RefreshRuntimePatches();
		return true;
	}

	Snapshot SettingsRepository::GetSnapshot() const
	{
		std::scoped_lock lock { _lock };
		return _committed;
	}

	bool SettingsRepository::IsCurrent(std::uint64_t a_revision) const
	{
		std::scoped_lock lock { _lock };
		return _committed.revision == a_revision;
	}

	bool SettingsRepository::IsLoaded() const noexcept
	{
		return _loaded.load(std::memory_order_acquire);
	}

	void SettingsRepository::PublishStatus(
	    std::uint64_t a_revision,
	    bool a_error,
	    std::string a_message)
	{
		std::scoped_lock lock { _lock };
		if (a_revision != 0 && a_revision != _committed.revision)
		{
			return;
		}
		_status = std::move(a_message);
		_statusError = a_error;
	}

	std::string SettingsRepository::StatusMessage() const
	{
		std::scoped_lock lock { _lock };
		return _status;
	}

	bool SettingsRepository::StatusIsError() const
	{
		std::scoped_lock lock { _lock };
		return _statusError;
	}

	Frontend SelectedFrontend()
	{
		static const auto selected = [] {
			std::ifstream input { kFrontendPath };
			if (!input)
			{
				return Frontend::kMCM;
			}
			std::string line;
			bool inInterface = false;
			while (std::getline(input, line))
			{
				line = Trim(std::move(line));
				if (line.empty() || line.front() == ';' || line.front() == '#')
				{
					continue;
				}
				if (line.front() == '[' && line.back() == ']')
				{
					inInterface =
					    Lower(Trim(line.substr(1, line.size() - 2))) ==
					    "interface";
					continue;
				}
				if (!inInterface)
				{
					continue;
				}
				const auto separator = line.find('=');
				if (separator == std::string::npos)
				{
					continue;
				}
				const auto key = Lower(Trim(line.substr(0, separator)));
				const auto value = Lower(Trim(line.substr(separator + 1)));
				if (key != "frontend")
				{
					continue;
				}
				if (value == "dmui")
				{
					return Frontend::kDearModdingUI;
				}
				if (value != "mcm")
				{
					REX::ERROR(
					    "Settings: unknown Frontend='{}' in {}; using mcm."sv,
					    value,
					    kFrontendPath);
				}
				return Frontend::kMCM;
			}
			REX::WARN(
			    "Settings: {} has no [Interface] Frontend value; using mcm."sv,
			    kFrontendPath);
			return Frontend::kMCM;
		}();
		return selected;
	}

	const char* FrontendName(Frontend a_frontend) noexcept
	{
		return a_frontend == Frontend::kDearModdingUI ? "dmui" : "mcm";
	}
}  // namespace HouseRules::Configuration
