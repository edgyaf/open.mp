/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "harness.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace harness
{

bool baseline = false;
static std::string componentsDir = COMPONENTS_DIR;
static int failures = 0;

std::vector<TestCase>& registry()
{
	static std::vector<TestCase> tests;
	return tests;
}

void fail(const char* file, int line, const char* expr)
{
	std::printf("  FAIL %s:%d: %s\n", file, line, expr);
	++failures;
}

IComponent* loadComponent(const char* name)
{
	typedef IComponent*(__CDECL * EntryPoint)();

	const std::string path = componentsDir + "/" + name + COMPONENT_SUFFIX;
#ifdef _WIN32
	HMODULE lib = LoadLibraryA(path.c_str());
	EntryPoint entry = lib ? reinterpret_cast<EntryPoint>(GetProcAddress(lib, "ComponentEntryPoint")) : nullptr;
#else
	void* lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	EntryPoint entry = lib ? reinterpret_cast<EntryPoint>(dlsym(lib, "ComponentEntryPoint")) : nullptr;
#endif
	if (!entry)
	{
		std::printf("could not load %s\n", path.c_str());
		std::exit(2);
	}
	// Libraries stay loaded until exit, as in the server.
	return entry();
}

}

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	const char* filter = nullptr;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--baseline") == 0 && i + 1 < argc)
		{
			harness::baseline = true;
			harness::componentsDir = argv[++i];
		}
		else if (argv[i][0] != '-')
		{
			filter = argv[i];
		}
		else
		{
			std::printf("usage: %s [--baseline <components dir>] [test name filter]\n", argv[0]);
			return 2;
		}
	}

	int ran = 0;
	for (const harness::TestCase& test : harness::registry())
	{
		if (filter && !std::strstr(test.name, filter))
		{
			continue;
		}
		const int before = harness::failures;
		test.fn();
		std::printf("%s %s\n", harness::failures == before ? "ok  " : "FAIL", test.name);
		++ran;
	}

	std::printf("%d tests, %d failed checks%s\n", ran, harness::failures, harness::baseline ? " (baseline)" : "");
	return harness::failures == 0 && ran > 0 ? 0 : 1;
}
