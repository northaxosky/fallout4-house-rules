#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace Survival::HCManagerScript
{
	class Batch
	{
	public:
		Batch();
		~Batch();

		Batch(const Batch&)            = delete;
		Batch& operator=(const Batch&) = delete;
		Batch(Batch&&)                 = delete;
		Batch& operator=(Batch&&)      = delete;

		explicit operator bool() const noexcept;

		[[nodiscard]] std::optional<float> GetFloat(const char* a_property) const;

		bool SetFloat(const char* a_property, float a_value);
		bool SetInt  (const char* a_property, std::int32_t a_value);

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	void Reset();
}
