#include "PCH.h"

#include "Survival/HCManagerScript.h"

#include <cmath>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace Survival::HCManagerScript
{
	struct Batch::Impl
	{
		RE::BSTSmartPointer<RE::BSScript::Object> obj;
	};

	namespace
	{
		constexpr std::uint32_t      kManagerQuestFormID = 0x0000080Eu;
		constexpr std::string_view   kScriptName{ "Hardcore:HC_ManagerScript" };

		bool g_warnedMissing = false;

		RE::TESQuest* FindHCManagerQuest()
		{
			// Linear-scan; GetFormByID can trip BSStaticTriShapeDB::Force during save-load on OG.
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				return nullptr;
			}
			for (auto* quest : dh->GetFormArray<RE::TESQuest>()) {
				if (quest && quest->formID == kManagerQuestFormID) {
					return quest;
				}
			}
			return nullptr;
		}

		bool TryAcquire(RE::BSTSmartPointer<RE::BSScript::Object>& a_out)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* quest = FindHCManagerQuest();
			if (!vm || !quest) {
				return false;
			}

			auto& policy = vm->GetObjectHandlePolicy();
			const auto handle = policy.GetHandleForObject(
				RE::BSScript::GetVMTypeID<RE::TESQuest>(), quest);
			if (handle == policy.EmptyHandle()) {
				return false;
			}

			if (!vm->FindBoundObject(handle, kScriptName.data(), true, a_out, true) || !a_out) {
				if (!g_warnedMissing) {
					REX::WARN("HCManagerScript: '{}' not bound on quest 0x{:08X}; Survival may be inactive",
						kScriptName, kManagerQuestFormID);
					g_warnedMissing = true;
				}
				return false;
			}
			g_warnedMissing = false;
			return true;
		}

		template <class T>
		RE::BSScript::Variable* FindTyped(RE::BSScript::Object& a_obj, const char* a_property)
		{
			auto* var = a_obj.GetProperty(RE::BSFixedString{ a_property });
			if (!var) {
				REX::WARN("HCManagerScript: property '{}' not found on bound object", a_property);
				return nullptr;
			}
			if (!var->is<T>()) {
				REX::WARN("HCManagerScript: '{}' is not {}; skipping", a_property,
					std::is_same_v<T, float> ? "float" : "int");
				return nullptr;
			}
			return var;
		}
	}

	Batch::Batch() :
		m_impl(std::make_unique<Impl>())
	{
		if (!TryAcquire(m_impl->obj)) {
			m_impl.reset();
		}
	}

	Batch::~Batch() = default;

	Batch::operator bool() const noexcept
	{
		return m_impl != nullptr;
	}

	std::optional<float> Batch::GetFloat(const char* a_property) const
	{
		const auto* var = m_impl ? FindTyped<float>(*m_impl->obj, a_property) : nullptr;
		return var ? std::optional{ RE::BSScript::get<float>(*var) } : std::nullopt;
	}

	bool Batch::SetFloat(const char* a_property, float a_value)
	{
		auto* var = m_impl ? FindTyped<float>(*m_impl->obj, a_property) : nullptr;
		if (!var) {
			return false;
		}
		if (std::abs(RE::BSScript::get<float>(*var) - a_value) > 1e-4f) {
			*var = a_value;
			REX::INFO("HCManagerScript: '{}' = {} (float)", a_property, a_value);
		}
		return true;
	}

	bool Batch::SetInt(const char* a_property, std::int32_t a_value)
	{
		auto* var = m_impl ? FindTyped<std::int32_t>(*m_impl->obj, a_property) : nullptr;
		if (!var) {
			return false;
		}
		if (RE::BSScript::get<std::int32_t>(*var) != a_value) {
			*var = a_value;
			REX::INFO("HCManagerScript: '{}' = {} (int)", a_property, a_value);
		}
		return true;
	}

	void Reset()
	{
		g_warnedMissing = false;
	}
}
