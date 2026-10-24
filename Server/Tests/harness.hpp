/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// A minimal test framework for loading component binaries and driving them through the SDK, as
// the server would. Written against the SDK only, so the same tests can run against older (pure
// C++) builds of the components with `--baseline <dir>` to check a port didn't change behaviour.

#include <sdk.hpp>
#include <vector>

namespace harness
{

struct TestCase
{
	const char* name;
	void (*fn)();
};

std::vector<TestCase>& registry();

struct Register
{
	Register(const char* name, void (*fn)())
	{
		registry().push_back({ name, fn });
	}
};

/// Whether we are running against baseline binaries rather than this build's.
extern bool baseline;

void fail(const char* file, int line, const char* expr);

/// Load a component binary by name (e.g. "Timers") and call its entry point.
IComponent* loadComponent(const char* name);

}

#define TEST_CASE(name)                                       \
	static void name();                                       \
	static harness::Register name##_register(#name, &(name)); \
	static void name()

#define CHECK(cond)                                   \
	do                                                \
	{                                                 \
		if (!(cond))                                  \
		{                                             \
			harness::fail(__FILE__, __LINE__, #cond); \
		}                                             \
	} while (0)

/// Check behaviour a port deliberately changed, e.g. a fixed bug. Skipped with `--baseline`.
#define CHECK_PORTED(cond)      \
	do                          \
	{                           \
		if (!harness::baseline) \
		{                       \
			CHECK(cond);        \
		}                       \
	} while (0)
