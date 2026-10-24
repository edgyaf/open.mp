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
	struct OmpPickupsPickup;
	struct OmpPickupsIDs;

	struct OmpPickupsVec3
	{
		float x, y, z;
	};

	OmpPickupsPickup* omp_pickups_pickup_new(int32_t model, uint8_t type, OmpPickupsVec3 pos, int32_t virtualWorld, bool isStatic);
	void omp_pickups_pickup_free(OmpPickupsPickup* pickup);
	int32_t omp_pickups_pickup_get_model(const OmpPickupsPickup* pickup);
	void omp_pickups_pickup_set_model(OmpPickupsPickup* pickup, int32_t model);
	uint8_t omp_pickups_pickup_get_type(const OmpPickupsPickup* pickup);
	void omp_pickups_pickup_set_type(OmpPickupsPickup* pickup, uint8_t type);
	OmpPickupsVec3 omp_pickups_pickup_get_position(const OmpPickupsPickup* pickup);
	void omp_pickups_pickup_set_position(OmpPickupsPickup* pickup, OmpPickupsVec3 pos);
	int32_t omp_pickups_pickup_get_virtual_world(const OmpPickupsPickup* pickup);
	void omp_pickups_pickup_set_virtual_world(OmpPickupsPickup* pickup, int32_t virtualWorld);
	bool omp_pickups_pickup_is_static(const OmpPickupsPickup* pickup);
	/// `player` is the owner's ID, or -1 for none.
	void omp_pickups_pickup_set_legacy_owner(OmpPickupsPickup* pickup, int32_t player);
	bool omp_pickups_pickup_is_hidden_for(const OmpPickupsPickup* pickup, int32_t player);
	void omp_pickups_pickup_set_hidden_for(OmpPickupsPickup* pickup, int32_t player, bool hidden);
	bool omp_pickups_pickup_should_stream_in(const OmpPickupsPickup* pickup, int32_t player, int32_t virtualWorld, OmpPickupsVec3 pos, float maxDistanceSqr);

	OmpPickupsIDs* omp_pickups_ids_new();
	void omp_pickups_ids_free(OmpPickupsIDs* ids);
	int32_t omp_pickups_ids_reserve(const OmpPickupsIDs* ids);
	void omp_pickups_ids_set(OmpPickupsIDs* ids, int32_t legacy, int32_t real);
	void omp_pickups_ids_release(OmpPickupsIDs* ids, int32_t legacy);
	void omp_pickups_ids_release_all(OmpPickupsIDs* ids);
	int32_t omp_pickups_ids_to_legacy(const OmpPickupsIDs* ids, int32_t real);
	int32_t omp_pickups_ids_from_legacy(const OmpPickupsIDs* ids, int32_t legacy);
}
