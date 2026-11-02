#include "PCH.h"

#include "Tweaks/Magnitudes.h"

#include "Settings.h"
#include "Tweaks/GameSettings.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Tweaks::Magnitudes
{
	namespace
	{
		using EffectScaler = std::function<float(std::uint32_t /*effect_setting_form_id*/)>;

		// Vanilla FormIDs (Fallout4.esm). Provenance: findings/vanilla-survival-stages.md.
		// AlchemyItem consumables
		constexpr std::uint32_t kAlchStimpak = 0x00023736;
		constexpr std::uint32_t kAlchRadAway = 0x00023742;
		constexpr std::uint32_t kAlchRadX    = 0x00024057;

		// Hunger stages (Peckish -> Starving)
		constexpr std::array<std::uint32_t, 5> kAlchHungerStages = {
			0x00000856, 0x00000857, 0x00000858, 0x00000859, 0x0000085A
		};
		// Thirst stages (Thirsty -> SeverelyDehydrated)
		constexpr std::array<std::uint32_t, 5> kAlchThirstStages = {
			0x00000871, 0x00000872, 0x00000873, 0x00000874, 0x00000875
		};
		// Sleep stages (Tired -> Incapacitated). Note: not contiguous FormIDs.
		constexpr std::array<std::uint32_t, 5> kAlchSleepStages = {
			0x00000818, 0x0000081A, 0x0000081C, 0x0000081E, 0x00000820
		};

		// Vanilla's HC_ReduceCarryWeightAbility is ESP-owned now (see
		// HouseRules.esp -- condition-gated on HR_NoCarryWeight global).
		// The DLL writes that global from MCM via Globals::WriteBool in
		// Settings::Update. No runtime SPEL scan / snapshot is needed here.

		// MGEFs (EffectSetting records) recovered from the archived survival dumps.
		constexpr std::uint32_t kEffectRestoreHealthFood          = 0x0000397E;
		constexpr std::uint32_t kEffectRestoreHealthStimpak       = 0x0021DDB8;
		constexpr std::uint32_t kEffectRestoreConditionHead       = 0x0005C529;
		constexpr std::uint32_t kEffectRestoreConditionLeftArm    = 0x0005C52C;
		constexpr std::uint32_t kEffectRestoreConditionLeftLeg    = 0x0005C52D;
		constexpr std::uint32_t kEffectRestoreConditionRightArm   = 0x0005C52A;
		constexpr std::uint32_t kEffectRestoreConditionRightLeg   = 0x0005C52B;
		constexpr std::uint32_t kEffectRestoreConditionTorso      = 0x0005C52E;
		constexpr std::uint32_t kEffectRestoreRadsChem            = 0x00023738;
		constexpr std::uint32_t kEffectFortifyResistRadsRadX      = 0x00246B11;

		// StimpakEffect also restores every limb condition by its health delta times this GMST.
		const std::array<GameSettings::FloatTarget, 1> kLimbRepairTargets = { {
			{ "fMagicVACNoPartTargetedMult", &MCM::Settings::Magnitudes::fStimpakLimbRepair },
		} };

		struct EffectItemSnapshot
		{
			std::uint32_t effectSettingFormID;
			float         baseline;
		};

		struct AlchSnapshot
		{
			std::uint32_t                 formID;
			std::vector<EffectItemSnapshot> effects;
		};

		struct MatchedAlchemyEffectSnapshot
		{
			std::uint32_t ownerFormID;
			std::uint32_t effectIndex;
			float         baseline;
		};

		std::vector<AlchSnapshot>                 g_alch_snapshots;
		std::vector<MatchedAlchemyEffectSnapshot> g_food_heal_snapshots;
		bool                                      g_snapshotted = false;

		struct LastApplied
		{
			std::uint32_t ownerFormID;
			std::uint32_t effectIndex;
			float         value;
		};
		std::vector<LastApplied> g_last_applied;

		struct MultVector
		{
			float stim_heal;
			float stim_limb;
			float radaway;
			float radx;
			float food;
			float hunger_pen;
			float thirst_pen;
			float sleep_pen;
			bool operator==(const MultVector&) const = default;
		};
		std::optional<MultVector> g_last_mults;

		bool MaterialDelta(float a_prev, float a_now)
		{
			const float scale = std::max(1.0f, std::max(std::abs(a_prev), std::abs(a_now)));
			return std::abs(a_prev - a_now) > 1e-4f * scale;
		}

		float* FindLastApplied(std::uint32_t a_owner, std::uint32_t a_index)
		{
			for (auto& entry : g_last_applied) {
				if (entry.ownerFormID == a_owner && entry.effectIndex == a_index) {
					return std::addressof(entry.value);
				}
			}
			return nullptr;
		}

		void RecordApplied(std::uint32_t a_owner, std::uint32_t a_index, float a_value)
		{
			if (auto* prev = FindLastApplied(a_owner, a_index)) {
				*prev = a_value;
				return;
			}
			g_last_applied.push_back({ a_owner, a_index, a_value });
		}

		// Intentionally not using TESDataHandler::LookupForm — that path walks
		// through the engine's form-by-ID hash in a way that triggers a lazy
		// BSStaticTriShapeDB preload on OG, null-derefing at Fallout4.exe+0x405A
		// whenever it runs before meshes are warm. Linear-scanning the typed
		// form arrays (same path our Diagnostics dump uses successfully) stays
		// clear of that trap. ~hundreds of entries per type — negligible on the
		// once-per-save-load cadence this runs at.
		RE::AlchemyItem* LookupAlch(std::uint32_t a_formID)
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) return nullptr;
			for (auto* alch : dh->GetFormArray<RE::AlchemyItem>()) {
				if (alch && alch->formID == a_formID) return alch;
			}
			return nullptr;
		}

		void SnapshotAlch(std::uint32_t a_formID)
		{
			auto* alch = LookupAlch(a_formID);
			if (!alch) {
				REX::WARN("Magnitudes: ALCH {:08X} not found", a_formID);
				return;
			}
			AlchSnapshot snap{ a_formID, {} };
			snap.effects.reserve(alch->listOfEffects.size());
			for (auto* eff : alch->listOfEffects) {
				snap.effects.push_back({
					eff && eff->effectSetting ? eff->effectSetting->formID : 0,
					eff ? eff->data.magnitude : 0.0f
				});
			}
			g_alch_snapshots.push_back(std::move(snap));
		}

		void SnapshotAlchemyEffectsBySetting(std::uint32_t a_effectSettingFormID)
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return;
			}

			for (auto* alch : dh->GetFormArray<RE::AlchemyItem>()) {
				if (!alch) {
					continue;
				}
				for (std::uint32_t i = 0; i < alch->listOfEffects.size(); ++i) {
					const auto* eff = alch->listOfEffects[i];
					if (!eff || !eff->effectSetting || eff->effectSetting->formID != a_effectSettingFormID) {
						continue;
					}
					g_food_heal_snapshots.push_back({
						alch->formID,
						i,
						eff->data.magnitude
					});
				}
			}
		}

		void SnapshotAll()
		{
			SnapshotAlch(kAlchStimpak);
			SnapshotAlch(kAlchRadAway);
			SnapshotAlch(kAlchRadX);
			for (auto id : kAlchHungerStages) SnapshotAlch(id);
			for (auto id : kAlchThirstStages) SnapshotAlch(id);
			for (auto id : kAlchSleepStages)  SnapshotAlch(id);
			SnapshotAlchemyEffectsBySetting(kEffectRestoreHealthFood);
			REX::INFO("Magnitudes: snapshotted {} ALCH baselines and {} RestoreHealthFood entries",
				g_alch_snapshots.size(), g_food_heal_snapshots.size());
		}

		const AlchSnapshot* FindAlchSnapshot(std::uint32_t a_formID)
		{
			for (const auto& s : g_alch_snapshots) {
				if (s.formID == a_formID) {
					return std::addressof(s);
				}
			}
			return nullptr;
		}

		void ApplyAlch(std::uint32_t a_formID, const EffectScaler& a_scaler, bool a_trace, bool a_trace_delta_only)
		{
			const auto* snap = FindAlchSnapshot(a_formID);
			if (!snap) return;
			auto* alch = LookupAlch(a_formID);
			if (!alch) return;
			const auto count = std::min<std::size_t>(alch->listOfEffects.size(), snap->effects.size());
			for (std::size_t i = 0; i < count; ++i) {
				auto* eff = alch->listOfEffects[static_cast<std::uint32_t>(i)];
				if (!eff) continue;
				const auto& entry = snap->effects[i];
				const auto  currentSettingFormID = eff->effectSetting ? eff->effectSetting->formID : 0;
				if (entry.effectSettingFormID != currentSettingFormID) {
					REX::WARN("Magnitudes: ALCH {:08X}[{}] effect changed from {:08X} to {:08X}; skipping",
						a_formID,
						i,
						entry.effectSettingFormID,
						currentSettingFormID);
					continue;
				}
				const float wanted = entry.baseline * a_scaler(currentSettingFormID);
				eff->data.magnitude = wanted;
				const auto readback = eff->data.magnitude;

				const std::uint32_t idx32 = static_cast<std::uint32_t>(i);
				if (a_trace) {
					bool emit = true;
					if (a_trace_delta_only) {
						const float* prev = FindLastApplied(a_formID, idx32);
						const bool changed_vs_baseline = MaterialDelta(entry.baseline, readback);
						const bool changed_vs_prev = prev ? MaterialDelta(*prev, readback) : changed_vs_baseline;
						emit = prev ? changed_vs_prev : changed_vs_baseline;
					}
					if (emit) {
						const int archetype = eff->effectSetting
							  ? static_cast<int>(eff->effectSetting->data.archetype.get())
							  : -1;
						const char* mgef_edid = eff->effectSetting ? eff->effectSetting->GetFormEditorID() : "";
						REX::INFO("  ALCH {:08X}[{}]: wrote={} readback={} archetype={} mgef='{}'",
							a_formID, i, wanted, readback, archetype, mgef_edid ? mgef_edid : "");
					}
				}
				RecordApplied(a_formID, idx32, readback);
			}
		}

		void ApplyFoodHeal(float a_mult, bool a_trace, bool a_trace_delta_only)
		{
			for (const auto& snap : g_food_heal_snapshots) {
				auto* alch = LookupAlch(snap.ownerFormID);
				if (!alch || snap.effectIndex >= alch->listOfEffects.size()) {
					continue;
				}

				auto* eff = alch->listOfEffects[snap.effectIndex];
				if (!eff || !eff->effectSetting || eff->effectSetting->formID != kEffectRestoreHealthFood) {
					REX::WARN("Magnitudes: food-heal entry {:08X}[{}] changed effect identity; skipping",
						snap.ownerFormID,
						snap.effectIndex);
					continue;
				}

				const float wanted = snap.baseline * a_mult;
				eff->data.magnitude = wanted;
				const float readback = eff->data.magnitude;

				if (a_trace) {
					bool emit = true;
					if (a_trace_delta_only) {
						const float* prev = FindLastApplied(snap.ownerFormID, snap.effectIndex);
						const bool changed_vs_baseline = MaterialDelta(snap.baseline, readback);
						const bool changed_vs_prev = prev ? MaterialDelta(*prev, readback) : changed_vs_baseline;
						emit = prev ? changed_vs_prev : changed_vs_baseline;
					}
					if (emit) {
						REX::INFO("  ALCH {:08X}[{}] (food-heal): wrote={} readback={}",
							snap.ownerFormID, snap.effectIndex, wanted, readback);
					}
				}
				RecordApplied(snap.ownerFormID, snap.effectIndex, readback);
			}
		}
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}

		// Must only be called from a safe context (pause-menu close inside a
		// loaded save). On OG, kGameDataReady runs inside Main_InitGameData
		// on the InitGameDataThread while mesh/UI subsystems are still warming
		// up; TESDataHandler::LookupForm and UI::GetMenu both trigger lazy
		// preloads there and null-deref in BSStaticTriShapeDB::Force (crashes
		// 2851eb7 and its MainMenu-gated follow-up). Hook the pause-menu sink
		// instead of Settings::Update.

		if (!g_snapshotted) {
			SnapshotAll();
			g_snapshotted = true;
		}

		const float stim_heal  = MCM::Settings::Magnitudes::fStimpakHeal.GetValue();
		const float stim_limb  = MCM::Settings::Magnitudes::fStimpakLimbRepair.GetValue();
		const float radaway    = MCM::Settings::Magnitudes::fRadAwayStrength.GetValue();
		const float radx       = MCM::Settings::Magnitudes::fRadXPotency.GetValue();
		const float food       = MCM::Settings::Magnitudes::fFoodHeal.GetValue();
		const float hunger_pen = MCM::Settings::Magnitudes::fHungerPenalty.GetValue();
		const float thirst_pen = MCM::Settings::Magnitudes::fThirstPenalty.GetValue();
		const float sleep_pen  = MCM::Settings::Magnitudes::fSleepPenalty.GetValue();

		const bool trace             = MCM::Settings::Diagnostic::bMagnitudesTrace.GetValue();
		const bool trace_delta_only  = MCM::Settings::Diagnostic::bMagnitudesTraceDeltaOnly.GetValue();
		const bool always_summarize  = MCM::Settings::Diagnostic::bMagnitudesAlwaysSummarize.GetValue();

		ApplyAlch(kAlchStimpak, [stim_heal, stim_limb](std::uint32_t a_effectSettingFormID) -> float {
			switch (a_effectSettingFormID) {
				case kEffectRestoreHealthStimpak:
					return stim_heal;
				case kEffectRestoreConditionHead:
				case kEffectRestoreConditionLeftArm:
				case kEffectRestoreConditionLeftLeg:
				case kEffectRestoreConditionRightArm:
				case kEffectRestoreConditionRightLeg:
				case kEffectRestoreConditionTorso:
					return stim_limb;
				default:
					return 1.0f;
			}
		}, trace, trace_delta_only);
		ApplyAlch(kAlchRadAway, [radaway](std::uint32_t a_effectSettingFormID) {
			return a_effectSettingFormID == kEffectRestoreRadsChem ? radaway : 1.0f;
		}, trace, trace_delta_only);
		ApplyAlch(kAlchRadX, [radx](std::uint32_t a_effectSettingFormID) {
			return a_effectSettingFormID == kEffectFortifyResistRadsRadX ? radx : 1.0f;
		}, trace, trace_delta_only);

		for (auto id : kAlchHungerStages) {
			ApplyAlch(id, [hunger_pen](std::uint32_t) { return hunger_pen; }, trace, trace_delta_only);
		}
		for (auto id : kAlchThirstStages) {
			ApplyAlch(id, [thirst_pen](std::uint32_t) { return thirst_pen; }, trace, trace_delta_only);
		}
		for (auto id : kAlchSleepStages) {
			ApplyAlch(id, [sleep_pen](std::uint32_t) { return sleep_pen; }, trace, trace_delta_only);
		}

		ApplyFoodHeal(food, trace, trace_delta_only);
		GameSettings::Apply("Magnitudes", std::span<const GameSettings::FloatTarget>{ kLimbRepairTargets });

		const MultVector mults{
			stim_heal, stim_limb, radaway, radx, food, hunger_pen, thirst_pen, sleep_pen
		};
		const bool changed = !g_last_mults.has_value() || !(*g_last_mults == mults);
		if (changed || always_summarize) {
			REX::INFO("Magnitudes: applied (stim_heal={} stim_limb={} radaway={} radx={} food={} hunger={} thirst={} sleep={})",
				stim_heal, stim_limb, radaway, radx, food, hunger_pen, thirst_pen, sleep_pen);
		}
		g_last_mults = mults;
	}
}
