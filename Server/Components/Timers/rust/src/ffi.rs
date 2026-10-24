/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! C ABI used by the C++ shim. Mirrored by `../../timers_rs.hpp`; keep the two in sync.
//!
//! Every function taking a pointer requires it to come from the matching constructor and still be
//! alive. Names are prefixed per component because Rust symbols from several components may end up
//! in one process.

use crate::{Facade, Host, Timer, Timers};
use std::time::Instant;

pub type FacadeCallback = unsafe extern "C" fn(facade: Facade);

pub struct CppHost {
	on_timeout: FacadeCallback,
	on_destroy: FacadeCallback,
}

impl Host for CppHost {
	fn timeout(&self, facade: Facade) {
		unsafe { (self.on_timeout)(facade) }
	}

	fn destroy(&self, facade: Facade) {
		unsafe { (self.on_destroy)(facade) }
	}

	fn now(&self) -> Instant {
		Instant::now()
	}
}

pub type Component = Timers<CppHost>;

#[unsafe(no_mangle)]
pub extern "C" fn omp_timers_new(on_timeout: FacadeCallback, on_destroy: FacadeCallback) -> *mut Component {
	Box::into_raw(Box::new(Timers::new(CppHost { on_timeout, on_destroy })))
}

/// Destroys every remaining timer (calling `on_destroy` for each), then the component.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_free(timers: *mut Component) {
	drop(unsafe { Box::from_raw(timers) });
}

/// The returned timer stays valid until `on_destroy` is called for `facade` and returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_create(
	timers: *const Component,
	facade: Facade,
	initial: i64,
	interval: i64,
	count: u32,
) -> *const Timer {
	unsafe { &*timers }.create(facade, initial, interval, count)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_tick(timers: *const Component) {
	unsafe { &*timers }.tick();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_count(timers: *const Component) -> usize {
	unsafe { &*timers }.count()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_running(timer: *const Timer) -> bool {
	unsafe { &*timer }.running()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_remaining(timer: *const Timer) -> i64 {
	unsafe { &*timer }.remaining(Instant::now())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_calls(timer: *const Timer) -> u32 {
	unsafe { &*timer }.calls()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_interval(timer: *const Timer) -> i64 {
	unsafe { &*timer }.interval()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_trigger(timer: *const Timer) -> bool {
	unsafe { &*timer }.trigger()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn omp_timers_timer_kill(timer: *const Timer) {
	unsafe { &*timer }.kill();
}
