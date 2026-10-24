/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../checkpoints_rs.hpp`; keep the two in sync.
//!
//! A player's checkpoints are allocated together; the checkpoint pointers handed out for them stay
//! valid until `omp_checkpoints_player_free`.

use crate::{Checkpoint, PlayerCheckpoints, RaceCheckpoint, Transition, Vec3};

#[unsafe(no_mangle)]
pub extern "C" fn omp_checkpoints_player_new() -> *mut PlayerCheckpoints {
	Box::into_raw(Box::default())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_player_free(player: *mut PlayerCheckpoints) {
	drop(unsafe { Box::from_raw(player) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_player_reset(player: *mut PlayerCheckpoints) {
	unsafe { &mut *player }.reset();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_player_checkpoint(player: *mut PlayerCheckpoints) -> *mut Checkpoint {
	unsafe { &raw mut (*player).checkpoint }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_player_race(player: *mut PlayerCheckpoints) -> *mut RaceCheckpoint {
	unsafe { &raw mut (*player).race }
}

/// The part of a race checkpoint shared with normal checkpoints, for the `omp_checkpoints_cp_*`
/// functions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_race_base(race: *mut RaceCheckpoint) -> *mut Checkpoint {
	unsafe { &raw mut (*race).base }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_get_position(cp: *const Checkpoint) -> Vec3 {
	unsafe { &*cp }.position
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_set_position(cp: *mut Checkpoint, position: Vec3) {
	unsafe { &mut *cp }.position = position;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_get_radius(cp: *const Checkpoint) -> f32 {
	unsafe { &*cp }.radius
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_set_radius(cp: *mut Checkpoint, radius: f32) {
	unsafe { &mut *cp }.radius = radius;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_is_inside(cp: *const Checkpoint) -> bool {
	unsafe { &*cp }.inside
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_set_inside(cp: *mut Checkpoint, inside: bool) {
	unsafe { &mut *cp }.inside = inside;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_is_enabled(cp: *const Checkpoint) -> bool {
	unsafe { &*cp }.enabled()
}

/// Returns whether to send a disable packet before sending the checkpoint.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_enable(cp: *mut Checkpoint) -> bool {
	unsafe { &mut *cp }.enable()
}

/// Returns whether to send a disable packet.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_disable(cp: *mut Checkpoint) -> bool {
	unsafe { &mut *cp }.disable()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_cp_update(cp: *mut Checkpoint, player_position: Vec3) -> Transition {
	unsafe { &mut *cp }.update(player_position)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_race_get_type(race: *const RaceCheckpoint) -> i32 {
	unsafe { &*race }.kind
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_race_set_type(race: *mut RaceCheckpoint, kind: i32) {
	unsafe { &mut *race }.kind = kind;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_race_get_next_position(race: *const RaceCheckpoint) -> Vec3 {
	unsafe { &*race }.next_position
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_checkpoints_race_set_next_position(race: *mut RaceCheckpoint, position: Vec3) {
	unsafe { &mut *race }.next_position = position;
}
