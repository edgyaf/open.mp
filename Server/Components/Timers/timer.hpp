/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "timers_rs.hpp"
#include <Server/Components/Timers/timers.hpp>

/// `ITimer` facade over a timer owned by the Rust implementation. The state is valid for the
/// lifetime of this object: Rust deletes the facade (through `TimersComponent`) before dropping it.
class Timer final : public ITimer
{
private:
	const OmpTimersTimer* state_ = nullptr;
	TimerTimeOutHandler* const handler_;

public:
	explicit Timer(TimerTimeOutHandler* handler)
		: handler_(handler)
	{
	}

	inline void setState(const OmpTimersTimer* state)
	{
		state_ = state;
	}

	bool running() const override
	{
		return omp_timers_timer_running(state_);
	}

	Milliseconds remaining() const override
	{
		return Milliseconds(omp_timers_timer_remaining(state_));
	}

	unsigned int calls() const override
	{
		return omp_timers_timer_calls(state_);
	}

	Milliseconds interval() const override
	{
		return Milliseconds(omp_timers_timer_interval(state_));
	}

	TimerTimeOutHandler* handler() const override
	{
		return handler_;
	}

	void kill() override
	{
		omp_timers_timer_kill(state_);
	}

	bool trigger() override
	{
		return omp_timers_timer_trigger(state_);
	}

	~Timer()
	{
		if (handler_)
		{
			handler_->free(*this);
		}
	}
};
