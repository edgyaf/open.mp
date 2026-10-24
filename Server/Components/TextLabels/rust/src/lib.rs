/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! TextLabels component logic, ported from `textlabel.hpp`/`textlabels_main.cpp`.
//!
//! The pools, the "streamed for" sets and the attachment data stay in C++ (the SDK hands the
//! attachment data out by reference, and resolving attached players or vehicles needs their pools).
//! We own the label properties and the streaming decision.

mod ffi;

use omp_common::{Colour, Vec3};

/// A label in every virtual world.
pub const ALL_WORLDS: i32 = -1;

pub struct Label {
	/// Boxed so views of it stay valid until the text changes.
	text: Box<[u8]>,
	pub pos: Vec3,
	pub colour: Colour,
	pub draw_distance: f32,
	pub test_los: bool,
	pub virtual_world: i32,
}

impl Label {
	pub fn new(text: &[u8], colour: Colour, pos: Vec3, draw_distance: f32, virtual_world: i32, test_los: bool) -> Self {
		Self {
			text: text.into(),
			pos,
			colour,
			draw_distance,
			test_los,
			virtual_world,
		}
	}

	pub fn text(&self) -> &[u8] {
		&self.text
	}

	pub fn set_text(&mut self, text: &[u8]) {
		self.text = text.into();
	}

	/// Whether a player in `virtual_world` can see the label, if it isn't attached to anything.
	pub fn world_matches(&self, virtual_world: i32) -> bool {
		virtual_world == self.virtual_world || self.virtual_world == ALL_WORLDS
	}
}

/// Whether a label at `label_pos` should be streamed in for a player at `player_pos`. `visible` is
/// whether the player can see it at all: the virtual world matches, or what it's attached to is
/// streamed in for the player.
pub fn should_stream_in(
	has_state: bool,
	visible: bool,
	label_pos: Vec3,
	player_pos: Vec3,
	max_distance_sqr: f32,
) -> bool {
	// Same evaluation order as `glm::dot`, so floats round identically at the edge.
	let (dx, dy, dz) = (
		label_pos.x - player_pos.x,
		label_pos.y - player_pos.y,
		label_pos.z - player_pos.z,
	);
	has_state && visible && dx * dx + dy * dy + dz * dz < max_distance_sqr
}

#[cfg(test)]
mod tests {
	use super::*;

	fn v(x: f32, y: f32, z: f32) -> Vec3 {
		Vec3 { x, y, z }
	}

	#[test]
	fn text_roundtrip_including_raw_bytes() {
		let mut label = Label::new(b"hello", Colour::NONE, v(0.0, 0.0, 0.0), 1.0, 0, false);
		assert_eq!(label.text(), b"hello");
		label.set_text(b"a\0\xe9");
		assert_eq!(label.text(), b"a\0\xe9");
	}

	#[test]
	fn worlds() {
		let mut label = Label::new(b"", Colour::NONE, v(0.0, 0.0, 0.0), 1.0, 3, false);
		assert!(label.world_matches(3));
		assert!(!label.world_matches(0));
		label.virtual_world = ALL_WORLDS;
		assert!(label.world_matches(0));
		assert!(label.world_matches(12));
	}

	#[test]
	fn stream_decision() {
		let origin = v(0.0, 0.0, 0.0);
		assert!(should_stream_in(true, true, v(9.99, 0.0, 0.0), origin, 100.0));
		assert!(!should_stream_in(true, true, v(10.0, 0.0, 0.0), origin, 100.0));
		assert!(!should_stream_in(false, true, origin, origin, 100.0));
		assert!(!should_stream_in(true, false, origin, origin, 100.0));
	}
}
