/*
 * \brief  Time source using Nova timed semaphore down
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* Genode includes */
#include <trace/timestamp.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>

/* local includes */
#include <threaded_time_source.h>

namespace Timer {

	using Genode::uint64_t;
	class Time_source;
}


class Timer::Time_source : public Threaded_time_source
{
	private:

		/* read the tsc frequency from platform info */
		static unsigned _obtain_tsc_mhz(Genode::Env &env)
		{
			unsigned result = 0;
			try {
				Genode::Attached_rom_dataspace info { env, "platform_info"};

				result = info.xml().sub_node("hardware")
				                   .sub_node("tsc")
				                   .attribute_value("freq_khz", 0U);
			} catch (...) { }

			if (result)
				return result / 1000;

			/*
			 * The returned value must never be zero because it is used as
			 * divisor by '_tsc_to_us'.
			 */
			Genode::warning("unable to obtain tsc frequency, assuming 1 GHz");
			return 1'000;
		}

		Genode::addr_t           _sem        { 0 };
		uint64_t                 _timeout_us { 0 };
		unsigned long      const _tsc_mhz;
		Duration                 _curr_time  { Microseconds(0) };
		Genode::Trace::Timestamp _tsc_start  { Genode::Trace::timestamp() };
		Genode::Trace::Timestamp _tsc_last   { _tsc_start };

		uint64_t _us_to_tsc(uint64_t us)  const { return  us * _tsc_mhz; }
		uint64_t _tsc_to_us(uint64_t tsc) const { return tsc / _tsc_mhz; }


		/**************************
		 ** Threaded_time_source **
		 **************************/

		Result_of_wait_for_irq _wait_for_irq() override;

	public:

		Time_source(Genode::Env &env)
		:
			Threaded_time_source(env), _tsc_mhz(_obtain_tsc_mhz(env))
		{
			start();
		}

		/*************************
		 ** Genode::Time_source **
		 *************************/

		void set_timeout(Microseconds duration,
		                 Timeout_handler &handler) override;

		Microseconds max_timeout() const override
		{
			return Microseconds(_tsc_to_us(~(uint64_t)0));
		}

		Duration curr_time() override
		{
			using namespace Genode::Trace;

			Timestamp    const curr_tsc = timestamp();
			Microseconds const diff(_tsc_to_us(curr_tsc - _tsc_last));

			_curr_time.add(diff);
			_tsc_last = curr_tsc;

			return _curr_time;
		}
};

#endif /* _TIME_SOURCE_H_ */
