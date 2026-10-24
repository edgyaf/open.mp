/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/GangZones/gangzones.hpp>
#include <netcode.hpp>

using namespace NetCode::RPC;

namespace
{

struct Events final : public GangZoneEventHandler
{
	int entered = 0, left = 0, clicked = 0;
	void onPlayerEnterGangZone(IPlayer& player, IGangZone& zone) override { ++entered; }
	void onPlayerLeaveGangZone(IPlayer& player, IGangZone& zone) override { ++left; }
	void onPlayerClickGangZone(IPlayer& player, IGangZone& zone) override { ++clicked; }
};

GangZonePos zonePos(float minX, float minY, float maxX, float maxY)
{
	GangZonePos pos;
	pos.min = Vector2(minX, minY);
	pos.max = Vector2(maxX, maxY);
	return pos;
}

const Colour Red(255, 0, 0);
const Colour Blue(0, 0, 255);

}

TEST_CASE(gangzones_pool_and_legacy_ids)
{
	ComponentFixture<IGangZonesComponent> f("GangZones");
	CHECK(f.component->getUID() == GangZoneComponent_UID);

	IGangZone* zone = f.api->create(zonePos(-10.0f, -20.0f, 30.0f, 40.0f));
	CHECK(zone != nullptr);
	const int id = zone->getID();
	CHECK(f.api->get(id) == zone);
	CHECK(f.api->count() == 1);
	CHECK(zone->getPosition().min == Vector2(-10.0f, -20.0f));
	CHECK(zone->getPosition().max == Vector2(30.0f, 40.0f));
	CHECK(f.core.warnings.empty());

	CHECK(f.api->toLegacyID(id) == -1);
	CHECK(f.api->reserveLegacyID() == 0);
	f.api->setLegacyID(0, id);
	CHECK(f.api->toLegacyID(id) == 0);
	CHECK(f.api->fromLegacyID(0) == id);
	CHECK(f.api->reserveLegacyID() == 1);
	CHECK(f.api->fromLegacyID(GANG_ZONE_POOL_SIZE) == 0);
	CHECK(f.api->fromLegacyID(-1) == 0);
	f.api->releaseLegacyID(0);
	CHECK(f.api->toLegacyID(id) == -1);

	f.api->useGangZoneCheck(*zone, true);
	CHECK(f.api->getCheckingGangZones().size() == 1);
	f.api->useGangZoneCheck(*zone, false);
	CHECK(f.api->getCheckingGangZones().empty());
	f.api->useGangZoneCheck(*zone, true);

	// Releasing removes it from the pool and the checking list.
	f.api->release(id);
	CHECK(f.api->get(id) == nullptr);
	CHECK(f.api->getCheckingGangZones().empty());
	CHECK(f.api->count() == 0);

	f.api->setLegacyID(5, 123);
	f.component->reset();
	CHECK(f.api->fromLegacyID(5) == 0);
}

TEST_CASE(gangzones_out_of_order_coordinates)
{
	ComponentFixture<IGangZonesComponent> f("GangZones");
	IGangZone* zone = f.api->create(zonePos(30.0f, 0.0f, -10.0f, 40.0f));
	CHECK(zone->getPosition().min == Vector2(-10.0f, 0.0f));
	CHECK(zone->getPosition().max == Vector2(30.0f, 40.0f));
	CHECK(f.core.warnings.size() == 1);
	CHECK(!f.core.warnings.empty() && f.core.warnings[0] == "Gangzone X co-ordinates 30.00 and -10.00 out of order, inverting.");

	f.core.warnings.clear();
	zone = f.api->create(zonePos(30.0f, 50.0f, -10.0f, 40.0f));
	CHECK(zone->getPosition().min == Vector2(-10.0f, 40.0f));
	CHECK(zone->getPosition().max == Vector2(30.0f, 50.0f));
	CHECK(f.core.warnings.size() == 2);
}

TEST_CASE(gangzones_per_player)
{
	ComponentFixture<IGangZonesComponent> f("GangZones");
	Events events;
	f.api->getEventDispatcher().addEventHandler(&events);
	MockPlayer player(3, f.network);
	f.core.players.connect(player);
	IPlayerGangZoneData* data = queryExtension<IPlayerGangZoneData>(player);
	if (!data)
	{
		CHECK(data != nullptr);
		return;
	}

	IGangZone* zone = f.api->create(zonePos(0.0f, 0.0f, 100.0f, 100.0f));
	zone->showForPlayer(player, Red);
	CHECK(zone->isShownForPlayer(player));
	CHECK(zone->getShownFor().size() == 1);
	CHECK(zone->getColourForPlayer(player).RGBA() == Red.RGBA());
	CHECK(data->toClientID(zone->getID()) == 0);
	CHECK(f.network.sentOnly({ ShowGangZone::PacketID }));

	zone->flashForPlayer(player, Blue);
	CHECK(zone->isFlashingForPlayer(player));
	CHECK(zone->getFlashingColourForPlayer(player).RGBA() == Blue.RGBA());
	zone->stopFlashForPlayer(player);
	CHECK(!zone->isFlashingForPlayer(player));
	CHECK(zone->getFlashingColourForPlayer(player).RGBA() == Colour::None().RGBA());
	CHECK(f.network.sentOnly({ FlashGangZone::PacketID, StopFlashGangZone::PacketID }));

	// Showing again stops flashing.
	zone->flashForPlayer(player, Blue);
	zone->showForPlayer(player, Blue);
	CHECK(!zone->isFlashingForPlayer(player));
	CHECK(f.network.sentOnly({ FlashGangZone::PacketID, ShowGangZone::PacketID }));

	// Enter/leave only fire for zones in the checking list.
	player.pos = Vector3(50.0f, 50.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.entered == 0);
	f.api->useGangZoneCheck(*zone, true);
	f.core.players.update(player);
	CHECK(events.entered == 1);
	CHECK(zone->isPlayerInside(player));
	f.core.players.update(player);
	CHECK(events.entered == 1);
	player.pos = Vector3(100.0f, 50.0f, 0.0f); // max edges are exclusive
	f.core.players.update(player);
	CHECK(events.left == 1);
	CHECK(!zone->isPlayerInside(player));

	f.core.players.clickDispatcher.dispatch(&PlayerClickEventHandler::onPlayerClickMap, player, Vector3(0.0f, 10.0f, 0.0f));
	f.core.players.clickDispatcher.dispatch(&PlayerClickEventHandler::onPlayerClickMap, player, Vector3(-0.1f, 10.0f, 0.0f));
	CHECK(events.clicked == 1);

	// Moving restreams: hide, then show again.
	zone->setPosition(zonePos(0.0f, 0.0f, 200.0f, 200.0f));
	CHECK(f.network.sentOnly({ HideGangZone::PacketID, ShowGangZone::PacketID }));

	zone->hideForPlayer(player);
	CHECK(!zone->isShownForPlayer(player));
	CHECK(zone->getColourForPlayer(player).RGBA() == Colour::None().RGBA());
	CHECK(data->toClientID(zone->getID()) == -1);
	CHECK(f.network.sentOnly({ HideGangZone::PacketID }));

	// Hidden zones don't fire events.
	player.pos = Vector3(50.0f, 50.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.entered == 1);

	CHECK(data->reserveLegacyID() == 0);
	data->setLegacyID(0, zone->getID());
	CHECK(data->fromLegacyID(0) == zone->getID());
	static_cast<IExtension*>(data)->reset();
	CHECK(data->fromLegacyID(0) == 0);

	f.api->getEventDispatcher().removeEventHandler(&events);
	f.core.players.disconnect(player);
}

TEST_CASE(gangzones_player_disconnect)
{
	ComponentFixture<IGangZonesComponent> f("GangZones");
	MockPlayer player(5, f.network);
	MockPlayer other(6, f.network);
	f.core.players.connect(player);
	f.core.players.connect(other);

	IGangZone* global = f.api->create(zonePos(0.0f, 0.0f, 10.0f, 10.0f));
	IGangZone* perPlayer = f.api->create(zonePos(0.0f, 0.0f, 10.0f, 10.0f));
	perPlayer->setLegacyPlayer(&player);
	const int perPlayerID = perPlayer->getID();
	global->showForPlayer(player, Red);
	global->showForPlayer(other, Red);

	f.core.players.disconnect(player);
	CHECK(!global->isShownForPlayer(player));
	CHECK(global->isShownForPlayer(other));
	// Legacy per-player zones go with their player.
	CHECK(f.api->get(perPlayerID) == nullptr);

	f.core.players.disconnect(other);
}
