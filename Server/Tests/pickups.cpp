/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/Pickups/pickups.hpp>
#include <netcode.hpp>

using namespace NetCode::RPC;

namespace
{

struct Events final : public PickupEventHandler
{
	int pickedUp = 0;
	IPickup* last = nullptr;

	void onPlayerPickUpPickup(IPlayer& player, IPickup& pickup) override
	{
		++pickedUp;
		last = &pickup;
	}
};

bool pickUp(MockNetwork& network, IPlayer& player, int clientID)
{
	NetworkBitStream bs;
	bs.writeINT32(clientID);
	return network.receive<OnPlayerPickUpPickup>(player, bs);
}

}

TEST_CASE(pickups_pool_and_properties)
{
	ComponentFixture<IPickupsComponent> f("Pickups");
	CHECK(f.component->getUID() == PickupsComponent_UID);

	IPickup* pickup = f.api->create(1240, 2, Vector3(1.0f, 2.0f, 3.0f), 5, false);
	CHECK(pickup != nullptr);
	CHECK(f.api->get(pickup->getID()) == pickup);
	CHECK(f.api->count() == 1);
	CHECK(pickup->getModel() == 1240);
	CHECK(pickup->getType() == 2);
	CHECK(pickup->getPosition() == Vector3(1.0f, 2.0f, 3.0f));
	CHECK(pickup->getVirtualWorld() == 5);

	pickup->setModel(1241, false);
	pickup->setType(3, false);
	pickup->setPositionNoUpdate(Vector3(4.0f, 5.0f, 6.0f));
	CHECK(pickup->getModel() == 1241);
	CHECK(pickup->getType() == 3);
	CHECK(pickup->getPosition() == Vector3(4.0f, 5.0f, 6.0f));

	CHECK(f.api->toLegacyID(pickup->getID()) == -1);
	CHECK(f.api->reserveLegacyID() == 0);
	f.api->setLegacyID(0, pickup->getID());
	CHECK(f.api->toLegacyID(pickup->getID()) == 0);
	CHECK(f.api->fromLegacyID(0) == pickup->getID());
	CHECK(f.api->fromLegacyID(PICKUP_POOL_SIZE) == 0);

	// Static pickups can't be released.
	IPickup* fixed = f.api->create(1240, 2, Vector3(0.0f), 0, true);
	const int fixedID = fixed->getID();
	f.api->release(fixedID);
	CHECK(f.api->get(fixedID) == fixed);

	f.api->release(pickup->getID());
	CHECK(f.api->count() == 1);

	f.component->reset();
	CHECK(f.api->count() == 0);
	CHECK(f.api->fromLegacyID(0) == 0);
}

TEST_CASE(pickups_streaming)
{
	ComponentFixture<IPickupsComponent> f("Pickups");
	MockPlayer player(2, f.network);
	f.core.players.connect(player);
	IPlayerPickupData* data = queryExtension<IPlayerPickupData>(player);
	if (!data)
	{
		CHECK(data != nullptr);
		return;
	}

	IPickup* pickup = f.api->create(1240, 2, Vector3(100.0f, 0.0f, 0.0f), 0, false);

	// In range (strictly less than the stream radius).
	player.pos = Vector3(-99.0f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(pickup->isStreamedInForPlayer(player));
	CHECK(data->toClientID(pickup->getID()) == 0);
	CHECK(f.network.sentOnly({ PlayerCreatePickup::PacketID }));
	f.core.players.update(player);
	CHECK(f.network.sentOnly({}));

	player.pos = Vector3(-100.0f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(!pickup->isStreamedInForPlayer(player));
	CHECK(data->toClientID(pickup->getID()) == -1);
	CHECK(f.network.sentOnly({ PlayerDestroyPickup::PacketID }));

	// Virtual worlds must match, unless the pickup is in all worlds (-1).
	player.pos = Vector3(100.0f, 0.0f, 0.0f);
	player.virtualWorld = 1;
	f.core.players.update(player);
	CHECK(!pickup->isStreamedInForPlayer(player));
	pickup->setVirtualWorld(-1);
	f.core.players.update(player);
	CHECK(pickup->isStreamedInForPlayer(player));
	f.network.sent.clear();

	// Hiding streams it out.
	pickup->setPickupHiddenForPlayer(player, true);
	CHECK(pickup->isPickupHiddenForPlayer(player));
	f.core.players.update(player);
	CHECK(!pickup->isStreamedInForPlayer(player));
	pickup->setPickupHiddenForPlayer(player, false);
	CHECK(!pickup->isPickupHiddenForPlayer(player));
	f.core.players.update(player);
	CHECK(pickup->isStreamedInForPlayer(player));
	f.network.sent.clear();

	// Players with no state (e.g. not spawned yet) are left alone.
	player.state = PlayerState_None;
	player.pos = Vector3(10000.0f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(pickup->isStreamedInForPlayer(player));
	player.state = PlayerState_OnFoot;
	player.pos = Vector3(100.0f, 0.0f, 0.0f);

	// Changes restream to players it's streamed for, unless asked not to.
	pickup->setModel(1242, true);
	pickup->setType(1, true);
	pickup->setPosition(Vector3(101.0f, 0.0f, 0.0f));
	CHECK(f.network.sentOnly({
		PlayerDestroyPickup::PacketID,
		PlayerCreatePickup::PacketID,
		PlayerDestroyPickup::PacketID,
		PlayerCreatePickup::PacketID,
		PlayerDestroyPickup::PacketID,
		PlayerCreatePickup::PacketID,
	}));
	pickup->setModel(1243, false);
	pickup->setPositionNoUpdate(Vector3(102.0f, 0.0f, 0.0f));
	CHECK(f.network.sentOnly({}));

	// Client IDs are reused once freed.
	IPickup* second = f.api->create(1240, 2, Vector3(100.0f, 0.0f, 0.0f), 1, false);
	f.core.players.update(player);
	CHECK(data->toClientID(second->getID()) == 1);
	f.api->release(pickup->getID());
	CHECK(f.network.sentOnly({ PlayerCreatePickup::PacketID, PlayerDestroyPickup::PacketID }));
	IPickup* third = f.api->create(1240, 2, Vector3(100.0f, 0.0f, 0.0f), 1, false);
	f.core.players.update(player);
	CHECK(data->toClientID(third->getID()) == 0);

	f.core.players.disconnect(player);
}

TEST_CASE(pickups_pick_up_event)
{
	ComponentFixture<IPickupsComponent> f("Pickups");
	Events events;
	f.api->getEventDispatcher().addEventHandler(&events);
	MockPlayer player(4, f.network);
	f.core.players.connect(player);

	IPickup* pickup = f.api->create(1240, 2, Vector3(0.0f), 0, false);
	f.core.players.update(player);
	CHECK(pickup->isStreamedInForPlayer(player));

	CHECK(pickUp(f.network, player, 0));
	CHECK(events.pickedUp == 1);
	CHECK(events.last == pickup);

	// Client IDs that don't map to a pickup are rejected.
	CHECK(!pickUp(f.network, player, 1));
	CHECK(!pickUp(f.network, player, -1));
	CHECK(!pickUp(f.network, player, PICKUP_POOL_SIZE));
	CHECK(events.pickedUp == 1);

	// Truncated packets are rejected.
	NetworkBitStream empty;
	CHECK(!f.network.receive<OnPlayerPickUpPickup>(player, empty));

	f.api->getEventDispatcher().removeEventHandler(&events);
	f.core.players.disconnect(player);
}

TEST_CASE(pickups_legacy_per_player)
{
	ComponentFixture<IPickupsComponent> f("Pickups");
	MockPlayer owner(1, f.network);
	MockPlayer other(2, f.network);
	f.core.players.connect(owner);
	f.core.players.connect(other);

	IPickup* perPlayer = f.api->create(1240, 2, Vector3(0.0f), 0, false);
	perPlayer->setLegacyPlayer(&owner);
	CHECK(perPlayer->getLegacyPlayer() == &owner);
	CHECK(!perPlayer->isPickupHiddenForPlayer(owner));
	CHECK(perPlayer->isPickupHiddenForPlayer(other));
	// Explicit hiding is ignored for per-player pickups.
	perPlayer->setPickupHiddenForPlayer(owner, true);
	CHECK(!perPlayer->isPickupHiddenForPlayer(owner));

	f.core.players.update(owner);
	f.core.players.update(other);
	CHECK(perPlayer->isStreamedInForPlayer(owner));
	CHECK(!perPlayer->isStreamedInForPlayer(other));

	IPickup* global = f.api->create(1240, 2, Vector3(0.0f), 0, false);
	global->setPickupHiddenForPlayer(owner, true);
	f.core.players.update(other);
	CHECK(global->isStreamedInForPlayer(other));

	// Disconnecting releases the player's legacy pickups and forgets them elsewhere.
	const int perPlayerID = perPlayer->getID();
	f.core.players.disconnect(owner);
	CHECK(f.api->get(perPlayerID) == nullptr);
	CHECK(!global->isPickupHiddenForPlayer(owner));
	CHECK(global->isStreamedInForPlayer(other));

	f.core.players.disconnect(other);
}
