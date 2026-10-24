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
}
