/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/Checkpoints/checkpoints.hpp>
#include <netcode.hpp>

using namespace NetCode::RPC;

namespace
{

struct Events final : public PlayerCheckpointEventHandler
{
	int entered = 0, left = 0, raceEntered = 0, raceLeft = 0;
	ICheckpointsComponent* component = nullptr;
	Vector3 moveTo = Vector3(0.0f);
	bool moveOnEnter = false;

	void onPlayerEnterCheckpoint(IPlayer& player) override
	{
		++entered;
		if (moveOnEnter)
		{
			player.setPosition(moveTo);
		}
	}

	void onPlayerLeaveCheckpoint(IPlayer& player) override { ++left; }
	void onPlayerEnterRaceCheckpoint(IPlayer& player) override { ++raceEntered; }
	void onPlayerLeaveRaceCheckpoint(IPlayer& player) override { ++raceLeft; }
};

}

TEST_CASE(checkpoints_enable_disable_packets)
{
	ComponentFixture<ICheckpointsComponent> f("Checkpoints");
	CHECK(f.component->getUID() == CheckpointsComponent_UID);
	MockPlayer player(7, f.network);
	f.core.players.connect(player);
	IPlayerCheckpointData* data = queryExtension<IPlayerCheckpointData>(player);
	CHECK(data != nullptr);
	if (!data)
	{
		return;
	}

	ICheckpointData& cp = data->getCheckpoint();
	CHECK(!cp.isEnabled());
	cp.setPosition(Vector3(1.0f, 2.0f, 3.0f));
	cp.setRadius(4.0f);
	CHECK(cp.getPosition() == Vector3(1.0f, 2.0f, 3.0f));
	CHECK(cp.getRadius() == 4.0f);
	cp.enable();
	CHECK(cp.isEnabled());
	CHECK(f.network.sentOnly({ SetCheckpoint::PacketID }));

	// Re-enabling disables first and resets "inside".
	cp.setPlayerInside(true);
	cp.enable();
	CHECK(!cp.isPlayerInside());
	CHECK(f.network.sentOnly({ DisableCheckpoint::PacketID, SetCheckpoint::PacketID }));
	cp.disable();
	cp.disable();
	CHECK(f.network.sentOnly({ DisableCheckpoint::PacketID }));

	IRaceCheckpointData& race = data->getRaceCheckpoint();
	CHECK(race.getType() == RaceCheckpointType::RACE_NONE);
	race.setType(RaceCheckpointType::RACE_AIR_FINISH);
	race.setNextPosition(Vector3(5.0f, 6.0f, 7.0f));
	CHECK(race.getType() == RaceCheckpointType::RACE_AIR_FINISH);
	CHECK(race.getNextPosition() == Vector3(5.0f, 6.0f, 7.0f));
	race.enable();
	CHECK(race.isEnabled());
	CHECK(f.network.sentOnly({ SetRaceCheckpoint::PacketID }));

	// GMX forgets them without telling the client.
	static_cast<IExtension*>(data)->reset();
	CHECK(!cp.isEnabled());
	CHECK(!race.isEnabled());
	race.disable();
	CHECK(f.network.sentOnly({}));

	f.core.players.disconnect(player);
}

TEST_CASE(checkpoints_enter_leave_events)
{
	ComponentFixture<ICheckpointsComponent> f("Checkpoints");
	Events events;
	f.api->getEventDispatcher().addEventHandler(&events);
	MockPlayer player(1, f.network);
	f.core.players.connect(player);
	IPlayerCheckpointData* data = queryExtension<IPlayerCheckpointData>(player);
	if (!data)
	{
		CHECK(data != nullptr);
		return;
	}

	ICheckpointData& cp = data->getCheckpoint();
	cp.setPosition(Vector3(10.0f, 0.0f, 0.0f));
	cp.setRadius(2.0f);

	// Disabled checkpoints never fire.
	player.pos = Vector3(10.0f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.entered == 0);

	cp.enable();
	f.core.players.update(player);
	CHECK(events.entered == 1);
	CHECK(cp.isPlayerInside());
	f.core.players.update(player);
	CHECK(events.entered == 1);

	// The boundary counts as inside.
	player.pos = Vector3(12.0f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.left == 0);
	player.pos = Vector3(12.1f, 0.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.left == 1);

	IRaceCheckpointData& race = data->getRaceCheckpoint();
	race.setPosition(Vector3(0.0f, 50.0f, 0.0f));
	race.setRadius(5.0f);
	race.enable();
	player.pos = Vector3(0.0f, 52.0f, 0.0f);
	f.core.players.update(player);
	CHECK(events.raceEntered == 1);
	CHECK(events.entered == 1);

	f.api->getEventDispatcher().removeEventHandler(&events);
	f.core.players.disconnect(player);
}

TEST_CASE(checkpoints_race_sees_position_set_by_checkpoint_handler)
{
	ComponentFixture<ICheckpointsComponent> f("Checkpoints");
	Events events;
	f.api->getEventDispatcher().addEventHandler(&events);
	MockPlayer player(2, f.network);
	f.core.players.connect(player);
	IPlayerCheckpointData* data = queryExtension<IPlayerCheckpointData>(player);
	if (!data)
	{
		CHECK(data != nullptr);
		return;
	}

	// Both contain the start position, but the checkpoint handler teleports the player out of the
	// race checkpoint before it is checked.
	ICheckpointData& cp = data->getCheckpoint();
	cp.setPosition(Vector3(0.0f));
	cp.setRadius(5.0f);
	cp.enable();
	IRaceCheckpointData& race = data->getRaceCheckpoint();
	race.setPosition(Vector3(0.0f));
	race.setRadius(5.0f);
	race.enable();
	events.moveOnEnter = true;
	events.moveTo = Vector3(100.0f, 0.0f, 0.0f);

	f.core.players.update(player);
	CHECK(events.entered == 1);
	CHECK(events.raceEntered == 0);

	f.api->getEventDispatcher().removeEventHandler(&events);
	f.core.players.disconnect(player);
}
