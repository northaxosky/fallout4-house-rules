#include "PCH.h"

#include "Configuration/SettingsRepository.h"
#include "Gameplay/Application.h"
#include "UI/ActionGate.h"
#include "UI/DearModdingUI.h"

#include <DearModdingUI/Client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace HouseRules::UI
{
	namespace
	{
		using Configuration::SettingChange;
		using SettingsCatalog::Value;

		dmui::Client g_client { "northaxosky.house-rules", "House Rules", dmui::Version { 1, 2 }, "scales" };
		std::atomic_bool g_registrationComplete { false };

		struct Edit
		{
			Value value;
			std::string error;
			bool rejected {};
		};
		// Only unfinished or failed edits live here; the repository owns saved state.
		std::map<std::size_t, Edit> g_edits;

		dmui::DialogSession g_dialog;

		Configuration::PresetDiscovery g_presets;
		bool g_presetRowsStale { true };

		Configuration::SettingsRepository& Repository()
		{
			return Configuration::SettingsRepository::GetSingleton();
		}

		void PublishStatus()
		{
			for (const auto& notification : Repository().TakeNotifications())
			{
				if (notification.error)
				{
					REX::ERROR("DearModdingUI: {}", notification.message);
				}
				if (!g_client.PostNotification(
				        notification.error ? DMUI_STATUS_SEVERITY_ERROR : DMUI_STATUS_SEVERITY_INFO,
				        notification.message.c_str()))
				{
					REX::ERROR("DearModdingUI: notification failed ({}): {}",
					           DMUI_ResultToString(g_client.LastResult()), notification.message);
				}
			}
			static std::string lastMessage;
			static bool lastError {};
			const auto message = Repository().StatusMessage();
			const bool error = Repository().StatusIsError();
			if (message == lastMessage && error == lastError)
			{
				return;
			}
			if (!g_client.SetStatus(error ? DMUI_STATUS_SEVERITY_ERROR : DMUI_STATUS_SEVERITY_INFO, message.c_str()))
			{
				return;
			}
			lastMessage = message;
			lastError = error;
			if (error)
			{
				REX::ERROR("DearModdingUI: {}", message);
			}
		}

		void ReportError(std::string a_message)
		{
			Repository().PublishNotification(0, true, std::move(a_message));
			PublishStatus();
		}

		void ReportRegistrationError(std::string a_message)
		{
			Repository().PublishStatus(0, true, std::move(a_message));
			PublishStatus();
		}

		bool MutationReady(bool a_report = true)
		{
			const auto gate = EvaluateMutationGate(
			    g_registrationComplete.load(std::memory_order_acquire), Repository().IsLoaded());
			if (!gate.allowed && a_report)
			{
				(void)g_client.SetStatus(DMUI_STATUS_SEVERITY_WARNING, std::string { gate.message }.c_str());
			}
			return gate.allowed;
		}

		Value ReadValue(std::size_t a_index)
		{
			if (const auto edit = g_edits.find(a_index); edit != g_edits.end())
			{
				return edit->second.value;
			}
			return Repository().GetValue(a_index).value_or(SettingsCatalog::All()[a_index].defaultValue);
		}

		std::optional<dmui::FieldFeedback> Feedback(std::size_t a_index)
		{
			const auto edit = g_edits.find(a_index);
			if (edit == g_edits.end() || edit->second.error.empty())
			{
				return std::nullopt;
			}
			return dmui::FieldFeedback { dmui::FieldFeedbackSeverity::kError, edit->second.error };
		}

		Value SetEdit(std::size_t a_index, Value a_value)
		{
			const auto previous = ReadValue(a_index);
			if (!MutationReady())
			{
				return previous;
			}
			const auto& descriptor = SettingsCatalog::All()[a_index];
			std::string error;
			if (!Configuration::ValidateRequestedValue(descriptor, a_value, error))
			{
				g_edits.insert_or_assign(a_index, Edit { previous, error, true });
				ReportError(error);
				return previous;
			}
			auto normalized = Configuration::QuantizeUiValue(descriptor, std::move(a_value));
			g_edits.insert_or_assign(a_index, Edit { normalized, {}, false });
			return normalized;
		}

		std::optional<std::string> CommitOrError(std::span<const SettingChange> a_changes)
		{
			if (!MutationReady())
			{
				return "Settings are not ready yet.";
			}
			auto result = Repository().SaveUserOverrides(a_changes);
			if (!result.success)
			{
				for (const auto& change : a_changes)
				{
					auto& edit = g_edits.try_emplace(change.index, Edit { ReadValue(change.index), {}, false }).first->second;
					edit.error = result.message + " Edit this field again to retry.";
				}
				PublishStatus();
				return result.message;
			}
			for (const auto& change : a_changes)
			{
				g_edits.erase(change.index);
			}
			if (result.changed)
			{
				(void)Gameplay::QueueApply(std::move(result.snapshot));
			}
			PublishStatus();
			return std::nullopt;
		}

		void CompleteEdit(std::size_t a_index, const dmui::SettingEditEvent& a_event)
		{
			// A release frame can complete an edit without changing its value again.
			if (!a_event.completed || !MutationReady())
			{
				return;
			}
			const auto edit = g_edits.find(a_index);
			if (edit != g_edits.end() && !edit->second.rejected)
			{
				const std::array changes { SettingChange { a_index, edit->second.value } };
				(void)CommitOrError(changes);
			}
		}

		std::vector<std::string> SplitOptions(std::string_view a_options)
		{
			std::vector<std::string> result;
			std::size_t start {};
			while (start <= a_options.size())
			{
				const auto end = a_options.find('|', start);
				result.emplace_back(a_options.substr(start, end == std::string_view::npos ? end : end - start));
				if (end == std::string_view::npos)
				{
					break;
				}
				start = end + 1;
			}
			return result;
		}

		dmui::SettingControl MakeControl(const SettingsCatalog::Descriptor& a_descriptor)
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
					control.dragSpeed = static_cast<float>((std::max)(a_descriptor.step, 0.001));
					if (a_descriptor.hasRange)
					{
						control.range = dmui::NumericSettingRange<double> { a_descriptor.minimum, a_descriptor.maximum };
						if (Configuration::CanUseNativeControlQuantization(a_descriptor))
						{
							control.quantization = dmui::NumericQuantization<double> { a_descriptor.step, a_descriptor.minimum };
						}
					}
					return control;
				}
				case ValueType::kInt:
				{
					if (!a_descriptor.options.empty())
					{
						dmui::ChoiceSettingControl control;
						for (const auto& option : SplitOptions(a_descriptor.options))
						{
							control.options.push_back({ option, option });
						}
						return control;
					}
					dmui::SignedSettingControl control;
					control.format = "%lld";
					control.dragSpeed = static_cast<float>((std::max)(a_descriptor.step, 1.0));
					if (a_descriptor.hasRange)
					{
						control.range = dmui::NumericSettingRange<std::int64_t> {
							static_cast<std::int64_t>(a_descriptor.minimum), static_cast<std::int64_t>(a_descriptor.maximum)
						};
						if (Configuration::CanUseNativeControlQuantization(a_descriptor))
						{
							control.quantization = dmui::NumericQuantization<std::int64_t> {
								static_cast<std::int64_t>(a_descriptor.step), static_cast<std::int64_t>(a_descriptor.minimum)
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

		dmui::SettingBinding MakeBinding(std::size_t a_index, const SettingsCatalog::Descriptor& a_descriptor)
		{
			if (!a_descriptor.options.empty())
			{
				const auto options = SplitOptions(a_descriptor.options);
				const auto get = [a_index, options] {
					return options.at(static_cast<std::size_t>(std::get<std::int64_t>(ReadValue(a_index))));
				};
				return dmui::BindSetting(get, [a_index, options, get](std::string a_value) {
					const auto found = std::ranges::find(options, a_value);
					(void)SetEdit(a_index, static_cast<std::int64_t>(std::distance(options.begin(), found)));
					return get();
				});
			}
			return std::visit([a_index]<class T>(const T&) -> dmui::SettingBinding {
				return dmui::BindSetting(
				    [a_index] { return std::get<T>(ReadValue(a_index)); },
				    [a_index](T a_value) { return std::get<T>(SetEdit(a_index, std::move(a_value))); });
			},
			                  a_descriptor.defaultValue);
		}

		dmui::SettingDescriptor MakeSetting(std::size_t a_index, const SettingsCatalog::Descriptor& a_descriptor)
		{
			dmui::SettingValue defaultValue = std::visit(
			    [](const auto& a_value) -> dmui::SettingValue { return a_value; }, a_descriptor.defaultValue);
			if (!a_descriptor.options.empty())
			{
				defaultValue = SplitOptions(a_descriptor.options).at(static_cast<std::size_t>(std::get<std::int64_t>(a_descriptor.defaultValue)));
			}
			dmui::SettingDescriptor setting {
				.id = std::string { a_descriptor.id },
				.label = std::string { a_descriptor.label },
				.description = std::string { a_descriptor.help },
				.control = MakeControl(a_descriptor),
				.defaultValue = std::move(defaultValue),
				.binding = MakeBinding(a_index, a_descriptor),
				.isEnabled = [] { return MutationReady(false); },
				.isDirty = [a_index] { return ReadValue(a_index) != Repository().GetValue(a_index).value_or(
				                                                        SettingsCatalog::All()[a_index].defaultValue); },
				.isModified = [a_index] { return ReadValue(a_index) != SettingsCatalog::All()[a_index].defaultValue; },
				.onEdit = [a_index](const dmui::SettingEditEvent& a_event) { CompleteEdit(a_index, a_event); },
				.resolveFeedback = [a_index] { return Feedback(a_index); }
			};
			return setting;
		}

		void RequestDialog(const DMUI_DialogDescriptor& a_descriptor, dmui::DialogSession::Submit a_submit)
		{
			if (!MutationReady() || g_dialog.Active())
			{
				return;
			}
			if (!g_dialog.Open(g_client, a_descriptor, [submit = std::move(a_submit)](std::string_view a_text) {
				    return MutationReady() ? submit(a_text) : std::optional<std::string> { "Settings are not ready yet." };
			    }))
			{
				ReportError(std::format("Could not open dialog: {}.", DMUI_ResultToString(g_dialog.LastResult())));
			}
		}

		void PollDialog()
		{
			if (!g_dialog.Active())
			{
				return;
			}
			g_dialog.Poll();
			if (g_dialog.LastResult() != DMUI_RESULT_OK)
			{
				ReportError(std::format("Dialog failed: {}.", DMUI_ResultToString(g_dialog.LastResult())));
			}
		}

		void RequestPageReset(std::string_view a_pageId, std::string_view a_pageName)
		{
			const auto title = std::format("Reset {}?", a_pageName);
			const auto body = std::format("Reset every setting on {} to its shipped default and save immediately?", a_pageName);
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_CONFIRM,
				.title = title.c_str(),
				.body = body.c_str(),
				.acceptLabel = "Reset all",
				.cancelLabel = "Cancel"
			};
			RequestDialog(descriptor, [page = std::string { a_pageId }](std::string_view) {
				std::vector<SettingChange> changes;
				const auto descriptors = SettingsCatalog::All();
				for (std::size_t index = 0; index < descriptors.size(); ++index)
				{
					if (descriptors[index].exposed && descriptors[index].pageId == page)
					{
						changes.push_back({ index, descriptors[index].defaultValue });
					}
				}
				return CommitOrError(changes);
			});
		}

		void RefreshPresets()
		{
			g_presets = Configuration::DiscoverPresets(Configuration::InstalledPresetDirectories());
			for (const auto& warning : g_presets.warnings)
			{
				REX::WARN("Presets: {}", warning);
			}
			g_presetRowsStale = true;
		}

		void RequestPresetApply(const Configuration::Preset& a_preset)
		{
			const auto title = std::format("Apply {}?", a_preset.name);
			const auto body = std::format(
			    "Set every House Rules gameplay setting to vanilla, apply the {} preset's changes, and save immediately? "
			    "General page settings are not changed.",
			    a_preset.name);
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_CONFIRM,
				.title = title.c_str(),
				.body = body.c_str(),
				.acceptLabel = "Apply preset",
				.cancelLabel = "Cancel"
			};
			RequestDialog(descriptor, [a_preset](std::string_view) -> std::optional<std::string> {
				auto resolved = Configuration::ResolvePreset(SettingsCatalog::All(), a_preset);
				for (const auto& warning : resolved.warnings)
				{
					REX::WARN("Presets: {}", warning);
				}
				if (!resolved.success)
				{
					ReportError(resolved.message);
					return resolved.message;
				}
				if (auto error = CommitOrError(resolved.changes))
				{
					return error;
				}
				REX::INFO("Presets: applied '{}' ({} settings).", a_preset.name, resolved.changes.size());
				return std::nullopt;
			});
		}

		void RequestPresetSave()
		{
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_TEXT_ENTRY,
				.title = "Save preset",
				.body = "Name the new preset. It records every gameplay setting that differs from vanilla.",
				.acceptLabel = "Save",
				.cancelLabel = "Cancel",
				.hint = "Preset name",
				.initialText = "",
				.maximumTextBytes = 64
			};
			RequestDialog(descriptor, [](std::string_view a_name) -> std::optional<std::string> {
				const auto saved = Configuration::SaveUserPreset(
				    SettingsCatalog::All(), Repository().GetSnapshot().values, a_name, Configuration::InstalledPresetDirectories().user);
				if (!saved.success)
				{
					ReportError(saved.message);
					return saved.message;
				}
				REX::INFO("Presets: saved '{}' to {}.", saved.preset.name, saved.preset.path.string());
				RefreshPresets();
				Repository().PublishNotification(0, false, saved.message);
				PublishStatus();
				return std::nullopt;
			});
		}

		std::string PresetLabel(const Configuration::Preset& a_preset)
		{
			switch (a_preset.origin)
			{
				case Configuration::PresetOrigin::kBuiltIn:
					return std::format("{} (built-in)", a_preset.name);
				case Configuration::PresetOrigin::kShipped:
					return std::format("{} (House Rules)", a_preset.name);
				case Configuration::PresetOrigin::kUser:
					return std::format("{} (yours)", a_preset.name);
			}
			return a_preset.name;
		}

		std::vector<dmui::SettingGroup> MakePresetGroups()
		{
			const auto enabled = [] { return MutationReady(false); };
			dmui::SettingGroup presets { .id = "presets", .label = "Presets" };
			for (const auto& preset : g_presets.presets)
			{
				presets.actionRows.push_back({ .id = std::format("preset-{}", preset.id),
				                               .label = PresetLabel(preset),
				                               .buttonLabel = "Apply...",
				                               .description = preset.description,
				                               .activate = [preset] { RequestPresetApply(preset); },
				                               .isEnabled = enabled });
			}
			dmui::SettingGroup manage { .id = "manage", .label = "Manage" };
			manage.actionRows.push_back({ .id = "preset-save",
			                              .label = "Save current settings",
			                              .buttonLabel = "Save as...",
			                              .description = "Saves your gameplay settings as a new preset in Data\\MCM\\Settings\\HouseRules\\Presets.",
			                              .activate = [] { RequestPresetSave(); },
			                              .isEnabled = enabled });
			manage.actionRows.push_back({ .id = "preset-rescan",
			                              .label = "Rescan preset folders",
			                              .buttonLabel = "Rescan",
			                              .description = "Reloads the preset list after adding or removing preset files.",
			                              .activate = [] {
				                              RefreshPresets();
				                              Repository().PublishNotification(0, false, "Presets reloaded.");
				                              PublishStatus();
			                              } });
			return { std::move(presets), std::move(manage) };
		}

		dmui::SettingsPage MakePresetsPage()
		{
			dmui::SettingsPage page {
				.filterOptions = { .showSearch = false, .showModifiedOnly = false },
				.notes = { { "Waiting for settings state.", false, "house-rules-status" },
				           { "Applying a preset replaces every gameplay setting with vanilla plus that preset's changes.", true, "house-rules-preset-help" } },
				.prepareView = [](dmui::SettingsPage& a_page) {
					if (g_presetRowsStale)
					{
						a_page.groups = MakePresetGroups();
						g_presetRowsStale = false;
					}
					a_page.notes[0].text = Repository().StatusMessage();
					a_page.notes[0].muted = !Repository().StatusIsError(); }
			};
			return page;
		}

		std::vector<dmui::SettingGroup> MakeGroups(std::string_view a_pageId)
		{
			struct Group
			{
				std::string name;
				std::vector<std::pair<std::int32_t, dmui::SettingDescriptor>> settings;
			};
			std::vector<Group> groups;
			const auto descriptors = SettingsCatalog::All();
			for (std::size_t index = 0; index < descriptors.size(); ++index)
			{
				const auto& descriptor = descriptors[index];
				if (!descriptor.exposed || descriptor.pageId != a_pageId)
				{
					continue;
				}
				auto group = std::ranges::find(groups, descriptor.group, &Group::name);
				if (group == groups.end())
				{
					groups.push_back({ std::string { descriptor.group }, {} });
					group = std::prev(groups.end());
				}
				group->settings.emplace_back(descriptor.sortKey, MakeSetting(index, descriptor));
			}
			std::vector<dmui::SettingGroup> result;
			for (auto& group : groups)
			{
				std::ranges::sort(group.settings, {}, &std::pair<std::int32_t, dmui::SettingDescriptor>::first);
				dmui::SettingGroup row { .id = group.name, .label = group.name };
				for (auto& entry : group.settings)
				{
					row.settings.push_back(std::move(entry.second));
				}
				result.push_back(std::move(row));
			}
			return result;
		}

		dmui::SettingsPage MakePage(const SettingsCatalog::Page& a_page)
		{
			dmui::SettingsPage page {
				.groups = MakeGroups(a_page.id),
				.filterOptions = { .showSearch = true, .showModifiedOnly = true, .searchHint = "Search House Rules settings..." },
				.notes = { { "Waiting for settings state.", false, "house-rules-status" },
				           { "Completed edits save automatically. Gameplay changes wait until a save is ready; some effects refresh on their next use or update.", true, "house-rules-save-help" } },
				.prepareView = [](dmui::SettingsPage& a_page) {
					a_page.notes[0].text = Repository().StatusMessage();
					a_page.notes[0].muted = !Repository().StatusIsError(); }
			};
			page.actions.reset = [id = std::string { a_page.id }, name = std::string { a_page.name }] {
				RequestPageReset(id, name);
			};
			page.actionTooltips.reset = "Reset this page to shipped defaults after confirmation.";
			return page;
		}
	}  // namespace

	void RegisterDearModdingUI() noexcept
	{
		if (Configuration::SelectedFrontend() != Configuration::Frontend::kDearModdingUI)
		{
			return;
		}
		if (!g_client.Connect())
		{
			if (g_client.HostPresent())
			{
				REX::ERROR("DearModdingUI: connection failed ({}). Check the matching host/API build; House Rules continues headless with saved settings.",
				           DMUI_ResultToString(g_client.LastResult()));
			}
			else
			{
				REX::ERROR("DearModdingUI: host is not loaded. House Rules continues headless with saved settings; no MCM fallback is registered.");
			}
			return;
		}
		for (const auto& category : SettingsCatalog::Categories())
		{
			if (!g_client.AddCategory(
			        { .id = category.id.data(), .displayName = category.name.data(), .sortKey = category.sortKey }))
			{
				ReportRegistrationError(std::format("Category '{}' registration failed: {}.", category.name, DMUI_ResultToString(g_client.LastResult())));
				return;
			}
		}
		for (const auto& page : SettingsCatalog::Pages())
		{
			if (!g_client.AddSettingsPage(
			        { .id = page.id.data(), .displayName = page.name.data(), .categoryId = page.categoryId.data(), .summary = page.summary.data(), .sortKey = page.sortKey }, MakePage(page)))
			{
				ReportRegistrationError(std::format("Page '{}' registration failed: {}. House Rules controls are disabled.",
				                        page.name, DMUI_ResultToString(g_client.LastResult())));
				return;
			}
		}
		RefreshPresets();
		// Sorts between General (0) and the next catalog page (100).
		if (!g_client.AddSettingsPage(
		        { .id = "presets", .displayName = "Presets", .categoryId = "overview", .summary = "Apply or save complete House Rules setups.", .sortKey = 50 }, MakePresetsPage()))
		{
			ReportRegistrationError(std::format("Page 'Presets' registration failed: {}. House Rules controls are disabled.",
			                        DMUI_ResultToString(g_client.LastResult())));
			return;
		}
		if (!g_client.AddFrameObserver([] {
			    if (g_registrationComplete.load(std::memory_order_acquire))
			    {
				    PollDialog();
				    PublishStatus();
			    }
		    }))
		{
			ReportRegistrationError(std::format("Status observer registration failed: {}.", DMUI_ResultToString(g_client.LastResult())));
			return;
		}
		Repository().EnableNotifications();
		g_registrationComplete.store(true, std::memory_order_release);
		PublishStatus();
		REX::INFO("DearModdingUI: registered {} House Rules settings pages and Presets.", SettingsCatalog::Pages().size());
	}
}  // namespace HouseRules::UI
