/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Decoders for RPCs and packets sent by clients that components (rather than `PlayerPool`)
//! handle, ported from the `read` methods in `Shared/NetCode`.
//!
//! As in `rpcs`, inputs are accepted exactly when the C++ accepts them, except where the C++
//! returns true after an earlier read ran out of data and left a field uninitialised
//! (`OnPlayerEnterVehicle`, `SetVehicleDamageStatus`, `SCMEvent` and `OnPlayerDialogResponse`
//! can); those are rejected.

use crate::bitstream::BitReader;
use crate::rpcs::{GiveTakeDamage, Message};
use omp_common::Vec3;

/// `OnPlayerDamageActor`: laid out like `OnPlayerGiveTakeDamage`, with an unused first bit where
/// that has `Taking`, and the actor ID in place of the player ID.
pub type DamageActor = GiveTakeDamage;

/// `PlayerRequestClass`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct RequestClass {
	pub class_id: i32,
}

impl RequestClass {
	pub const RPC_ID: u8 = 128;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self {
			class_id: r.read_u16()?.into(),
		})
	}
}

/// `OnPlayerDialogResponse`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct DialogResponse {
	pub id: i32,
	pub response: u8,
	pub list_item: i32,
	pub text: Message,
}

impl DialogResponse {
	pub const RPC_ID: u8 = 62;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let id = r.read_u16();
		let response = r.read_u8();
		// With 8 to 15 bits left here, the C++ fails this read but goes on to read an empty text.
		let list_item = r.read_u16();
		let len = r.read_u8()?;
		let text = Message::read_bytes(r, len.into())?;
		Some(Self {
			id: (id? as i16).into(),
			response: response?,
			list_item: (list_item? as i16).into(),
			text,
		})
	}
}

/// `OnPlayerSelectedMenuRow`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct SelectedMenuRow {
	pub row: u8,
}

impl SelectedMenuRow {
	pub const RPC_ID: u8 = 132;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self { row: r.read_u8()? })
	}
}

/// `OnPlayerSelectObject`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct SelectObject {
	pub select_type: i32,
	pub object_id: i32,
	pub model: i32,
	pub position: Vec3,
}

impl SelectObject {
	pub const RPC_ID: u8 = 27;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let select_type = r.read_u32();
		let object_id = r.read_u16();
		let model = r.read_u32();
		let position = r.read_vec3().ok()?;
		Some(Self {
			select_type: select_type? as i32,
			object_id: object_id?.into(),
			model: model? as i32,
			position,
		})
	}
}

/// `OnPlayerEditObject`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct EditObject {
	pub player_object: bool,
	pub object_id: i32,
	pub response: i32,
	pub offset: Vec3,
	pub rotation: Vec3,
}

impl EditObject {
	pub const RPC_ID: u8 = 117;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let player_object = r.read_bit();
		let object_id = r.read_u16();
		let response = r.read_u32();
		let offset = r.read_vec3().ok()?;
		let rotation = r.read_vec3().ok()?;
		Some(Self {
			player_object: player_object?,
			object_id: object_id?.into(),
			response: response? as i32,
			offset,
			rotation,
		})
	}
}

/// `OnPlayerEditAttachedObject`. Colours are as sent, ARGB.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct EditAttachedObject {
	pub response: u32,
	pub index: u32,
	pub model: i32,
	pub bone: i32,
	pub offset: Vec3,
	pub rotation: Vec3,
	pub scale: Vec3,
	pub colour1: u32,
	pub colour2: u32,
}

impl EditAttachedObject {
	pub const RPC_ID: u8 = 116;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let response = r.read_u32();
		let index = r.read_u32();
		let model = r.read_u32();
		let bone = r.read_u32();
		let offset = r.read_vec3().ok()?;
		let rotation = r.read_vec3().ok()?;
		let scale = r.read_vec3().ok()?;
		let colour1 = r.read_u32();
		let colour2 = r.read_u32()?;
		Some(Self {
			response: response?,
			index: index?,
			model: model? as i32,
			bone: bone? as i32,
			offset,
			rotation,
			scale,
			colour1: colour1?,
			colour2,
		})
	}
}

/// `OnPlayerPickUpPickup`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct PickUpPickup {
	pub pickup_id: i32,
}

impl PickUpPickup {
	pub const RPC_ID: u8 = 131;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self {
			pickup_id: r.read_i32()?,
		})
	}
}

/// `GLOBAL_TEXTDRAW_POOL_SIZE`: IDs from here up are player textdraws.
const GLOBAL_TEXTDRAW_POOL_SIZE: i32 = 2048;
/// `INVALID_TEXTDRAW`: sent when the player cancels selection.
const INVALID_TEXTDRAW: i32 = 0xFFFF;

/// `OnPlayerSelectTextDraw`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct SelectTextDraw {
	/// Meaningless (false) when `invalid`.
	pub player_textdraw: bool,
	pub invalid: bool,
	/// Within its pool: global or player.
	pub textdraw_id: i32,
}

impl SelectTextDraw {
	pub const RPC_ID: u8 = 83;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let id = i32::from(r.read_u16()?);
		let invalid = id == INVALID_TEXTDRAW;
		let player_textdraw = !invalid && id >= GLOBAL_TEXTDRAW_POOL_SIZE;
		Some(Self {
			player_textdraw,
			invalid,
			textdraw_id: if player_textdraw {
				id - GLOBAL_TEXTDRAW_POOL_SIZE
			} else {
				id
			},
		})
	}
}

/// `OnPlayerEnterVehicle`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct EnterVehicle {
	pub vehicle_id: i32,
	pub passenger: u8,
}

impl EnterVehicle {
	pub const RPC_ID: u8 = 26;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		// The C++ only checks the second read, which still succeeds with 8 to 15 bits left.
		let vehicle_id = r.read_u16();
		let passenger = r.read_u8()?;
		Some(Self {
			vehicle_id: vehicle_id?.into(),
			passenger,
		})
	}
}

/// A vehicle ID alone: `OnPlayerExitVehicle` and `VehicleDeath`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct VehicleId {
	pub vehicle_id: i32,
}

impl VehicleId {
	pub const EXIT_VEHICLE_RPC_ID: u8 = 154;
	pub const VEHICLE_DEATH_RPC_ID: u8 = 136;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self {
			vehicle_id: r.read_u16()?.into(),
		})
	}
}

/// `SetVehicleDamageStatus`, as sent by the driver.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct VehicleDamageStatus {
	pub vehicle_id: i32,
	pub door_status: u32,
	pub panel_status: u32,
	pub light_status: u8,
	pub tyre_status: u8,
}

impl VehicleDamageStatus {
	pub const RPC_ID: u8 = 106;

	/// The C++ only checks the last read; the earlier, longer ones can fail while it succeeds.
	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u16()?;
		let panel_status = r.read_u32()?;
		let door_status = r.read_u32()?;
		let light_status = r.read_u8()?;
		let tyre_status = r.read_u8()?;
		Some(Self {
			vehicle_id: vehicle_id.into(),
			door_status,
			panel_status,
			light_status,
			tyre_status,
		})
	}
}

/// `SCMEvent`, as sent by the client (without the player ID).
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct ScmEvent {
	pub vehicle_id: i32,
	pub arg1: u32,
	pub arg2: u32,
	pub event_type: u32,
}

impl ScmEvent {
	pub const RPC_ID: u8 = 96;

	/// The C++ ignores whether its reads succeed; this requires all of them to.
	pub fn read(r: &mut BitReader) -> Option<Self> {
		let vehicle_id = r.read_u32()?;
		let arg1 = r.read_u32()?;
		let arg2 = r.read_u32()?;
		let event_type = r.read_u32()?;
		Some(Self {
			vehicle_id: vehicle_id as i32,
			arg1,
			arg2,
			event_type,
		})
	}
}

/// `PlayerRconCommand` (a packet, ID 201): a command prefixed by its length in four bytes, as
/// long as the packet allows.
pub struct RconCommand {
	pub command: Vec<u8>,
}

impl RconCommand {
	pub const PACKET_ID: u8 = 201;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let len = r.read_u32()?;
		Some(Self {
			command: crate::rpcs::read_string_bytes(r, len)?,
		})
	}
}

const _: () = {
	assert!(size_of::<RequestClass>() == 4);
	assert!(size_of::<DialogResponse>() == 272);
	assert!(size_of::<SelectedMenuRow>() == 1);
	assert!(size_of::<SelectObject>() == 24);
	assert!(size_of::<EditObject>() == 36);
	assert!(size_of::<EditAttachedObject>() == 60);
	assert!(size_of::<PickUpPickup>() == 4);
	assert!(size_of::<SelectTextDraw>() == 8);
	assert!(size_of::<EnterVehicle>() == 8);
	assert!(size_of::<VehicleId>() == 4);
	assert!(size_of::<VehicleDamageStatus>() == 16);
	assert!(size_of::<ScmEvent>() == 16);
};

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn dialog_response_signs_and_text() {
		let data = [0xFF, 0xFF, 1, 0xFE, 0xFF, 2, b'o', b'k'];
		let r = DialogResponse::read(&mut BitReader::new(&data)).unwrap();
		assert_eq!((r.id, r.response, r.list_item), (-1, 1, -2));
		assert_eq!(r.text.as_bytes(), b"ok");
	}

	#[test]
	fn dialog_response_rejects_a_missing_list_item() {
		// ID, response, then one byte: the C++ reads it as an empty text after failing the list item.
		assert!(DialogResponse::read(&mut BitReader::new(&[1, 0, 1, 0])).is_none());
	}

	#[test]
	fn select_textdraw_pools() {
		let read = |id: u16| SelectTextDraw::read(&mut BitReader::new(&id.to_le_bytes())).unwrap();
		assert_eq!(
			read(5),
			SelectTextDraw {
				player_textdraw: false,
				invalid: false,
				textdraw_id: 5
			}
		);
		assert_eq!(
			read(2048 + 7),
			SelectTextDraw {
				player_textdraw: true,
				invalid: false,
				textdraw_id: 7
			}
		);
		assert!(read(0xFFFF).invalid);
	}

	#[test]
	fn damage_status_requires_every_field() {
		// Vehicle ID and the two light/tyre bytes, but no panel or door status.
		assert!(VehicleDamageStatus::read(&mut BitReader::new(&[1, 0, 2, 3])).is_none());
		let data = [1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 5];
		assert_eq!(
			VehicleDamageStatus::read(&mut BitReader::new(&data)),
			Some(VehicleDamageStatus {
				vehicle_id: 1,
				door_status: 3,
				panel_status: 2,
				light_status: 4,
				tyre_status: 5
			})
		);
	}

	#[test]
	fn rcon_command_length() {
		let mut data = 5u32.to_le_bytes().to_vec();
		data.extend_from_slice(b"login");
		assert_eq!(RconCommand::read(&mut BitReader::new(&data)).unwrap().command, b"login");
		data[0] = 6;
		assert!(RconCommand::read(&mut BitReader::new(&data)).is_none());
		// A length too big to multiply into bits on 32-bit targets.
		let data = u32::MAX.to_le_bytes();
		assert!(RconCommand::read(&mut BitReader::new(&data)).is_none());
	}
}
