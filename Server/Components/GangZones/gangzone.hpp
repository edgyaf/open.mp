/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "gangzones_rs.hpp"
#include <Impl/pool_impl.hpp>
#include <Server/Components/GangZones/gangzones.hpp>
#include <netcode.hpp>
#include <sdk.hpp>

using namespace Impl;

// Zone state and ID mapping live in Rust (see `rust/`). The "shown for" set stays here because the
// SDK hands it out by reference.

inline OmpGangZonesPos toRust(const GangZonePos& pos)
{
	return OmpGangZonesPos { { pos.min.x, pos.min.y }, { pos.max.x, pos.max.y } };
}

inline GangZonePos fromRust(const OmpGangZonesPos& pos)
{
	GangZonePos out;
	out.min = Vector2(pos.min.x, pos.min.y);
	out.max = Vector2(pos.max.x, pos.max.y);
	return out;
}

inline OmpGangZonesColour toRust(const Colour& colour)
{
	return OmpGangZonesColour { colour.r, colour.g, colour.b, colour.a };
}

inline Colour fromRust(const OmpGangZonesColour& colour)
{
	return Colour(colour.r, colour.g, colour.b, colour.a);
}

/// Owning handle to a Rust ID mapper (`FiniteLegacyIDMapper<GANG_ZONE_POOL_SIZE>`).
class GangZoneIDMapper final : public NoCopy
{
private:
	OmpGangZonesIDs* const ids_ = omp_gangzones_ids_new();

public:
	~GangZoneIDMapper()
	{
		omp_gangzones_ids_free(ids_);
	}

	int reserve() const
	{
		return omp_gangzones_ids_reserve(ids_);
	}

	void set(int legacy, int real)
	{
		omp_gangzones_ids_set(ids_, legacy, real);
	}

	void release(int legacy)
	{
		omp_gangzones_ids_release(ids_, legacy);
	}

	void releaseAll()
	{
		omp_gangzones_ids_release_all(ids_);
	}

	int toLegacy(int real) const
	{
		return omp_gangzones_ids_to_legacy(ids_, real);
	}

	int fromLegacy(int legacy) const
	{
		return omp_gangzones_ids_from_legacy(ids_, legacy);
	}
};

class GangZone final : public IGangZone, public PoolIDProvider, public NoCopy
{
private:
	OmpGangZonesZone* const state_;
	UniqueIDArray<IPlayer, PLAYER_POOL_SIZE> shownFor_;
	IPlayer* legacyPerPlayer_ = nullptr;

	void restream()
	{
		const Colour colour = fromRust(omp_gangzones_zone_get_colour(state_));
		for (IPlayer* player : shownFor_.entries())
		{
			hideForClient(*player);
			showForClient(*player, colour);
		}
	}

	void hideForClient(IPlayer& player)
	{
		auto data = queryExtension<IPlayerGangZoneData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id != INVALID_GANG_ZONE_ID)
			{
				data->releaseClientID(id);
				NetCode::RPC::HideGangZone hideGangZoneRPC;
				hideGangZoneRPC.ID = id;
				PacketHelper::send(hideGangZoneRPC, player);
			}
		}
	}

	void showForClient(IPlayer& player, const Colour& colour) const
	{
		auto data = queryExtension<IPlayerGangZoneData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id == INVALID_GANG_ZONE_ID)
			{
				id = data->reserveClientID();
			}
			if (id != INVALID_GANG_ZONE_ID)
			{
				const GangZonePos pos = getPosition();
				data->setClientID(id, poolID);
				NetCode::RPC::ShowGangZone showGangZoneRPC;
				showGangZoneRPC.ID = id;
				showGangZoneRPC.Min = pos.min;
				showGangZoneRPC.Max = pos.max;
				showGangZoneRPC.Col = colour;
				PacketHelper::send(showGangZoneRPC, player);
			}
		}
	}

public:
	void removeFor(int pid, IPlayer& player)
	{
		if (shownFor_.valid(pid))
		{
			shownFor_.remove(pid, player);
		}

		omp_gangzones_zone_remove_for(state_, pid);
	}

	GangZone(GangZonePos pos)
		: state_(omp_gangzones_zone_new(toRust(pos)))
	{
	}

	bool isShownForPlayer(const IPlayer& player) const override
	{
		return shownFor_.valid(player.getID());
	}

	bool isFlashingForPlayer(const IPlayer& player) const override
	{
		return omp_gangzones_zone_is_flashing(state_, player.getID());
	}

	void showForPlayer(IPlayer& player, const Colour& colour) override
	{
		const int playerId = player.getID();
		shownFor_.add(playerId, player);
		omp_gangzones_zone_show(state_, playerId, toRust(colour));
		showForClient(player, colour);
	}

	void hideForPlayer(IPlayer& player) override
	{
		removeFor(player.getID(), player);
		hideForClient(player);
	}

	void flashForPlayer(IPlayer& player, const Colour& colour) override
	{
		auto data = queryExtension<IPlayerGangZoneData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id != INVALID_GANG_ZONE_ID)
			{
				NetCode::RPC::FlashGangZone flashGangZoneRPC;
				flashGangZoneRPC.ID = id;
				flashGangZoneRPC.Col = colour;
				PacketHelper::send(flashGangZoneRPC, player);
			}
			omp_gangzones_zone_flash(state_, player.getID(), toRust(colour));
		}
	}

	void stopFlashForPlayer(IPlayer& player) override
	{
		auto data = queryExtension<IPlayerGangZoneData>(player);
		if (data)
		{
			int id = data->toClientID(poolID);
			if (id != INVALID_GANG_ZONE_ID)
			{
				NetCode::RPC::StopFlashGangZone stopFlashGangZoneRPC;
				stopFlashGangZoneRPC.ID = id;
				PacketHelper::send(stopFlashGangZoneRPC, player);
			}
			omp_gangzones_zone_stop_flash(state_, player.getID());
		}
	}

	const Colour getFlashingColourForPlayer(IPlayer& player) const override
	{
		return fromRust(omp_gangzones_zone_flash_colour_for(state_, player.getID()));
	}

	const Colour getColourForPlayer(IPlayer& player) const override
	{
		return fromRust(omp_gangzones_zone_colour_for(state_, player.getID()));
	}

	const FlatHashSet<IPlayer*>& getShownFor() override
	{
		return shownFor_.entries();
	}

	bool isPlayerInside(const IPlayer& player) const override
	{
		return omp_gangzones_zone_is_inside(state_, player.getID());
	}

	void setPlayerInside(const IPlayer& player, const bool status)
	{
		omp_gangzones_zone_set_inside(state_, player.getID(), status);
	}

	/// Whether the player just entered or left the zone, given their position. Doesn't record it.
	OmpGangZonesTransition check(const IPlayer& player, const Vector3& position) const
	{
		return omp_gangzones_zone_check(state_, player.getID(), position.x, position.y);
	}

	bool contains(const Vector3& position) const
	{
		return omp_gangzones_contains(omp_gangzones_zone_get_position(state_), position.x, position.y);
	}

	int getID() const override
	{
		return poolID;
	}

	GangZonePos getPosition() const override
	{
		return fromRust(omp_gangzones_zone_get_position(state_));
	}

	void setPosition(const GangZonePos& position) override
	{
		omp_gangzones_zone_set_position(state_, toRust(position));
		restream();
	}

	~GangZone()
	{
		omp_gangzones_zone_free(state_);
	}

	void destream()
	{
		for (IPlayer* player : shownFor_.entries())
		{
			hideForClient(*player);
		}
	}

	virtual void setLegacyPlayer(IPlayer* player) override
	{
		legacyPerPlayer_ = player;
	}

	virtual IPlayer* getLegacyPlayer() const override
	{
		return legacyPerPlayer_;
	}
};
