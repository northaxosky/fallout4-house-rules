#pragma once

#include <atomic>
#include <cstdint>

namespace HouseRules::Gameplay
{
	enum class ApplyDisposition : std::uint8_t
	{
		kApply,
		kDeferred,
		kStale
	};

	[[nodiscard]] constexpr ApplyDisposition EvaluateApply(
	    bool a_revisionCurrent,
	    bool a_ready) noexcept
	{
		if (!a_revisionCurrent)
		{
			return ApplyDisposition::kStale;
		}
		return a_ready ? ApplyDisposition::kApply : ApplyDisposition::kDeferred;
	}

	class LifecycleState
	{
	public:
		void MarkNotReady() noexcept;
		void OnMainMenu(bool a_opening) noexcept;
		void OnLoadingMenu(bool a_opening) noexcept;
		[[nodiscard]] bool ConfirmLoadingComplete(
		    bool a_mainMenuCurrentlyOpen) noexcept;
		[[nodiscard]] bool IsReady() const noexcept;

	private:
		enum Flag : std::uint8_t
		{
			kReady = 1 << 0,
			kMainMenuOpen = 1 << 1,
			kLoadingMenuOpen = 1 << 2,
			kLoadingClosePending = 1 << 3
		};

		std::atomic_uint8_t _flags {};
	};
}  // namespace HouseRules::Gameplay
