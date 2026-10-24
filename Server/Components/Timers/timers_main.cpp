/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "timer.hpp"
#include <sdk.hpp>

// The timer logic lives in Rust (see `rust/`); this only adapts it to the SDK interfaces.

static void onTimerTimeout(void* facade)
{
	Timer* timer = static_cast<Timer*>(facade);
	timer->handler()->timeout(*timer);
}

static void onTimerDestroy(void* facade)
{
	delete static_cast<Timer*>(facade);
}

class TimersComponent final : public ITimersComponent, public CoreEventHandler
{
private:
	ICore* core = nullptr;
	OmpTimersComponent* timers = omp_timers_new(&onTimerTimeout, &onTimerDestroy);

	ITimer* createTimer(TimerTimeOutHandler* handler, Milliseconds initial, Milliseconds interval, unsigned int count)
	{
		Timer* timer = new Timer(handler);
		timer->setState(omp_timers_create(timers, timer, initial.count(), interval.count(), count));
		return timer;
	}

public:
	StringView componentName() const override
	{
		return "Timers";
	}

	SemanticVersion componentVersion() const override
	{
		return SemanticVersion(OMP_VERSION_MAJOR, OMP_VERSION_MINOR, OMP_VERSION_PATCH, BUILD_NUMBER);
	}

	void onLoad(ICore* core) override
	{
		this->core = core;
		core->getEventDispatcher().addEventHandler(this);
	}

	~TimersComponent()
	{
		if (core)
		{
			core->getEventDispatcher().removeEventHandler(this);
		}

		omp_timers_free(timers);
	}

	ITimer* create(TimerTimeOutHandler* handler, Milliseconds interval, bool repeating) override
	{
		return createTimer(handler, interval, interval, repeating ? 0 : 1);
	}

	ITimer* create(TimerTimeOutHandler* handler, Milliseconds initial, Milliseconds interval, unsigned int count) override
	{
		return createTimer(handler, initial, interval, count);
	}

	void onTick(Microseconds elapsed, TimePoint now) override
	{
		omp_timers_tick(timers);
	}

	void free() override
	{
		delete this;
	}

	void reset() override
	{
		// Nothing to reset here.  Although some timers are reset on GMX it isn't all of them, and
		// that's handled at a lower level by pawn itself.
	}

	const size_t count() const override
	{
		return omp_timers_count(timers);
	}
};

COMPONENT_ENTRY_POINT()
{
	return new TimersComponent();
}
