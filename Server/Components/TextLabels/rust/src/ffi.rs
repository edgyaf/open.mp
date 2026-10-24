/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../textlabels_rs.hpp`; keep the two in sync.
//!
//! Every function taking a label pointer requires it to come from `omp_textlabels_label_new` and
//! still be alive. Text is passed as pointer and length and may contain any bytes.

use crate::Label;
use omp_common::{Colour, Vec3};
use std::ffi::c_char;

/// # Safety
/// `ptr` must point to `len` readable bytes, or `len` must be 0.
unsafe fn bytes<'a>(ptr: *const c_char, len: usize) -> &'a [u8] {
	if len == 0 {
		&[]
	} else {
		unsafe { std::slice::from_raw_parts(ptr.cast(), len) }
	}
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_new(
	text: *const c_char,
	text_len: usize,
	colour: Colour,
	pos: Vec3,
	draw_distance: f32,
	virtual_world: i32,
	test_los: bool,
) -> *mut Label {
	let text = unsafe { bytes(text, text_len) };
	Box::into_raw(Box::new(Label::new(
		text,
		colour,
		pos,
		draw_distance,
		virtual_world,
		test_los,
	)))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_free(label: *mut Label) {
	drop(unsafe { Box::from_raw(label) });
}

/// The view stays valid until the text is changed or the label freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_text(
	label: *const Label,
	out_ptr: *mut *const c_char,
	out_len: *mut usize,
) {
	let text = unsafe { &*label }.text();
	unsafe {
		*out_ptr = text.as_ptr().cast();
		*out_len = text.len();
	}
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_text(label: *mut Label, text: *const c_char, text_len: usize) {
	unsafe { &mut *label }.set_text(unsafe { bytes(text, text_len) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_position(label: *const Label) -> Vec3 {
	unsafe { &*label }.pos
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_position(label: *mut Label, pos: Vec3) {
	unsafe { &mut *label }.pos = pos;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_colour(label: *const Label) -> Colour {
	unsafe { &*label }.colour
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_colour(label: *mut Label, colour: Colour) {
	unsafe { &mut *label }.colour = colour;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_draw_distance(label: *const Label) -> f32 {
	unsafe { &*label }.draw_distance
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_draw_distance(label: *mut Label, draw_distance: f32) {
	unsafe { &mut *label }.draw_distance = draw_distance;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_test_los(label: *const Label) -> bool {
	unsafe { &*label }.test_los
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_test_los(label: *mut Label, test_los: bool) {
	unsafe { &mut *label }.test_los = test_los;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_get_virtual_world(label: *const Label) -> i32 {
	unsafe { &*label }.virtual_world
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_set_virtual_world(label: *mut Label, virtual_world: i32) {
	unsafe { &mut *label }.virtual_world = virtual_world;
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_textlabels_label_world_matches(label: *const Label, virtual_world: i32) -> bool {
	unsafe { &*label }.world_matches(virtual_world)
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_textlabels_should_stream_in(
	has_state: bool,
	visible: bool,
	label_pos: Vec3,
	player_pos: Vec3,
	max_distance_sqr: f32,
) -> bool {
	crate::should_stream_in(has_state, visible, label_pos, player_pos, max_distance_sqr)
}
