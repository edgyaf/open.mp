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
	struct OmpGangZonesZone;
	struct OmpGangZonesIDs;

	struct OmpGangZonesVec2
	{
		float x, y;
	};

	struct OmpGangZonesPos
	{
		OmpGangZonesVec2 min, max;
	};

	struct OmpGangZonesColour
	{
		uint8_t r, g, b, a;
	};

	struct OmpGangZonesSwapped
	{
		bool x, y;
	};

	enum OmpGangZonesTransition : int32_t
	{
		OmpGangZonesTransition_None = 0,
		OmpGangZonesTransition_Enter = 1,
		OmpGangZonesTransition_Leave = 2,
	};

	OmpGangZonesSwapped omp_gangzones_normalise(OmpGangZonesPos* pos);
	bool omp_gangzones_contains(OmpGangZonesPos pos, float x, float y);

	OmpGangZonesZone* omp_gangzones_zone_new(OmpGangZonesPos pos);
	void omp_gangzones_zone_free(OmpGangZonesZone* zone);
	OmpGangZonesPos omp_gangzones_zone_get_position(const OmpGangZonesZone* zone);
	void omp_gangzones_zone_set_position(OmpGangZonesZone* zone, OmpGangZonesPos pos);
	OmpGangZonesColour omp_gangzones_zone_get_colour(const OmpGangZonesZone* zone);
	void omp_gangzones_zone_show(OmpGangZonesZone* zone, int32_t player, OmpGangZonesColour colour);
	void omp_gangzones_zone_remove_for(OmpGangZonesZone* zone, int32_t player);
	void omp_gangzones_zone_flash(OmpGangZonesZone* zone, int32_t player, OmpGangZonesColour colour);
	void omp_gangzones_zone_stop_flash(OmpGangZonesZone* zone, int32_t player);
	bool omp_gangzones_zone_is_flashing(const OmpGangZonesZone* zone, int32_t player);
	OmpGangZonesColour omp_gangzones_zone_colour_for(const OmpGangZonesZone* zone, int32_t player);
	OmpGangZonesColour omp_gangzones_zone_flash_colour_for(const OmpGangZonesZone* zone, int32_t player);
	bool omp_gangzones_zone_is_inside(const OmpGangZonesZone* zone, int32_t player);
	void omp_gangzones_zone_set_inside(OmpGangZonesZone* zone, int32_t player, bool inside);
	OmpGangZonesTransition omp_gangzones_zone_check(const OmpGangZonesZone* zone, int32_t player, float x, float y);

	OmpGangZonesIDs* omp_gangzones_ids_new();
	void omp_gangzones_ids_free(OmpGangZonesIDs* ids);
	int32_t omp_gangzones_ids_reserve(const OmpGangZonesIDs* ids);
	void omp_gangzones_ids_set(OmpGangZonesIDs* ids, int32_t legacy, int32_t real);
	void omp_gangzones_ids_release(OmpGangZonesIDs* ids, int32_t legacy);
	void omp_gangzones_ids_release_all(OmpGangZonesIDs* ids);
	int32_t omp_gangzones_ids_to_legacy(const OmpGangZonesIDs* ids, int32_t real);
	int32_t omp_gangzones_ids_from_legacy(const OmpGangZonesIDs* ids, int32_t legacy);
}
