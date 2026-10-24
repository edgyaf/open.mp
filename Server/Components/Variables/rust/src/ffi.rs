/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../variables_rs.hpp`; keep the two in sync.
//!
//! Strings are passed as pointer and length and may contain any bytes. Every function taking a
//! storage pointer requires it to come from `omp_variables_storage_new` and still be alive.

use crate::{Storage, Value, VariableType};
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

/// # Safety
/// The out pointers must be valid for writes.
unsafe fn write_view(view: &[u8], out_ptr: *mut *const c_char, out_len: *mut usize) {
	unsafe {
		*out_ptr = view.as_ptr().cast();
		*out_len = view.len();
	}
}

#[unsafe(no_mangle)]
pub extern "C" fn omp_variables_storage_new() -> *mut Storage {
	Box::into_raw(Box::new(Storage::new()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_storage_free(storage: *mut Storage) {
	drop(unsafe { Box::from_raw(storage) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_set_string(
	storage: *mut Storage,
	key: *const c_char,
	key_len: usize,
	value: *const c_char,
	value_len: usize,
) {
	let value = Value::String(unsafe { bytes(value, value_len) }.into());
	unsafe { &mut *storage }.set(unsafe { bytes(key, key_len) }, value);
}

/// Returns whether the variable exists and is a string. The view stays valid until the variable is
/// changed or erased, or the storage is cleared or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_get_string(
	storage: *const Storage,
	key: *const c_char,
	key_len: usize,
	out_ptr: *mut *const c_char,
	out_len: *mut usize,
) -> bool {
	match unsafe { &*storage }.get_string(unsafe { bytes(key, key_len) }) {
		Some(value) => {
			unsafe { write_view(value, out_ptr, out_len) };
			true
		}
		None => false,
	}
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_set_int(storage: *mut Storage, key: *const c_char, key_len: usize, value: i32) {
	unsafe { &mut *storage }.set(unsafe { bytes(key, key_len) }, Value::Int(value));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_get_int(storage: *const Storage, key: *const c_char, key_len: usize) -> i32 {
	unsafe { &*storage }.get_int(unsafe { bytes(key, key_len) })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_set_float(
	storage: *mut Storage,
	key: *const c_char,
	key_len: usize,
	value: f32,
) {
	unsafe { &mut *storage }.set(unsafe { bytes(key, key_len) }, Value::Float(value));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_get_float(storage: *const Storage, key: *const c_char, key_len: usize) -> f32 {
	unsafe { &*storage }.get_float(unsafe { bytes(key, key_len) })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_get_type(
	storage: *const Storage,
	key: *const c_char,
	key_len: usize,
) -> VariableType {
	unsafe { &*storage }.get_type(unsafe { bytes(key, key_len) })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_erase(storage: *mut Storage, key: *const c_char, key_len: usize) -> bool {
	unsafe { &mut *storage }.erase(unsafe { bytes(key, key_len) })
}

/// The view stays valid until the variable is erased, or the storage is cleared or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_key_at(
	storage: *const Storage,
	index: i32,
	out_ptr: *mut *const c_char,
	out_len: *mut usize,
) -> bool {
	match unsafe { &*storage }.key_at(index) {
		Some(key) => {
			unsafe { write_view(key, out_ptr, out_len) };
			true
		}
		None => false,
	}
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_size(storage: *const Storage) -> usize {
	unsafe { &*storage }.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_variables_clear(storage: *mut Storage) {
	unsafe { &mut *storage }.clear();
}
