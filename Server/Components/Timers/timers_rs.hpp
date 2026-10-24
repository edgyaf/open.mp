/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// C ABI of the Rust implementation in `rust/src/ffi.rs`; keep the two in sync.

#include <cstddef>
#include <cstdint>

extern "C"
{
	struct OmpTimersComponent;
	struct OmpTimersTimer;

	typedef void (*OmpTimersFacadeCallback)(void* facade);

	OmpTimersComponent* omp_timers_new(OmpTimersFacadeCallback onTimeout, OmpTimersFacadeCallback onDestroy);
	void omp_timers_free(OmpTimersComponent* timers);
	const OmpTimersTimer* omp_timers_create(const OmpTimersComponent* timers, void* facade, int64_t initial, int64_t interval, uint32_t count);
	void omp_timers_tick(const OmpTimersComponent* timers);
	size_t omp_timers_count(const OmpTimersComponent* timers);

	bool omp_timers_timer_running(const OmpTimersTimer* timer);
	int64_t omp_timers_timer_remaining(const OmpTimersTimer* timer);
	uint32_t omp_timers_timer_calls(const OmpTimersTimer* timer);
	int64_t omp_timers_timer_interval(const OmpTimersTimer* timer);
	bool omp_timers_timer_trigger(const OmpTimersTimer* timer);
	void omp_timers_timer_kill(const OmpTimersTimer* timer);
}
