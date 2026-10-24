/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "pickups_rs.hpp"
#include <Impl/pool_impl.hpp>
#include <Server/Components/Pickups/pickups.hpp>
#include <netcode.hpp>
#include <sdk.hpp>

using namespace Impl;

// Pickup properties, hidden state, streaming decisions and ID mapping live in Rust (see `rust/`).
// The "streamed for" set stays here because restreaming needs the player objects.

inline OmpPickupsVec3 toRust(const Vector3& v)
{
	return OmpPickupsVec3 { v.x, v.y, v.z };
}

inline Vector3 fromRust(const OmpPickupsVec3& v)
{
	return Vector3(v.x, v.y, v.z);
}

/// Owning handle to a Rust ID mapper (`FiniteLegacyIDMapper<PICKUP_POOL_SIZE>`).
class PickupIDMapper final : public NoCopy
{
private:
	OmpPickupsIDs* const ids_ = omp_pickups_ids_new();

public:
	~PickupIDMapper()
	{
		omp_pickups_ids_free(ids_);
	}

	int reserve() const
	{
		return omp_pickups_ids_reserve(ids_);
	}

	void set(int legacy, int real)
	{
		omp_pickups_ids_set(ids_, legacy, real);
	}

	void release(int legacy)
	{
		omp_pickups_ids_release(ids_, legacy);
	}

	void releaseAll()
	{
		omp_pickups_ids_release_all(ids_);
	}

	int toLegacy(int real) const
	{
		return omp_pickups_ids_to_legacy(ids_, real);
	}

	int fromLegacy(int legacy) const
	{
		return omp_pickups_ids_from_legacy(ids_, legacy);
	}
};

class Pickup final : public IPickup, public PoolIDProvider, public NoCopy
{
private:
	OmpPickupsPickup* const state_;
	UniqueIDArray<IPlayer, PLAYER_POOL_SIZE> streamedFor_;
	IPlayer* legacyPerPlayer_ = nullptr;

	void restream()
	{
		for (IPlayer* player : streamedFor_.entries())
		{
			streamOutForClient(*player);
			streamInForClient(*player);
		}
	}

	void streamInForClient(IPlayer& player)
	{
		auto data = queryExtension<IPlayerPickupData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id == INVALID_PICKUP_ID)
			{
				id = data->reserveClientID();
			}
			if (id != INVALID_PICKUP_ID)
			{
				data->setClientID(id, poolID);
				NetCode::RPC::PlayerCreatePickup createPickupRPC;
				createPickupRPC.PickupID = id;
				createPickupRPC.Model = getModel();
				createPickupRPC.Type = getType();
				createPickupRPC.Position = getPosition();
				PacketHelper::send(createPickupRPC, player);
			}
		}
	}

	void streamOutForClient(IPlayer& player)
	{
		auto data = queryExtension<IPlayerPickupData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id != INVALID_PICKUP_ID)
			{
				data->releaseClientID(id);
				NetCode::RPC::PlayerDestroyPickup destroyPickupRPC;
				destroyPickupRPC.PickupID = id;
				PacketHelper::send(destroyPickupRPC, player);
			}
		}
	}

public:
	void removeFor(int pid, IPlayer& player)
	{
		if (streamedFor_.valid(pid))
		{
			streamedFor_.remove(pid, player);
		}
	}

	inline bool isStatic() const
	{
		return omp_pickups_pickup_is_static(state_);
	}

	Pickup(int modelId, PickupType type, Vector3 pos, uint32_t virtualWorld, bool isStatic)
		: state_(omp_pickups_pickup_new(modelId, type, toRust(pos), static_cast<int>(virtualWorld), isStatic))
	{
	}

	/// Whether the pickup should be streamed in for the player, given their current state.
	bool shouldStreamIn(const IPlayer& player, Vector3 playerPos, float maxDistanceSqr) const
	{
		return omp_pickups_pickup_should_stream_in(state_, player.getID(), player.getVirtualWorld(), toRust(playerPos), maxDistanceSqr);
	}

	bool isStreamedInForPlayer(const IPlayer& player) const override
	{
		return streamedFor_.valid(player.getID());
	}

	void streamInForPlayer(IPlayer& player) override
	{
		streamedFor_.add(player.getID(), player);
		streamInForClient(player);
	}

	void streamOutForPlayer(IPlayer& player) override
	{
		streamedFor_.remove(player.getID(), player);
		streamOutForClient(player);
	}

	bool isPickupHiddenForPlayer(IPlayer& player) const override
	{
		return omp_pickups_pickup_is_hidden_for(state_, player.getID());
	}

	void setPickupHiddenForPlayer(IPlayer& player, bool hidden) override
	{
		omp_pickups_pickup_set_hidden_for(state_, player.getID(), hidden);
	}

	int getVirtualWorld() const override
	{
		return omp_pickups_pickup_get_virtual_world(state_);
	}

	void setVirtualWorld(int vw) override
	{
		omp_pickups_pickup_set_virtual_world(state_, vw);
		restream();
	}

	int getID() const override
	{
		return poolID;
	}

	Vector3 getPosition() const override
	{
		return fromRust(omp_pickups_pickup_get_position(state_));
	}

	void setPositionNoUpdate(Vector3 position) override
	{
		omp_pickups_pickup_set_position(state_, toRust(position));
	}

	void setPosition(Vector3 position) override
	{
		omp_pickups_pickup_set_position(state_, toRust(position));
		restream();
	}

	GTAQuat getRotation() const override { return GTAQuat(); }

	void setRotation(GTAQuat rotation) override { }

	void setType(PickupType t, bool update) override
	{
		omp_pickups_pickup_set_type(state_, t);
		if (update)
		{
			restream();
		}
	}

	PickupType getType() const override
	{
		return omp_pickups_pickup_get_type(state_);
	}

	void setModel(int id, bool update) override
	{
		omp_pickups_pickup_set_model(state_, id);
		if (update)
		{
			restream();
		}
	}

	int getModel() const override
	{
		return omp_pickups_pickup_get_model(state_);
	}

	~Pickup()
	{
		omp_pickups_pickup_free(state_);
	}

	void destream()
	{
		for (IPlayer* player : streamedFor_.entries())
		{
			streamOutForClient(*player);
		}
	}

	virtual void setLegacyPlayer(IPlayer* player) override
	{
		legacyPerPlayer_ = player;
		omp_pickups_pickup_set_legacy_owner(state_, player ? player->getID() : -1);
	}

	virtual IPlayer* getLegacyPlayer() const override
	{
		return legacyPerPlayer_;
	}
};
