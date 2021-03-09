/*
 * \brief  VirtualBox libc runtime: pthread adaptions
 * \author Christian Helmuth
 * \date   2021-03-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* libc includes */
#include <errno.h>
#include <sched.h>
#include <pthread.h>

/* libc internal */
#include <internal/thread_create.h> /* Libc::pthread_create() */

/* VirtualBox includes */
#include <VBox/vmm/uvm.h>
#include <internal/thread.h> /* RTTHREADINT etc. */

/* Genode includes */
#include <base/env.h>
#include <base/entrypoint.h>
#include <base/registry.h>
#include <util/string.h>
#include <util/reconstructible.h>

/* local includes */
#include <init.h>
#include <pthread_emt.h>
#include <sup.h>
#include <stub_macros.h>

static bool const debug = true; /* required by stub_macros.h */


using namespace Genode;


extern "C" int sched_yield()
{
	static unsigned long counter = 0;

	if (++counter % 100'000 == 0)
		warning(__func__, " called ", counter, " times");

	return 0;
}

extern "C" int sched_get_priority_max(int policy) TRACE(0)
extern "C" int sched_get_priority_min(int policy) TRACE(0)
extern "C" int pthread_setschedparam(pthread_t thread, int policy,
                          const struct sched_param *param) TRACE(0)
extern "C" int pthread_getschedparam(pthread_t thread, int *policy,
                          struct sched_param *param) TRACE(0)


static void print(Output &o, RTTHREADTYPE type)
{
	switch (type) {
	case RTTHREADTYPE_INFREQUENT_POLLER: print(o, "POLLER");            return;
	case RTTHREADTYPE_MAIN_HEAVY_WORKER: print(o, "MAIN_HEAVY_WORKER"); return;
	case RTTHREADTYPE_EMULATION:         print(o, "EMULATION");         return;
	case RTTHREADTYPE_DEFAULT:           print(o, "DEFAULT");           return;
	case RTTHREADTYPE_GUI:               print(o, "GUI");               return;
	case RTTHREADTYPE_MAIN_WORKER:       print(o, "MAIN_WORKER");       return;
	case RTTHREADTYPE_VRDP_IO:           print(o, "VRDP_IO");           return;
	case RTTHREADTYPE_DEBUGGER:          print(o, "DEBUGGER");          return;
	case RTTHREADTYPE_MSG_PUMP:          print(o, "MSG_PUMP");          return;
	case RTTHREADTYPE_IO:                print(o, "IO");                return;
	case RTTHREADTYPE_TIMER:             print(o, "TIMER");             return;

	case RTTHREADTYPE_INVALID: print(o, "invalid?"); return;
	case RTTHREADTYPE_END:     print(o, "end?");     return;
	}
}


namespace Pthread {

	struct Entrypoint;
	struct Factory;

} /* namespace Pthread */


struct Pthread::Entrypoint
{
	Sup::Cpu_index const cpu;
	size_t         const stack_size; /* stack size for EMT mode */

	Genode::Entrypoint genode_ep;

	Entrypoint(Env &env, Sup::Cpu_index cpu, size_t stack_size,
	           char const *name, Affinity::Location location)
	:
		cpu(cpu), stack_size(stack_size),
		genode_ep(env, 64*1024, name, location)
	{ }

	/* registered object must have virtual destructor */
	virtual ~Entrypoint() { }

};


class Pthread::Factory
{
	private:

		Env &_env;

		Registry<Registered<Pthread::Entrypoint>> _entrypoints;

		Affinity::Space const _affinity_space { _env.cpu().affinity_space() };

	public:

		Factory(Env &env) : _env(env) { }

		Entrypoint & create(Sup::Cpu_index cpu, size_t stack_size)
		{

			Affinity::Location const location =
				_affinity_space.location_of_index(cpu.value);

			Genode::String<12> const name("EP-EMT-", cpu.value);

			return *new Registered<Entrypoint>(_entrypoints, _env, cpu, stack_size,
			                                   name.string(), location);
		}

		struct Entrypoint_for_cpu_not_found : Exception { };

		Genode::Entrypoint & genode_ep_for_cpu(Sup::Cpu_index cpu)
		{
			Entrypoint *found = nullptr;

			_entrypoints.for_each([&] (Entrypoint &ep) {
				if (ep.cpu.value == cpu.value)
					found = &ep;
			});

			if (found)
				return found->genode_ep;

			throw Entrypoint_for_cpu_not_found();
		}
};


static Pthread::Factory *factory;


Genode::Entrypoint & Pthread::genode_ep_for_cpu(Sup::Cpu_index cpu)
{
	return factory->genode_ep_for_cpu(cpu);
}


void Pthread::init(Env &env)
{
	factory = new Pthread::Factory(env);
}


static int create_emt_thread(pthread_t *thread, const pthread_attr_t *attr,
                             void *(*start_routine) (void *),
                             PRTTHREADINT rtthread)
{
	PUVMCPU pUVCpu = (PUVMCPU)rtthread->pvUser;

	log("************ ", __func__, ":"
	   , " idCpu=", pUVCpu->idCpu
	   );

	Sup::Cpu_index const cpu { pUVCpu->idCpu };

	size_t stack_size = 0;

	/* try to fetch configured stack size form attribute */
	pthread_attr_getstacksize(attr, &stack_size);

	Assert(stack_size);

	Pthread::Entrypoint &ep = factory->create(cpu, stack_size);

	/* TODO */ (void)ep;
	/* TODO sync with thread startup */
	/* TODO *thread = ep.pthread() */

	/* remove replace thread creation by registration */
	return Libc::pthread_create(thread, attr, start_routine, rtthread);
}



extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg)
{
	PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

error("************ ", __func__, ":"
     , " szName='", Cstring(rtthread->szName), "'"
     , " enmType=", rtthread->enmType
     , " cbStack=", rtthread->cbStack
     );

	/*
	 * Emulation threads (EMT) represent the guest CPU, so we implement them in
	 * dedicated entrypoints that also handle vCPU events in combination with
	 * user-level threading (i.e., setjmp/longjmp).
	 */
	if (rtthread->enmType == RTTHREADTYPE_EMULATION)
		return create_emt_thread(thread, attr, start_routine, rtthread);
	else
		return Libc::pthread_create(thread, attr, start_routine, arg);
}
