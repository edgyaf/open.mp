/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Decoding of packets and RPCs sent by clients: data the server can't trust, read in safe Rust.

pub mod bitstream;
pub mod checks;
pub mod component_rpcs;
mod ffi;
pub mod packets;
pub mod rpcs;

pub use bitstream::{BitReader, Quat, Read};
pub use packets::{FootSync, SurfingType};
