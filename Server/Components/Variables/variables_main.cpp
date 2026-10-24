/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "variables_rs.hpp"
#include <Server/Components/Variables/variables.hpp>
#include <sdk.hpp>

// The variable storage lives in Rust (see `rust/`); this only adapts it to the SDK interfaces.

template <class ToInherit>
class VariableStorageBase : public ToInherit
{
private:
	OmpVariablesStorage* const data_ = omp_variables_storage_new();

public:
	~VariableStorageBase()
	{
		omp_variables_storage_free(data_);
	}

	void setString(StringView key, StringView value) override
	{
		omp_variables_set_string(data_, key.data(), key.length(), value.data(), value.length());
	}

	const StringView getString(StringView key) const override
	{
		const char* ptr;
		size_t len;
		if (!omp_variables_get_string(data_, key.data(), key.length(), &ptr, &len))
		{
			return StringView();
		}
		return StringView(ptr, len);
	}

	void setInt(StringView key, int value) override
	{
		omp_variables_set_int(data_, key.data(), key.length(), value);
	}

	int getInt(StringView key) const override
	{
		return omp_variables_get_int(data_, key.data(), key.length());
	}

	void setFloat(StringView key, float value) override
	{
		omp_variables_set_float(data_, key.data(), key.length(), value);
	}

	float getFloat(StringView key) const override
	{
		return omp_variables_get_float(data_, key.data(), key.length());
	}

	VariableType getType(StringView key) const override
	{
		return VariableType(omp_variables_get_type(data_, key.data(), key.length()));
	}

	bool erase(StringView key) override
	{
		return omp_variables_erase(data_, key.data(), key.length());
	}

	bool getKeyAtIndex(int index, StringView& key) const override
	{
		const char* ptr;
		size_t len;
		if (!omp_variables_key_at(data_, index, &ptr, &len))
		{
			return false;
		}
		key = StringView(ptr, len);
		return true;
	}

	int size() const override
	{
		return omp_variables_size(data_);
	}

	void clear()
	{
		omp_variables_clear(data_);
	}
};

class PlayerVariableData final : public VariableStorageBase<IPlayerVariableData>
{
public:
	void freeExtension() override
	{
		delete this;
	}

	void reset() override
	{
		clear();
	}
};

class VariablesComponent final : public VariableStorageBase<IVariablesComponent>, public PlayerConnectEventHandler
{
private:
	ICore* core = nullptr;

public:
	void onPlayerConnect(IPlayer& player) override
	{
		player.addExtension(new PlayerVariableData(), true);
	}

	StringView componentName() const override
	{
		return "Variables";
	}

	SemanticVersion componentVersion() const override
	{
		return SemanticVersion(OMP_VERSION_MAJOR, OMP_VERSION_MINOR, OMP_VERSION_PATCH, BUILD_NUMBER);
	}

	void onLoad(ICore* core) override
	{
		this->core = core;
		core->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
	}

	void free() override
	{
		delete this;
	}

	~VariablesComponent()
	{
		if (core)
		{
			core->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
		}
	}

	void reset() override
	{
		// Nothing to reset here.  SVars persist.
	}
};

COMPONENT_ENTRY_POINT()
{
	return new VariablesComponent();
}
