/*
 * \brief  POSIX semaphore implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/semaphore.h>
#include <semaphore.h>
#include <libc/allocator.h>

/* libc includes */
#include <errno.h>
#include <time.h>

/* libc-internal includes */
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/kernel.h>
#include <internal/monitor.h>
#include <internal/time.h>
#include <internal/types.h>

using namespace Libc;

#define NEW_SEMA 1


static Monitor *_monitor_ptr;


void Libc::init_semaphore_support(Monitor &monitor)
{
	_monitor_ptr = &monitor;
}


extern "C" {

	/*
	 * This class is named 'struct sem' because the 'sem_t' type is
	 * defined as 'struct sem*' in 'semaphore.h'
	 */
	struct sem
	{
		int      _count;
		unsigned _applicants { 0 };
		Lock     _data_mutex;
		Lock     _monitor_mutex; /* TODO remove this one with NEW_SEMA */

		struct Applicant_
		{
			Applicant_ *next { nullptr };

			Libc::Blockade &blockade;

			Applicant_(Libc::Blockade &blockade) : blockade(blockade) { }
		};

		Applicant_ *_applicants_ { nullptr };

		void _append_applicant(Applicant_ *applicant)
		{
			Applicant_ **tail = &_applicants_;

			for (; *tail; tail = &(*tail)->next) ;

			*tail = applicant;
		}

		void _remove_applicant(Applicant_ *applicant)
		{
			Applicant_ **a = &_applicants_;

			for (; *a && *a != applicant; a = &(*a)->next) ;

			*a = applicant->next;
		}

		void _count_up()
		{
			if (Applicant_ *next = _applicants_) {
				_remove_applicant(next);
				next->blockade.wakeup();
			} else {
				++_count;
			}
		}

		bool _applicant_for_semaphore(Libc::Blockade &blockade)
		{
			Applicant_ applicant { blockade };

			_append_applicant(&applicant);

			_data_mutex.unlock();

			blockade.block();

			_data_mutex.lock();

			if (blockade.woken_up()) {
				return true;
			} else {
				_remove_applicant(&applicant);
				return false;
			}
		}

		/**
		 * Enqueue current context as applicant for semaphore
		 *
		 * Return true if down was successful, false on timeout expiration.
		 */
		bool _apply_for_semaphore(Libc::uint64_t timeout_ms)
		{
			/* XXX _data_mutex must be hold */

			if (Libc::Kernel::kernel().main_context()) {
				Main_blockade blockade {
					Libc::Kernel::kernel(), timeout_ms };
				return _applicant_for_semaphore(blockade);
			} else {
				Pthread_blockade blockade {
					Libc::Kernel::kernel().timer_accessor(), timeout_ms };
				return _applicant_for_semaphore(blockade);
			}
		}

		struct Applicant
		{
			sem &s;

			Applicant(sem &s) : s(s)
			{
				Lock::Guard lock_guard(s._data_mutex);
				++s._applicants;
			}

			~Applicant()
			{
				Lock::Guard lock_guard(s._data_mutex);
				--s._applicants;
			}
		};

		struct Missing_call_of_init_semaphore_support : Exception { };

		Monitor & _monitor()
		{
			if (!_monitor_ptr)
				throw Missing_call_of_init_semaphore_support();
			return *_monitor_ptr;
		}

		sem(int value) : _count(value) { }

		int trydown()
		{
			Lock::Guard lock_guard(_data_mutex);
			
			if (_count > 0) {
				_count--;
				return 0;
			}

			return EBUSY;
		}

		int down()
		{
#if NEW_SEMA
			Lock::Guard lock_guard(_data_mutex);
			
			/* fast path without contention */
			if (_count > 0) {
				_count--;
				return 0;
			}

			_apply_for_semaphore(0);
#else
			Lock::Guard monitor_guard(_monitor_mutex);

			/* fast path without contention */
			if (trydown() == 0)
				return 0;

			{
				Applicant guard { *this };

				auto fn = [&] { return trydown() == 0; };

				(void)_monitor().monitor(_monitor_mutex, fn);
			}
#endif

			return 0;
		}

		int down_timed(timespec const &abs_timeout)
		{
#if NEW_SEMA
			Lock::Guard lock_guard(_data_mutex);
			
			/* fast path without contention */
			if (_count > 0) {
				_count--;
				return 0;
			}

			timespec abs_now;
			clock_gettime(CLOCK_REALTIME, &abs_now);

			Libc::uint64_t const timeout_ms = calculate_relative_timeout_ms(abs_now, abs_timeout);
			if (!timeout_ms)
				return ETIMEDOUT;

			if (_apply_for_semaphore(timeout_ms))
				return 0;
			else
				return ETIMEDOUT;
#else
			Lock::Guard monitor_guard(_monitor_mutex);

			/* fast path without wait - does not check abstimeout according to spec */
			if (trydown() == 0)
				return 0;

			timespec abs_now;
			clock_gettime(CLOCK_REALTIME, &abs_now);

			Libc::uint64_t const timeout_ms = calculate_relative_timeout_ms(abs_now, abs_timeout);
			if (!timeout_ms)
				return ETIMEDOUT;

			{
				Applicant guard { *this };

				auto fn = [&] { return trydown() == 0; };

				if (_monitor().monitor(_monitor_mutex, fn, timeout_ms))
					return 0;
				else
					return ETIMEDOUT;
			}
#endif
		}

		int up()
		{
#if NEW_SEMA
			Lock::Guard lock_guard(_data_mutex);

			_count_up();
#else
bool charge = false;
{
			Lock::Guard monitor_guard(_monitor_mutex);
			Lock::Guard lock_guard(_data_mutex);

			_count++;

			if (_applicants)
charge = true;
}
if (charge) {
				_monitor().charge_monitors();
}
#endif

			return 0;
		}

		int count() { return _count; }
	};


	int sem_close(sem_t *)
	{
		warning(__func__, " not implemented");
		return Errno(ENOSYS);
	}


	int sem_destroy(sem_t *sem)
	{
		Libc::Allocator alloc { };
		destroy(alloc, *sem);
		return 0;
	}


	int sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
	{
		*sval = (*sem)->count();
		return 0;
	}


	int sem_init(sem_t *sem, int pshared, unsigned int value)
	{
		Libc::Allocator alloc { };
		*sem = new (alloc) struct sem(value);
		return 0;
	}


	sem_t *sem_open(const char *, int, ...)
	{
		warning(__func__, " not implemented");
		return 0;
	}


	int sem_post(sem_t *sem)
	{
		if (int res = (*sem)->up())
			return Errno(res);

		return 0;
	}


	int sem_timedwait(sem_t * __restrict sem, const struct timespec * __restrict abstime)
	{
		/* abstime must be non-null according to the spec */
		if (int res = (*sem)->down_timed(*abstime))
			return Errno(res);

		return 0;
	}


	int sem_trywait(sem_t *sem)
	{
		if (int res = (*sem)->trydown())
			return Errno(res);

		return 0;
	}


	int sem_unlink(const char *)
	{
		warning(__func__, " not implemented");
		return Errno(ENOSYS);
	}


	int sem_wait(sem_t *sem)
	{
		if (int res = (*sem)->down())
			return Errno(res);

		return 0;
	}

}
