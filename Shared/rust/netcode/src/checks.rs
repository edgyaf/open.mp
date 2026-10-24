/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Checks and fix-ups `PlayerPool` applies to decoded sync data, where they don't depend on
//! server state.
//!
//! Each is ported from an expression in `Server/Source/player_pool.hpp` and keeps its
//! floating-point operations in the same order (glm's `dot` adds the products left to right,
//! `length` is `sqrt(dot)`), so results match the C++ bit for bit wherever the C++ compiler doesn't
//! fuse multiply-adds. The tests in `Server/Tests` hold them to that.
//!
//! Negated comparisons (`!(a >= b)`) are kept from the C++ on purpose: they decide what NaN does.
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use crate::packets::Vec4;
use omp_common::Vec3;

/// `INVALID_WEAPON_SLOT`.
pub const INVALID_WEAPON_SLOT: i8 = -1;

/// Slot of each weapon ID, from `WeaponInfoList`.
const WEAPON_SLOTS: [i8; 47] = [
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, // 0-9
	10, 10, 10, 10, 10, 10, 8, 8, 8, -1, // 10-19
	-1, -1, 2, 2, 2, 3, 3, 3, 4, 4, // 20-29
	5, 5, 4, 6, 6, 7, 7, 7, 7, 8, // 30-39
	12, 9, 9, 9, 11, 11, 11, // 40-46
];

/// `WeaponSlotData::slot`.
pub fn weapon_slot(weapon: u8) -> i8 {
	WEAPON_SLOTS
		.get(usize::from(weapon))
		.copied()
		.unwrap_or(INVALID_WEAPON_SLOT)
}

/// `WeaponSlotData::shootable`: whether the weapon fires bullets.
pub fn weapon_shootable(weapon: u8) -> bool {
	matches!(weapon, 22..=34 | 38)
}

mod key {
	pub const FIRE: u16 = 4;
	pub const AIM: u16 = 128;
	pub const YES: u32 = 65536;
	pub const NO: u32 = 131072;
	pub const CTRL_BACK: u32 = 262144;
}

const WEAPON_BOMB: u32 = 40;
const WEAPON_NIGHT_VIS_GOGGLES: u32 = 44;
const WEAPON_THERMAL_GOGGLES: u32 = 45;

fn dot3(a: Vec3, b: Vec3) -> f32 {
	a.x * b.x + a.y * b.y + a.z * b.z
}

fn dot4(a: Vec4, b: Vec4) -> f32 {
	(a.x * b.x + a.y * b.y) + (a.z * b.z + a.w * b.w)
}

/// The key state a sync packet reports: its keys plus the extra key packed beside the weapon.
pub fn keys_with_additional_key(keys: u16, additional_key: u8) -> u32 {
	let keys = u32::from(keys);
	match additional_key {
		1 => keys | key::YES,
		2 => keys | key::NO,
		3 => keys | key::CTRL_BACK,
		_ => keys,
	}
}

/// Whether to believe a player is surfing a vehicle at `offset` from it. A far offset is the old
/// "invisible" cheat.
pub fn vehicle_surfing_offset_ok(offset: Vec3) -> bool {
	!(dot3(offset, offset) >= 50.0 * 50.0)
}

/// The keys to send other players for someone on foot holding `armed_weapon`: firing goggles
/// would show their effect to everyone, and aiming a detonator crashes clients.
pub fn foot_keys_for_broadcast(keys: u16, armed_weapon: u32) -> u16 {
	match armed_weapon {
		WEAPON_NIGHT_VIS_GOGGLES | WEAPON_THERMAL_GOGGLES => keys & !key::FIRE,
		WEAPON_BOMB => keys & !key::AIM,
		_ => keys,
	}
}

/// Whether an aim sync's camera front vector is plausible enough to accept the packet.
pub fn aim_front_vector_ok(front: Vec3) -> bool {
	let length_squared = dot3(front, front);
	length_squared > 0.0 && length_squared < 1.5
}

/// The camera mode to record and pass on: invalid modes become 4 (following behind the player).
/// See https://gtag.sannybuilder.com/sanandreas/camera-modes/.
pub fn aim_cam_mode(mode: u8) -> u8 {
	match mode {
		0..=2 | 5 | 6 | 9..=13 | 17 | 19..=21 | 23..=28 | 30..=45 | 48..=50 | 52 | 54 | 60 | 61 | 65.. => 4,
		valid => valid,
	}
}

/// The camera zoom an aim sync's 6-bit zoom field stands for.
pub fn aim_cam_zoom(zoom: u8) -> f32 {
	f32::from(zoom) * 0.015873017 * 35.0 + 35.0
}

/// The aspect ratio an aim sync's aspect ratio byte stands for.
pub fn aim_aspect_ratio(aspect_ratio: u8) -> f32 {
	(f32::from(aspect_ratio) * 1.0 / 255.0) + 1.0
}

/// `PlayerBulletHitType_None`.
const BULLET_HIT_NONE: u8 = 0;

/// Whether a bullet's origin, and its offset from what it hit (or from the origin, for a miss),
/// are within range.
pub fn bullet_in_bounds(hit_type: u8, origin: Vec3, offset: Vec3) -> bool {
	const BOUNDS: f32 = 20000.0 * 20000.0;
	if dot3(origin, origin) > BOUNDS {
		return false;
	}
	let limit = if hit_type == BULLET_HIT_NONE {
		BOUNDS
	} else {
		1000.0 * 1000.0
	};
	!(dot3(offset, offset) > limit)
}

fn within_unit_box(v: Vec3) -> bool {
	!(v.x < -1.0 || v.x > 1.0 || v.y < -1.0 || v.y > 1.0 || v.z < -1.0 || v.z > 1.0)
}

/// Whether an unoccupied vehicle sync's motion is plausible: angular velocity within bounds, and
/// roll and rotation (direction) vectors of unit length and perpendicular.
pub fn unoccupied_motion_ok(angular_velocity: Vec3, roll: Vec3, rotation: Vec3) -> bool {
	if !within_unit_box(angular_velocity) {
		return false;
	}
	// The C++ compares in double precision after computing lengths and dot products in single.
	let off_unit = |v: Vec3| (1.0 - f64::from(dot3(v, v).sqrt())).abs() >= 0.000001;
	!(off_unit(roll) || off_unit(rotation) || f64::from(dot3(roll, rotation).abs()) >= 0.000001)
}

/// Whether a trailer sync's turn velocity is within bounds.
pub fn trailer_turn_velocity_ok(turn_velocity: Vec3) -> bool {
	within_unit_box(turn_velocity)
}

/// A trailer sync's quaternion, normalised; one too short to normalise becomes (0.5, 0.5, 0.5, 0.5).
pub fn trailer_quat_normalised(quat: Vec4) -> Vec4 {
	let magnitude = dot4(quat, quat).sqrt();
	if (1.0 - magnitude).abs() >= 0.000001 {
		if magnitude < 0.1 {
			Vec4 {
				x: 0.5,
				y: 0.5,
				z: 0.5,
				w: 0.5,
			}
		} else {
			Vec4 {
				x: quat.x / magnitude,
				y: quat.y / magnitude,
				z: quat.z / magnitude,
				w: quat.w / magnitude,
			}
		}
	} else {
		quat
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	fn v3(x: f32, y: f32, z: f32) -> Vec3 {
		Vec3 { x, y, z }
	}

	#[test]
	fn weapon_table() {
		assert_eq!(weapon_slot(0), 0);
		assert_eq!(weapon_slot(19), INVALID_WEAPON_SLOT);
		assert_eq!(weapon_slot(31), 5);
		assert_eq!(weapon_slot(46), 11);
		assert_eq!(weapon_slot(47), INVALID_WEAPON_SLOT);
		assert!(weapon_shootable(24) && weapon_shootable(38));
		assert!(!weapon_shootable(35) && !weapon_shootable(0) && !weapon_shootable(200));
	}

	#[test]
	fn additional_keys() {
		assert_eq!(keys_with_additional_key(5, 0), 5);
		assert_eq!(keys_with_additional_key(5, 1), 5 | 65536);
		assert_eq!(keys_with_additional_key(0, 3), 262144);
	}

	#[test]
	fn broadcast_keys() {
		assert_eq!(foot_keys_for_broadcast(0xFFFF, 44), !4);
		assert_eq!(foot_keys_for_broadcast(0xFFFF, 40), !128);
		assert_eq!(foot_keys_for_broadcast(0xFFFF, 31), 0xFFFF);
	}

	#[test]
	fn surfing_and_front_vector() {
		assert!(vehicle_surfing_offset_ok(v3(49.9, 0.0, 0.0)));
		assert!(!vehicle_surfing_offset_ok(v3(50.0, 0.0, 0.0)));
		assert!(vehicle_surfing_offset_ok(v3(f32::NAN, 0.0, 0.0)));
		assert!(aim_front_vector_ok(v3(1.0, 0.0, 0.0)));
		assert!(!aim_front_vector_ok(v3(0.0, 0.0, 0.0)));
		assert!(!aim_front_vector_ok(v3(f32::NAN, 0.0, 0.0)));
	}

	#[test]
	fn cam_modes() {
		assert_eq!(aim_cam_mode(4), 4);
		assert_eq!(aim_cam_mode(53), 53);
		assert_eq!(aim_cam_mode(64), 64);
		assert_eq!(aim_cam_mode(65), 4);
		assert_eq!(aim_cam_mode(0), 4);
	}

	#[test]
	fn bullet_bounds() {
		assert!(bullet_in_bounds(0, v3(20000.0, 0.0, 0.0), v3(0.0, 0.0, 0.0)));
		assert!(!bullet_in_bounds(0, v3(20000.1, 0.0, 0.0), v3(0.0, 0.0, 0.0)));
		assert!(bullet_in_bounds(1, v3(0.0, 0.0, 0.0), v3(1000.0, 0.0, 0.0)));
		assert!(!bullet_in_bounds(1, v3(0.0, 0.0, 0.0), v3(1000.1, 0.0, 0.0)));
		assert!(bullet_in_bounds(0, v3(0.0, 0.0, 0.0), v3(1000.1, 0.0, 0.0)));
	}

	#[test]
	fn unoccupied_motion() {
		let x = v3(1.0, 0.0, 0.0);
		let y = v3(0.0, 1.0, 0.0);
		assert!(unoccupied_motion_ok(v3(0.0, 0.0, 0.0), x, y));
		assert!(!unoccupied_motion_ok(v3(1.5, 0.0, 0.0), x, y));
		assert!(!unoccupied_motion_ok(v3(0.0, 0.0, 0.0), x, x));
		assert!(!unoccupied_motion_ok(v3(0.0, 0.0, 0.0), v3(2.0, 0.0, 0.0), y));
	}

	#[test]
	fn trailer_quat() {
		let q = Vec4 {
			x: 2.0,
			y: 0.0,
			z: 0.0,
			w: 0.0,
		};
		assert_eq!(trailer_quat_normalised(q).x, 1.0);
		let tiny = Vec4 {
			x: 0.01,
			y: 0.0,
			z: 0.0,
			w: 0.0,
		};
		assert_eq!(trailer_quat_normalised(tiny).w, 0.5);
	}
}
