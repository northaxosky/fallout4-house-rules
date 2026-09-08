#include "Gameplay/Lifecycle.h"

namespace HouseRules::Gameplay
{
	void LifecycleState::MarkNotReady() noexcept
	{
		_flags.fetch_and(
		    static_cast<std::uint8_t>(~(kReady | kLoadingClosePending)),
		    std::memory_order_release);
	}

	void LifecycleState::OnMainMenu(bool a_opening) noexcept
	{
		if (a_opening)
		{
			_flags.fetch_or(kMainMenuOpen, std::memory_order_acq_rel);
			MarkNotReady();
		}
		else
		{
			_flags.fetch_and(
			    static_cast<std::uint8_t>(~kMainMenuOpen),
			    std::memory_order_release);
		}
	}

	void LifecycleState::OnLoadingMenu(bool a_opening) noexcept
	{
		if (a_opening)
		{
			_flags.fetch_or(kLoadingMenuOpen, std::memory_order_acq_rel);
			_flags.fetch_and(
			    static_cast<std::uint8_t>(
			        ~(kReady | kLoadingClosePending)),
			    std::memory_order_release);
		}
		else
		{
			_flags.fetch_and(
			    static_cast<std::uint8_t>(
			        ~(kReady | kLoadingMenuOpen)),
			    std::memory_order_acq_rel);
			_flags.fetch_or(
			    kLoadingClosePending,
			    std::memory_order_release);
		}
	}

	bool LifecycleState::ConfirmLoadingComplete(
	    bool a_mainMenuCurrentlyOpen) noexcept
	{
		auto flags = _flags.load(std::memory_order_acquire);
		for (;;)
		{
			if ((flags & kLoadingClosePending) == 0)
			{
				return false;
			}
			auto next = static_cast<std::uint8_t>(
			    flags & ~kLoadingClosePending);
			if (!a_mainMenuCurrentlyOpen &&
			    (flags & (kMainMenuOpen | kLoadingMenuOpen)) == 0)
			{
				next = static_cast<std::uint8_t>(next | kReady);
			}
			else
			{
				next = static_cast<std::uint8_t>(next & ~kReady);
			}
			if (_flags.compare_exchange_weak(
			        flags,
			        next,
			        std::memory_order_acq_rel,
			        std::memory_order_acquire))
			{
				return (next & kReady) != 0;
			}
		}
	}

	bool LifecycleState::IsReady() const noexcept
	{
		const auto flags = _flags.load(std::memory_order_acquire);
		return (flags & kReady) != 0 &&
		       (flags & (kMainMenuOpen | kLoadingMenuOpen)) == 0;
	}
}  // namespace HouseRules::Gameplay
