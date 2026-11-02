#include "PCH.h"

#include "Configuration/SettingsRepository.h"
#include "Gameplay/Application.h"
#include "UI/ActionGate.h"
#include "UI/DearModdingUI.h"
#include "Version.h"

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
		using SettingsCatalog::Text;
		using SettingsCatalog::Value;
		namespace Strings = SettingsCatalog::Strings;

		// Also names the translation files under Interface\Translations.
		constexpr std::string_view kClientId { "northaxosky.house-rules" };

		dmui::Client g_client { kClientId, "House Rules", dmui::Version { Version::kMajor, Version::kMinor }, "scales" };
		std::atomic_bool g_registrationComplete { false };

		// Render thread only: the host knows the game language after startup.
		bool TextFinal()
		{
			static bool resolved {};
			if (!resolved)
			{
				DMUI_Result result {};
				(void)dmui::Client::ResolveText(kClientId, Strings::kPresetsPage.key, &result);
				resolved = result != DMUI_RESULT_HOST_NOT_READY;
			}
			return resolved;
		}

		std::string Localize(std::string_view a_key, std::string_view a_english)
		{
			return a_key.empty() ? std::string { a_english } : dmui::Client::Localize(kClientId, a_key, a_english);
		}

		std::string Localize(const Text& a_text)
		{
			return Localize(a_text.key, a_text.english);
		}

		// Not std::format: a malformed translation must not throw.
		std::string Localize(const Text& a_text, std::string_view a_value)
		{
			auto text = Localize(a_text);
			if (const auto at = text.find("{}"); at != std::string::npos)
			{
				text.replace(at, 2, a_value);
			}
			return text;
		}

		// Hosts before ABI 2.2 would show registration keys verbatim.
		const char* RegistrationText(std::string_view a_key, std::string_view a_english)
		{
			return (g_client.AbiMinor() >= 2 ? a_key : a_english).data();
		}

		const char* RegistrationText(const Text& a_text)
		{
			return RegistrationText(a_text.key, a_text.english);
		}

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
				return Localize(Strings::kNotReady);
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
						const auto options = SplitOptions(a_descriptor.options);
						const auto keys = SplitOptions(a_descriptor.optionKeys);
						for (std::size_t index = 0; index < options.size(); ++index)
						{
							// Values stay English; the binding maps them to indices.
							control.options.push_back({ options[index], Localize(keys.at(index), options[index]) });
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
				.label = Localize(a_descriptor.labelKey, a_descriptor.label),
				.description = Localize(a_descriptor.helpKey, a_descriptor.help),
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
				    return MutationReady() ? submit(a_text) : std::optional<std::string> { Localize(Strings::kNotReady) };
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

		void RequestPageReset(const SettingsCatalog::Page& a_page)
		{
			const auto name = Localize(a_page.nameKey, a_page.name);
			const auto title = Localize(Strings::kResetPageTitle, name);
			const auto body = Localize(Strings::kResetPageBody, name);
			const auto accept = Localize(Strings::kResetPageAccept);
			const auto cancel = Localize(Strings::kDialogCancel);
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_CONFIRM,
				.title = title.c_str(),
				.body = body.c_str(),
				.acceptLabel = accept.c_str(),
				.cancelLabel = cancel.c_str()
			};
			RequestDialog(descriptor, [page = a_page.id](std::string_view) {
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
			const auto title = Localize(Strings::kApplyPresetTitle, a_preset.name);
			const auto body = Localize(Strings::kApplyPresetBody, a_preset.name);
			const auto accept = Localize(Strings::kApplyPresetAccept);
			const auto cancel = Localize(Strings::kDialogCancel);
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_CONFIRM,
				.title = title.c_str(),
				.body = body.c_str(),
				.acceptLabel = accept.c_str(),
				.cancelLabel = cancel.c_str()
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
			const auto title = Localize(Strings::kSavePresetTitle);
			const auto body = Localize(Strings::kSavePresetBody);
			const auto accept = Localize(Strings::kSavePresetAccept);
			const auto cancel = Localize(Strings::kDialogCancel);
			const auto hint = Localize(Strings::kSavePresetHint);
			const DMUI_DialogDescriptor descriptor {
				.kind = DMUI_DIALOG_KIND_TEXT_ENTRY,
				.title = title.c_str(),
				.body = body.c_str(),
				.acceptLabel = accept.c_str(),
				.cancelLabel = cancel.c_str(),
				.hint = hint.c_str(),
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
					return Localize(Strings::kPresetBuiltIn, a_preset.name);
				case Configuration::PresetOrigin::kShipped:
					return Localize(Strings::kPresetShipped, a_preset.name);
				case Configuration::PresetOrigin::kUser:
					return Localize(Strings::kPresetUser, a_preset.name);
			}
			return a_preset.name;
		}

		std::vector<dmui::SettingGroup> MakePresetGroups()
		{
			const auto enabled = [] { return MutationReady(false); };
			dmui::SettingGroup presets { .id = "presets", .label = Localize(Strings::kPresetsGroup) };
			for (const auto& preset : g_presets.presets)
			{
				presets.actionRows.push_back({ .id = std::format("preset-{}", preset.id),
				                               .label = PresetLabel(preset),
				                               .buttonLabel = Localize(Strings::kPresetApplyButton),
				                               .description = preset.description,
				                               .activate = [preset] { RequestPresetApply(preset); },
				                               .isEnabled = enabled });
			}
			dmui::SettingGroup manage { .id = "manage", .label = Localize(Strings::kManageGroup) };
			manage.actionRows.push_back({ .id = "preset-save",
			                              .label = Localize(Strings::kPresetSaveRow),
			                              .buttonLabel = Localize(Strings::kPresetSaveButton),
			                              .description = Localize(Strings::kPresetSaveRowHelp),
			                              .activate = [] { RequestPresetSave(); },
			                              .isEnabled = enabled });
			manage.actionRows.push_back({ .id = "preset-rescan",
			                              .label = Localize(Strings::kPresetRescanRow),
			                              .buttonLabel = Localize(Strings::kPresetRescanButton),
			                              .description = Localize(Strings::kPresetRescanRowHelp),
			                              .activate = [] {
				                              RefreshPresets();
				                              Repository().PublishNotification(0, false, Localize(Strings::kPresetsReloaded));
				                              PublishStatus();
			                              } });
			return { std::move(presets), std::move(manage) };
		}

		dmui::SettingsPage MakePresetsPage()
		{
			dmui::SettingsPage page {
				.filterOptions = { .showSearch = false, .showModifiedOnly = false },
				.notes = { { "Waiting for settings state.", false, "house-rules-status" },
				           { Localize(Strings::kPresetHelpNote), true, "house-rules-preset-help" } },
				.prepareView = [localized = false](dmui::SettingsPage& a_page) mutable {
					if (!localized && TextFinal())
					{
						a_page.notes[1].text = Localize(Strings::kPresetHelpNote);
						g_presetRowsStale = true;
						localized = true;
					}
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
				std::string label;
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
					groups.push_back({ std::string { descriptor.group }, Localize(descriptor.groupKey, descriptor.group), {} });
					group = std::prev(groups.end());
				}
				group->settings.emplace_back(descriptor.sortKey, MakeSetting(index, descriptor));
			}
			std::vector<dmui::SettingGroup> result;
			for (auto& group : groups)
			{
				std::ranges::sort(group.settings, {}, &std::pair<std::int32_t, dmui::SettingDescriptor>::first);
				dmui::SettingGroup row { .id = group.name, .label = group.label };
				for (auto& entry : group.settings)
				{
					row.settings.push_back(std::move(entry.second));
				}
				result.push_back(std::move(row));
			}
			return result;
		}

		void LocalizePage(dmui::SettingsPage& a_view, const SettingsCatalog::Page& a_page)
		{
			// Rebuilt, not resolved per frame: search matches the static text.
			a_view.groups = MakeGroups(a_page.id);
			a_view.notes[1].text = Localize(Strings::kSaveHelpNote);
			a_view.actionTooltips.reset = Localize(Strings::kResetTooltip);
		}

		dmui::SettingsPage MakePage(const SettingsCatalog::Page& a_page)
		{
			dmui::SettingsPage page {
				.filterOptions = { .showSearch = true, .showModifiedOnly = true },
				.notes = { { "Waiting for settings state.", false, "house-rules-status" },
				           { {}, true, "house-rules-save-help" } },
				.prepareView = [&a_page, localized = false](dmui::SettingsPage& a_view) mutable {
					if (!localized && TextFinal())
					{
						LocalizePage(a_view, a_page);
						localized = true;
					}
					a_view.notes[0].text = Repository().StatusMessage();
					a_view.notes[0].muted = !Repository().StatusIsError(); }
			};
			LocalizePage(page, a_page);
			page.actions.reset = [&a_page] { RequestPageReset(a_page); };
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
			        { .id = category.id.data(),
			          .displayName = RegistrationText(category.nameKey, category.name),
			          .sortKey = category.sortKey,
			          .iconName = category.iconName.data() }))
			{
				ReportRegistrationError(std::format("Category '{}' registration failed: {}.", category.name, DMUI_ResultToString(g_client.LastResult())));
				return;
			}
		}
		for (const auto& page : SettingsCatalog::Pages())
		{
			if (!g_client.AddSettingsPage(
			        { .id = page.id.data(),
			          .displayName = RegistrationText(page.nameKey, page.name),
			          .categoryId = page.categoryId.data(),
			          .summary = RegistrationText(page.summaryKey, page.summary),
			          .sortKey = page.sortKey,
			          .iconName = page.iconName.data() },
			        MakePage(page)))
			{
				ReportRegistrationError(std::format("Page '{}' registration failed: {}. House Rules controls are disabled.",
				                        page.name, DMUI_ResultToString(g_client.LastResult())));
				return;
			}
		}
		RefreshPresets();
		// Sorts between General (0) and the next catalog page (100).
		if (!g_client.AddSettingsPage(
		        { .id = "presets",
		          .displayName = RegistrationText(Strings::kPresetsPage),
		          .categoryId = "overview",
		          .summary = RegistrationText(Strings::kPresetsPageSummary),
		          .sortKey = 50,
		          .iconName = "sliders-horizontal" },
		        MakePresetsPage()))
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
