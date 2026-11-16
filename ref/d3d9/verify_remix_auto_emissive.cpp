#include "dx_remix_auto_emissive.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

static void check(bool pass, const char *description)
{
	if (!pass)
	{
		std::fprintf(stderr, "%s\n", description);
		std::exit(1);
	}
}

int main()
{
	using namespace dx_remix_auto_emissive;
	Config config;
	bool ready = false;
	std::vector<std::pair<std::string, std::string>> calls;
	const auto sink = [&](const char *key, const char *value) {
		calls.emplace_back(key, value);
		return ready;
	};
	const auto defaultCvar = static_cast<float>(std::atof(defaultValue));

	config.update(0, defaultCvar, 0, sink);
	check(calls.empty(), "Non-Remix frames must not submit configuration");
	config.update(1, defaultCvar, 0, sink);
	check(calls.size() == 1 && calls.back() == std::make_pair(
		std::string("rtx.enableEmissiveBlendEmissiveOverride"), std::string("False")),
		"Default must disable only the automatic blend override");
	config.update(1, defaultCvar, 999, sink);
	check(calls.size() == 1, "Failed writes must be throttled");
	config.update(1, defaultCvar, 1000, sink);
	check(calls.size() == 2, "An unavailable bridge must be retried");

	ready = true;
	for (std::uint64_t now : {2000u, 3000u, 4000u, 5000u})
		config.update(1, defaultCvar, now, sink);
	check(calls.size() == 5 && calls.back().second == "False",
		"Startup must repeat three successful writes and then settle");

	config.update(1, 1.0f, 5001, sink);
	check(calls.size() == 6 && calls.back().second == "True", "Toggle on must apply immediately");
	config.update(1, 0.0f, 5002, sink);
	check(calls.size() == 7 && calls.back().second == "False", "Toggle off must apply immediately");
	config.update(2, 0.0f, 5003, sink);
	check(calls.size() == 8, "Runtime module changes must replay the policy");
	config.invalidate();
	config.update(2, 0.0f, 5004, sink);
	check(calls.size() == 9, "Device reset must replay the policy on the same module");
	config.update(0, 0.0f, 5005, sink);
	check(calls.size() == 9, "Disabling Remix must suspend submissions");
	config.update(2, 0.0f, 5006, sink);
	check(calls.size() == 10 && calls.back().second == "False",
		"Re-enabling Remix must replay the policy");
	std::puts("Remix automatic emissive configuration checks passed");
}
