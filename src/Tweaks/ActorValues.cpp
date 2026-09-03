#include "PCH.h"

#include "Tweaks/ActorValues.h"

#include "Diagnostics/Logging.h"
#include "Settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Tweaks::ActorValues
{
	namespace
	{
		struct FloatTarget
		{
			const char*                  label;
			RE::ActorValueInfo* RE::ActorValue::* member;
			REX::TIniSetting<float>*    setting;
			float                        neutral;
			float                        minClamp = 0.0f;
			float                        maxClamp = 0.0f;
		};

		struct TargetState
		{
			float lastUser = 0.0f;
			bool  hasLast = false;
		};

		struct ApplyResult
		{
			std::size_t applied = 0;
			std::size_t changed = 0;
			std::size_t skipped = 0;
			std::size_t passed = 0;
			std::size_t failed = 0;
		};

		const std::array<FloatTarget, 3> kTargets = { {
			{ "restoreAPRate",          &RE::ActorValue::restoreAPRate,          &MCM::Settings::Character::fAPRegenRate,           6.0f, 0.0f, 200.0f },
			{ "restoreHealthRate",      &RE::ActorValue::restoreHealthRate,      &MCM::Settings::Character::fPassiveHealthRegen,    0.0f, 0.0f, 100.0f },
			{ "combatHealthRegenMult",  &RE::ActorValue::combatHealthRegenMult,  &MCM::Settings::Character::fCombatHealthRegenMult, 0.0f, 0.0f, 10.0f  },
		} };

		bool g_warnedMissingActorValues = false;
		bool g_warnedMissingPlayer = false;
		bool g_appliedNonNeutral = false;
		std::unordered_map<std::string, TargetState> g_state;
		std::unordered_set<std::string> g_warnedKeys;

		bool MaterialDelta(float a_prev, float a_now)
		{
			const float scale = std::max(1.0f, std::max(std::abs(a_prev), std::abs(a_now)));
			return std::abs(a_prev - a_now) > 1e-4f * scale;
		}

		bool NearlyEqual(float a_left, float a_right)
		{
			return !MaterialDelta(a_left, a_right);
		}

		float Clamp(float a_value, const FloatTarget& a_target)
		{
			return std::clamp(a_value, a_target.minClamp, a_target.maxClamp);
		}

		bool WarnKey(std::string_view a_key, std::string_view a_msg)
		{
			std::string key{ a_key };
			if (g_warnedKeys.contains(key)) {
				return false;
			}
			g_warnedKeys.insert(std::move(key));
			REX::WARN("ActorValues: {} ({})", a_msg, a_key);
			return true;
		}

		void LogMissingRuntimeSummary()
		{
			if (!Diagnostics::Logging::ValidationAuditEnabled()) {
				return;
			}

			REX::INFO("HRVERIFY_SUMMARY module=ActorValues type=actorvalue applied=0 passed=0 failed=0 skipped={} result=FAIL",
				kTargets.size());
		}

		RE::ActorValueInfo* ResolveTarget(RE::ActorValue& a_actorValues, const FloatTarget& a_target)
		{
			return a_actorValues.*(a_target.member);
		}

		float UserValue(const FloatTarget& a_target, bool a_forceNeutral)
		{
			return a_forceNeutral ? a_target.neutral : Clamp(a_target.setting->GetValue(), a_target);
		}

		bool CurrentBasesDifferFromNeutral(RE::PlayerCharacter& a_player, RE::ActorValue& a_actorValues)
		{
			return std::ranges::any_of(kTargets, [&](const FloatTarget& a_target) {
				auto* info = ResolveTarget(a_actorValues, a_target);
				return info && MaterialDelta(a_player.GetBaseActorValue(*info), a_target.neutral);
			});
		}

		ApplyResult ApplyTargets(RE::PlayerCharacter& a_player, RE::ActorValue& a_actorValues, bool a_forceNeutral)
		{
			ApplyResult result;
			const bool trace = MCM::Settings::Diagnostic::bGameSettingsTrace.GetValue();
			const bool audit = Diagnostics::Logging::ValidationAuditEnabled();
			const bool auditFull = audit &&
				Diagnostics::Logging::ValidationAuditMode() == Diagnostics::Logging::AuditMode::Full;

			for (const auto& target : kTargets) {
				auto* info = ResolveTarget(a_actorValues, target);
				if (!info) {
					WarnKey(target.label, "ActorValue unavailable; skipping");
					++result.skipped;
					if (auditFull) {
						REX::INFO("HRVERIFY module=ActorValues target={} type=actorvalue mode=direct result=SKIP reason=missing_actorvalue",
							target.label);
					}
					continue;
				}

				auto& state = g_state[target.label];
				const float user = UserValue(target, a_forceNeutral);
				const float wanted = user;
				const float before = a_player.GetBaseActorValue(*info);

				if (!a_forceNeutral && !NearlyEqual(user, target.neutral)) {
					g_appliedNonNeutral = true;
				}

				if (MaterialDelta(before, wanted)) {
					a_player.SetBaseActorValue(*info, wanted);
				}
				++result.applied;

				const float readback = a_player.GetBaseActorValue(*info);
				const bool pass = NearlyEqual(readback, wanted);
				if (pass) {
					++result.passed;
				} else {
					++result.failed;
				}

				if (!state.hasLast || MaterialDelta(state.lastUser, user)) {
					++result.changed;
					state.lastUser = user;
					state.hasLast = true;
				}

				if (trace) {
					REX::INFO("  ActorValues {} neutral={} user={} before={} wrote={} readback={}",
						target.label, target.neutral, user, before, wanted, readback);
				}

				if (auditFull) {
					REX::INFO("HRVERIFY module=ActorValues target={} type=actorvalue mode=direct baseline={:g} user={:g} expected={:g} readback={:g} result={}",
						target.label, target.neutral, user, wanted, readback, pass ? "PASS" : "FAIL");
				}
			}

			return result;
		}

		bool GetRuntime(RE::PlayerCharacter*& a_player, RE::ActorValue*& a_actorValues)
		{
			a_player = RE::PlayerCharacter::GetSingleton();
			a_actorValues = RE::ActorValue::GetSingleton();

			if (!a_actorValues) {
				if (!g_warnedMissingActorValues) {
					REX::WARN("ActorValues: ActorValue singleton unavailable; skipping");
					g_warnedMissingActorValues = true;
				}
				LogMissingRuntimeSummary();
				return false;
			}

			if (!a_player) {
				if (!g_warnedMissingPlayer) {
					REX::WARN("ActorValues: player unavailable; skipping");
					g_warnedMissingPlayer = true;
				}
				LogMissingRuntimeSummary();
				return false;
			}

			return true;
		}
	}

	void Apply()
	{
		RE::PlayerCharacter* player = nullptr;
		RE::ActorValue* actorValues = nullptr;
		if (!GetRuntime(player, actorValues)) {
			return;
		}

		if (!MCM::Settings::General::bEnabled.GetValue()) {
			if (g_appliedNonNeutral && CurrentBasesDifferFromNeutral(*player, *actorValues)) {
				const auto result = ApplyTargets(*player, *actorValues, true);
				if (result.applied > 0) {
					REX::INFO("ActorValues: restored vanilla neutral values after plugin disable");
				}
				g_appliedNonNeutral = false;
			}
			return;
		}

		const auto result = ApplyTargets(*player, *actorValues, false);

		if (result.changed > 0 || MCM::Settings::Diagnostic::bGameSettingsTrace.GetValue()) {
			REX::INFO("ActorValues: applied {} target(s), {} changed, {} skipped",
				result.applied, result.changed, result.skipped);
		}

		if (Diagnostics::Logging::ValidationAuditEnabled()) {
			const bool overallPass = (result.failed == 0) && (result.skipped == 0);
			REX::INFO("HRVERIFY_SUMMARY module=ActorValues type=actorvalue applied={} passed={} failed={} skipped={} result={}",
				result.applied, result.passed, result.failed, result.skipped, overallPass ? "PASS" : "FAIL");
		}
	}

	void ResetSnapshots()
	{
		g_state.clear();
		g_appliedNonNeutral = false;
	}
}
