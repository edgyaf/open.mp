/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI. Mirrored by `../../../NetCode/netcode_rs.hpp`; keep the two in sync.
//!
//! Decoders take a `NetworkBitStream`'s buffer, its size in bits and the current read offset in
//! bits. On success they fill `out`, advance `*offset` past what they read and return true. On
//! failure `out` and `*offset` are left alone.

use crate::{BitReader, FootSync};

/// # Safety
/// `data` must point to `bits.div_ceil(8)` readable bytes (or be anything if `bits` is 0).
unsafe fn reader<'a>(data: *const u8, bits: usize, offset: usize) -> Option<BitReader<'a>> {
	let bytes = if bits == 0 {
		&[][..]
	} else {
		unsafe { std::slice::from_raw_parts(data, bits.div_ceil(8)) }
	};
	let mut reader = BitReader::with_bits(bytes, bits);
	reader.skip(offset)?;
	Some(reader)
}

/// # Safety
/// See the module docs; `offset` and `out` must be valid for reads and writes.
unsafe fn decode<T>(
	data: *const u8,
	bits: usize,
	offset: *mut usize,
	out: *mut T,
	read: impl FnOnce(&mut BitReader) -> Option<T>,
) -> bool {
	let Some(mut reader) = (unsafe { reader(data, bits, *offset) }) else {
		return false;
	};
	match read(&mut reader) {
		Some(value) => {
			unsafe {
				*out = value;
				*offset = bits - reader.unread_bits();
			}
			true
		}
		None => false,
	}
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_netcode_read_foot_sync(
	data: *const u8,
	bits: usize,
	offset: *mut usize,
	out: *mut FootSync,
) -> bool {
	unsafe { decode(data, bits, offset, out, FootSync::read) }
}

macro_rules! decoders {
	($($name:ident => $packet:ty;)*) => {
		$(
			#[unsafe(no_mangle)]
			pub unsafe extern "C" fn $name(data: *const u8, bits: usize, offset: *mut usize, out: *mut $packet) -> bool {
				unsafe { decode(data, bits, offset, out, <$packet>::read) }
			}
		)*
	};
}

decoders! {
	omp_netcode_read_aim_sync => crate::packets::AimSync;
	omp_netcode_read_bullet_sync => crate::packets::BulletSync;
	omp_netcode_read_stats_sync => crate::packets::StatsSync;
	omp_netcode_read_weapons_update => crate::packets::WeaponsUpdate;
	omp_netcode_read_spectator_sync => crate::packets::SpectatorSync;
	omp_netcode_read_vehicle_sync => crate::packets::VehicleSync;
	omp_netcode_read_passenger_sync => crate::packets::PassengerSync;
	omp_netcode_read_unoccupied_sync => crate::packets::UnoccupiedSync;
	omp_netcode_read_trailer_sync => crate::packets::TrailerSync;

	omp_netcode_read_click_map => crate::rpcs::ClickMap;
	omp_netcode_read_click_player => crate::rpcs::ClickPlayer;
	omp_netcode_read_give_take_damage => crate::rpcs::GiveTakeDamage;
	omp_netcode_read_interior_change => crate::rpcs::InteriorChange;
	omp_netcode_read_death => crate::rpcs::Death;
	omp_netcode_read_camera_target => crate::rpcs::CameraTarget;
	omp_netcode_read_client_check_response => crate::rpcs::ClientCheckResponse;
	omp_netcode_read_chat_message => crate::rpcs::ChatMessage;
	omp_netcode_read_command_message => crate::rpcs::CommandMessage;

	omp_netcode_read_damage_actor => crate::component_rpcs::DamageActor;
	omp_netcode_read_request_class => crate::component_rpcs::RequestClass;
	omp_netcode_read_dialog_response => crate::component_rpcs::DialogResponse;
	omp_netcode_read_selected_menu_row => crate::component_rpcs::SelectedMenuRow;
	omp_netcode_read_select_object => crate::component_rpcs::SelectObject;
	omp_netcode_read_edit_object => crate::component_rpcs::EditObject;
	omp_netcode_read_edit_attached_object => crate::component_rpcs::EditAttachedObject;
	omp_netcode_read_pick_up_pickup => crate::component_rpcs::PickUpPickup;
	omp_netcode_read_select_textdraw => crate::component_rpcs::SelectTextDraw;
	omp_netcode_read_enter_vehicle => crate::component_rpcs::EnterVehicle;
	omp_netcode_read_vehicle_id => crate::component_rpcs::VehicleId;
	omp_netcode_read_vehicle_damage_status => crate::component_rpcs::VehicleDamageStatus;
	omp_netcode_read_scm_event => crate::component_rpcs::ScmEvent;
}

/// Gives a decoder somewhere to put a string of `len` bytes: returns a buffer of at least that
/// many, or null to fail the decode.
pub type ReserveString = unsafe extern "C" fn(context: *mut std::ffi::c_void, len: usize) -> *mut u8;

/// `PlayerRconCommand`. On success the command has been written to the buffer `reserve` returned.
///
/// # Safety
/// As for the other decoders; `reserve` must return null or a buffer of at least `len` writable
/// bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_netcode_read_rcon_command(
	data: *const u8,
	bits: usize,
	offset: *mut usize,
	context: *mut std::ffi::c_void,
	reserve: ReserveString,
) -> bool {
	let Some(mut reader) = (unsafe { reader(data, bits, *offset) }) else {
		return false;
	};
	let Some(command) = crate::component_rpcs::RconCommand::read(&mut reader) else {
		return false;
	};
	let buffer = unsafe { reserve(context, command.command.len()) };
	if buffer.is_null() {
		return false;
	}
	unsafe {
		std::ptr::copy_nonoverlapping(command.command.as_ptr(), buffer, command.command.len());
		*offset = bits - reader.unread_bits();
	}
	true
}

// Checks on decoded sync data; see `checks.rs`.

use crate::checks;
use crate::packets::Vec4;
use omp_common::Vec3;

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_weapon_slot(weapon: u8) -> i8 {
	checks::weapon_slot(weapon)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_weapon_shootable(weapon: u8) -> bool {
	checks::weapon_shootable(weapon)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_keys_with_additional_key(keys: u16, additional_key: u8) -> u32 {
	checks::keys_with_additional_key(keys, additional_key)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_vehicle_surfing_offset_ok(offset: Vec3) -> bool {
	checks::vehicle_surfing_offset_ok(offset)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_foot_keys_for_broadcast(keys: u16, armed_weapon: u32) -> u16 {
	checks::foot_keys_for_broadcast(keys, armed_weapon)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_aim_front_vector_ok(front: Vec3) -> bool {
	checks::aim_front_vector_ok(front)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_aim_cam_mode(mode: u8) -> u8 {
	checks::aim_cam_mode(mode)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_aim_cam_zoom(zoom: u8) -> f32 {
	checks::aim_cam_zoom(zoom)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_aim_aspect_ratio(aspect_ratio: u8) -> f32 {
	checks::aim_aspect_ratio(aspect_ratio)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_bullet_in_bounds(hit_type: u8, origin: Vec3, offset: Vec3) -> bool {
	checks::bullet_in_bounds(hit_type, origin, offset)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_unoccupied_motion_ok(angular_velocity: Vec3, roll: Vec3, rotation: Vec3) -> bool {
	checks::unoccupied_motion_ok(angular_velocity, roll, rotation)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_trailer_turn_velocity_ok(turn_velocity: Vec3) -> bool {
	checks::trailer_turn_velocity_ok(turn_velocity)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_netcode_trailer_quat_normalised(quat: Vec4) -> Vec4 {
	checks::trailer_quat_normalised(quat)
}
