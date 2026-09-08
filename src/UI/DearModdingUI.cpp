#include "PCH.h"

#include "UI/ActionGate.h"
#include "UI/DearModdingUI.h"

#include "Configuration/SettingsPersistence.h"
#include "Configuration/SettingsRepository.h"
#include "Gameplay/Application.h"
#include "SettingsCatalog.generated.h"

#include <DearModdingUI/Client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace HouseRules::UI
{
	using namespace std::literals;

	namespace
	{
		constexpr dmui::ClientOptions kClientOptions {
			.requiredServices = DMUI_HOST_SERVICE_NONE,
			.minimumForwardingVersion = DMUI_FORWARDING_VERSION_CURRENT
		};
		constexpr dmui::CategoryDescriptor kCategory {
			.id = "house-rules",
			.displayName = "House Rules",
			.sortKey = 0
		};

		dmui::Client g_client {
			"northaxosky.house-rules",
			"House Rules",
			dmui::Version { 1, 2 },
			dmui::kForwardingClient,
			{},
			{},
			kClientOptions
		};
		std::atomic_bool g_registrationComplete { false };

		struct Draft
		{
			bool active {};
			std::uint64_t revision {};
			std::vector<SettingsCatalog::Value> committed;
			std::vector<SettingsCatalog::Value> values;
		};

		Draft g_draft;

		[[nodiscard]] std::vector<SettingsCatalog::Value> Defaults()
		{
			std::vector<SettingsCatalog::Value> values;
			for (const auto& descriptor : SettingsCatalog::All())
			{
				values.push_back(descriptor.defaultValue);
			}
			return values;
		}

		[[nodiscard]] bool IsDirty() noexcept
		{
			return g_draft.active && g_draft.values != g_draft.committed;
		}

		[[nodiscard]] bool SettingsLoaded()
		{
			return Configuration::SettingsRepository::GetSingleton().IsLoaded();
		}

		[[nodiscard]] bool MutationReady(bool a_publishStatus = true)
		{
			const auto result = EvaluateMutationGate(
			    g_registrationComplete.load(std::memory_order_acquire),
			    SettingsLoaded());
			if (!result.allowed && a_publishStatus)
			{
				(void)g_client.SetStatus(
				    DMUI_STATUS_SEVERITY_WARNING,
				    std::string { result.message }.c_str());
			}
			return result.allowed;
		}

		[[nodiscard]] std::size_t PendingCount() noexcept
		{
			if (!g_draft.active)
			{
				return 0;
			}
			std::size_t count = 0;
			const auto descriptors = SettingsCatalog::All();
			for (std::size_t index = 0; index < descriptors.size(); ++index)
			{
				if (descriptors[index].exposed &&
				    g_draft.values[index] != g_draft.committed[index])
				{
					++count;
				}
			}
			return count;
		}

		void EnsureDraft()
		{
			const auto snapshot =
			    Configuration::SettingsRepository::GetSingleton().GetSnapshot();
			if (!g_draft.active)
			{
				g_draft.active = true;
				g_draft.revision = snapshot.revision;
				g_draft.committed =
				    snapshot.values.empty() ? Defaults() : snapshot.values;
				g_draft.values = g_draft.committed;
				return;
			}
			if (!IsDirty() && snapshot.revision != g_draft.revision &&
			    !snapshot.values.empty())
			{
				g_draft.revision = snapshot.revision;
				g_draft.committed = snapshot.values;
				g_draft.values = snapshot.values;
			}
		}

		[[nodiscard]] std::vector<std::string> SplitOptions(
		    std::string_view a_options)
		{
			std::vector<std::string> output;
			std::size_t start = 0;
			while (start <= a_options.size())
			{
				const auto separator = a_options.find('|', start);
				output.emplace_back(a_options.substr(
				    start,
				    separator == std::string_view::npos ? a_options.size() - start : separator - start));
				if (separator == std::string_view::npos)
				{
					break;
				}
				start = separator + 1;
			}
			return output;
		}

		template<class T>
		[[nodiscard]] T DraftValue(std::size_t a_index, T a_fallback)
		{
			if (a_index >= g_draft.values.size())
			{
				return a_fallback;
			}
			if (const auto* value = std::get_if<T>(&g_draft.values[a_index]))
			{
				return *value;
			}
			return a_fallback;
		}

		template<class T>
		[[nodiscard]] T SetDraftValue(
		    std::size_t a_index,
		    T a_value,
		    T a_fallback)
		{
			if (!MutationReady() ||
			    a_index >= g_draft.values.size() ||
			    !std::holds_alternative<T>(g_draft.values[a_index]))
			{
				return a_fallback;
			}
			g_draft.values[a_index] = a_value;
			return a_value;
		}

		[[nodiscard]] dmui::SettingControl MakeControl(
		    const SettingsCatalog::Descriptor& a_descriptor)
		{
			using SettingsCatalog::ValueType;
			switch (a_descriptor.type)
			{
				case ValueType::kBool:
					return dmui::CheckboxSettingControl {};
				case ValueType::kFloat:
				{
					dmui::DoubleSettingControl control;
					control.format = std::abs(a_descriptor.step) < 0.01 ? "%.3f" : "%.2f";
					control.dragSpeed =
					    static_cast<float>((std::max)(a_descriptor.step, 0.001));
					if (a_descriptor.hasRange)
					{
						control.range = dmui::NumericSettingRange<double> {
							a_descriptor.minimum,
							a_descriptor.maximum
						};
						if (Configuration::CanUseNativeControlQuantization(
						        a_descriptor))
						{
							control.quantization =
							    dmui::NumericQuantization<double> {
								    a_descriptor.step,
								    a_descriptor.minimum
							    };
						}
					}
					return control;
				}
				case ValueType::kInt:
					if (!a_descriptor.options.empty())
					{
						dmui::ChoiceSettingControl control;
						for (const auto& option :
						     SplitOptions(a_descriptor.options))
						{
							control.options.push_back({ option, option });
						}
						return control;
					}
					else
					{
						dmui::SignedSettingControl control;
						control.format = "%lld";
						control.dragSpeed =
						    static_cast<float>((std::max)(a_descriptor.step, 1.0));
						if (a_descriptor.hasRange)
						{
							control.range =
							    dmui::NumericSettingRange<std::int64_t> {
								    static_cast<std::int64_t>(
								        a_descriptor.minimum),
								    static_cast<std::int64_t>(
								        a_descriptor.maximum)
							    };
							if (Configuration::CanUseNativeControlQuantization(
							        a_descriptor))
							{
								control.quantization =
								    dmui::NumericQuantization<std::int64_t> {
									    static_cast<std::int64_t>(
									        a_descriptor.step),
									    static_cast<std::int64_t>(
									        a_descriptor.minimum)
								    };
							}
						}
						return control;
					}
				case ValueType::kString:
					return dmui::TextSettingControl { 512 };
			}
			return dmui::UnsupportedSettingControl {};
		}

		[[nodiscard]] dmui::SettingBinding MakeBinding(
		    std::size_t a_index,
		    const SettingsCatalog::Descriptor& a_descriptor)
		{
			using SettingsCatalog::ValueType;
			switch (a_descriptor.type)
			{
				case ValueType::kBool:
				{
					const auto fallback = std::get<bool>(a_descriptor.defaultValue);
					return dmui::BindSetting(
					    [a_index, fallback] {
						    return DraftValue(a_index, fallback);
					    },
					    [a_index, fallback](bool a_value) {
						    return SetDraftValue(a_index, a_value, fallback);
					    });
				}
				case ValueType::kFloat:
				{
					const auto fallback = std::get<double>(a_descriptor.defaultValue);
					return dmui::BindSetting(
					    [a_index, fallback] {
						    return DraftValue(a_index, fallback);
					    },
					    [a_index, fallback, &a_descriptor](double a_value) {
						    auto quantized = Configuration::QuantizeUiValue(
						        a_descriptor,
						        a_value);
						    return SetDraftValue(
						        a_index,
						        std::get<double>(quantized),
						        fallback);
					    });
				}
				case ValueType::kInt:
					if (!a_descriptor.options.empty())
					{
						const auto options = SplitOptions(a_descriptor.options);
						const auto defaultIndex =
						    std::get<std::int64_t>(a_descriptor.defaultValue);
						const auto fallback =
						    options.at(static_cast<std::size_t>(defaultIndex));
						return dmui::BindSetting(
						    [a_index, options, fallback] {
							    const auto value =
							        DraftValue<std::int64_t>(
							            a_index,
							            0);
							    if (value < 0 ||
							        static_cast<std::size_t>(value) >=
							            options.size())
							    {
								    return fallback;
							    }
							    return options[static_cast<std::size_t>(value)];
						    },
						    [a_index, options, fallback](std::string a_value) {
							    const auto found =
							        std::ranges::find(options, a_value);
							    if (found == options.end())
							    {
								    return fallback;
							    }
							    const auto index = static_cast<std::int64_t>(
							        std::distance(options.begin(), found));
							    (void)SetDraftValue<std::int64_t>(
							        a_index,
							        index,
							        0);
							    return a_value;
						    });
					}
					else
					{
						const auto fallback =
						    std::get<std::int64_t>(a_descriptor.defaultValue);
						return dmui::BindSetting(
						    [a_index, fallback] {
							    return DraftValue(a_index, fallback);
						    },
						    [a_index, fallback, &a_descriptor](std::int64_t a_value) {
							    auto quantized = Configuration::QuantizeUiValue(
							        a_descriptor,
							        a_value);
							    return SetDraftValue(
							        a_index,
							        std::get<std::int64_t>(quantized),
							        fallback);
						    });
					}
				case ValueType::kString:
				{
					const auto fallback =
					    std::get<std::string>(a_descriptor.defaultValue);
					return dmui::BindSetting(
					    [a_index, fallback] {
						    return DraftValue(a_index, fallback);
					    },
					    [a_index, fallback](std::string a_value) {
						    return SetDraftValue(
						        a_index,
						        std::move(a_value),
						        fallback);
					    });
				}
			}
			return {};
		}

		[[nodiscard]] dmui::SettingDescriptor MakeSetting(
		    std::size_t a_index,
		    const SettingsCatalog::Descriptor& a_descriptor)
		{
			dmui::SettingValue defaultValue = std::visit(
			    []<class T>(const T& a_value) -> dmui::SettingValue {
				    return a_value;
			    },
			    a_descriptor.defaultValue);
			if (!a_descriptor.options.empty())
			{
				const auto options = SplitOptions(a_descriptor.options);
				const auto defaultIndex =
				    std::get<std::int64_t>(a_descriptor.defaultValue);
				defaultValue =
				    options.at(static_cast<std::size_t>(defaultIndex));
			}
			return {
				.id = std::string { a_descriptor.id },
				.label = std::string { a_descriptor.label },
				.description = std::string { a_descriptor.help },
				.control = MakeControl(a_descriptor),
				.defaultValue = std::move(defaultValue),
				.binding = MakeBinding(a_index, a_descriptor),
				.applyTiming = dmui::SettingApplyTiming::kNextLaunch,
				.isEnabled = [] { return g_registrationComplete.load(
				                             std::memory_order_acquire) &&
				                         SettingsLoaded(); },
				.isDirty = [a_index] { return g_draft.active &&
				                              a_index < g_draft.values.size() &&
				                              g_draft.values[a_index] !=
				                                  g_draft.committed[a_index]; },
				.isModified = [a_index, defaultValue = a_descriptor.defaultValue] { return g_draft.active &&
				                                                                           a_index < g_draft.values.size() &&
				                                                                           g_draft.values[a_index] != defaultValue; },
				.onEdit = [](const dmui::SettingEditEvent& a_event) {
					if (a_event.changed && a_event.completed) {
						const auto pending = PendingCount();
						(void)g_client.SetStatus(
							DMUI_STATUS_SEVERITY_INFO,
							std::format(
								"{} pending change{}; select Apply to save.",
								pending,
								pending == 1 ? "" : "s")
								.c_str());
					} }
			};
		}

		void ResetPage(std::string_view a_pageId)
		{
			if (!MutationReady())
			{
				return;
			}
			EnsureDraft();
			const auto descriptors = SettingsCatalog::All();
			for (std::size_t index = 0; index < descriptors.size(); ++index)
			{
				if (descriptors[index].pageId == a_pageId)
				{
					g_draft.values[index] = descriptors[index].defaultValue;
				}
			}
		}

		void RevertPage(std::string_view a_pageId)
		{
			if (!MutationReady())
			{
				return;
			}
			EnsureDraft();
			const auto descriptors = SettingsCatalog::All();
			for (std::size_t index = 0; index < descriptors.size(); ++index)
			{
				if (descriptors[index].pageId == a_pageId)
				{
					g_draft.values[index] = g_draft.committed[index];
				}
			}
		}

		void ApplyDraft()
		{
			if (!MutationReady())
			{
				return;
			}
			EnsureDraft();
			auto result =
			    Configuration::SettingsRepository::GetSingleton()
			        .SaveUserOverrides(g_draft.values);
			if (!result.success)
			{
				(void)g_client.SetStatus(
				    DMUI_STATUS_SEVERITY_ERROR,
				    result.message.c_str());
				REX::ERROR("DearModdingUI: {}"sv, result.message);
				return;
			}

			g_draft.revision = result.snapshot.revision;
			g_draft.committed = result.snapshot.values;
			g_draft.values = result.snapshot.values;
			const auto queued =
			    Gameplay::QueueApply(std::move(result.snapshot));
			const auto ready = Gameplay::IsReady();
			const auto* message = !queued ? "Settings saved, but gameplay application could not be queued." : ready ? "Settings saved; gameplay application queued."
			                                                                                                        : "Settings saved; gameplay changes will apply after the next loading screen.";
			(void)g_client.SetStatus(
			    queued ? DMUI_STATUS_SEVERITY_SUCCESS : DMUI_STATUS_SEVERITY_ERROR,
			    message);
		}

		[[nodiscard]] std::vector<dmui::SettingGroup> MakeGroups(
		    std::string_view a_pageId)
		{
			struct GroupBuild
			{
				std::string name;
				std::vector<std::pair<std::int32_t, dmui::SettingDescriptor>>
				    settings;
			};
			std::vector<GroupBuild> builds;
			const auto descriptors = SettingsCatalog::All();
			for (std::size_t index = 0; index < descriptors.size(); ++index)
			{
				const auto& descriptor = descriptors[index];
				if (!descriptor.exposed || descriptor.pageId != a_pageId)
				{
					continue;
				}
				auto group = std::ranges::find_if(
				    builds,
				    [&](const GroupBuild& a_group) {
					    return a_group.name == descriptor.group;
				    });
				if (group == builds.end())
				{
					builds.push_back({ std::string { descriptor.group }, {} });
					group = std::prev(builds.end());
				}
				group->settings.emplace_back(
				    descriptor.sortKey,
				    MakeSetting(index, descriptor));
			}

			std::vector<dmui::SettingGroup> groups;
			for (auto& build : builds)
			{
				std::ranges::sort(
				    build.settings,
				    {},
				    &std::pair<std::int32_t, dmui::SettingDescriptor>::first);
				dmui::SettingGroup group {
					.id = build.name,
					.label = build.name
				};
				for (auto& [sortKey, setting] : build.settings)
				{
					(void)sortKey;
					group.settings.push_back(std::move(setting));
				}
				groups.push_back(std::move(group));
			}
			return groups;
		}

		[[nodiscard]] dmui::SettingsPage MakePage(
		    const SettingsCatalog::Page& a_page)
		{
			const auto pageId = std::string { a_page.id };
			return {
				.groups = MakeGroups(a_page.id),
				.actions = { .showReset = true,
				             .reset = [pageId] { ResetPage(pageId); },
				             .revert = [pageId] { RevertPage(pageId); },
				             .apply = [] { ApplyDraft(); } },
				.actionTooltips = { .reset =
				                        "Reset this page to shipped defaults. Select Apply to save.",
				                    .revert =
				                        "Discard pending edits on this page.",
				                    .apply = [](std::size_t) {
				                        const auto pending = PendingCount();
				                        return std::format(
				                            "Save and apply {} pending change{} across House Rules.",
				                            pending,
				                            pending == 1 ? "" : "s");
				                    } },
				.filterOptions = { .showSearch = true, .showModifiedOnly = true, .searchHint = "Search House Rules settings..." },
				.notes = { { "Waiting for settings state.", false, "house-rules-status" }, { "Edits remain pending until Apply. During loading or at the main menu, saved gameplay changes are deferred until the next LoadingMenu close.", true, "house-rules-apply-help" } },
				.prepare = [] {
					if (MutationReady(false)) {
						EnsureDraft();
					} },
				.prepareView = [](dmui::SettingsPage& a_settingsPage) {
					auto& repository =
						Configuration::SettingsRepository::GetSingleton();
					auto status = repository.StatusMessage();
					const auto pending = PendingCount();
					if (pending > 0) {
						status += std::format(
							" {} pending change{}.",
							pending,
							pending == 1 ? "" : "s");
					}
					a_settingsPage.notes[0].text = std::move(status);
					a_settingsPage.notes[0].muted =
						!repository.StatusIsError(); }
			};
		}
	}  // namespace

	void RegisterDearModdingUI() noexcept
	{
		if (Configuration::SelectedFrontend() !=
		    Configuration::Frontend::kDearModdingUI)
		{
			REX::INFO(
			    "DearModdingUI: frontend is mcm; native registration skipped."sv);
			return;
		}

		if (!g_client.Connect())
		{
			if (!g_client.HostPresent())
			{
				REX::ERROR(
				    "DearModdingUI: native frontend selected, but DearModdingUI.dll is not loaded. House Rules will run headless with saved gameplay settings; no MCM fallback is registered."sv);
			}
			else
			{
				REX::ERROR(
				    "DearModdingUI: native frontend connection failed ({}). Install the matching DearModdingUI host/API build. House Rules will run headless with saved gameplay settings."sv,
				    DMUI_ResultToString(g_client.LastResult()));
			}
			return;
		}
		if (!g_client.AddCategory(kCategory))
		{
			REX::ERROR(
			    "DearModdingUI: category registration failed ({}); House Rules native pages are unavailable."sv,
			    DMUI_ResultToString(g_client.LastResult()));
			return;
		}

		for (const auto& page : SettingsCatalog::Pages())
		{
			const auto handle = g_client.AddSettingsPage(
			    { .id = page.id.data(),
			      .displayName = page.name.data(),
			      .categoryId = kCategory.id,
			      .summary = page.summary.data(),
			      .sortKey = page.sortKey },
			    MakePage(page));
			if (!handle)
			{
				REX::ERROR(
				    "DearModdingUI: page '{}' registration failed ({}). Native registration is incomplete and all House Rules controls are disabled."sv,
				    page.name,
				    DMUI_ResultToString(g_client.LastResult()));
				(void)g_client.SetStatus(
				    DMUI_STATUS_SEVERITY_ERROR,
				    "House Rules page registration failed; controls are disabled.");
				return;
			}
		}

		g_registrationComplete.store(true, std::memory_order_release);
		(void)g_client.SetStatus(
		    DMUI_STATUS_SEVERITY_SUCCESS,
		    "House Rules settings loaded.");
		REX::INFO("DearModdingUI: registered 16 House Rules settings pages."sv);
	}
}  // namespace HouseRules::UI
