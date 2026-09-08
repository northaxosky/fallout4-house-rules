#pragma once

#include <string_view>
#include <utility>

namespace HouseRules::UI
{
	struct MutationGateResult
	{
		bool allowed {};
		std::string_view message;
	};

	[[nodiscard]] constexpr MutationGateResult EvaluateMutationGate(
	    bool a_registrationComplete,
	    bool a_settingsLoaded) noexcept
	{
		if (!a_registrationComplete)
		{
			return {
				false,
				"House Rules native page registration is incomplete; settings actions are disabled."
			};
		}
		if (!a_settingsLoaded)
		{
			return {
				false,
				"Settings are not loaded yet; enter or load a game before editing or applying changes."
			};
		}
		return { true, {} };
	}

	template<class F>
	[[nodiscard]] MutationGateResult TryMutation(
	    bool a_registrationComplete,
	    bool a_settingsLoaded,
	    F&& a_mutation)
	{
		const auto result = EvaluateMutationGate(
		    a_registrationComplete,
		    a_settingsLoaded);
		if (result.allowed)
		{
			std::forward<F>(a_mutation)();
		}
		return result;
	}
}  // namespace HouseRules::UI
