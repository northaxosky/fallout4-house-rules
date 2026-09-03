#include "PCH.h"

#include "Hooks/ExitSave.h"

#include "Settings.h"

#include <cstring>
#include <string_view>

namespace Hooks::ExitSave
{
	namespace
	{
		using DeleteFileA_t = REX::W32::BOOL(__stdcall*)(const char*);
		DeleteFileA_t g_original = nullptr;

		std::string_view Basename(const char* a_path)
		{
			std::string_view sv{ a_path ? a_path : "" };
			const auto sep = sv.find_last_of("\\/");
			return sep == std::string_view::npos ? sv : sv.substr(sep + 1);
		}

		bool IsExitSavePath(const char* a_path)
		{
			if (!a_path || !*a_path) {
				return false;
			}
			// Vanilla exit save filenames always start with "Exitsave".
			constexpr std::string_view kPrefix{ "Exitsave" };
			const std::string_view bn = Basename(a_path);
			if (bn.size() < kPrefix.size()) {
				return false;
			}
			return _strnicmp(bn.data(), kPrefix.data(), kPrefix.size()) == 0;
		}

		REX::W32::BOOL __stdcall Hook_DeleteFileA(const char* a_filename)
		{
			if (MCM::Settings::Unlocks::bKeepExitSave.GetValue() && IsExitSavePath(a_filename)) {
				REX::INFO("ExitSave: preserved '{}'", a_filename ? a_filename : "(null)");
				return 1;
			}
			return g_original(a_filename);
		}
	}

	void Install()
	{
		const auto module = REX::FModule::GetExecutingModule();
		const auto entry = module.GetImportFunctionPointer("DeleteFileA", "KERNEL32.dll");
		if (!entry) {
			REX::WARN("ExitSave: failed to patch KERNEL32!DeleteFileA IAT entry");
			return;
		}
		g_original = *reinterpret_cast<DeleteFileA_t*>(entry);
		module.SetImportFunctionPointer("DeleteFileA", "KERNEL32.dll", REX::UNRESTRICTED_CAST<void*>(&Hook_DeleteFileA));
		REX::INFO("ExitSave: hooked KERNEL32!DeleteFileA");
	}
}
