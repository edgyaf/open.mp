/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Types and helpers shared by open.mp components ported to Rust. Types that cross the C ABI are
//! `#[repr(C)]` and laid out like their SDK counterparts.

mod legacy_ids;
mod player_bitset;

pub use legacy_ids::LegacyIdMapper;
pub use player_bitset::PlayerBitset;

/// `values.hpp`
pub mod values {
	pub const PLAYER_POOL_SIZE: usize = 1000;
	pub const GANG_ZONE_POOL_SIZE: usize = 1024;
	pub const PICKUP_POOL_SIZE: usize = 4096;
}

/// `Vector2` (`glm::vec2`).
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Vec2 {
	pub x: f32,
	pub y: f32,
}

/// `Vector3` (`glm::vec3`).
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Vec3 {
	pub x: f32,
	pub y: f32,
	pub z: f32,
}

/// `Colour`.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Colour {
	pub r: u8,
	pub g: u8,
	pub b: u8,
	pub a: u8,
}

impl Colour {
	/// `Colour::None()`
	pub const NONE: Colour = Colour { r: 0, g: 0, b: 0, a: 0 };
}

/// Index into per-player storage, or `None` for IDs outside the player pool. The C++ indexed
/// fixed-size arrays without checking; we treat out-of-range IDs as absent instead.
pub fn player_index(id: i32) -> Option<usize> {
	usize::try_from(id).ok().filter(|&i| i < values::PLAYER_POOL_SIZE)
}
