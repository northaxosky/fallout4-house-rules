#include "PCH.h"

#include "Hooks/Unlocks.h"

#include "Hooks/SafeTravel.h"
#include "Settings.h"

#include <memory>
#include <vector>

namespace Hooks::Unlocks
{
	namespace
	{
		using GDLFn = RE::DifficultyLevel(RE::PlayerCharacter*);
		using GDLHook = REL::THook<GDLFn>;

		using VoidPCFn = void(RE::PlayerCharacter*);
		using VoidPCHook = REL::THook<VoidPCFn>;

		using QueueSaveLoadTaskFn = void(RE::BGSSaveLoadManager*, RE::BGSSaveLoadManager::QUEUED_TASK);
		using QueueSaveLoadTaskHook = REL::THook<QueueSaveLoadTaskFn>;

		using QueryStatFn = bool(const RE::BSFixedString&, std::int32_t&);
		using QueryStatHook = REL::THook<QueryStatFn>;

		std::vector<std::unique_ptr<REL::IHook>> g_hooks;

		// Typed back-pointers for hooks whose spoof calls through to the original
		// via Hook::operator(). Aliases into the heap-allocated hook inside g_hooks.
		QueryStatHook* g_querystat_hook = nullptr;

		template <class HookT, class FnT>
		HookT* AddHook(std::uint64_t a_id, std::size_t a_offset, FnT* a_fn, const char* a_label)
		{
			auto hook = std::make_unique<HookT>(REL::ID(a_id), a_offset, a_fn);
			auto* raw = hook.get();
			const bool init_ok   = hook->Init();
			const bool enable_ok = hook->Enable();
			REX::INFO("Unlocks: hook {} (id={} +0x{:X}) init={} enable={}",
				a_label, a_id, a_offset, init_ok, enable_ok);
			g_hooks.push_back(std::move(hook));
			return raw;
		}

		struct RuntimeAddr
		{
			std::uint64_t id     = 0;  // 0 = skip on this runtime
			std::size_t   offset = 0;
		};

		// NG and AE mostly share offsets, but not always. Real-world exception:
		// FastTravel / PipboyMenu is at NG +0x347 / AE +0x34C (5-byte delta).
		// Verify any new site with tools/audit_hook_offsets.py against each runtime.
		const RuntimeAddr* SelectSite(
			const RuntimeAddr& og, const RuntimeAddr& ng, const RuntimeAddr& ae)
		{
			using RT = REX::FModule::Runtime;
			const RuntimeAddr* ra = nullptr;
			switch (REX::FModule::GetRuntimeIndex()) {
				case RT::kOG: ra = &og; break;
				case RT::kNG: ra = &ng; break;
				case RT::kAE: ra = &ae; break;
				default:      return nullptr;
			}
			return ra->id != 0 ? ra : nullptr;
		}

		RE::DifficultyLevel RealDifficulty(RE::PlayerCharacter* a_this)
		{
			using func_t = RE::DifficultyLevel (*)(RE::PlayerCharacter*);
			static REL::Relocation<func_t> real{ RE::ID::PlayerCharacter::GetDifficultyLevel };
			return real(a_this);
		}

		bool OverridesEnabled(bool a_toggle)
		{
			return MCM::Settings::General::bEnabled.GetValue() && a_toggle;
		}

		bool IsRealSurvival(RE::PlayerCharacter* a_this = RE::PlayerCharacter::GetSingleton())
		{
			return a_this && RealDifficulty(a_this) == RE::DifficultyLevel::kTrueSurvival;
		}

		bool ShouldSpoof(bool a_toggle, RE::PlayerCharacter* a_this = RE::PlayerCharacter::GetSingleton())
		{
			return OverridesEnabled(a_toggle) && IsRealSurvival(a_this);
		}

		bool AllowRestrictedAction(
			bool                  a_toggle,
			RE::PlayerCharacter*  a_this = RE::PlayerCharacter::GetSingleton())
		{
			return !IsRealSurvival(a_this) || OverridesEnabled(a_toggle);
		}

		RE::DifficultyLevel Spoof_Console(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bConsole.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_SaveSelf(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bSaveSelf.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_FastTravelEligibility(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bFastTravel.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_FastTravelRequest(RE::PlayerCharacter* a_this)
		{
			if (ShouldSpoof(MCM::Settings::Unlocks::bFastTravel.GetValue(), a_this)) {
				Hooks::SafeTravel::ArmFastTravel();
				return RE::DifficultyLevel::kVeryEasy;
			}
			return RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_SaveAuto(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bSaveAuto.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_CompassEnemies(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bCompassEnemies.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_CompassLocations(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bCompassLocations.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_NoAlchWeight(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bNoAlchWeight.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		RE::DifficultyLevel Spoof_NoAmmoWeight(RE::PlayerCharacter* a_this)
		{
			return ShouldSpoof(MCM::Settings::Unlocks::bNoAmmoWeight.GetValue(), a_this)
				? RE::DifficultyLevel::kVeryEasy
				: RealDifficulty(a_this);
		}

		void Queue_SaveSelf(RE::BGSSaveLoadManager* a_this, RE::BGSSaveLoadManager::QUEUED_TASK a_task)
		{
			if (!a_this || !AllowRestrictedAction(MCM::Settings::Unlocks::bSaveSelf.GetValue())) {
				return;
			}
			a_this->QueueSaveLoadTask(a_task);
		}

		void Queue_SaveAuto(RE::BGSSaveLoadManager* a_this, RE::BGSSaveLoadManager::QUEUED_TASK a_task)
		{
			if (!a_this || !AllowRestrictedAction(MCM::Settings::Unlocks::bSaveAuto.GetValue())) {
				return;
			}
			a_this->QueueSaveLoadTask(a_task);
		}

		void Replace_RequestQueueDoorAutosave(RE::PlayerCharacter* a_this)
		{
			if (!a_this) {
				return;
			}
			if (!IsRealSurvival(a_this)) {
				a_this->doorAutosaveQueued = true;
				return;
			}
			if (OverridesEnabled(MCM::Settings::Unlocks::bSaveAuto.GetValue())) {
				a_this->doorAutosaveQueued = true;
			}
		}

		bool Spoof_ReenableSurvival_QueryStat(const RE::BSFixedString& a_name, std::int32_t& a_value)
		{
			if (!OverridesEnabled(MCM::Settings::Unlocks::bReenableSurvival.GetValue())) {
				return (*g_querystat_hook)(a_name, a_value);
			}

			a_value = 0;
			return false;
		}

		template <class Fn>
		struct Site
		{
			RuntimeAddr og;
			RuntimeAddr ng;
			RuntimeAddr ae;
			Fn*         fn;
			const char* label;
		};

		// OG offsets: scan of the unpacked 1.10.163 Fallout4.exe, named via IDA
		// port JSON. NG/AE: Baka's source cross-checked with our scans of the
		// unpacked NG/AE binaries. An id of 0 skips on that runtime (call site
		// doesn't exist there, or can't be verified yet).
		//
		// NG and AE mostly match, but the FastTravel/PipboyMenu offset differs
		// (NG 0x347 vs AE 0x34C). Run tools/audit_hook_offsets.py after any
		// change here to catch drift.
		const Site<GDLFn> kGdlSites[] = {
			{ {927099,  0x20A}, {2249425, 0x1AA}, {2249425, 0x1AA}, &Spoof_Console,          "Console / MenuOpenHandler" },

			{ {425422,  0x047}, {2223965, 0x04D}, {2223965, 0x04D}, &Spoof_SaveSelf,         "SaveSelf / PauseMenu::CheckIfSaveLoadPossible" },
			{ {1330449, 0x0C1}, {2223964, 0x0C6}, {2223964, 0x0C6}, &Spoof_SaveSelf,         "SaveSelf / PauseMenu::InitMainList" },
			{ {1470086, 0x06C}, {2249427, 0x06C}, {2249427, 0x06C}, &Spoof_SaveSelf,         "SaveSelf / QuickSaveLoadHandler::HandleEvent" },

			{ {712982,  0x31E}, {2224179, 0x347}, {2224179, 0x34C}, &Spoof_FastTravelEligibility, "FastTravel / PipboyMenu::PipboyMenu" },
			{ {1327120, 0x013}, {2224206, 0x014}, {2224206, 0x014}, &Spoof_FastTravelRequest,     "FastTravel / nsPipboyMenu::CheckHardcoreFastTravel" },

			{ {1475119, 0x017}, {2220612, 0x01B}, {2220612, 0x01B}, &Spoof_CompassEnemies,   "CompassEnemies / HUDMarkerUtils::GetHostileEnemyMaxDistance" },

			{ {1301956, 0x00B}, {2220611, 0x00B}, {2220611, 0x00B}, &Spoof_CompassLocations, "CompassLocations / HUDMarkerUtils::GetLocationMaxDistance" },
			{ {1153736, 0x0A2}, {2220617, 0x086}, {2220617, 0x086}, &Spoof_CompassLocations, "CompassLocations / CalculateCompassMarkersFunctor::UpdateLocationMarkers" },

			{ {1321341, 0x092}, {2193446, 0x08E}, {2193446, 0x08E}, &Spoof_NoAlchWeight,     "Weight / TESWeightForm::GetFormWeight (alch)" },
			{ {1321341, 0x11C}, {2193446, 0x110}, {2193446, 0x110}, &Spoof_NoAmmoWeight,     "Weight / TESWeightForm::GetFormWeight (ammo)" },
		};

		const Site<VoidPCFn> kVoidPCSites[] = {
			{ {1397471, 0x1B4}, {2198697, 0x1B4}, {2198697, 0x1B4}, &Replace_RequestQueueDoorAutosave,
			  "SaveAuto / TESObjectDOOR::DoorTeleportPlayerArrivalCallback" },
		};

		const Site<QueueSaveLoadTaskFn> kQueueSaveLoadSites[] = {
			// Belt-and-braces queue gates: F5 has an earlier GDL gate above,
			// while menu surfaces still need their PauseMenu GDL hooks for UI state.
			{ {1470086, 0x082}, {2249427, 0x082}, {2249427, 0x082}, &Queue_SaveSelf,
			  "SaveSelf / QuickSaveLoadHandler (QueueSaveLoadTask #1)" },
			{ {1470086, 0x0C3}, {2249427, 0x0C3}, {2249427, 0x0C3}, &Queue_SaveSelf,
			  "SaveSelf / QuickSaveLoadHandler (QueueSaveLoadTask #2)" },

			// These autosaves already converge on BGSSaveLoadManager; hook there
			// instead of spoofing difficulty in each caller.
			{ {1158548, 0x07E}, {2223294, 0x07E}, {2223294, 0x07E}, &Queue_SaveAuto,
			  "SaveAuto / LevelUpMenu dtor" },
			{ {0,       0    }, {2223327, 0x087}, {2223327, 0x087}, &Queue_SaveAuto,
			  "SaveAuto / LevelUpMenu dtor (NG/AE extra)" },
			{ {98443,   0x1A9}, {2224974, 0x154}, {2224974, 0x154}, &Queue_SaveAuto,
			  "SaveAuto / WorkshopMenu dtor" },
			{ {592088,  0x12C}, {2225458, 0x1C4}, {2225458, 0x1C4}, &Queue_SaveAuto,
			  "SaveAuto / PipboyManager::OnPipboyClosed" },
			{ {146861,  0x1000}, {2232905, 0x10EA}, {2232905, 0x10EA}, &Queue_SaveAuto,
			  "SaveAuto / PlayerCharacter::HandlePositionPlayerRequest" },
		};

		// Re-enable Survival: hook the actual `MiscStatManager::QueryStat` call in
		// `PauseMenu::CheckIfSaveLoadPossible` instead of signature-scanning and
		// NOPing the call instruction. Proof/verification:
		//   - OG: tools/find_call_sites.py reports QueryStat at 425422 +0x14D.
		//   - NG: local disassembly of 2223965 shows the stat-query call at +0x153.
		//   - AE: local disassembly of 2223965 shows the same call shifted to +0x14E.
		// If any runtime drifts again, re-run those audits before changing offsets.
		const Site<QueryStatFn> kQueryStatSites[] = {
			{ {425422, 0x14D}, {2223965, 0x153}, {2223965, 0x14E}, &Spoof_ReenableSurvival_QueryStat,
			  "ReenableSurvival / PauseMenu::CheckIfSaveLoadPossible (MiscStatManager::QueryStat)" },
		};
	}

	void Install()
	{
		for (const auto& s : kGdlSites) {
			if (const auto* ra = SelectSite(s.og, s.ng, s.ae)) {
				AddHook<GDLHook>(ra->id, ra->offset, s.fn, s.label);
			}
		}
		for (const auto& s : kVoidPCSites) {
			if (const auto* ra = SelectSite(s.og, s.ng, s.ae)) {
				AddHook<VoidPCHook>(ra->id, ra->offset, s.fn, s.label);
			}
		}
		for (const auto& s : kQueueSaveLoadSites) {
			if (const auto* ra = SelectSite(s.og, s.ng, s.ae)) {
				AddHook<QueueSaveLoadTaskHook>(ra->id, ra->offset, s.fn, s.label);
			}
		}
		for (const auto& s : kQueryStatSites) {
			if (const auto* ra = SelectSite(s.og, s.ng, s.ae)) {
				g_querystat_hook = AddHook<QueryStatHook>(ra->id, ra->offset, s.fn, s.label);
			}
		}
	}

	void RefreshRuntimePatches()
	{
		// No byte patches remain in Unlocks; hooks read settings live.
	}
}
