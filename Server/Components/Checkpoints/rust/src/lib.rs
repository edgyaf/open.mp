/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Checkpoints component logic, ported from `checkpoint.hpp`/`dllmain.cpp`.
//!
//! Each player has a normal and a race checkpoint. We keep their state and decide what has to
//! happen; the C++ shim sends the packets and dispatches the events.

mod ffi;

#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Vec3 {
	pub x: f32,
	pub y: f32,
	pub z: f32,
}

/// Matches `RaceCheckpointType::RACE_NONE`.
pub const RACE_NONE: i32 = 9;

/// What a position update means for a checkpoint.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Transition {
	None = 0,
	Enter = 1,
	Leave = 2,
}

#[derive(Debug, Default)]
pub struct Checkpoint {
	pub position: Vec3,
	pub radius: f32,
	pub inside: bool,
	enabled: bool,
}

impl Checkpoint {
	pub fn enabled(&self) -> bool {
		self.enabled
	}

	/// Returns whether the client must first be told to disable the checkpoint it currently shows.
	/// The caller then sends the checkpoint itself.
	pub fn enable(&mut self) -> bool {
		let was_enabled = self.disable();
		self.enabled = true;
		self.inside = false;
		was_enabled
	}

	/// Returns whether the client must be told to disable the checkpoint.
	pub fn disable(&mut self) -> bool {
		if !self.enabled {
			return false;
		}
		self.enabled = false;
		self.inside = false;
		true
	}

	/// Forget the checkpoint without telling the client, as on GMX.
	pub fn reset(&mut self) {
		self.enabled = false;
	}

	/// Track whether the player at `player` is inside the checkpoint.
	pub fn update(&mut self, player: Vec3) -> Transition {
		if !self.enabled {
			return Transition::None;
		}
		// Same evaluation order as `glm::dot`, so floats round identically at the edge.
		let (dx, dy, dz) = (
			self.position.x - player.x,
			self.position.y - player.y,
			self.position.z - player.z,
		);
		let inside = dx * dx + dy * dy + dz * dz <= self.radius * self.radius;
		if inside == self.inside {
			return Transition::None;
		}
		self.inside = inside;
		if inside { Transition::Enter } else { Transition::Leave }
	}
}

#[derive(Debug)]
pub struct RaceCheckpoint {
	pub base: Checkpoint,
	pub kind: i32,
	pub next_position: Vec3,
}

impl Default for RaceCheckpoint {
	fn default() -> Self {
		Self {
			base: Checkpoint::default(),
			kind: RACE_NONE,
			next_position: Vec3::default(),
		}
	}
}

#[derive(Debug, Default)]
pub struct PlayerCheckpoints {
	pub checkpoint: Checkpoint,
	pub race: RaceCheckpoint,
}

impl PlayerCheckpoints {
	pub fn reset(&mut self) {
		self.race.base.reset();
		self.checkpoint.reset();
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	fn v(x: f32, y: f32, z: f32) -> Vec3 {
		Vec3 { x, y, z }
	}

	fn checkpoint() -> Checkpoint {
		let mut cp = Checkpoint {
			position: v(10.0, 0.0, 0.0),
			radius: 2.0,
			..Default::default()
		};
		assert!(!cp.enable());
		cp
	}

	#[test]
	fn enter_and_leave_fire_once_each() {
		let mut cp = checkpoint();
		assert_eq!(cp.update(v(0.0, 0.0, 0.0)), Transition::None);
		assert_eq!(cp.update(v(9.0, 0.0, 0.0)), Transition::Enter);
		assert_eq!(cp.update(v(10.0, 1.0, 0.0)), Transition::None);
		assert_eq!(cp.update(v(13.0, 0.0, 0.0)), Transition::Leave);
		assert_eq!(cp.update(v(14.0, 0.0, 0.0)), Transition::None);
	}

	#[test]
	fn boundary_counts_as_inside() {
		let mut cp = checkpoint();
		assert_eq!(cp.update(v(12.0, 0.0, 0.0)), Transition::Enter);
	}

	#[test]
	fn disabled_checkpoint_never_transitions() {
		let mut cp = Checkpoint {
			radius: 100.0,
			..Default::default()
		};
		assert_eq!(cp.update(v(0.0, 0.0, 0.0)), Transition::None);
		cp.enable();
		assert!(cp.disable());
		assert!(!cp.disable());
		assert_eq!(cp.update(v(0.0, 0.0, 0.0)), Transition::None);
	}

	#[test]
	fn re_enabling_disables_first_and_resets_inside() {
		let mut cp = checkpoint();
		cp.update(v(10.0, 0.0, 0.0));
		assert!(cp.inside);
		assert!(cp.enable());
		assert!(cp.enabled());
		assert!(!cp.inside);
		// Still inside, so entering fires again.
		assert_eq!(cp.update(v(10.0, 0.0, 0.0)), Transition::Enter);
	}

	#[test]
	fn reset_disables_silently_but_keeps_inside() {
		let mut player = PlayerCheckpoints {
			checkpoint: checkpoint(),
			..Default::default()
		};
		player.checkpoint.update(v(10.0, 0.0, 0.0));
		player.race.base.enable();
		player.reset();
		assert!(!player.checkpoint.enabled());
		assert!(!player.race.base.enabled());
		assert!(player.checkpoint.inside);
		// Nothing to tell the client after a reset.
		assert!(!player.checkpoint.disable());
	}

	#[test]
	fn race_checkpoint_defaults() {
		let race = RaceCheckpoint::default();
		assert_eq!(race.kind, RACE_NONE);
		assert!(!race.base.enabled());
	}
}
