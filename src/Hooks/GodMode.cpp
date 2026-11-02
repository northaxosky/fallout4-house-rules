#include "PCH.h"

#include "Hooks/GodMode.h"

#include "Settings.h"

// Byte-patch approach, mirroring what Unlimited Survival Mode does. Vanilla
// PC::IsGodMode and PC::IsImmortal share the same survival-gate:
//
//   +0x22  cmp eax, 5          ; eax = difficulty
//   +0x25  jl  +0x35           ; low-diff path (fallthrough returns 1)
//   +0x27  xor eax, eax        ; eax = 0
//   +0x29  cmp [survival_flag], al
//   +0x2F  setne al            ; al = 1 if flag is set
//   +0x32  add eax, 5          ; eax = 5 or 6  <-- patch target here
//   +0x35  cmp eax, 6
//   +0x38  je  return_0        ; god mode blocked when eax == 6
//
// We flip the `5` immediate of `add eax, 5` to `0`, so the high-diff path
// produces eax = 0 or 1, never 6. The final `je` never fires, so god mode
// returns 1 whenever the engine's flag is set, regardless of Survival.
// Low-difficulty (diff < 5) takes the `jl` skip and isn't affected.
//
// PlayerCharacter::CheckClampDamageModifier (vfunc 0x131) inlines both
// checks with the same gate; the health-damage clamp lives there, so those
// immediates need the same patch. Offsets are identical in OG/NG/AE.
//
// Why byte-patch over replace_func: leaves vanilla IsGodMode's return-value
// logic intact, so the console's ToggleGodModeFunction reads a faithful
// answer and prints "god mode on/off" correctly. The replace_func variant
// we tried first created a "says off but I'm actually invincible" glitch
// because our gating made IsGodMode lie about state.
namespace Hooks::GodMode
{
	namespace
	{
		constexpr std::size_t  kImmIndex  = 2;     // 'add e?x, 5' immediate within the sequence
		constexpr std::uint8_t kOrigByte  = 0x05;  // vanilla: adds 5 (path can produce 6)
		constexpr std::uint8_t kPatchByte = 0x00;  // patched: adds 0 (never 6, never blocked)

		using Sequence = std::array<std::uint8_t, 6>;
		constexpr Sequence kGateEax{ 0x83, 0xC0, kOrigByte, 0x83, 0xF8, 0x06 };  // add eax, 5; cmp eax, 6
		constexpr Sequence kGateEcx{ 0x83, 0xC1, kOrigByte, 0x83, 0xF9, 0x06 };  // add ecx, 5; cmp ecx, 6

		struct Site
		{
			const char*    label;
			REL::ID        function;
			std::size_t    offset;  // start of the gate sequence
			Sequence       vanilla;
			std::uintptr_t imm = 0;
		};

		std::array g_sites{
			Site{ "IsGodMode", REL::ID({ 1032309, 2232986 }), 0x32, kGateEax },
			Site{ "IsImmortal", REL::ID({ 500346, 2232988 }), 0x32, kGateEax },
			Site{ "CheckClampDamageModifier (immortal)", REL::ID({ 1500216, 2233170 }), 0x54, kGateEcx },
			Site{ "CheckClampDamageModifier (god mode)", REL::ID({ 1500216, 2233170 }), 0xBB, kGateEax }
		};

		bool g_patched   = false;
		bool g_validated = false;

		bool ValidateSite(Site& a_site)
		{
			const auto start = a_site.function.address() + a_site.offset;
			Sequence bytes{};
			std::memcpy(bytes.data(), reinterpret_cast<const void*>(start), bytes.size());
			auto patched = a_site.vanilla;
			patched[kImmIndex] = kPatchByte;
			if (bytes == a_site.vanilla || bytes == patched) {
				a_site.imm = start + kImmIndex;
				REX::INFO("GodMode / {}: verified anchor bytes {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} at 0x{:X}",
					a_site.label, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], start);
				return true;
			}

			REX::ERROR("GodMode / {}: unexpected bytes {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} at 0x{:X}; refusing patch",
				a_site.label, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], start);
			return false;
		}

		void ApplyPatchState()
		{
			if (!g_validated) {
				return;
			}
			const bool want = MCM::Settings::General::bEnabled.GetValue() &&
				MCM::Settings::Unlocks::bGodMode.GetValue();
			if (want == g_patched) {
				return;
			}
			const std::uint8_t byte = want ? kPatchByte : kOrigByte;
			bool ok = true;
			for (const auto& site : g_sites) {
				ok = REL::WriteSafe(site.imm, &byte, 1) && ok;
			}
			if (ok) {
				g_patched = want;
				REX::INFO("GodMode: {} {} survival-gate patches", want ? "applied" : "reverted", g_sites.size());
			} else {
				REX::ERROR("GodMode: WriteSafe failed for at least one survival-gate patch");
			}
		}
	}

	void Install()
	{
		g_validated = true;
		for (auto& site : g_sites) {
			g_validated = ValidateSite(site) && g_validated;
		}
		if (!g_validated) {
			return;
		}

		std::size_t patchedCount = 0;
		for (const auto& site : g_sites) {
			patchedCount += *reinterpret_cast<const std::uint8_t*>(site.imm) == kPatchByte;
		}
		g_patched = patchedCount == g_sites.size();
		if (patchedCount != 0) {
			REX::WARN("GodMode: {} of {} survival gates already patched at startup; toggles will now own every byte",
				patchedCount, g_sites.size());
		}

		ApplyPatchState();
	}

	void RefreshRuntimePatches()
	{
		ApplyPatchState();
	}
}
