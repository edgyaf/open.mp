/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

//! Timers component logic, ported from `timers_main.cpp`/`timer.hpp`.
//!
//! The C++ shim owns one `ITimer` facade object per timer and hands it to us as an opaque pointer.
//! We own the timer state and the schedule, and call back into the shim (through [`Host`]) when a
//! timer times out or has to be destroyed.
//!
//! Handlers run arbitrary code (usually Pawn) from inside [`Timers::tick`], and that code may call
//! straight back into us to create, kill or count timers. Nothing here holds a borrow across a
//! host call for that reason, and per-timer state lives in `Cell`s behind stable `Box` addresses.

mod ffi;

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::time::{Duration, Instant};

/// Opaque pointer to the C++ `ITimer` facade for a timer.
pub type Facade = *mut c_void;

/// The embedding side: where timeouts and destruction are reported, and where time comes from.
pub trait Host {
	/// Called when a timer times out. Corresponds to `handler->timeout(timer)`.
	fn timeout(&self, facade: Facade);

	/// Called exactly once per timer, right before its state is dropped. The facade may still query
	/// the timer during this call.
	fn destroy(&self, facade: Facade);

	/// The monotonic clock (`std::chrono::steady_clock` on the C++ side).
	fn now(&self) -> Instant;
}

pub struct Timer {
	running: Cell<bool>,
	count: Cell<u32>,
	interval: i64,
	timeout: Cell<Instant>,
	facade: Facade,
}

impl Timer {
	pub fn running(&self) -> bool {
		self.running.get()
	}

	/// Milliseconds until the timer times out, negative if it is overdue.
	pub fn remaining(&self, now: Instant) -> i64 {
		signed_millis(self.timeout.get(), now)
	}

	pub fn calls(&self) -> u32 {
		self.count.get()
	}

	pub fn interval(&self) -> i64 {
		self.interval
	}

	pub fn kill(&self) {
		self.running.set(false);
	}

	/// Mark the timer as being called now. Returns `true` when there are more calls to make after
	/// this one.
	pub fn trigger(&self) -> bool {
		if !self.running.get() {
			return false;
		}
		match self.count.get() {
			// Repeat forever.
			0 => true,
			count => {
				self.count.set(count - 1);
				if count == 1 {
					self.running.set(false);
				}
				self.running.get()
			}
		}
	}
}

pub struct Timers<H: Host> {
	host: H,
	/// Slots are only `None` transiently, while `tick` compacts the list.
	timers: RefCell<Vec<Option<Box<Timer>>>>,
}

impl<H: Host> Timers<H> {
	pub fn new(host: H) -> Self {
		Self {
			host,
			timers: RefCell::new(Vec::new()),
		}
	}

	pub fn host(&self) -> &H {
		&self.host
	}

	/// Create a timer firing first after `initial` ms, then every `interval` ms, `count` times in
	/// total (0 = forever). The returned pointer stays valid until the host's `destroy` for `facade`
	/// returns.
	pub fn create(&self, facade: Facade, initial: i64, interval: i64, count: u32) -> *const Timer {
		let timer = Box::new(Timer {
			running: Cell::new(true),
			count: Cell::new(count),
			interval,
			timeout: Cell::new(add_millis(self.host.now(), initial)),
			facade,
		});
		let ptr: *const Timer = &*timer;
		self.timers.borrow_mut().push(Some(timer));
		ptr
	}

	/// Number of timers that are still running.
	pub fn count(&self) -> usize {
		self.timers
			.borrow()
			.iter()
			.flatten()
			.filter(|timer| timer.running())
			.count()
	}

	pub fn tick(&self) {
		// Timers are kept in creation order. Survivors are compacted towards the front as we go; the
		// list can grow under us if a handler creates timers, and those get visited this tick too.
		let mut read = 0;
		let mut write = 0;
		loop {
			let timer: *const Timer = match self.timers.borrow().get(read) {
				None => break,
				Some(slot) => slot.as_deref().expect("timer slot visited twice"),
			};
			// SAFETY: the box is only dropped below, by us, after which we don't touch `timer`.
			let timer = unsafe { &*timer };

			let mut delete = !timer.running();
			if !delete {
				let now = self.host.now();
				let diff = signed_millis(now, timer.timeout.get());
				if diff >= 0 {
					self.host.timeout(timer.facade);
					if timer.trigger() {
						timer.timeout.set(add_millis(now, timer.interval - diff));
					} else {
						delete = true;
					}
				}
			}

			if delete {
				let timer = self.timers.borrow_mut()[read].take();
				self.destroy(timer);
			} else {
				self.timers.borrow_mut().swap(write, read);
				write += 1;
			}
			read += 1;
		}
		self.timers.borrow_mut().truncate(write);
	}

	/// Report destruction while the state is still alive, then drop it.
	fn destroy(&self, timer: Option<Box<Timer>>) {
		if let Some(timer) = timer {
			self.host.destroy(timer.facade);
		}
	}
}

impl<H: Host> Drop for Timers<H> {
	fn drop(&mut self) {
		// Repeat in case a handler's `free` creates more timers.
		loop {
			let timers = std::mem::take(&mut *self.timers.borrow_mut());
			if timers.is_empty() {
				break;
			}
			for timer in timers {
				self.destroy(timer);
			}
		}
	}
}

/// `duration_cast<Milliseconds>(a - b)`: truncated towards zero, may be negative.
fn signed_millis(a: Instant, b: Instant) -> i64 {
	let to_i64 = |d: Duration| i64::try_from(d.as_millis()).unwrap_or(i64::MAX);
	match a.checked_duration_since(b) {
		Some(d) => to_i64(d),
		None => -to_i64(b - a),
	}
}

fn add_millis(t: Instant, ms: i64) -> Instant {
	let d = Duration::from_millis(ms.unsigned_abs());
	let shifted = if ms >= 0 { t.checked_add(d) } else { t.checked_sub(d) };
	shifted.unwrap_or(t)
}

#[cfg(test)]
mod tests {
	use super::*;
	use std::rc::Rc;

	#[derive(Debug, PartialEq, Clone, Copy)]
	enum Event {
		Timeout(usize),
		Destroy(usize),
	}

	type Callback = Box<dyn Fn(&Timers<Rc<TestHost>>, usize)>;

	/// Facades are just small integers in the tests.
	struct TestHost {
		now: Cell<Instant>,
		events: RefCell<Vec<Event>>,
		on_timeout: RefCell<Option<Callback>>,
		/// Lets callbacks reach back into the component, like handlers do through the C++ side.
		timers: Cell<*const Timers<Rc<TestHost>>>,
	}

	impl Host for Rc<TestHost> {
		fn timeout(&self, facade: Facade) {
			let id = facade as usize;
			self.events.borrow_mut().push(Event::Timeout(id));
			if let Some(cb) = &*self.on_timeout.borrow() {
				cb(unsafe { &*self.timers.get() }, id);
			}
		}

		fn destroy(&self, facade: Facade) {
			self.events.borrow_mut().push(Event::Destroy(facade as usize));
		}

		fn now(&self) -> Instant {
			self.now.get()
		}
	}

	fn host() -> Rc<TestHost> {
		Rc::new(TestHost {
			now: Cell::new(Instant::now()),
			events: RefCell::new(Vec::new()),
			on_timeout: RefCell::new(None),
			timers: Cell::new(std::ptr::null()),
		})
	}

	fn advance(host: &TestHost, ms: u64) {
		host.now.set(host.now.get() + Duration::from_millis(ms));
	}

	fn events(host: &TestHost) -> Vec<Event> {
		std::mem::take(&mut *host.events.borrow_mut())
	}

	fn f(id: usize) -> Facade {
		id as Facade
	}

	#[test]
	fn one_shot_fires_once_then_is_destroyed() {
		let h = host();
		let timers = Timers::new(h.clone());
		let t = timers.create(f(1), 100, 100, 1);
		assert_eq!(timers.count(), 1);
		assert_eq!(unsafe { &*t }.remaining(h.now()), 100);

		advance(&h, 99);
		timers.tick();
		assert_eq!(events(&h), []);

		advance(&h, 1);
		timers.tick();
		assert_eq!(events(&h), [Event::Timeout(1), Event::Destroy(1)]);
		assert_eq!(timers.count(), 0);
	}

	#[test]
	fn repeating_timer_compensates_for_lateness() {
		let h = host();
		let timers = Timers::new(h.clone());
		let t = timers.create(f(1), 100, 100, 0);

		advance(&h, 130);
		timers.tick();
		assert_eq!(events(&h), [Event::Timeout(1)]);
		// Next timeout is scheduled relative to when it should have fired.
		assert_eq!(unsafe { &*t }.remaining(h.now()), 70);

		advance(&h, 70);
		timers.tick();
		assert_eq!(events(&h), [Event::Timeout(1)]);
		assert!(unsafe { &*t }.running());
		assert_eq!(unsafe { &*t }.calls(), 0);
	}

	#[test]
	fn counted_timer_stops_after_count_calls() {
		let h = host();
		let timers = Timers::new(h.clone());
		let t = timers.create(f(1), 0, 10, 3);
		for remaining in [2, 1] {
			timers.tick();
			assert_eq!(events(&h), [Event::Timeout(1)]);
			assert_eq!(unsafe { &*t }.calls(), remaining);
			advance(&h, 10);
		}
		timers.tick();
		assert_eq!(events(&h), [Event::Timeout(1), Event::Destroy(1)]);
	}

	#[test]
	fn fires_up_to_a_millisecond_early_like_cpp() {
		// `duration_cast` truncates towards zero, so -0.5ms counts as due.
		let h = host();
		let timers = Timers::new(h.clone());
		timers.create(f(1), 1, 1, 1);
		h.now.set(h.now.get() + Duration::from_micros(500));
		timers.tick();
		assert_eq!(events(&h), [Event::Timeout(1), Event::Destroy(1)]);
	}

	#[test]
	fn killed_timer_is_destroyed_on_next_tick_without_firing() {
		let h = host();
		let timers = Timers::new(h.clone());
		let t = timers.create(f(1), 1000, 1000, 0);
		timers.create(f(2), 1000, 1000, 0);
		unsafe { &*t }.kill();
		assert_eq!(timers.count(), 1);
		timers.tick();
		assert_eq!(events(&h), [Event::Destroy(1)]);
		assert_eq!(timers.count(), 1);
	}

	#[test]
	fn order_is_preserved_across_deletions() {
		let h = host();
		let timers = Timers::new(h.clone());
		for id in 1..=5 {
			timers.create(f(id), 0, 0, if id % 2 == 0 { 1 } else { 0 });
		}
		timers.tick();
		use Event::*;
		assert_eq!(
			events(&h),
			[
				Timeout(1),
				Timeout(2),
				Destroy(2),
				Timeout(3),
				Timeout(4),
				Destroy(4),
				Timeout(5)
			]
		);
		timers.tick();
		assert_eq!(events(&h), [Timeout(1), Timeout(3), Timeout(5)]);
	}

	#[test]
	fn handler_can_create_and_kill_timers_during_tick() {
		let h = host();
		let timers = Timers::new(h.clone());
		h.timers.set(&timers);
		let victim = timers.create(f(2), 0, 0, 0) as usize;
		timers.create(f(1), 0, 0, 1);
		*h.on_timeout.borrow_mut() = Some(Box::new(move |timers, id| {
			if id == 1 {
				unsafe { &*(victim as *const Timer) }.kill();
				// Due immediately, so it fires in this same tick, as with the C++ `std::list`.
				timers.create(f(3), 0, 0, 1);
				assert_eq!(timers.count(), 2);
			}
		}));

		timers.tick();
		use Event::*;
		assert_eq!(events(&h), [Timeout(2), Timeout(1), Destroy(1), Timeout(3), Destroy(3)]);
		timers.tick();
		assert_eq!(events(&h), [Destroy(2)]);
		assert_eq!(timers.count(), 0);
	}

	#[test]
	fn drop_destroys_remaining_timers() {
		let h = host();
		let timers = Timers::new(h.clone());
		timers.create(f(1), 1000, 1000, 0);
		timers.create(f(2), 1000, 1000, 0);
		drop(timers);
		assert_eq!(events(&h), [Event::Destroy(1), Event::Destroy(2)]);
	}

	#[test]
	fn negative_remaining_when_overdue() {
		let h = host();
		let timers = Timers::new(h.clone());
		let t = timers.create(f(1), 10, 10, 0);
		advance(&h, 25);
		assert_eq!(unsafe { &*t }.remaining(h.now()), -15);
	}
}
