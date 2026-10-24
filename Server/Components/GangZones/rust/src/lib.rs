/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! GangZones component logic, ported from `gangzone.hpp`/`gangzone.cpp`.
//!
//! The pool, the per-zone "shown for" player set and the checking list stay in C++: the SDK hands
//! them out as references to C++ containers. We own the zone geometry, the per-player zone state,
//! the ID mappers, and the enter/leave decisions.

mod ffi;

use omp_common::values::{GANG_ZONE_POOL_SIZE, PLAYER_POOL_SIZE};
use omp_common::{Colour, LegacyIdMapper, PlayerBitset, Vec2, player_index};

/// Maps SDK zone IDs to Pawn (legacy) or client IDs.
pub type IdMapper = LegacyIdMapper<{ GANG_ZONE_POOL_SIZE as i32 }>;

/// `GangZonePos`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct ZonePos {
	pub min: Vec2,
	pub max: Vec2,
}

impl ZonePos {
	/// Swap out-of-order coordinates. Returns which axes were swapped, so the caller can warn.
	pub fn normalise(&mut self) -> Swapped {
		let x = self.max.x < self.min.x;
		if x {
			std::mem::swap(&mut self.min.x, &mut self.max.x);
		}
		let y = self.max.y < self.min.y;
		if y {
			std::mem::swap(&mut self.min.y, &mut self.max.y);
		}
		Swapped { x, y }
	}

	/// Whether a point is in the zone: min edges inclusive, max edges exclusive.
	pub fn contains(&self, x: f32, y: f32) -> bool {
		x >= self.min.x && x < self.max.x && y >= self.min.y && y < self.max.y
	}
}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Swapped {
	pub x: bool,
	pub y: bool,
}

/// What a player position update means for a zone.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Transition {
	None = 0,
	Enter = 1,
	Leave = 2,
}

pub struct Zone {
	pub pos: ZonePos,
	/// The colour it was last shown with, to any player. Used when restreaming.
	pub colour: Colour,
	flashing: PlayerBitset,
	inside: PlayerBitset,
	colours: Box<[Colour]>,
	flash_colours: Box<[Colour]>,
}

impl Zone {
	pub fn new(pos: ZonePos) -> Self {
		Self {
			pos,
			colour: Colour::default(),
			flashing: PlayerBitset::default(),
			inside: PlayerBitset::default(),
			colours: vec![Colour::NONE; PLAYER_POOL_SIZE].into_boxed_slice(),
			flash_colours: vec![Colour::NONE; PLAYER_POOL_SIZE].into_boxed_slice(),
		}
	}

	pub fn show(&mut self, player: i32, colour: Colour) {
		self.colour = colour;
		self.flashing.set(player, false);
		if let Some(i) = player_index(player) {
			self.colours[i] = colour;
			self.flash_colours[i] = Colour::NONE;
		}
	}

	/// Forget everything about a player, as when hiding the zone for them or when they leave.
	pub fn remove_for(&mut self, player: i32) {
		self.inside.set(player, false);
		self.flashing.set(player, false);
		if let Some(i) = player_index(player) {
			self.colours[i] = Colour::NONE;
			self.flash_colours[i] = Colour::NONE;
		}
	}

	pub fn flash(&mut self, player: i32, colour: Colour) {
		self.flashing.set(player, true);
		if let Some(i) = player_index(player) {
			self.flash_colours[i] = colour;
		}
	}

	pub fn stop_flash(&mut self, player: i32) {
		self.flashing.set(player, false);
		if let Some(i) = player_index(player) {
			self.flash_colours[i] = Colour::NONE;
		}
	}

	pub fn is_flashing(&self, player: i32) -> bool {
		self.flashing.test(player)
	}

	pub fn colour_for(&self, player: i32) -> Colour {
		player_index(player).map_or(Colour::NONE, |i| self.colours[i])
	}

	pub fn flash_colour_for(&self, player: i32) -> Colour {
		player_index(player).map_or(Colour::NONE, |i| self.flash_colours[i])
	}

	pub fn is_inside(&self, player: i32) -> bool {
		self.inside.test(player)
	}

	pub fn set_inside(&mut self, player: i32, inside: bool) {
		self.inside.set(player, inside);
	}

	/// Whether the player at (`x`, `y`) just entered or left. Doesn't record it: the caller does
	/// that with `set_inside` when it dispatches the event.
	pub fn check(&self, player: i32, x: f32, y: f32) -> Transition {
		match (self.pos.contains(x, y), self.is_inside(player)) {
			(true, false) => Transition::Enter,
			(false, true) => Transition::Leave,
			_ => Transition::None,
		}
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	fn pos(min: (f32, f32), max: (f32, f32)) -> ZonePos {
		ZonePos {
			min: Vec2 { x: min.0, y: min.1 },
			max: Vec2 { x: max.0, y: max.1 },
		}
	}

	const RED: Colour = Colour {
		r: 255,
		g: 0,
		b: 0,
		a: 255,
	};
	const BLUE: Colour = Colour {
		r: 0,
		g: 0,
		b: 255,
		a: 255,
	};

	#[test]
	fn normalise_swaps_each_axis_independently() {
		let mut p = pos((10.0, 0.0), (0.0, 5.0));
		assert_eq!(p.normalise(), Swapped { x: true, y: false });
		assert_eq!(p, pos((0.0, 0.0), (10.0, 5.0)));
		let mut p = pos((0.0, 5.0), (10.0, 0.0));
		assert_eq!(p.normalise(), Swapped { x: false, y: true });
		assert_eq!(p.normalise(), Swapped::default());
	}

	#[test]
	fn contains_is_half_open() {
		let p = pos((0.0, 0.0), (10.0, 10.0));
		assert!(p.contains(0.0, 0.0));
		assert!(p.contains(9.99, 9.99));
		assert!(!p.contains(10.0, 5.0));
		assert!(!p.contains(5.0, 10.0));
		assert!(!p.contains(-0.01, 5.0));
	}

	#[test]
	fn show_flash_and_hide_track_per_player_state() {
		let mut z = Zone::new(pos((0.0, 0.0), (1.0, 1.0)));
		z.show(3, RED);
		assert_eq!(z.colour, RED);
		assert_eq!(z.colour_for(3), RED);
		assert_eq!(z.colour_for(4), Colour::NONE);

		z.flash(3, BLUE);
		assert!(z.is_flashing(3));
		assert_eq!(z.flash_colour_for(3), BLUE);

		// Showing again stops flashing.
		z.show(3, BLUE);
		assert!(!z.is_flashing(3));
		assert_eq!(z.flash_colour_for(3), Colour::NONE);

		z.flash(3, RED);
		z.stop_flash(3);
		assert!(!z.is_flashing(3));

		z.set_inside(3, true);
		z.remove_for(3);
		assert!(!z.is_inside(3));
		assert_eq!(z.colour_for(3), Colour::NONE);
		// The last shown colour is kept for restreaming to others.
		assert_eq!(z.colour, BLUE);
	}

	#[test]
	fn check_reports_transitions_without_recording_them() {
		let mut z = Zone::new(pos((0.0, 0.0), (10.0, 10.0)));
		assert_eq!(z.check(1, 5.0, 5.0), Transition::Enter);
		assert_eq!(z.check(1, 5.0, 5.0), Transition::Enter);
		z.set_inside(1, true);
		assert_eq!(z.check(1, 5.0, 5.0), Transition::None);
		assert_eq!(z.check(1, 50.0, 5.0), Transition::Leave);
		z.set_inside(1, false);
		assert_eq!(z.check(1, 50.0, 5.0), Transition::None);
	}

	#[test]
	fn out_of_range_players_are_ignored() {
		let mut z = Zone::new(ZonePos::default());
		z.show(PLAYER_POOL_SIZE as i32, RED);
		z.flash(-1, RED);
		assert_eq!(z.colour_for(PLAYER_POOL_SIZE as i32), Colour::NONE);
		assert!(!z.is_flashing(-1));
	}
}
