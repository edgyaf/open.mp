/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Port of `FiniteLegacyIDMapper` from `legacy_id_mapper.hpp`.
//!
//! Maps one set of IDs to another. For example the gang zones internal pool is infinite, but the
//! Pawn API has finite pools (a global one, and one per player), so each SDK ID used from Pawn is
//! given a legacy ID in `MIN..MAX`.

/// `MAX` is the highest valid legacy ID plus one, `MIN` the lowest. Errors return `INVALID` for
/// legacy IDs and `NOT_FOUND` for real IDs. The defaults match the C++ template's.
pub struct LegacyIdMapper<const MAX: i32, const MIN: i32 = 0, const INVALID: i32 = -1, const NOT_FOUND: i32 = 0> {
	ids: Box<[i32]>,
}

impl<const MAX: i32, const MIN: i32, const INVALID: i32, const NOT_FOUND: i32> Default
	for LegacyIdMapper<MAX, MIN, INVALID, NOT_FOUND>
{
	fn default() -> Self {
		Self::new()
	}
}

impl<const MAX: i32, const MIN: i32, const INVALID: i32, const NOT_FOUND: i32>
	LegacyIdMapper<MAX, MIN, INVALID, NOT_FOUND>
{
	pub const INVALID: i32 = INVALID;
	pub const NOT_FOUND: i32 = NOT_FOUND;

	pub fn new() -> Self {
		Self {
			ids: vec![NOT_FOUND; (MAX - MIN) as usize].into_boxed_slice(),
		}
	}

	fn slot(legacy: i32) -> Option<usize> {
		(MIN..MAX).contains(&legacy).then(|| (legacy - MIN) as usize)
	}

	fn legacy(slot: usize) -> i32 {
		slot as i32 + MIN
	}

	/// The lowest free legacy ID, or `INVALID`. Doesn't claim it; call `set` for that.
	pub fn reserve(&self) -> i32 {
		self.ids
			.iter()
			.position(|&id| id == NOT_FOUND)
			.map_or(INVALID, Self::legacy)
	}

	/// Store `real` in `legacy`. Out-of-range legacy IDs are ignored.
	pub fn set(&mut self, legacy: i32, real: i32) {
		if let Some(slot) = Self::slot(legacy) {
			self.ids[slot] = real;
		}
	}

	/// Free `legacy`. Out-of-range legacy IDs are ignored.
	pub fn release(&mut self, legacy: i32) {
		self.set(legacy, NOT_FOUND);
	}

	pub fn release_all(&mut self) {
		self.ids.fill(NOT_FOUND);
	}

	/// The lowest legacy ID storing `real`, or `INVALID`.
	pub fn to_legacy(&self, real: i32) -> i32 {
		self.ids.iter().position(|&id| id == real).map_or(INVALID, Self::legacy)
	}

	/// The real ID stored in `legacy`, or `NOT_FOUND`.
	pub fn from_legacy(&self, legacy: i32) -> i32 {
		Self::slot(legacy).map_or(NOT_FOUND, |slot| self.ids[slot])
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	type Mapper = LegacyIdMapper<4>;

	#[test]
	fn reserve_set_release_roundtrip() {
		let mut m = Mapper::new();
		assert_eq!(m.reserve(), 0);
		// Reserving alone doesn't claim the slot.
		assert_eq!(m.reserve(), 0);
		m.set(0, 100);
		assert_eq!(m.reserve(), 1);
		m.set(1, 200);
		assert_eq!(m.to_legacy(200), 1);
		assert_eq!(m.from_legacy(1), 200);
		m.release(0);
		assert_eq!(m.reserve(), 0);
		assert_eq!(m.to_legacy(100), Mapper::INVALID);
		assert_eq!(m.from_legacy(0), Mapper::NOT_FOUND);
	}

	#[test]
	fn full_mapper_returns_invalid() {
		let mut m = Mapper::new();
		for legacy in 0..4 {
			m.set(legacy, legacy + 10);
		}
		assert_eq!(m.reserve(), Mapper::INVALID);
		m.release_all();
		assert_eq!(m.reserve(), 0);
	}

	#[test]
	fn out_of_range_is_ignored() {
		let mut m = Mapper::new();
		m.set(-1, 5);
		m.set(4, 5);
		assert_eq!(m.to_legacy(5), Mapper::INVALID);
		assert_eq!(m.from_legacy(-1), Mapper::NOT_FOUND);
		assert_eq!(m.from_legacy(4), Mapper::NOT_FOUND);
		m.release(1000);
	}

	#[test]
	fn non_zero_min() {
		let mut m = LegacyIdMapper::<5, 1>::new();
		assert_eq!(m.reserve(), 1);
		m.set(1, 7);
		assert_eq!(m.to_legacy(7), 1);
		m.set(0, 9);
		assert_eq!(m.to_legacy(9), -1);
	}

	#[test]
	fn duplicate_real_ids_map_to_lowest_legacy() {
		let mut m = Mapper::new();
		m.set(2, 42);
		m.set(1, 42);
		assert_eq!(m.to_legacy(42), 1);
	}
}
