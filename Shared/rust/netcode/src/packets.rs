/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Decoders for packets sent by clients, ported from the `read` methods in `Shared/NetCode`.
//!
//! Each decoder reads exactly what its C++ counterpart reads, in the same order, and accepts
//! exactly the same inputs; the differential tests in `Server/Tests` hold them to that.

use crate::bitstream::{BitReader, Quat};
use omp_common::{Vec2, Vec3};

const VEHICLE_POOL_SIZE: i32 = 2000;
const OBJECT_POOL_SIZE: i32 = 2000;

/// `PlayerSurfingData::Type`.
#[repr(i32)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum SurfingType {
	#[default]
	None = 0,
	Vehicle = 1,
	Object = 2,
	PlayerObject = 3,
}

/// `NetCode::Packet::PlayerFootSync`, minus `PlayerID` (which the server fills in).
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct FootSync {
	pub left_right: u16,
	pub up_down: u16,
	pub keys: u16,
	/// Low 6 bits: weapon; high 2 bits: additional key.
	pub weapon_additional_key: u8,
	pub special_action: u8,
	pub position: Vec3,
	pub rotation: Quat,
	pub health_armour: Vec2,
	pub velocity: Vec3,
	pub animation_id: u16,
	pub animation_flags: u16,
	pub surfing_type: SurfingType,
	pub surfing_id: i32,
	pub surfing_offset: Vec3,
}

impl FootSync {
	pub const PACKET_ID: u8 = 207;

	pub fn weapon(&self) -> u8 {
		self.weapon_additional_key & 0x3F
	}

	pub fn additional_key(&self) -> u8 {
		self.weapon_additional_key >> 6
	}

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let left_right = r.read_u16();
		let up_down = r.read_u16();
		let keys = r.read_u16();
		let position = r.read_pos_vec3().ok()?;
		let rotation = r.read_quat().ok()?;
		// Two separate byte reads, like `readCompressedPercentPair`: the first is consumed even if
		// the second fails.
		let health = r.read_u8();
		let armour = health.and_then(|_| r.read_u8());
		let weapon_additional_key = r.read_u8();
		let special_action = r.read_u8();
		let velocity = r.read_vel_vec3().ok()?;
		let surfing_offset = r.read_vec3().ok()?;
		let surfing_id = r.read_u16();
		let animation_id = r.read_u16();
		let animation_flags = r.read_u16()?;

		// The C++ only checks some reads, but once one runs out of data all later ones do too, so
		// the last succeeding means every read above did. Requiring them all is equivalent, and
		// can't panic on a hostile packet if that reasoning is ever wrong.
		let (left_right, up_down, keys, health, armour) = (left_right?, up_down?, keys?, health?, armour?);
		let (weapon_additional_key, special_action, surfing_id, animation_id) =
			(weapon_additional_key?, special_action?, surfing_id?, animation_id?);

		let (surfing_type, surfing_id) = classify_surfing(i32::from(surfing_id));
		Some(Self {
			left_right,
			up_down,
			keys,
			weapon_additional_key,
			special_action,
			position,
			rotation,
			health_armour: Vec2 {
				x: health.into(),
				y: armour.into(),
			},
			velocity,
			animation_id,
			animation_flags,
			surfing_type,
			surfing_id,
			surfing_offset,
		})
	}
}

/// `NetCode::Packet::PlayerAimSync`, minus `PlayerID`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct AimSync {
	pub cam_mode: u8,
	/// Low 6 bits: camera zoom; high 2 bits: weapon state.
	pub zoom_weapon_state: u8,
	pub aspect_ratio: u8,
	pub cam_front_vector: Vec3,
	pub cam_pos: Vec3,
	pub aim_z: f32,
}

impl AimSync {
	pub const PACKET_ID: u8 = 203;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let cam_mode = r.read_u8();
		let cam_front_vector = r.read_vec3().ok()?;
		let cam_pos = r.read_pos_vec3().ok()?;
		let aim_z = r.read_f32().ok()?;
		let zoom_weapon_state = r.read_u8();
		let aspect_ratio = r.read_u8()?;
		Some(Self {
			cam_mode: cam_mode?,
			zoom_weapon_state: zoom_weapon_state?,
			aspect_ratio,
			cam_front_vector,
			cam_pos,
			aim_z,
		})
	}
}

/// `NetCode::Packet::PlayerBulletSync`, minus `PlayerID`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct BulletSync {
	pub hit_type: u8,
	pub weapon_id: u8,
	pub hit_id: u16,
	pub origin: Vec3,
	pub hit_pos: Vec3,
	pub offset: Vec3,
}

impl BulletSync {
	pub const PACKET_ID: u8 = 206;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let hit_type = r.read_u8();
		let hit_id = r.read_u16();
		let origin = r.read_vec3().ok()?;
		let hit_pos = r.read_vec3().ok()?;
		let offset = r.read_vec3().ok()?;
		let weapon_id = r.read_u8()?;
		Some(Self {
			hit_type: hit_type?,
			weapon_id,
			hit_id: hit_id?,
			origin,
			hit_pos,
			offset,
		})
	}
}

/// `NetCode::Packet::PlayerStatsSync`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct StatsSync {
	pub money: i32,
	pub drunk_level: i32,
}

impl StatsSync {
	pub const PACKET_ID: u8 = 205;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let money = r.read_i32();
		let drunk_level = r.read_i32()?;
		Some(Self {
			money: money?,
			drunk_level,
		})
	}
}

pub const MAX_WEAPON_SLOTS: usize = 13;

/// One entry of `PlayerWeaponsUpdate::WeaponData`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct WeaponSlot {
	pub slot: u8,
	pub id: u8,
	pub ammo: u32,
}

/// `NetCode::Packet::PlayerWeaponsUpdate`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct WeaponsUpdate {
	pub target_player: i32,
	pub target_actor: i32,
	pub weapon_count: i32,
	pub weapons: [WeaponSlot; MAX_WEAPON_SLOTS],
}

impl WeaponsUpdate {
	pub const PACKET_ID: u8 = 204;

	/// Unlike the other decoders, the result hinges on the target actor read, not the last one:
	/// the slot list that follows is optional and runs to the end of the packet.
	pub fn read(r: &mut BitReader) -> Option<Self> {
		let target_player = r.read_u16();
		let target_actor = r.read_u16();
		let mut update = Self::default();
		while (update.weapon_count as usize) < MAX_WEAPON_SLOTS {
			let Some(slot) = r.read_u8() else {
				break;
			};
			if usize::from(slot) >= MAX_WEAPON_SLOTS {
				return None;
			}
			let id = r.read_u8()?;
			let ammo = r.read_u16()?;
			update.weapons[update.weapon_count as usize] = WeaponSlot {
				slot,
				id,
				ammo: ammo.into(),
			};
			update.weapon_count += 1;
		}
		// A failed target actor read means there was too little data for the loop to have read
		// anything either, so the target player read failed too.
		update.target_actor = target_actor?.into();
		update.target_player = target_player?.into();
		Some(update)
	}
}

/// `NetCode::Packet::PlayerSpectatorSync`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct SpectatorSync {
	pub left_right: u16,
	pub up_down: u16,
	pub keys: u16,
	pub position: Vec3,
}

impl SpectatorSync {
	pub const PACKET_ID: u8 = 212;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let left_right = r.read_u16();
		let up_down = r.read_u16();
		let keys = r.read_u16();
		let position = r.read_pos_vec3().ok()?;
		Some(Self {
			left_right: left_right?,
			up_down: up_down?,
			keys: keys?,
			position,
		})
	}
}

/// `NetCode::Packet::PlayerVehicleSync` (`VehicleDriverSyncPacket`), minus `PlayerID` and
/// `HasTrailer`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct VehicleSync {
	pub vehicle_id: u16,
	pub left_right: u16,
	pub up_down: u16,
	pub keys: u16,
	pub rotation: Quat,
	pub position: Vec3,
	pub velocity: Vec3,
	pub health: f32,
	pub player_health_armour: Vec2,
	/// Low 6 bits: weapon; high 2 bits: additional key.
	pub additional_key_weapon: u8,
	pub siren: u8,
	pub landing_gear: u8,
	pub trailer_id: u16,
	/// Also the train speed, as a float.
	pub hydra_thrust_angle: u32,
}

impl VehicleSync {
	pub const PACKET_ID: u8 = 200;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u16();
		let left_right = r.read_u16();
		let up_down = r.read_u16();
		let keys = r.read_u16();
		let rotation = r.read_quat().ok()?;
		let position = r.read_pos_vec3().ok()?;
		let velocity = r.read_vel_vec3().ok()?;
		let health = r.read_f32().ok()?;
		let player_health = r.read_u8();
		let player_armour = player_health.and_then(|_| r.read_u8());
		let additional_key_weapon = r.read_u8();
		let siren = r.read_u8();
		let landing_gear = r.read_u8();
		let trailer_id = r.read_u16();
		let hydra_thrust_angle = r.read_u32()?;
		Some(Self {
			vehicle_id: vehicle_id?,
			left_right: left_right?,
			up_down: up_down?,
			keys: keys?,
			rotation,
			position,
			velocity,
			health,
			player_health_armour: Vec2 {
				x: player_health?.into(),
				y: player_armour?.into(),
			},
			additional_key_weapon: additional_key_weapon?,
			siren: siren?,
			landing_gear: landing_gear?,
			trailer_id: trailer_id?,
			hydra_thrust_angle,
		})
	}
}

/// `NetCode::Packet::PlayerPassengerSync` (`VehiclePassengerSyncPacket`), minus `PlayerID`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct PassengerSync {
	pub vehicle_id: i32,
	/// Bits 0-5: seat; 6: drive-by; 7: cuffed; 8-13: weapon; 14-15: additional key.
	pub drive_by_seat_additional_key_weapon: u16,
	pub keys: u16,
	pub health_armour: Vec2,
	pub left_right: u16,
	pub up_down: u16,
	pub position: Vec3,
}

impl PassengerSync {
	pub const PACKET_ID: u8 = 211;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u16();
		let drive_by_seat_additional_key_weapon = r.read_u16();
		let health = r.read_u8();
		let armour = health.and_then(|_| r.read_u8());
		let left_right = r.read_u16();
		let up_down = r.read_u16();
		let keys = r.read_u16();
		let position = r.read_pos_vec3().ok()?;
		Some(Self {
			vehicle_id: vehicle_id?.into(),
			drive_by_seat_additional_key_weapon: drive_by_seat_additional_key_weapon?,
			keys: keys?,
			health_armour: Vec2 {
				x: health?.into(),
				y: armour?.into(),
			},
			left_right: left_right?,
			up_down: up_down?,
			position,
		})
	}
}

/// `NetCode::Packet::PlayerUnoccupiedSync` (`VehicleUnoccupiedSyncPacket`), minus `PlayerID`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct UnoccupiedSync {
	pub vehicle_id: i32,
	pub seat_id: u8,
	pub roll: Vec3,
	pub rotation: Vec3,
	pub position: Vec3,
	pub velocity: Vec3,
	pub angular_velocity: Vec3,
	pub health: f32,
}

impl UnoccupiedSync {
	pub const PACKET_ID: u8 = 209;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u16();
		let seat_id = r.read_u8();
		let roll = r.read_vec3().ok()?;
		let rotation = r.read_vec3().ok()?;
		let position = r.read_pos_vec3().ok()?;
		let velocity = r.read_vel_vec3().ok()?;
		let angular_velocity = r.read_vec3().ok()?;
		let health = r.read_f32().ok()?;
		Some(Self {
			vehicle_id: vehicle_id?.into(),
			seat_id: seat_id?,
			roll,
			rotation,
			position,
			velocity,
			angular_velocity,
			health,
		})
	}
}

/// `glm::vec4`, in x, y, z, w order.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Vec4 {
	pub x: f32,
	pub y: f32,
	pub z: f32,
	pub w: f32,
}

/// `NetCode::Packet::PlayerTrailerSync` (`VehicleTrailerSyncPacket`), minus `PlayerID`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct TrailerSync {
	pub vehicle_id: i32,
	pub position: Vec3,
	pub quat: Vec4,
	pub velocity: Vec3,
	pub turn_velocity: Vec3,
}

impl TrailerSync {
	pub const PACKET_ID: u8 = 210;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u16();
		let position = r.read_pos_vec3().ok()?;
		let quat = r.read_vec4().ok()?;
		let velocity = r.read_vel_vec3().ok()?;
		let turn_velocity = r.read_vec3().ok()?;
		Some(Self {
			vehicle_id: vehicle_id?.into(),
			position,
			quat,
			velocity,
			turn_velocity,
		})
	}
}

/// Surfing IDs below the vehicle pool size are vehicles; the next range is objects.
fn classify_surfing(id: i32) -> (SurfingType, i32) {
	if id < 1 {
		(SurfingType::None, id)
	} else if id < VEHICLE_POOL_SIZE {
		(SurfingType::Vehicle, id)
	} else if id < VEHICLE_POOL_SIZE + OBJECT_POOL_SIZE {
		(SurfingType::Object, id - VEHICLE_POOL_SIZE)
	} else {
		(SurfingType::None, id)
	}
}

// These structs cross the C ABI; `netcode_rs.hpp` asserts the same sizes.
const _: () = {
	use std::mem::size_of;
	assert!(size_of::<FootSync>() == 80);
	assert!(size_of::<AimSync>() == 32);
	assert!(size_of::<BulletSync>() == 40);
	assert!(size_of::<StatsSync>() == 8);
	assert!(size_of::<WeaponsUpdate>() == 116);
	assert!(size_of::<SpectatorSync>() == 20);
	assert!(size_of::<VehicleSync>() == 72);
	assert!(size_of::<PassengerSync>() == 32);
	assert!(size_of::<UnoccupiedSync>() == 72);
	assert!(size_of::<TrailerSync>() == 56);
};

#[cfg(test)]
mod tests {
	use super::*;

	struct Writer(Vec<u8>);

	impl Writer {
		fn u8(mut self, v: u8) -> Self {
			self.0.push(v);
			self
		}
		fn u16(mut self, v: u16) -> Self {
			self.0.extend(v.to_le_bytes());
			self
		}
		fn f32s(mut self, vs: &[f32]) -> Self {
			for v in vs {
				self.0.extend(v.to_le_bytes());
			}
			self
		}
	}

	fn foot_sync(surfing_id: u16) -> Vec<u8> {
		Writer(Vec::new())
			.u16(1)
			.u16(2)
			.u16(3)
			.f32s(&[10.0, 20.0, 30.0])
			.f32s(&[1.0, 0.0, 0.0, 0.0])
			.u8(100)
			.u8(50)
			.u8(0b01_000101)
			.u8(7)
			.f32s(&[0.5, 0.0, 0.0])
			.f32s(&[0.0, 0.0, 1.0])
			.u16(surfing_id)
			.u16(1189)
			.u16(4356)
			.0
	}

	#[test]
	fn decodes_foot_sync() {
		let data = foot_sync(0);
		let sync = FootSync::read(&mut BitReader::new(&data)).unwrap();
		assert_eq!((sync.left_right, sync.up_down, sync.keys), (1, 2, 3));
		assert_eq!(
			sync.position,
			Vec3 {
				x: 10.0,
				y: 20.0,
				z: 30.0
			}
		);
		assert_eq!(sync.rotation.w, 1.0);
		assert_eq!(sync.health_armour, Vec2 { x: 100.0, y: 50.0 });
		assert_eq!((sync.weapon(), sync.additional_key()), (5, 1));
		assert_eq!(sync.special_action, 7);
		assert_eq!((sync.animation_id, sync.animation_flags), (1189, 4356));
		assert_eq!(sync.surfing_type, SurfingType::None);
	}

	#[test]
	fn classifies_surfing() {
		let surf = |id| {
			let s = FootSync::read(&mut BitReader::new(&foot_sync(id))).unwrap();
			(s.surfing_type, s.surfing_id)
		};
		assert_eq!(surf(1), (SurfingType::Vehicle, 1));
		assert_eq!(surf(1999), (SurfingType::Vehicle, 1999));
		assert_eq!(surf(2000), (SurfingType::Object, 0));
		assert_eq!(surf(3999), (SurfingType::Object, 1999));
		assert_eq!(surf(4000), (SurfingType::None, 4000));
	}

	#[test]
	fn rejects_truncated_or_invalid() {
		let data = foot_sync(0);
		for len in 0..data.len() {
			assert!(
				FootSync::read(&mut BitReader::new(&data[..len])).is_none(),
				"accepted {len} bytes"
			);
		}
		let mut bad = data.clone();
		bad[6..10].copy_from_slice(&f32::NAN.to_le_bytes());
		assert!(FootSync::read(&mut BitReader::new(&bad)).is_none());
	}
}
