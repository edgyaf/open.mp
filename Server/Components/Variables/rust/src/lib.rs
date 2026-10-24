/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Variables component logic, ported from `variables_main.cpp`.
//!
//! One [`Storage`] backs the server variables and one backs each player's variables. Keys are
//! case-insensitive for ASCII and stored uppercased. Keys and string values are bytes, not UTF-8:
//! they come straight from Pawn.

mod ffi;

use std::collections::HashMap;
use std::rc::Rc;

/// Matches the SDK's `VariableType`.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VariableType {
	None = 0,
	Int = 1,
	String = 2,
	Float = 3,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
	Int(i32),
	/// Boxed so the bytes don't move when the storage grows; `get_string` hands out views of them.
	String(Box<[u8]>),
	Float(f32),
}

impl Value {
	fn variable_type(&self) -> VariableType {
		match self {
			Value::Int(_) => VariableType::Int,
			Value::String(_) => VariableType::String,
			Value::Float(_) => VariableType::Float,
		}
	}
}

/// Insertion-ordered map with O(1) access by index, so iterating with `key_at` over `0..len()` (as
/// Pawn scripts do) is linear rather than quadratic. Erasing moves the last entry into the hole.
#[derive(Default)]
pub struct Storage {
	index: HashMap<Rc<[u8]>, usize>,
	entries: Vec<(Rc<[u8]>, Value)>,
}

impl Storage {
	pub fn new() -> Self {
		Self::default()
	}

	pub fn set(&mut self, key: &[u8], value: Value) {
		let key = upper(key);
		match self.index.get(&*key) {
			Some(&i) => self.entries[i].1 = value,
			None => {
				let key: Rc<[u8]> = key.into();
				self.index.insert(key.clone(), self.entries.len());
				self.entries.push((key, value));
			}
		}
	}

	pub fn get(&self, key: &[u8]) -> Option<&Value> {
		self.index.get(&*upper(key)).map(|&i| &self.entries[i].1)
	}

	pub fn get_int(&self, key: &[u8]) -> i32 {
		match self.get(key) {
			Some(&Value::Int(value)) => value,
			_ => 0,
		}
	}

	pub fn get_float(&self, key: &[u8]) -> f32 {
		match self.get(key) {
			Some(&Value::Float(value)) => value,
			_ => 0.0,
		}
	}

	/// `None` if the variable doesn't exist or isn't a string.
	pub fn get_string(&self, key: &[u8]) -> Option<&[u8]> {
		match self.get(key) {
			Some(Value::String(value)) => Some(value),
			_ => None,
		}
	}

	pub fn get_type(&self, key: &[u8]) -> VariableType {
		self.get(key).map_or(VariableType::None, Value::variable_type)
	}

	pub fn erase(&mut self, key: &[u8]) -> bool {
		let Some(i) = self.index.remove(&*upper(key)) else {
			return false;
		};
		self.entries.swap_remove(i);
		if let Some((moved, _)) = self.entries.get(i) {
			*self.index.get_mut(moved).expect("index out of sync") = i;
		}
		true
	}

	/// The (uppercased) key at `index`, for `index` in `0..len()`.
	pub fn key_at(&self, index: i32) -> Option<&[u8]> {
		let index = usize::try_from(index).ok()?;
		self.entries.get(index).map(|(key, _)| &**key)
	}

	pub fn len(&self) -> usize {
		self.entries.len()
	}

	pub fn is_empty(&self) -> bool {
		self.entries.is_empty()
	}

	pub fn clear(&mut self) {
		self.index.clear();
		self.entries.clear();
	}
}

fn upper(key: &[u8]) -> Box<[u8]> {
	key.to_ascii_uppercase().into_boxed_slice()
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn keys_are_ascii_case_insensitive() {
		let mut s = Storage::new();
		s.set(b"score", Value::Int(5));
		assert_eq!(s.get_int(b"SCORE"), 5);
		assert_eq!(s.get_int(b"ScOrE"), 5);
		assert_eq!(s.key_at(0), Some(&b"SCORE"[..]));
		// Non-ASCII bytes are left alone.
		s.set(b"\xe9t\xe9", Value::Int(1));
		assert_eq!(s.get_int(b"\xe9T\xe9"), 1);
		assert_eq!(s.get_int(b"\xc9T\xc9"), 0);
	}

	#[test]
	fn getters_return_defaults_for_missing_or_mismatched_types() {
		let mut s = Storage::new();
		s.set(b"a", Value::Float(1.5));
		assert_eq!(s.get_int(b"a"), 0);
		assert_eq!(s.get_string(b"a"), None);
		assert_eq!(s.get_float(b"a"), 1.5);
		assert_eq!(s.get_float(b"missing"), 0.0);
		assert_eq!(s.get_type(b"a"), VariableType::Float);
		assert_eq!(s.get_type(b"missing"), VariableType::None);
	}

	#[test]
	fn set_replaces_value_and_type() {
		let mut s = Storage::new();
		s.set(b"a", Value::Int(1));
		s.set(b"A", Value::String(b"hi"[..].into()));
		assert_eq!(s.len(), 1);
		assert_eq!(s.get_type(b"a"), VariableType::String);
		assert_eq!(s.get_string(b"a"), Some(&b"hi"[..]));
	}

	#[test]
	fn erase_keeps_index_consistent() {
		let mut s = Storage::new();
		for (i, key) in [b"a", b"b", b"c"].into_iter().enumerate() {
			s.set(key, Value::Int(i as i32));
		}
		assert!(s.erase(b"A"));
		assert!(!s.erase(b"a"));
		assert_eq!(s.len(), 2);
		assert_eq!(s.key_at(0), Some(&b"C"[..]));
		assert_eq!(s.key_at(1), Some(&b"B"[..]));
		assert_eq!(s.key_at(2), None);
		assert_eq!(s.key_at(-1), None);
		assert_eq!(s.get_int(b"c"), 2);
		assert!(s.erase(b"c"));
		assert!(s.erase(b"b"));
		assert!(s.is_empty());
	}

	#[test]
	fn string_views_survive_growth() {
		let mut s = Storage::new();
		s.set(b"first", Value::String(b"value"[..].into()));
		let ptr = s.get_string(b"first").unwrap().as_ptr();
		for i in 0..1000 {
			s.set(format!("k{i}").as_bytes(), Value::Int(i));
		}
		assert_eq!(s.get_string(b"first").unwrap().as_ptr(), ptr);
	}

	#[test]
	fn clear_empties_everything() {
		let mut s = Storage::new();
		s.set(b"a", Value::Int(1));
		s.clear();
		assert!(s.is_empty());
		assert_eq!(s.get_type(b"a"), VariableType::None);
	}
}
