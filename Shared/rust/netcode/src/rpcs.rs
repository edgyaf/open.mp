/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Decoders for RPCs sent by clients, ported from the `read` methods in `Shared/NetCode`.
//!
//! Like the packet decoders, each reads what its C++ counterpart reads and accepts the same
//! inputs, with one deliberate difference: where the C++ returns true after an earlier read ran
//! out of data, leaving a field uninitialised (a truncated `OnPlayerClickPlayer` or
//! `ClientCheck`), these reject the RPC instead of passing garbage on.

use crate::bitstream::BitReader;
use omp_common::Vec3;

/// `OnPlayerClickMap`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct ClickMap {
	pub pos: Vec3,
}

impl ClickMap {
	pub const RPC_ID: u8 = 119;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self {
			pos: r.read_pos_vec3().ok()?,
		})
	}
}

/// `OnPlayerClickPlayer`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct ClickPlayer {
	pub player_id: i32,
	pub source: u8,
}

impl ClickPlayer {
	pub const RPC_ID: u8 = 23;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		// The C++ only checks the source read, which still succeeds with 8 to 15 bits left after a
		// failed player ID read.
		let player_id = r.read_u16();
		let source = r.read_u8()?;
		Some(Self {
			player_id: player_id?.into(),
			source,
		})
	}
}

/// `OnPlayerGiveTakeDamage`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct GiveTakeDamage {
	pub taking: bool,
	pub player_id: i32,
	pub damage: f32,
	pub weapon_id: u32,
	pub bodypart: u32,
}

impl GiveTakeDamage {
	pub const RPC_ID: u8 = 115;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let taking = r.read_bit();
		let player_id = r.read_u16();
		let damage = r.read_f32().ok()?;
		let weapon_id = r.read_u32();
		let bodypart = r.read_u32()?;
		// Each earlier read needs no more data than a later checked one, so these succeeded.
		Some(Self {
			taking: taking?,
			player_id: player_id?.into(),
			damage,
			weapon_id: weapon_id?,
			bodypart,
		})
	}
}

/// `OnPlayerInteriorChange`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct InteriorChange {
	pub interior: u32,
}

impl InteriorChange {
	pub const RPC_ID: u8 = 118;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		Some(Self {
			interior: r.read_u8()?.into(),
		})
	}
}

/// `OnPlayerDeath`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Death {
	pub reason: u8,
	pub killer_id: u16,
}

impl Death {
	pub const RPC_ID: u8 = 53;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let reason = r.read_u8();
		let killer_id = r.read_u16()?;
		Some(Self {
			reason: reason?,
			killer_id,
		})
	}
}

/// `OnPlayerCameraTarget`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct CameraTarget {
	pub object_id: i32,
	pub vehicle_id: i32,
	pub player_id: i32,
	pub actor_id: i32,
}

impl CameraTarget {
	pub const RPC_ID: u8 = 168;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let object_id = r.read_u16();
		let vehicle_id = r.read_u16();
		let player_id = r.read_u16();
		let actor_id = r.read_u16()?;
		Some(Self {
			object_id: object_id?.into(),
			vehicle_id: vehicle_id?.into(),
			player_id: player_id?.into(),
			actor_id: actor_id.into(),
		})
	}
}

/// `ClientCheck`, as the client sends it back.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct ClientCheckResponse {
	pub kind: i32,
	pub address: i32,
	pub results: i32,
}

impl ClientCheckResponse {
	pub const RPC_ID: u8 = 103;

	/// The C++ ignores whether its reads succeed; this requires all of them to.
	pub fn read(r: &mut BitReader) -> Option<Self> {
		let kind = r.read_u8()?;
		let address = r.read_u32()?;
		let results = r.read_u8()?;
		Some(Self {
			kind: kind.into(),
			address: address as i32,
			results: results.into(),
		})
	}
}

/// The longest message a client can send: a command (`PlayerRequestCommandMessage`).
pub const MAX_MESSAGE_LEN: usize = 256;

/// A string sent by the client: raw bytes, which may include NULs.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Message {
	pub len: u32,
	pub data: [u8; MAX_MESSAGE_LEN],
}

impl Default for Message {
	fn default() -> Self {
		Self {
			len: 0,
			data: [0; MAX_MESSAGE_LEN],
		}
	}
}

impl Message {
	pub fn as_bytes(&self) -> &[u8] {
		&self.data[..self.len as usize]
	}

	/// `NetworkBitStream::Read(char*, len)`, which accepts an empty read only on a byte boundary
	/// (otherwise it goes through `ReadBits`, which rejects reading nothing).
	fn read_bytes(r: &mut BitReader, len: usize) -> Option<Self> {
		if len == 0 && !r.is_byte_aligned() {
			return None;
		}
		if r.unread_bits() < len * 8 {
			return None;
		}
		let mut message = Self {
			len: len as u32,
			..Self::default()
		};
		for byte in message.data.get_mut(..len)? {
			*byte = r.read_u8()?;
		}
		Some(message)
	}
}

/// `PlayerRequestChatMessage`: a message prefixed by its length in a byte.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct ChatMessage {
	pub message: Message,
}

impl ChatMessage {
	pub const RPC_ID: u8 = 101;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let len = r.read_u8()?;
		Some(Self {
			message: Message::read_bytes(r, len.into())?,
		})
	}
}

/// `PlayerRequestCommandMessage`: a message prefixed by its length in four bytes.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct CommandMessage {
	pub message: Message,
}

impl CommandMessage {
	pub const RPC_ID: u8 = 50;

	pub fn read(r: &mut BitReader) -> Option<Self> {
		let len = r.read_u32()? as usize;
		// At least "/" and one more character, and at most 256.
		if !(2..=MAX_MESSAGE_LEN).contains(&len) {
			return None;
		}
		Some(Self {
			message: Message::read_bytes(r, len)?,
		})
	}
}

const _: () = {
	assert!(size_of::<ClickMap>() == 12);
	assert!(size_of::<ClickPlayer>() == 8);
	assert!(size_of::<GiveTakeDamage>() == 20);
	assert!(size_of::<InteriorChange>() == 4);
	assert!(size_of::<Death>() == 4);
	assert!(size_of::<CameraTarget>() == 16);
	assert!(size_of::<ClientCheckResponse>() == 12);
	assert!(size_of::<ChatMessage>() == 260);
	assert!(size_of::<CommandMessage>() == 260);
};

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn click_player_rejects_a_missing_player_id() {
		// 12 bits: enough for the source byte but not the player ID.
		let mut r = BitReader::with_bits(&[0xAB, 0xC0], 12);
		assert_eq!(ClickPlayer::read(&mut r), None);
		let mut r = BitReader::new(&[0x34, 0x12, 0x02]);
		assert_eq!(
			ClickPlayer::read(&mut r),
			Some(ClickPlayer {
				player_id: 0x1234,
				source: 2
			})
		);
	}

	#[test]
	fn client_check_requires_every_field() {
		assert_eq!(ClientCheckResponse::read(&mut BitReader::new(&[1, 0, 0, 0, 0])), None);
		let r = ClientCheckResponse::read(&mut BitReader::new(&[0x48, 0x78, 0x56, 0x34, 0x12, 7])).unwrap();
		assert_eq!(
			r,
			ClientCheckResponse {
				kind: 0x48,
				address: 0x12345678,
				results: 7
			}
		);
	}

	#[test]
	fn chat_message_reads_its_length_prefix() {
		let mut r = BitReader::new(&[3, b'h', 0, b'i', b'!']);
		let chat = ChatMessage::read(&mut r).unwrap();
		assert_eq!(chat.message.as_bytes(), b"h\0i");
		assert_eq!(r.unread_bits(), 8);
		// Too long for what's left.
		assert_eq!(ChatMessage::read(&mut BitReader::new(&[3, b'h', b'i'])), None);
	}

	#[test]
	fn empty_chat_message_needs_a_byte_boundary() {
		assert!(ChatMessage::read(&mut BitReader::new(&[0])).is_some());
		let mut r = BitReader::new(&[0, 0]);
		r.skip(1);
		assert_eq!(ChatMessage::read(&mut r), None);
	}

	#[test]
	fn command_message_length_limits() {
		let mut data = vec![1, 0, 0, 0, b'/'];
		assert_eq!(CommandMessage::read(&mut BitReader::new(&data)), None);
		data[0] = 2;
		data.push(b'a');
		assert_eq!(
			CommandMessage::read(&mut BitReader::new(&data))
				.unwrap()
				.message
				.as_bytes(),
			b"/a"
		);

		let mut data = vec![0u8; 4 + 257];
		data[..4].copy_from_slice(&256u32.to_le_bytes());
		assert_eq!(
			CommandMessage::read(&mut BitReader::new(&data)).unwrap().message.len,
			256
		);
		data[..4].copy_from_slice(&257u32.to_le_bytes());
		assert_eq!(CommandMessage::read(&mut BitReader::new(&data)), None);
	}
}
