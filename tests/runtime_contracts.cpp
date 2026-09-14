#include "Configuration/SettingsPersistence.h"
#include "Gameplay/Lifecycle.h"
#include "UI/ActionGate.h"

#include <DearModdingUI/Client.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{
	using HouseRules::SettingsCatalog::Descriptor;
	using HouseRules::SettingsCatalog::Value;
	using HouseRules::SettingsCatalog::ValueType;

	int g_failures {};

	void Check(bool a_condition, std::string_view a_message)
	{
		if (!a_condition)
		{
			++g_failures;
			std::cerr << "FAIL: " << a_message << '\n';
		}
	}

	void WriteText(
	    const std::filesystem::path& a_path,
	    std::string_view a_text)
	{
		std::filesystem::create_directories(a_path.parent_path());
		std::ofstream output { a_path, std::ios::binary | std::ios::trunc };
		output.write(
		    a_text.data(),
		    static_cast<std::streamsize>(a_text.size()));
	}

	[[nodiscard]] std::string ReadText(
	    const std::filesystem::path& a_path)
	{
		std::ifstream input { a_path, std::ios::binary };
		return {
			std::istreambuf_iterator<char> { input },
			std::istreambuf_iterator<char> {}
		};
	}

	[[nodiscard]] Descriptor MakeDescriptor(
	    std::string_view a_id,
	    std::string_view a_section,
	    std::string_view a_key,
	    ValueType a_type,
	    Value a_default,
	    bool a_exposed,
	    double a_minimum = 0.0,
	    double a_maximum = 0.0,
	    double a_step = 0.0,
	    bool a_hasRange = false)
	{
		return {
			a_id,
			a_section,
			a_key,
			a_type,
			std::move(a_default),
			a_exposed,
			"test",
			"Test",
			"Test page",
			"Test",
			"Test",
			"Test",
			"slider",
			a_minimum,
			a_maximum,
			a_step,
			a_hasRange,
			"",
			0,
			nullptr,
			nullptr
		};
	}

	void TestPersistence(const std::filesystem::path& a_root)
	{
		using namespace HouseRules::Configuration;

		const auto defaults = a_root / "defaults.ini";
		const auto user = a_root / "user" / "HouseRules.ini";
		constexpr auto defaultsText =
		    "[Survival]\n"
		    "fCaffeineInducedSleepDelay=2.333\n"
		    "[General]\n"
		    "bEnabled=1\n"
		    "[Hidden]\n"
		    "iSecret=5\n";
		WriteText(defaults, defaultsText);
		WriteText(
		    user,
		    "[Survival]\n"
		    "fCaffeineInducedSleepDelay=2.333\n"
		    "[Hidden]\n"
		    "iSecret=77\n"
		    "[Unknown]\n"
		    "sKeepMe=preserved\n");

		const std::array descriptors {
			MakeDescriptor(
			    "fCaffeineInducedSleepDelay:Survival",
			    "Survival",
			    "fCaffeineInducedSleepDelay",
			    ValueType::kFloat,
			    2.333,
			    true,
			    0.0,
			    24.0,
			    0.1,
			    true),
			MakeDescriptor(
			    "bEnabled:General",
			    "General",
			    "bEnabled",
			    ValueType::kBool,
			    true,
			    true),
			MakeDescriptor(
			    "iSecret:Hidden",
			    "Hidden",
			    "iSecret",
			    ValueType::kInt,
			    std::int64_t { 5 },
			    false)
		};

		const auto defaultsBefore = ReadText(defaults);
		auto loaded = LoadPersistedValues(
		    descriptors,
		    { defaults, user });
		Check(loaded.success, "initial persistence load succeeds");
		Check(
		    std::get<double>(loaded.values[0]) == 2.333,
		    "off-grid declared default loads exactly");
		Check(
		    std::get<std::int64_t>(loaded.values[2]) == 77,
		    "hidden user value loads");

		auto committed = loaded.values;
		for (const auto expected : { 0.0, 1.0, 3.0, 12.0, 2.333 })
		{
			auto requested = committed;
			requested[0] = expected;
			requested[2] = std::int64_t { 999 };
			auto saved = SavePersistedOverrides(
			    descriptors,
			    committed,
			    std::move(requested),
			    user);
			Check(saved.success, "valid value save succeeds");
			if (!saved.success)
			{
				continue;
			}
			committed = saved.values;
			auto restarted = LoadPersistedValues(
			    descriptors,
			    { defaults, user });
			Check(restarted.success, "saved value reload succeeds");
			Check(
			    std::abs(std::get<double>(restarted.values[0]) - expected) <
			        1e-12,
			    "finite in-range value round-trips without step snapping");
			Check(
			    std::get<std::int64_t>(restarted.values[2]) == 77,
			    "hidden user entry remains unchanged");
		}
		const auto userAfterRoundTrips = ReadText(user);
		Check(
		    userAfterRoundTrips.find("sKeepMe") != std::string::npos &&
		        userAfterRoundTrips.find("preserved") != std::string::npos,
		    "unknown user entry is preserved");
		Check(
		    ReadText(defaults) == defaultsBefore,
		    "packaged defaults bytes remain unchanged");

		const auto userBeforeInvalid = ReadText(user);
		auto wrongType = committed;
		wrongType[0] = std::string { "not a number" };
		auto wrongTypeResult = SavePersistedOverrides(
		    descriptors,
		    committed,
		    std::move(wrongType),
		    user);
		Check(!wrongTypeResult.success, "wrong request type is rejected");
		Check(
		    ReadText(user) == userBeforeInvalid,
		    "wrong request type does not replace persisted state");
		Check(
		    std::get<double>(committed[0]) == 2.333,
		    "failed persistence does not replace committed memory state");

		auto nonfinite = committed;
		nonfinite[0] = (std::numeric_limits<double>::quiet_NaN)();
		auto nonfiniteResult = SavePersistedOverrides(
		    descriptors,
		    committed,
		    std::move(nonfinite),
		    user);
		Check(!nonfiniteResult.success, "nonfinite request is rejected");
		Check(
		    ReadText(user) == userBeforeInvalid,
		    "nonfinite request does not replace persisted state");

		const auto temporary = std::filesystem::path {
			user.string() + ".tmp"
		};
		std::filesystem::create_directories(temporary);
		WriteText(temporary / "blocker", "block");
		auto atomicFailure = SavePersistedOverrides(
		    descriptors,
		    committed,
		    committed,
		    user);
		Check(!atomicFailure.success, "blocked temporary sibling fails save");
		Check(
		    ReadText(user) == userBeforeInvalid,
		    "atomic save failure preserves original override file");
		std::filesystem::remove_all(temporary);

		const auto invalidParent = a_root / "parent-is-file";
		WriteText(invalidParent, "not a directory");
		auto ioFailure = SavePersistedOverrides(
		    descriptors,
		    committed,
		    committed,
		    invalidParent / "HouseRules.ini");
		Check(!ioFailure.success, "directory creation I/O error is reported");

		WriteText(
		    user,
		    "[Survival]\n"
		    "fCaffeineInducedSleepDelay=999\n"
		    "[General]\n"
		    "bEnabled=invalid\n");
		auto legacyInvalid = LoadPersistedValues(
		    descriptors,
		    { defaults, user });
		Check(legacyInvalid.success, "legacy invalid file remains loadable");
		Check(
		    std::get<double>(legacyInvalid.values[0]) == 24.0,
		    "legacy out-of-range float is explicitly clamped");
		Check(
		    std::get<bool>(legacyInvalid.values[1]),
		    "legacy malformed bool restores declared default");
		Check(
		    legacyInvalid.warnings.size() >= 2,
		    "legacy normalization produces warnings");
	}

	void TestCompletedChanges(const std::filesystem::path& a_root)
	{
		using namespace HouseRules::Configuration;
		const auto user = a_root / "HouseRules.ini";
		WriteText(user, "[Unknown]\nkeep=untouched\n");
		const std::array descriptors {
			MakeDescriptor("number", "Test", "number", ValueType::kFloat, 2.333, true, 0, 24, 0.1, true),
			MakeDescriptor("toggle", "Test", "toggle", ValueType::kBool, false, true),
			MakeDescriptor("hidden", "Test", "hidden", ValueType::kInt, std::int64_t { 7 }, false)
		};
		std::vector<Value> saved { 2.333, false, std::int64_t { 7 } };
		double edited = 3.0;
		int commits {};
		PersistenceResult result;
		dmui::SettingDescriptor field {
			.id = "number",
			.control = dmui::DoubleSettingControl {},
			.defaultValue = 2.333,
			.binding = dmui::BindSetting([&] { return edited; }, [&](double value) { edited = value; return value; }),
			.onEdit = [&](const dmui::SettingEditEvent& event) {
			    if (!event.completed)
				    return;
			    const std::array changes { SettingChange { 0, edited } };
			    result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { changes }, user);
			    if (result.success)
			    {
				    commits += result.values != saved;
				    saved = result.values;
			    }
			}
		};
		const auto notify = [&](bool changed, bool completed) {
			dmui::setting_detail::NotifySettingEdit(field, { edited, changed, completed });
		};
		const auto original = ReadText(user);
		notify(true, false);
		Check(commits == 0 && ReadText(user) == original, "drag frames do not persist");
		notify(false, true);
		Check(commits == 1 && std::get<double>(saved[0]) == 3.0, "release commits without a same-frame change");
		Check(!std::get<bool>(saved[1]), "completion leaves other fields at their saved values");
		notify(false, true);
		Check(commits == 1, "duplicate completion is a no-op");
		edited = 4.0;
		notify(true, true);
		Check(commits == 2, "discrete completed edit commits once");

		const auto beforeFailure = ReadText(user);
		edited = (std::numeric_limits<double>::quiet_NaN)();
		notify(true, true);
		Check(!result.success && commits == 2, "invalid completion is rejected");
		Check(ReadText(user) == beforeFailure, "invalid completion preserves persisted values");
		edited = 5.0;
		const auto temporary = std::filesystem::path { user.string() + ".tmp" };
		WriteText(temporary / "blocker", "blocked");
		notify(false, true);
		Check(!result.success && commits == 2, "save failure does not commit");
		Check(ReadText(user) == beforeFailure, "save failure preserves the original file");
		std::filesystem::remove_all(temporary);
		notify(false, true);
		Check(result.success && commits == 3, "completion can retry a failed save without changing its value");

		const auto reset = dmui::ResetSettingToDefault(field);
		Check(reset && edited == 2.333 && std::get<double>(saved[0]) == 2.333, "SDK reset persists the exact off-grid default");
		Check(commits == 4, "SDK reset commits once");
		(void)dmui::ResetSettingToDefault(field);
		Check(commits == 4, "resetting an unchanged field is a no-op");

		const std::array toggled { SettingChange { 1, true } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { toggled }, user);
		Check(result.success && std::get<bool>(result.values[1]), "a second field merges into the current snapshot");
		saved = result.values;
		const std::array batch { SettingChange { 0, 6.0 }, SettingChange { 1, false } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { batch }, user);
		Check(result.success && std::get<double>(result.values[0]) == 6.0 && !std::get<bool>(result.values[1]), "a confirmed bulk change saves one complete snapshot");
		saved = result.values;
		const auto beforeRejectedBatch = ReadText(user);
		const std::array invalidBatch { SettingChange { 1, true }, SettingChange { 0, 99.0 } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { invalidBatch }, user);
		Check(!result.success && ReadText(user) == beforeRejectedBatch, "an invalid batch cannot partially save");
		const std::array hidden { SettingChange { 2, std::int64_t { 8 } } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { hidden }, user);
		Check(!result.success, "hidden setting edits are rejected");
		const std::array missing { SettingChange { 99, true } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { missing }, user);
		Check(!result.success, "unknown setting indices are rejected");
		WriteText(temporary / "blocker", "blocked");
		const std::array unchanged { SettingChange { 0, 6.0 } };
		result = SavePersistedOverrides(descriptors, saved, std::span<const SettingChange> { unchanged }, user);
		Check(result.success, "unchanged edits do not attempt a write");
		std::filesystem::remove_all(temporary);
		Check(ReadText(user).find("untouched") != std::string::npos, "completed edits preserve unknown user settings");
	}

	void TestQuantizationAndReset()
	{
		using namespace HouseRules;
		const auto descriptor = MakeDescriptor(
		    "fCaffeineInducedSleepDelay:Survival",
		    "Survival",
		    "fCaffeineInducedSleepDelay",
		    ValueType::kFloat,
		    2.333,
		    true,
		    0.0,
		    24.0,
		    0.1,
		    true);

		Check(
		    std::abs(
		        std::get<double>(
		            Configuration::QuantizeUiValue(descriptor, 1.04)) -
		        1.0) <
		        1e-12,
		    "native UI increments use the minimum as quantization origin");
		Check(
		    std::get<double>(
		        Configuration::QuantizeUiValue(descriptor, 2.333)) ==
		        2.333,
		    "UI quantization preserves the exact off-grid default");
		Check(
		    !Configuration::CanUseNativeControlQuantization(descriptor),
		    "off-grid default bypasses SDK reset quantization");
		const auto onGrid = MakeDescriptor(
		    "on-grid",
		    "Test",
		    "OnGrid",
		    ValueType::kFloat,
		    1.0,
		    true,
		    0.0,
		    5.0,
		    0.1,
		    true);
		Check(
		    Configuration::CanUseNativeControlQuantization(onGrid),
		    "on-grid controls use native minimum-origin quantization");

		double draft = 1.0;
		dmui::DoubleSettingControl control;
		control.range = dmui::NumericSettingRange<double> { 0.0, 24.0 };
		dmui::SettingDescriptor setting {
			.id = "caffeine",
			.label = "Caffeine",
			.control = control,
			.defaultValue = 2.333,
			.binding = dmui::BindSetting(
			    [&] { return draft; },
			    [&](double a_value) {
			        draft = std::get<double>(
			            Configuration::QuantizeUiValue(
			                descriptor,
			                a_value));
			        return draft;
			    }),
			.isEnabled = [] { return true; }
		};
		const auto reset = dmui::ResetSettingToDefault(setting);
		Check(reset.has_value(), "per-control reset executes");
		Check(
		    draft == 2.333,
		    "per-control reset restores exact declared default");
	}

	void TestActionGate()
	{
		using HouseRules::UI::TryMutation;
		int mutations {};
		int saves {};
		int queues {};
		const auto action = [&] {
			++mutations;
			++saves;
			++queues;
		};

		const auto partial = TryMutation(false, true, action);
		Check(!partial.allowed, "partial registration rejects mutation");
		Check(
		    mutations == 0 && saves == 0 && queues == 0,
		    "partial registration rejection has no side effects");

		const auto notLoaded = TryMutation(true, false, action);
		Check(!notLoaded.allowed, "unloaded settings reject mutation");
		Check(
		    mutations == 0 && saves == 0 && queues == 0,
		    "unloaded rejection has no side effects");

		const auto ready = TryMutation(true, true, action);
		Check(ready.allowed, "fully ready action is accepted");
		Check(
		    mutations == 1 && saves == 1 && queues == 1,
		    "fully ready action executes exactly once");
	}

	void TestLifecycle()
	{
		using HouseRules::Gameplay::ApplyDisposition;
		using HouseRules::Gameplay::EvaluateApply;
		using HouseRules::Gameplay::LifecycleState;

		LifecycleState launch;
		Check(!launch.IsReady(), "launch starts not ready");
		int gameplayMutations {};
		const auto attemptApply = [&](bool a_current, LifecycleState& a_state) {
			if (EvaluateApply(a_current, a_state.IsReady()) ==
			    ApplyDisposition::kApply)
			{
				++gameplayMutations;
			}
		};
		attemptApply(true, launch);
		Check(
		    gameplayMutations == 0,
		    "launch and title apply remains persisted but deferred");

		LifecycleState game;
		game.OnLoadingMenu(true);
		game.OnLoadingMenu(false);
		Check(
		    game.ConfirmLoadingComplete(false),
		    "loading close confirms playable save");
		Check(game.IsReady(), "playable save is ready");
		attemptApply(true, game);
		Check(
		    gameplayMutations == 1,
		    "game apply mutates only after loading-close readiness");

		game.OnLoadingMenu(true);
		Check(!game.IsReady(), "save-to-save load clears readiness");
		game.OnLoadingMenu(false);
		Check(
		    game.ConfirmLoadingComplete(false),
		    "save-to-save close restores readiness");

		LifecycleState titleFirst;
		titleFirst.OnLoadingMenu(true);
		titleFirst.OnMainMenu(true);
		titleFirst.OnLoadingMenu(false);
		Check(
		    !titleFirst.ConfirmLoadingComplete(true),
		    "main-menu-open before loading close remains not ready");
		Check(!titleFirst.IsReady(), "title transition remains not ready");
		attemptApply(true, titleFirst);
		Check(
		    gameplayMutations == 1,
		    "title transition ordering one rejects gameplay mutation");

		LifecycleState closeFirst;
		closeFirst.OnLoadingMenu(true);
		closeFirst.OnLoadingMenu(false);
		closeFirst.OnMainMenu(true);
		Check(
		    !closeFirst.ConfirmLoadingComplete(true),
		    "main-menu-open after close but before confirmation remains not ready");
		Check(!closeFirst.IsReady(), "alternate title ordering remains not ready");
		attemptApply(true, closeFirst);
		Check(
		    gameplayMutations == 1,
		    "title transition ordering two rejects gameplay mutation");

		LifecycleState revisionFlow;
		revisionFlow.OnLoadingMenu(true);
		Check(
		    !revisionFlow.IsReady(),
		    "queued snapshots remain deferred during load");
		revisionFlow.OnLoadingMenu(false);
		Check(
		    revisionFlow.ConfirmLoadingComplete(false),
		    "latest snapshot may apply after confirmed save readiness");
		attemptApply(false, revisionFlow);
		Check(
		    gameplayMutations == 1,
		    "stale queued snapshot revision is ignored");
		attemptApply(true, revisionFlow);
		Check(
		    gameplayMutations == 2,
		    "current queued snapshot revision applies after readiness");

		LifecycleState interrupted;
		interrupted.OnLoadingMenu(true);
		interrupted.OnLoadingMenu(false);
		interrupted.MarkNotReady();
		Check(
		    !interrupted.ConfirmLoadingComplete(false),
		    "a new load invalidates a pending confirmation from the previous load");
		Check(
		    !interrupted.IsReady(),
		    "stale loading confirmation cannot restore readiness");
		interrupted.OnLoadingMenu(true);
		interrupted.OnLoadingMenu(false);
		Check(
		    interrupted.ConfirmLoadingComplete(false),
		    "a fresh loading completion restores readiness after interruption");
	}
}  // namespace

int main()
{
	const auto root =
	    std::filesystem::current_path() / "build" / "runtime-contract-tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);

	TestPersistence(root / "persistence");
	TestCompletedChanges(root / "completed");
	TestQuantizationAndReset();
	TestActionGate();
	TestLifecycle();

	std::filesystem::remove_all(root);
	if (g_failures != 0)
	{
		std::cerr << g_failures << " runtime contract test(s) failed\n";
		return 1;
	}
	std::cout << "All runtime contract tests passed\n";
	return 0;
}
