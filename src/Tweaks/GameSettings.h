#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// Reusable GameSettingCollection helper. Future tweak modules build a
// static table of Target<float>/Target<std::int32_t> entries and call Apply
// from a safe context (PauseMenu close / loading-menu close), same as
// Tweaks::Magnitudes / Tweaks::Difficulty.
//
// Two value modes:
//   - Multiplier: snapshot the engine baseline once per process, then write
//     baseline * MCM value. Neutral MCM value (typically 1.0) is a no-op.
//   - Direct:     write the MCM value verbatim. By default, when the MCM
//     value equals `neutral` the helper restores the snapshotted baseline
//     instead, so other mods' edits aren't clobbered until the user opts in.
//     Set `preserveBaselineAtNeutral = false` to force literal writes.
//
// Snapshots are taken once per GMST per session. Missing or wrong-type
// settings warn exactly once. Per-target trace readback is gated by
// MCM::Settings::Diagnostic::bGameSettingsTrace.
namespace Tweaks::GameSettings
{
	enum class Mode
	{
		Multiplier,
		Direct,
	};

	template <typename T>
	struct Target
	{
		const char*           gmst;     // GameSettingCollection key
		REX::TIniSetting<T>*  setting;  // MCM-backed value (REX::INI::F32<> / I32<>)
		Mode                  mode                       = Mode::Multiplier;
		T                     neutral                    = T{ 1 };
		bool                  preserveBaselineAtNeutral  = true;
		std::optional<T>      minClamp                   = std::nullopt;
		std::optional<T>      maxClamp                   = std::nullopt;
	};

	using FloatTarget = Target<float>;
	using IntTarget   = Target<std::int32_t>;

	struct ApplyResult
	{
		std::size_t applied = 0;  // writes attempted
		std::size_t changed = 0;  // MCM value materially changed since last Apply
		std::size_t skipped = 0;  // missing / wrong-type
	};

	ApplyResult Apply(std::string_view a_module, std::span<const FloatTarget> a_targets);
	ApplyResult Apply(std::string_view a_module, std::span<const IntTarget>   a_targets);

	// Optional: emit a one-shot warning under the helper's tag, e.g. for
	// modules that want to log a paired-setting violation without standing
	// up their own once-only state.
	void WarnOnce(std::string_view a_module, std::string_view a_key, std::string_view a_message);
}
