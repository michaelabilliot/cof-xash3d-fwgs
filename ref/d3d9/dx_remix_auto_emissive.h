#pragma once

#include <cstdint>

namespace dx_remix_auto_emissive {
inline constexpr char defaultValue[] = "0";

class Config {
	std::uintptr_t module = 0;
	std::uint64_t retryAt = 0;
	unsigned submissions = 0;
	bool enabled = false;

public:
	void invalidate() { *this = {}; }

	template <typename Sink>
	void update(std::uintptr_t runtime, float value, std::uint64_t now, Sink &&setConfig)
	{
		const bool requested = value != 0.0f;
		if (runtime != module || requested != enabled)
		{
			invalidate();
			module = runtime;
			enabled = requested;
		}

		// Repeat successful writes across startup while Remix loads its configuration.
		if (!module || submissions == 3 || now < retryAt)
			return;
		retryAt = now + 1000;
		if (setConfig("rtx.enableEmissiveBlendEmissiveOverride", enabled ? "True" : "False"))
			++submissions;
	}
};
}
