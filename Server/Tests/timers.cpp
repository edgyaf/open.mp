/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/Timers/timers.hpp>
#include <chrono>
#include <thread>

namespace
{

struct Handler final : public TimerTimeOutHandler
{
	int fired = 0;
	int freed = 0;
	ITimersComponent* timers = nullptr;
	ITimer* killOnOtherTimeout = nullptr;
	bool spawnOnTimeout = false;

	void timeout(ITimer& timer) override
	{
		++fired;
		if (killOnOtherTimeout && killOnOtherTimeout != &timer)
		{
			killOnOtherTimeout->kill();
			killOnOtherTimeout = nullptr;
		}
		if (spawnOnTimeout)
		{
			spawnOnTimeout = false;
			timers->create(this, Milliseconds(0), false);
		}
	}

	void free(ITimer& timer) override
	{
		// The timer must still be usable while it's being freed.
		(void)timer.running();
		++freed;
	}
};

}

TEST_CASE(timers_one_shot)
{
	ComponentFixture<ITimersComponent> f("Timers");
	CHECK(f.component->getUID() == TimersComponent_UID);
	Handler h;

	ITimer* timer = f.api->create(&h, Milliseconds(0), false);
	CHECK(timer->running());
	CHECK(timer->calls() == 1);
	CHECK(f.api->count() == 1);
	f.core.tick();
	CHECK(h.fired == 1);
	CHECK(h.freed == 1);
	CHECK(f.api->count() == 0);
}

TEST_CASE(timers_repeating_killed_by_other_handler)
{
	ComponentFixture<ITimersComponent> f("Timers");
	Handler h;

	ITimer* repeat = f.api->create(&h, Milliseconds(0), true);
	CHECK(repeat->calls() == 0);
	f.core.tick();
	f.core.tick();
	CHECK(h.fired == 2);

	f.api->create(&h, Milliseconds(0), false);
	h.killOnOtherTimeout = repeat;
	f.core.tick(); // repeat fires, then the one-shot fires and kills it
	CHECK(h.fired == 4);
	CHECK(!repeat->running());
	CHECK(h.freed == 1);
	f.core.tick();
	CHECK(h.fired == 4);
	CHECK(h.freed == 2);
	CHECK(f.api->count() == 0);
}

TEST_CASE(timers_created_during_tick_fire_in_same_tick)
{
	ComponentFixture<ITimersComponent> f("Timers");
	Handler h;
	h.timers = f.api;
	h.spawnOnTimeout = true;

	f.api->create(&h, Milliseconds(0), false);
	f.core.tick();
	CHECK(h.fired == 2);
	CHECK(h.freed == 2);
}

TEST_CASE(timers_counted_remaining_interval)
{
	ComponentFixture<ITimersComponent> f("Timers");
	Handler h;

	ITimer* timer = f.api->create(&h, Milliseconds(100), Milliseconds(1000), 3);
	CHECK(timer->interval() == Milliseconds(1000));
	CHECK(timer->remaining() <= Milliseconds(100) && timer->remaining() > Milliseconds(50));
	CHECK(timer->calls() == 3);
	f.core.tick();
	CHECK(h.fired == 0);

	std::this_thread::sleep_for(std::chrono::milliseconds(110));
	f.core.tick();
	CHECK(h.fired == 1);
	CHECK(timer->calls() == 2);
	CHECK(timer->remaining() <= Milliseconds(1000) && timer->remaining() > Milliseconds(900));
}

TEST_CASE(timers_freed_with_component)
{
	Handler h;
	{
		ComponentFixture<ITimersComponent> f("Timers");
		f.api->create(&h, Milliseconds(10000), true);
		f.api->create(&h, Milliseconds(10000), false);
	}
	CHECK(h.freed == 2);
	CHECK(h.fired == 0);
}
