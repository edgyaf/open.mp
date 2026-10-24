/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/Variables/variables.hpp>
#include <cstring>

namespace
{

void checkStorage(IVariableStorageBase& vars)
{
	vars.setInt("Score", 42);
	CHECK(vars.getInt("SCORE") == 42);
	CHECK(vars.getType("score") == VariableType_Int);
	CHECK(vars.getFloat("score") == 0.0f);
	CHECK(vars.getString("score").empty());

	vars.setFloat("pi", 3.5f);
	CHECK(vars.getFloat("PI") == 3.5f);

	vars.setString("name", "open.mp");
	CHECK(vars.getString("NAME") == StringView("open.mp"));
	CHECK(vars.getString("missing").empty());
	CHECK(vars.getType("missing") == VariableType_None);

	// Setting replaces the type.
	vars.setString("pi", "3.14");
	CHECK(vars.getType("PI") == VariableType_String);
	CHECK(vars.getFloat("pi") == 0.0f);

	CHECK(vars.size() == 3);
	StringView key;
	CHECK(vars.getKeyAtIndex(0, key));
	CHECK(!vars.getKeyAtIndex(3, key));
	// Keys are stored uppercased, in insertion order (C++: unspecified hash order).
	CHECK_PORTED(vars.getKeyAtIndex(0, key) && key == StringView("SCORE"));
	CHECK_PORTED(vars.getKeyAtIndex(2, key) && key == StringView("NAME"));
	// Undefined behaviour in C++.
	CHECK_PORTED(!vars.getKeyAtIndex(-1, key));

	CHECK(vars.erase("score"));
	CHECK(!vars.erase("score"));
	CHECK(vars.size() == 2);

	// Keys and values are bytes: embedded NULs and non-ASCII survive, case folding is ASCII only.
	const char raw[] = { 'a', '\0', '\xe9' };
	vars.setString(StringView(raw, 3), StringView(raw, 3));
	StringView got = vars.getString(StringView("A\0\xe9", 3));
	CHECK(got.length() == 3 && std::memcmp(got.data(), raw, 3) == 0);
	CHECK(vars.getString(StringView("A\0\xc9", 3)).empty());
}

}

TEST_CASE(variables_server)
{
	ComponentFixture<IVariablesComponent> f("Variables");
	checkStorage(*f.api);
	// Server variables persist over GMX.
	const int size = f.api->size();
	CHECK(size == 3);
	f.component->reset();
	CHECK(f.api->size() == size);
}

TEST_CASE(variables_player)
{
	ComponentFixture<IVariablesComponent> f("Variables");
	MockPlayer player(4, f.network);
	f.core.players.connect(player);
	IPlayerVariableData* data = queryExtension<IPlayerVariableData>(player);
	CHECK(data != nullptr);
	if (data)
	{
		checkStorage(*data);
		// Player variables don't.
		data->reset();
		CHECK(data->size() == 0);
		// And are separate from server variables.
		data->setInt("x", 1);
		CHECK(f.api->getType("x") == VariableType_None);
	}
	f.core.players.disconnect(player);
}
