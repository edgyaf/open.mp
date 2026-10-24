/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// Just enough of the server for components to load and run: a core with a player pool and event
// dispatchers, players, and a network that records the RPCs sent. Methods not implemented here are
// stubs generated from the SDK headers (see genmock.py).

#include "harness.hpp"
#include <Impl/events_impl.hpp>
#include <cstdarg>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <type_traits>

using namespace Impl;

/// Default value for a stubbed method. Reference returns point at zeroed storage, so tests must not
/// depend on them.
template <class T>
T mockReturn()
{
	if constexpr (std::is_void_v<T>)
	{
		return;
	}
	else if constexpr (std::is_default_constructible_v<T>)
	{
		return T {};
	}
	else
	{
		alignas(16) static char storage[1 << 16] = {};
		return *reinterpret_cast<std::remove_reference_t<T>*>(storage);
	}
}

#include "mocks_gen.hpp"

struct MockNetwork final : public INetwork
{
	/// IDs of the RPCs sent, in order.
	std::vector<int> sent;

	ENetworkType getNetworkType() const override
	{
		return ENetworkType(0);
	}

	bool sendPacket(IPlayer& peer, Span<uint8_t> data, int channel, bool dispatchEvents) override
	{
		return true;
	}

	bool sendRPC(IPlayer& peer, int id, Span<uint8_t> data, int channel, bool dispatchEvents) override
	{
		sent.push_back(id);
		return true;
	}

	/// Whether exactly these RPCs were sent since the last call, which clears the record.
	bool sentOnly(std::initializer_list<int> ids)
	{
		const bool same = sent == std::vector<int>(ids);
		sent.clear();
		return same;
	}

	MOCK_STUBS_INetwork
};

struct MockPlayer final : public IPlayer
{
	int id;
	Vector3 pos = Vector3(0.0f);
	PeerNetworkData netData {};

	MockPlayer(int id, MockNetwork& network)
		: id(id)
	{
		netData.network = &network;
	}

	int getID() const override
	{
		return id;
	}

	Vector3 getPosition() const override
	{
		return pos;
	}

	void setPosition(Vector3 position) override
	{
		pos = position;
	}

	const PeerNetworkData& getNetworkData() const override
	{
		return netData;
	}

	StringView getName() const override
	{
		return "MockPlayer";
	}

	MOCK_STUBS_IEntity
		MOCK_STUBS_IPlayer
};

struct MockPlayerPool final : public IPlayerPool
{
	FlatPtrHashSet<IPlayer> entries_;
	FlatPtrHashSet<IPlayer> bots_;
	DefaultEventDispatcher<PlayerSpawnEventHandler> spawnDispatcher;
	DefaultEventDispatcher<PlayerConnectEventHandler> connectDispatcher;
	DefaultEventDispatcher<PlayerStreamEventHandler> streamDispatcher;
	DefaultEventDispatcher<PlayerTextEventHandler> textDispatcher;
	DefaultEventDispatcher<PlayerShotEventHandler> shotDispatcher;
	DefaultEventDispatcher<PlayerChangeEventHandler> changeDispatcher;
	DefaultEventDispatcher<PlayerDamageEventHandler> damageDispatcher;
	DefaultEventDispatcher<PlayerClickEventHandler> clickDispatcher;
	DefaultEventDispatcher<PlayerCheckEventHandler> checkDispatcher;
	DefaultEventDispatcher<PlayerUpdateEventHandler> updateDispatcher;
	DefaultEventDispatcher<PoolEventHandler<IPlayer>> poolDispatcher;

	void connect(IPlayer& player)
	{
		entries_.insert(&player);
		poolDispatcher.dispatch(&PoolEventHandler<IPlayer>::onPoolEntryCreated, player);
		connectDispatcher.dispatch(&PlayerConnectEventHandler::onPlayerConnect, player);
	}

	void disconnect(IPlayer& player)
	{
		connectDispatcher.dispatch(&PlayerConnectEventHandler::onPlayerDisconnect, player, PeerDisconnectReason_Quit);
		poolDispatcher.dispatch(&PoolEventHandler<IPlayer>::onPoolEntryDestroyed, player);
		entries_.erase(&player);
	}

	void update(IPlayer& player)
	{
		updateDispatcher.stopAtFalse([&player](PlayerUpdateEventHandler* handler)
			{
				return handler->onPlayerUpdate(player, TimePoint());
			});
	}

	IPlayer* get(int index) override
	{
		for (IPlayer* player : entries_)
		{
			if (player->getID() == index)
			{
				return player;
			}
		}
		return nullptr;
	}

	Pair<size_t, size_t> bounds() const override
	{
		return std::make_pair(0, PLAYER_POOL_SIZE);
	}

	const FlatPtrHashSet<IPlayer>& entries() override
	{
		return entries_;
	}

	const FlatPtrHashSet<IPlayer>& players() override
	{
		return entries_;
	}

	const FlatPtrHashSet<IPlayer>& bots() override
	{
		return bots_;
	}

	IEventDispatcher<PlayerSpawnEventHandler>& getPlayerSpawnDispatcher() override
	{
		return spawnDispatcher;
	}

	IEventDispatcher<PlayerConnectEventHandler>& getPlayerConnectDispatcher() override
	{
		return connectDispatcher;
	}

	IEventDispatcher<PlayerStreamEventHandler>& getPlayerStreamDispatcher() override
	{
		return streamDispatcher;
	}

	IEventDispatcher<PlayerTextEventHandler>& getPlayerTextDispatcher() override
	{
		return textDispatcher;
	}

	IEventDispatcher<PlayerShotEventHandler>& getPlayerShotDispatcher() override
	{
		return shotDispatcher;
	}

	IEventDispatcher<PlayerChangeEventHandler>& getPlayerChangeDispatcher() override
	{
		return changeDispatcher;
	}

	IEventDispatcher<PlayerDamageEventHandler>& getPlayerDamageDispatcher() override
	{
		return damageDispatcher;
	}

	IEventDispatcher<PlayerClickEventHandler>& getPlayerClickDispatcher() override
	{
		return clickDispatcher;
	}

	IEventDispatcher<PlayerCheckEventHandler>& getPlayerCheckDispatcher() override
	{
		return checkDispatcher;
	}

	IEventDispatcher<PlayerUpdateEventHandler>& getPlayerUpdateDispatcher() override
	{
		return updateDispatcher;
	}

	IEventDispatcher<PoolEventHandler<IPlayer>>& getPoolEventDispatcher() override
	{
		return poolDispatcher;
	}

	MOCK_STUBS_IPlayerPool
};

struct MockCore final : public ICore
{
	MockPlayerPool players;
	DefaultEventDispatcher<CoreEventHandler> coreDispatcher;
	/// Messages logged at warning level or above.
	std::vector<std::string> warnings;

	void tick()
	{
		coreDispatcher.dispatch(&CoreEventHandler::onTick, Microseconds(0), TimePoint());
	}

	IPlayerPool& getPlayers() override
	{
		return players;
	}

	IEventDispatcher<CoreEventHandler>& getEventDispatcher() override
	{
		return coreDispatcher;
	}

	void printLn(const char* fmt, ...) override { }
	void vprintLn(const char* fmt, va_list args) override { }
	void printLnU8(const char* fmt, ...) override { }
	void vprintLnU8(const char* fmt, va_list args) override { }

	void logLn(LogLevel level, const char* fmt, ...) override
	{
		va_list args;
		va_start(args, fmt);
		vlogLn(level, fmt, args);
		va_end(args);
	}

	void vlogLn(LogLevel level, const char* fmt, va_list args) override
	{
		if (level >= LogLevel::Warning)
		{
			char buffer[512];
			std::vsnprintf(buffer, sizeof(buffer), fmt, args);
			warnings.emplace_back(buffer);
		}
	}

	MOCK_STUBS_ILogger
		MOCK_STUBS_ICore
};

struct MockComponentList final : public IComponentList
{
	IComponent* queryComponent(UID id) override
	{
		return nullptr;
	}
};

/// Loads a component and takes it through the server's load sequence; frees it on destruction.
/// Declared after the mocks it uses, so they outlive the component.
template <class ComponentT>
struct ComponentFixture
{
	MockNetwork network;
	MockCore core;
	MockComponentList components;
	IComponent* component;
	ComponentT* api;

	explicit ComponentFixture(const char* name)
		: component(harness::loadComponent(name))
		, api(static_cast<ComponentT*>(component))
	{
		component->onLoad(&core);
		component->onInit(&components);
		component->onReady();
	}

	~ComponentFixture()
	{
		component->onFree(component);
		component->free();
	}

	ComponentFixture(const ComponentFixture&) = delete;
	ComponentFixture& operator=(const ComponentFixture&) = delete;
};
