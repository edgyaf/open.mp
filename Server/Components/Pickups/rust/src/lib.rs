/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Pickups component logic, ported from `pickup.hpp`/`pickups_main.cpp`.
//!
//! The pool and each pickup's "streamed for" player set stay in C++ (the SDK pool hands out C++
//! containers, and restreaming needs the player objects). We own the pickup properties, which
//! players it's hidden for, the streaming decision, and the ID mappers.

mod ffi;

use omp_common::values::PICKUP_POOL_SIZE;
use omp_common::{LegacyIdMapper, PlayerBitset, Vec3};

/// Maps SDK pickup IDs to Pawn (legacy) or client IDs.
pub type IdMapper = LegacyIdMapper<{ PICKUP_POOL_SIZE as i32 }>;

/// A pickup in every virtual world.
pub const ALL_WORLDS: i32 = -1;

pub struct Pickup {
	pub model: i32,
	pub kind: u8,
	pub pos: Vec3,
	pub virtual_world: i32,
	pub is_static: bool,
	hidden: PlayerBitset,
	/// For legacy per-player pickups, the only player who can see it.
	legacy_owner: Option<i32>,
}

impl Pickup {
	pub fn new(model: i32, kind: u8, pos: Vec3, virtual_world: i32, is_static: bool) -> Self {
		Self {
			model,
			kind,
			pos,
			virtual_world,
			is_static,
			hidden: PlayerBitset::default(),
			legacy_owner: None,
		}
	}

	pub fn set_legacy_owner(&mut self, player: Option<i32>) {
		self.legacy_owner = player;
	}

	/// Per-player pickups are hidden from everyone but their owner.
	pub fn is_hidden_for(&self, player: i32) -> bool {
		match self.legacy_owner {
			Some(owner) => owner != player,
			None => self.hidden.test(player),
		}
	}

	/// Ignored for per-player pickups.
	pub fn set_hidden_for(&mut self, player: i32, hidden: bool) {
		if self.legacy_owner.is_none() {
			self.hidden.set(player, hidden);
		}
	}

	/// Whether the pickup should be streamed in for a player in `virtual_world` at `pos`.
	pub fn should_stream_in(&self, player: i32, virtual_world: i32, pos: Vec3, max_distance_sqr: f32) -> bool {
		// Same evaluation order as `glm::dot`, so floats round identically at the edge.
		let (dx, dy, dz) = (self.pos.x - pos.x, self.pos.y - pos.y, self.pos.z - pos.z);
		!self.is_hidden_for(player)
			&& (virtual_world == self.virtual_world || self.virtual_world == ALL_WORLDS)
			&& dx * dx + dy * dy + dz * dz < max_distance_sqr
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	fn v(x: f32, y: f32, z: f32) -> Vec3 {
		Vec3 { x, y, z }
	}

	fn pickup() -> Pickup {
		Pickup::new(1240, 2, v(0.0, 0.0, 0.0), 0, false)
	}

	#[test]
	fn streams_within_radius_exclusive() {
		let p = pickup();
		assert!(p.should_stream_in(1, 0, v(9.99, 0.0, 0.0), 100.0));
		assert!(!p.should_stream_in(1, 0, v(10.0, 0.0, 0.0), 100.0));
		assert!(p.should_stream_in(1, 0, v(3.0, 4.0, 5.0), 100.0));
	}

	#[test]
	fn virtual_worlds_must_match_unless_all_worlds() {
		let mut p = pickup();
		assert!(!p.should_stream_in(1, 1, v(0.0, 0.0, 0.0), 100.0));
		p.virtual_world = ALL_WORLDS;
		assert!(p.should_stream_in(1, 1, v(0.0, 0.0, 0.0), 100.0));
		assert!(p.should_stream_in(1, -5, v(0.0, 0.0, 0.0), 100.0));
	}

	#[test]
	fn hidden_players_are_not_streamed() {
		let mut p = pickup();
		p.set_hidden_for(1, true);
		assert!(p.is_hidden_for(1));
		assert!(!p.is_hidden_for(2));
		assert!(!p.should_stream_in(1, 0, v(0.0, 0.0, 0.0), 100.0));
		p.set_hidden_for(1, false);
		assert!(p.should_stream_in(1, 0, v(0.0, 0.0, 0.0), 100.0));
	}

	#[test]
	fn legacy_owner_overrides_hidden_state() {
		let mut p = pickup();
		p.set_hidden_for(2, true);
		p.set_legacy_owner(Some(1));
		assert!(!p.is_hidden_for(1));
		assert!(p.is_hidden_for(2));
		assert!(p.is_hidden_for(3));
		// Ignored while owned.
		p.set_hidden_for(1, true);
		p.set_hidden_for(2, false);
		assert!(!p.is_hidden_for(1));
		// The underlying hidden state comes back without an owner.
		p.set_legacy_owner(None);
		assert!(p.is_hidden_for(2));
		assert!(!p.is_hidden_for(1));
	}
}
