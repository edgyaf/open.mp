/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// C ABI of the Rust implementation in `rust/src/ffi.rs`; keep the two in sync.

#include <cstdint>

extern "C"
{
	struct OmpCheckpointsPlayer;
	struct OmpCheckpointsCheckpoint;
	struct OmpCheckpointsRace;

	struct OmpCheckpointsVec3
	{
		float x, y, z;
	};

	enum OmpCheckpointsTransition : int32_t
	{
		OmpCheckpointsTransition_None = 0,
		OmpCheckpointsTransition_Enter = 1,
		OmpCheckpointsTransition_Leave = 2,
	};

	OmpCheckpointsPlayer* omp_checkpoints_player_new();
	void omp_checkpoints_player_free(OmpCheckpointsPlayer* player);
	void omp_checkpoints_player_reset(OmpCheckpointsPlayer* player);
	OmpCheckpointsCheckpoint* omp_checkpoints_player_checkpoint(OmpCheckpointsPlayer* player);
	OmpCheckpointsRace* omp_checkpoints_player_race(OmpCheckpointsPlayer* player);
	OmpCheckpointsCheckpoint* omp_checkpoints_race_base(OmpCheckpointsRace* race);

	OmpCheckpointsVec3 omp_checkpoints_cp_get_position(const OmpCheckpointsCheckpoint* cp);
	void omp_checkpoints_cp_set_position(OmpCheckpointsCheckpoint* cp, OmpCheckpointsVec3 position);
	float omp_checkpoints_cp_get_radius(const OmpCheckpointsCheckpoint* cp);
	void omp_checkpoints_cp_set_radius(OmpCheckpointsCheckpoint* cp, float radius);
	bool omp_checkpoints_cp_is_inside(const OmpCheckpointsCheckpoint* cp);
	void omp_checkpoints_cp_set_inside(OmpCheckpointsCheckpoint* cp, bool inside);
	bool omp_checkpoints_cp_is_enabled(const OmpCheckpointsCheckpoint* cp);
	/// Returns whether to send a disable packet before sending the checkpoint.
	bool omp_checkpoints_cp_enable(OmpCheckpointsCheckpoint* cp);
	/// Returns whether to send a disable packet.
	bool omp_checkpoints_cp_disable(OmpCheckpointsCheckpoint* cp);
	OmpCheckpointsTransition omp_checkpoints_cp_update(OmpCheckpointsCheckpoint* cp, OmpCheckpointsVec3 playerPosition);

	int32_t omp_checkpoints_race_get_type(const OmpCheckpointsRace* race);
	void omp_checkpoints_race_set_type(OmpCheckpointsRace* race, int32_t type);
	OmpCheckpointsVec3 omp_checkpoints_race_get_next_position(const OmpCheckpointsRace* race);
	void omp_checkpoints_race_set_next_position(OmpCheckpointsRace* race, OmpCheckpointsVec3 position);
}
