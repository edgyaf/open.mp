/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../pickups_rs.hpp`; keep the two in sync.
//!
//! Every function taking a pointer requires it to come from the matching constructor and still be
//! alive.

use crate::{IdMapper, Pickup};
use omp_common::Vec3;

#[unsafe(no_mangle)]
pub extern "C" fn omp_pickups_pickup_new(
	model: i32,
	kind: u8,
	pos: Vec3,
	virtual_world: i32,
	is_static: bool,
) -> *mut Pickup {
	Box::into_raw(Box::new(Pickup::new(model, kind, pos, virtual_world, is_static)))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_free(pickup: *mut Pickup) {
	drop(unsafe { Box::from_raw(pickup) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_get_model(pickup: *const Pickup) -> i32 {
	unsafe { &*pickup }.model
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_model(pickup: *mut Pickup, model: i32) {
	unsafe { &mut *pickup }.model = model;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_get_type(pickup: *const Pickup) -> u8 {
	unsafe { &*pickup }.kind
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_type(pickup: *mut Pickup, kind: u8) {
	unsafe { &mut *pickup }.kind = kind;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_get_position(pickup: *const Pickup) -> Vec3 {
	unsafe { &*pickup }.pos
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_position(pickup: *mut Pickup, pos: Vec3) {
	unsafe { &mut *pickup }.pos = pos;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_get_virtual_world(pickup: *const Pickup) -> i32 {
	unsafe { &*pickup }.virtual_world
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_virtual_world(pickup: *mut Pickup, virtual_world: i32) {
	unsafe { &mut *pickup }.virtual_world = virtual_world;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_is_static(pickup: *const Pickup) -> bool {
	unsafe { &*pickup }.is_static
}

/// `player` is the owner's ID, or -1 for none.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_legacy_owner(pickup: *mut Pickup, player: i32) {
	unsafe { &mut *pickup }.set_legacy_owner((player >= 0).then_some(player));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_is_hidden_for(pickup: *const Pickup, player: i32) -> bool {
	unsafe { &*pickup }.is_hidden_for(player)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_set_hidden_for(pickup: *mut Pickup, player: i32, hidden: bool) {
	unsafe { &mut *pickup }.set_hidden_for(player, hidden);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_pickup_should_stream_in(
	pickup: *const Pickup,
	player: i32,
	virtual_world: i32,
	pos: Vec3,
	max_distance_sqr: f32,
) -> bool {
	unsafe { &*pickup }.should_stream_in(player, virtual_world, pos, max_distance_sqr)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_pickups_ids_new() -> *mut IdMapper {
	Box::into_raw(Box::new(IdMapper::new()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_free(ids: *mut IdMapper) {
	drop(unsafe { Box::from_raw(ids) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_reserve(ids: *const IdMapper) -> i32 {
	unsafe { &*ids }.reserve()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_set(ids: *mut IdMapper, legacy: i32, real: i32) {
	unsafe { &mut *ids }.set(legacy, real);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_release(ids: *mut IdMapper, legacy: i32) {
	unsafe { &mut *ids }.release(legacy);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_release_all(ids: *mut IdMapper) {
	unsafe { &mut *ids }.release_all();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_to_legacy(ids: *const IdMapper, real: i32) -> i32 {
	unsafe { &*ids }.to_legacy(real)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_pickups_ids_from_legacy(ids: *const IdMapper, legacy: i32) -> i32 {
	unsafe { &*ids }.from_legacy(legacy)
}
