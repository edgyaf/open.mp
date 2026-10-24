/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "checkpoint.hpp"
#include <Impl/events_impl.hpp>
#include <sdk.hpp>

using namespace Impl;

class CheckpointsComponent final : public ICheckpointsComponent, public PlayerConnectEventHandler
{
private:
	DefaultEventDispatcher<PlayerCheckpointEventHandler> eventDispatcher;
	ICore* core = nullptr;

	DefaultEventDispatcher<PlayerCheckpointEventHandler>& getEventDispatcher() override
	{
		return eventDispatcher;
	}

	void onPlayerConnect(IPlayer& player) override
	{
		player.addExtension(new PlayerCheckpointData(player), true);
	}

	static void processPlayerCheckpoints(CheckpointsComponent& component, IPlayer& player)
	{
		PlayerCheckpointData* playerCheckpointData = queryExtension<PlayerCheckpointData>(player);
		if (playerCheckpointData)
		{
			// Event handlers may move the player or change the race checkpoint, so these run in turn.
			const OmpCheckpointsTransition checkpoint = playerCheckpointData->updateCheckpoint(player.getPosition());
			if (checkpoint == OmpCheckpointsTransition_Enter)
			{
				component.eventDispatcher.dispatch(&PlayerCheckpointEventHandler::onPlayerEnterCheckpoint, player);
			}
			else if (checkpoint == OmpCheckpointsTransition_Leave)
			{
				component.eventDispatcher.dispatch(&PlayerCheckpointEventHandler::onPlayerLeaveCheckpoint, player);
			}

			const OmpCheckpointsTransition race = playerCheckpointData->updateRaceCheckpoint(player.getPosition());
			if (race == OmpCheckpointsTransition_Enter)
			{
				component.eventDispatcher.dispatch(&PlayerCheckpointEventHandler::onPlayerEnterRaceCheckpoint, player);
			}
			else if (race == OmpCheckpointsTransition_Leave)
			{
				component.eventDispatcher.dispatch(&PlayerCheckpointEventHandler::onPlayerLeaveRaceCheckpoint, player);
			}
		}
	}

	struct PlayerCheckpointActionHandler : public PlayerUpdateEventHandler
	{
		CheckpointsComponent& self;
		PlayerCheckpointActionHandler(CheckpointsComponent& self)
			: self(self)
		{
		}

		bool onPlayerUpdate(IPlayer& player, TimePoint now) override
		{
			processPlayerCheckpoints(self, player);
			return true;
		}
	} playerCheckpointActionHandler;

public:
	CheckpointsComponent()
		: playerCheckpointActionHandler(*this)
	{
	}

	void onLoad(ICore* c) override
	{
		core = c;
		core->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
		core->getPlayers().getPlayerUpdateDispatcher().addEventHandler(&playerCheckpointActionHandler);
	}

	StringView componentName() const override
	{
		return "Checkpoints";
	}

	SemanticVersion componentVersion() const override
	{
		return SemanticVersion(OMP_VERSION_MAJOR, OMP_VERSION_MINOR, OMP_VERSION_PATCH, BUILD_NUMBER);
	}

	void free() override
	{
		delete this;
	}

	void reset() override
	{
		// Destroy all stored entity instances.
	}

	~CheckpointsComponent()
	{
		if (core)
		{
			core->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
			core->getPlayers().getPlayerUpdateDispatcher().removeEventHandler(&playerCheckpointActionHandler);
		}
	}
};

COMPONENT_ENTRY_POINT()
{
	return new CheckpointsComponent();
}
