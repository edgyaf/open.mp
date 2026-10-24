/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

use crate::player_index;
use crate::values::PLAYER_POOL_SIZE;

/// One bit per player ID, like `StaticBitset<PLAYER_POOL_SIZE>`. IDs outside the pool read as
/// unset and are ignored on write.
#[derive(Clone)]
pub struct PlayerBitset([u64; PLAYER_POOL_SIZE.div_ceil(64)]);

impl Default for PlayerBitset {
	fn default() -> Self {
		Self([0; PLAYER_POOL_SIZE.div_ceil(64)])
	}
}

impl PlayerBitset {
	pub fn test(&self, id: i32) -> bool {
		player_index(id).is_some_and(|i| self.0[i / 64] & (1 << (i % 64)) != 0)
	}

	pub fn set(&mut self, id: i32, value: bool) {
		if let Some(i) = player_index(id) {
			let bit = 1 << (i % 64);
			if value {
				self.0[i / 64] |= bit;
			} else {
				self.0[i / 64] &= !bit;
			}
		}
	}

	pub fn clear(&mut self) {
		self.0.fill(0);
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn set_test_and_bounds() {
		let mut b = PlayerBitset::default();
		for id in [0, 63, 64, 999] {
			assert!(!b.test(id));
			b.set(id, true);
			assert!(b.test(id));
		}
		b.set(63, false);
		assert!(!b.test(63));
		assert!(b.test(64));
		b.set(-1, true);
		b.set(1000, true);
		assert!(!b.test(-1));
		assert!(!b.test(1000));
		b.clear();
		assert!(!b.test(999));
	}
}
