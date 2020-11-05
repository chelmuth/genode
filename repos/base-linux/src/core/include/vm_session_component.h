/*
 * \brief  Linux stub for the VM session interface
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-11-17
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VM_SESSION_COMPONENT_H_
#define _CORE__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/session_object.h>
#include <base/attached_ram_dataspace.h>
#include <vm_session/vm_session.h>

/* core-internal includes */
#include <region_map_component.h>

namespace Genode { class Vm_session_component; }


class Genode::Vm_session_component
:
	public Session_object<Vm_session, Vm_session_component>
{
	public:

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;

		Attached_ram_dataspace _dummy_vcpu_state;


		Signal_context_capability _sigh { };

		Vm_session_component(Rpc_entrypoint &ep, Resources resources,
		                     Label const &label, Diag diag,
		                     Ram_allocator &ram, Region_map &local_rm, unsigned,
		                     Trace::Source_registry &)
		:
			Session_object(ep, resources, label, diag),
			_dummy_vcpu_state(ram, local_rm, 4096)
		{ }

		~Vm_session_component() { }

		/**************************
		 ** Vm session interface **
		 **************************/

		Capability<Native_vcpu> create_vcpu(Thread_capability thread_cap)
		{
			diag("create vCPU bound to ", thread_cap);
			return { };
		}

		void attach(Dataspace_capability, addr_t at, Attach_attr attr) override
		{
			diag("attach at=",  Hex(at),          " "
			     "offset=",     Hex(attr.offset), " "
			     "size=",       Hex(attr.size),   " "
			     "writeable=",  attr.writeable,   " "
			     "executable=", attr.executable);
		}

		void attach_pic(addr_t) override { }

		void detach(addr_t at, size_t size) override
		{
			diag("detach at=", (void *)at, " size=", Hex(size));
		}
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */