/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Reading side of `NetworkBitStream` (RakNet's `BitStream`), bit for bit.
//!
//! Bits are read most significant first. Multi-byte values are read as raw bytes in native
//! (little-endian) order, assembled across byte boundaries when the read offset isn't aligned. A
//! read that runs past the end fails without consuming anything, like `ReadBits`.

use omp_common::{Vec2, Vec3};

pub struct BitReader<'a> {
	data: &'a [u8],
	/// In bits.
	offset: usize,
	/// In bits; may be less than `data.len() * 8`.
	len: usize,
}

impl<'a> BitReader<'a> {
	pub fn new(data: &'a [u8]) -> Self {
		Self::with_bits(data, data.len() * 8)
	}

	/// Read only the first `bits` bits of `data`.
	pub fn with_bits(data: &'a [u8], bits: usize) -> Self {
		Self {
			data,
			offset: 0,
			len: bits.min(data.len() * 8),
		}
	}

	pub fn unread_bits(&self) -> usize {
		self.len - self.offset
	}

	/// Skip `bits` bits, e.g. the packet ID. Fails if there aren't enough.
	pub fn skip(&mut self, bits: usize) -> Option<()> {
		if self.unread_bits() < bits {
			return None;
		}
		self.offset += bits;
		Some(())
	}

	pub fn read_bit(&mut self) -> Option<bool> {
		if self.unread_bits() == 0 {
			return None;
		}
		let bit = self.data[self.offset >> 3] & (0x80 >> (self.offset & 7)) != 0;
		self.offset += 1;
		Some(bit)
	}

	/// `ReadBits(output, N * 8)` into a byte array.
	pub fn read_bytes<const N: usize>(&mut self) -> Option<[u8; N]> {
		if self.unread_bits() < N * 8 {
			return None;
		}
		let shift = self.offset & 7;
		let start = self.offset >> 3;
		let mut out = [0u8; N];
		for (i, byte) in out.iter_mut().enumerate() {
			*byte = self.data[start + i] << shift;
			if shift > 0 {
				*byte |= self.data[start + i + 1] >> (8 - shift);
			}
		}
		self.offset += N * 8;
		Some(out)
	}

	pub fn read_u8(&mut self) -> Option<u8> {
		self.read_bytes::<1>().map(|[b]| b)
	}

	pub fn read_u16(&mut self) -> Option<u16> {
		self.read_bytes().map(u16::from_le_bytes)
	}

	pub fn read_u32(&mut self) -> Option<u32> {
		self.read_bytes().map(u32::from_le_bytes)
	}

	pub fn read_i32(&mut self) -> Option<i32> {
		self.read_bytes().map(i32::from_le_bytes)
	}

	/// Raw float, with no finiteness check.
	pub fn read_f32_raw(&mut self) -> Option<f32> {
		self.read_bytes().map(f32::from_le_bytes)
	}

	fn read_floats<const N: usize>(&mut self) -> Option<[f32; N]> {
		if self.unread_bits() < N * 32 {
			return None;
		}
		let mut out = [0.0; N];
		for f in &mut out {
			*f = self.read_f32_raw()?;
		}
		Some(out)
	}

	/// `readFLOAT`: consumes the bytes even when the value is rejected for not being finite.
	pub fn read_f32(&mut self) -> Read<f32> {
		match self.read_f32_raw() {
			None => Read::Short,
			Some(f) if f.is_finite() => Read::Ok(f),
			Some(f) => Read::Invalid(f),
		}
	}

	/// `readVEC3`.
	pub fn read_vec3(&mut self) -> Read<Vec3> {
		self.read_vec3_checked(|_| true)
	}

	/// `readPosVEC3`: finite and within the map bounds the C++ accepts.
	pub fn read_pos_vec3(&mut self) -> Read<Vec3> {
		self.read_vec3_checked(|v| {
			v.x < 20000.0 && v.x > -20000.0 && v.y < 20000.0 && v.y > -20000.0 && v.z < 200000.0 && v.z > -1000.0
		})
	}

	/// `readVelVEC3`: finite and no faster than 100 units per tick.
	pub fn read_vel_vec3(&mut self) -> Read<Vec3> {
		self.read_vec3_checked(|v| v.x * v.x + v.y * v.y + v.z * v.z <= 100.0 * 100.0)
	}

	fn read_vec3_checked(&mut self, ok: impl Fn(&Vec3) -> bool) -> Read<Vec3> {
		let Some([x, y, z]) = self.read_floats::<3>() else {
			return Read::Short;
		};
		let v = Vec3 { x, y, z };
		if x.is_finite() && y.is_finite() && z.is_finite() && ok(&v) {
			Read::Ok(v)
		} else {
			Read::Invalid(v)
		}
	}

	/// `readVEC2`.
	pub fn read_vec2(&mut self) -> Read<Vec2> {
		let Some([x, y]) = self.read_floats::<2>() else {
			return Read::Short;
		};
		let v = Vec2 { x, y };
		if x.is_finite() && y.is_finite() {
			Read::Ok(v)
		} else {
			Read::Invalid(v)
		}
	}

	/// `readVEC4`: four finite floats in x, y, z, w order.
	pub fn read_vec4(&mut self) -> Read<crate::packets::Vec4> {
		let Some([x, y, z, w]) = self.read_floats::<4>() else {
			return Read::Short;
		};
		let v = crate::packets::Vec4 { x, y, z, w };
		if x.is_finite() && y.is_finite() && z.is_finite() && w.is_finite() {
			Read::Ok(v)
		} else {
			Read::Invalid(v)
		}
	}

	/// `readGTAQuat`: four finite floats in w, x, y, z order.
	pub fn read_quat(&mut self) -> Read<Quat> {
		let Some([w, x, y, z]) = self.read_floats::<4>() else {
			return Read::Short;
		};
		let q = Quat { w, x, y, z };
		if w.is_finite() && x.is_finite() && y.is_finite() && z.is_finite() {
			Read::Ok(q)
		} else {
			Read::Invalid(q)
		}
	}

	/// `readCompressedPercentPair`: despite the name, two whole bytes.
	pub fn read_percent_pair(&mut self) -> Option<Vec2> {
		if self.unread_bits() < 16 {
			return None;
		}
		let health = self.read_u8()?;
		let armour = self.read_u8()?;
		Some(Vec2 {
			x: health.into(),
			y: armour.into(),
		})
	}
}

/// `GTAQuat`, in its wire (and memory) order.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Quat {
	pub w: f32,
	pub x: f32,
	pub y: f32,
	pub z: f32,
}

/// The result of a checked read. The C++ readers store the value even when they reject it, and
/// later code may observe it, so `Invalid` keeps it.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Read<T> {
	Ok(T),
	/// Read, but rejected by the validity check.
	Invalid(T),
	/// Not enough data; nothing was consumed.
	Short,
}

impl<T> Read<T> {
	pub fn ok(self) -> Option<T> {
		match self {
			Read::Ok(v) => Some(v),
			_ => None,
		}
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn aligned_reads_are_little_endian() {
		let mut r = BitReader::new(&[0x34, 0x12, 0x78, 0x56, 0x34, 0x12]);
		assert_eq!(r.read_u16(), Some(0x1234));
		assert_eq!(r.read_u32(), Some(0x12345678));
		assert_eq!(r.read_u8(), None);
	}

	#[test]
	fn bits_are_msb_first_and_unaligned_bytes_straddle() {
		// 1, then the byte 0xAB spread over the next 8 bits.
		let mut r = BitReader::new(&[0b1101_0101, 0b1000_0000]);
		assert_eq!(r.read_bit(), Some(true));
		assert_eq!(r.read_u8(), Some(0xAB));
		assert_eq!(r.unread_bits(), 7);
	}

	#[test]
	fn short_reads_consume_nothing() {
		let mut r = BitReader::new(&[1, 2, 3]);
		assert_eq!(r.read_u32(), None);
		assert_eq!(r.unread_bits(), 24);
		assert_eq!(r.read_vec3(), Read::Short);
		assert_eq!(r.unread_bits(), 24);
	}

	#[test]
	fn with_bits_limits_length() {
		let mut r = BitReader::with_bits(&[0xFF, 0xFF], 9);
		assert_eq!(r.read_u8(), Some(0xFF));
		assert_eq!(r.read_bit(), Some(true));
		assert_eq!(r.read_bit(), None);
	}

	fn floats(values: &[f32]) -> Vec<u8> {
		values.iter().flat_map(|f| f.to_le_bytes()).collect()
	}

	#[test]
	fn checked_vectors() {
		let data = floats(&[1.0, 2.0, 3.0]);
		assert_eq!(
			BitReader::new(&data).read_pos_vec3(),
			Read::Ok(Vec3 { x: 1.0, y: 2.0, z: 3.0 })
		);

		let data = floats(&[20000.0, 0.0, 0.0]);
		assert!(matches!(BitReader::new(&data).read_pos_vec3(), Read::Invalid(_)));
		let data = floats(&[0.0, 0.0, -1000.0]);
		assert!(matches!(BitReader::new(&data).read_pos_vec3(), Read::Invalid(_)));

		let data = floats(&[100.0, 0.0, 0.0]);
		assert!(matches!(BitReader::new(&data).read_vel_vec3(), Read::Ok(_)));
		let data = floats(&[100.0, 0.1, 0.0]);
		assert!(matches!(BitReader::new(&data).read_vel_vec3(), Read::Invalid(_)));

		let data = floats(&[f32::NAN, 0.0, 0.0]);
		let mut r = BitReader::new(&data);
		assert!(matches!(r.read_vec3(), Read::Invalid(_)));
		// Rejected values are still consumed.
		assert_eq!(r.unread_bits(), 0);
	}
}
