/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

#include "checkpoints_rs.hpp"
#include <Server/Components/Checkpoints/checkpoints.hpp>
#include <netcode.hpp>

// The checkpoint state lives in Rust (see `rust/`); these only adapt it to the SDK interfaces and
// send the packets it asks for.

inline OmpCheckpointsVec3 toRust(const Vector3& v)
{
	return OmpCheckpointsVec3 { v.x, v.y, v.z };
}

inline Vector3 fromRust(const OmpCheckpointsVec3& v)
{
	return Vector3(v.x, v.y, v.z);
}

template <class T>
struct CheckpointDataBase : public T
{
	OmpCheckpointsCheckpoint* const cp_;
	IPlayer& player_;

	CheckpointDataBase(OmpCheckpointsCheckpoint* cp, IPlayer& player)
		: cp_(cp)
		, player_(player)
	{
	}

	Vector3 getPosition() const override
	{
		return fromRust(omp_checkpoints_cp_get_position(cp_));
	}

	void setPosition(const Vector3& position) override
	{
		omp_checkpoints_cp_set_position(cp_, toRust(position));
	}

	float getRadius() const override
	{
		return omp_checkpoints_cp_get_radius(cp_);
	}

	void setRadius(float radius) override
	{
		omp_checkpoints_cp_set_radius(cp_, radius);
	}

	bool isPlayerInside() const override
	{
		return omp_checkpoints_cp_is_inside(cp_);
	}

	void setPlayerInside(bool inside) override
	{
		omp_checkpoints_cp_set_inside(cp_, inside);
	}

	bool isEnabled() const override
	{
		return omp_checkpoints_cp_is_enabled(cp_);
	}

	/// Track whether the player is inside, returning whether they just entered or left.
	OmpCheckpointsTransition update(const Vector3& playerPosition)
	{
		return omp_checkpoints_cp_update(cp_, toRust(playerPosition));
	}
};

class CheckpointData final : public CheckpointDataBase<ICheckpointData>
{
public:
	CheckpointData(OmpCheckpointsCheckpoint* cp, IPlayer& player)
		: CheckpointDataBase<ICheckpointData>(cp, player)
	{
	}

	void enable() override
	{
		if (omp_checkpoints_cp_enable(cp_))
		{
			NetCode::RPC::DisableCheckpoint disableCP;
			PacketHelper::send(disableCP, player_);
		}

		NetCode::RPC::SetCheckpoint setCP;
		setCP.position = getPosition();
		setCP.size = getRadius(); // samp client asks for radius (not diameter) for checkpoints
		PacketHelper::send(setCP, player_);
	}

	void disable() override
	{
		if (omp_checkpoints_cp_disable(cp_))
		{
			NetCode::RPC::DisableCheckpoint disableCP;
			PacketHelper::send(disableCP, player_);
		}
	}
};

class RaceCheckpointData final : public CheckpointDataBase<IRaceCheckpointData>
{
private:
	OmpCheckpointsRace* const race_;

public:
	RaceCheckpointData(OmpCheckpointsRace* race, IPlayer& player)
		: CheckpointDataBase<IRaceCheckpointData>(omp_checkpoints_race_base(race), player)
		, race_(race)
	{
	}

	RaceCheckpointType getType() const override
	{
		return RaceCheckpointType(omp_checkpoints_race_get_type(race_));
	}

	void setType(const RaceCheckpointType type) override
	{
		omp_checkpoints_race_set_type(race_, static_cast<int32_t>(type));
	}

	Vector3 getNextPosition() const override
	{
		return fromRust(omp_checkpoints_race_get_next_position(race_));
	}

	void setNextPosition(const Vector3& nextPosition) override
	{
		omp_checkpoints_race_set_next_position(race_, toRust(nextPosition));
	}

	void enable() override
	{
		if (omp_checkpoints_cp_enable(cp_))
		{
			NetCode::RPC::DisableRaceCheckpoint disableRaceCP;
			PacketHelper::send(disableRaceCP, player_);
		}

		NetCode::RPC::SetRaceCheckpoint setRaceCP;
		setRaceCP.type = static_cast<uint8_t>(getType());
		setRaceCP.position = getPosition();
		setRaceCP.nextPosition = getNextPosition();
		setRaceCP.size = getRadius(); // samp client asks for radius for race checkpoints
		PacketHelper::send(setRaceCP, player_);
	}

	void disable() override
	{
		if (omp_checkpoints_cp_disable(cp_))
		{
			NetCode::RPC::DisableRaceCheckpoint disableRaceCP;
			PacketHelper::send(disableRaceCP, player_);
		}
	}
};

class PlayerCheckpointData final : public IPlayerCheckpointData
{
private:
	OmpCheckpointsPlayer* const data_;
	RaceCheckpointData raceCheckpoint;
	CheckpointData checkpoint;

public:
	PlayerCheckpointData(IPlayer& player)
		: data_(omp_checkpoints_player_new())
		, raceCheckpoint(omp_checkpoints_player_race(data_), player)
		, checkpoint(omp_checkpoints_player_checkpoint(data_), player)
	{
	}

	~PlayerCheckpointData()
	{
		omp_checkpoints_player_free(data_);
	}

	void freeExtension() override
	{
		delete this;
	}

	IRaceCheckpointData& getRaceCheckpoint() override
	{
		return raceCheckpoint;
	}

	ICheckpointData& getCheckpoint() override
	{
		return checkpoint;
	}

	OmpCheckpointsTransition updateCheckpoint(const Vector3& playerPosition)
	{
		return checkpoint.update(playerPosition);
	}

	OmpCheckpointsTransition updateRaceCheckpoint(const Vector3& playerPosition)
	{
		return raceCheckpoint.update(playerPosition);
	}

	void reset() override
	{
		omp_checkpoints_player_reset(data_);
	}
};
