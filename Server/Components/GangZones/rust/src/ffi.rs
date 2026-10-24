/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../gangzones_rs.hpp`; keep the two in sync.
//!
//! Every function taking a pointer requires it to come from the matching constructor and still be
//! alive.

use crate::{IdMapper, Swapped, Transition, Zone, ZonePos};
use omp_common::Colour;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_normalise(pos: *mut ZonePos) -> Swapped {
	unsafe { &mut *pos }.normalise()
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_gangzones_contains(pos: ZonePos, x: f32, y: f32) -> bool {
	pos.contains(x, y)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_gangzones_zone_new(pos: ZonePos) -> *mut Zone {
	Box::into_raw(Box::new(Zone::new(pos)))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_free(zone: *mut Zone) {
	drop(unsafe { Box::from_raw(zone) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_get_position(zone: *const Zone) -> ZonePos {
	unsafe { &*zone }.pos
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_set_position(zone: *mut Zone, pos: ZonePos) {
	unsafe { &mut *zone }.pos = pos;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_get_colour(zone: *const Zone) -> Colour {
	unsafe { &*zone }.colour
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_show(zone: *mut Zone, player: i32, colour: Colour) {
	unsafe { &mut *zone }.show(player, colour);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_remove_for(zone: *mut Zone, player: i32) {
	unsafe { &mut *zone }.remove_for(player);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_flash(zone: *mut Zone, player: i32, colour: Colour) {
	unsafe { &mut *zone }.flash(player, colour);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_stop_flash(zone: *mut Zone, player: i32) {
	unsafe { &mut *zone }.stop_flash(player);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_is_flashing(zone: *const Zone, player: i32) -> bool {
	unsafe { &*zone }.is_flashing(player)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_colour_for(zone: *const Zone, player: i32) -> Colour {
	unsafe { &*zone }.colour_for(player)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_flash_colour_for(zone: *const Zone, player: i32) -> Colour {
	unsafe { &*zone }.flash_colour_for(player)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_is_inside(zone: *const Zone, player: i32) -> bool {
	unsafe { &*zone }.is_inside(player)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_set_inside(zone: *mut Zone, player: i32, inside: bool) {
	unsafe { &mut *zone }.set_inside(player, inside);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_zone_check(zone: *const Zone, player: i32, x: f32, y: f32) -> Transition {
	unsafe { &*zone }.check(player, x, y)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_gangzones_ids_new() -> *mut IdMapper {
	Box::into_raw(Box::new(IdMapper::new()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_free(ids: *mut IdMapper) {
	drop(unsafe { Box::from_raw(ids) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_reserve(ids: *const IdMapper) -> i32 {
	unsafe { &*ids }.reserve()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_set(ids: *mut IdMapper, legacy: i32, real: i32) {
	unsafe { &mut *ids }.set(legacy, real);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_release(ids: *mut IdMapper, legacy: i32) {
	unsafe { &mut *ids }.release(legacy);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_release_all(ids: *mut IdMapper) {
	unsafe { &mut *ids }.release_all();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_to_legacy(ids: *const IdMapper, real: i32) -> i32 {
	unsafe { &*ids }.to_legacy(real)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_gangzones_ids_from_legacy(ids: *const IdMapper, legacy: i32) -> i32 {
	unsafe { &*ids }.from_legacy(legacy)
}
