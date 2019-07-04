/*
 * \brief  Example app to utilize ACPICA library
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/log.h>
#include <base/signal.h>
#include <base/heap.h>
#include <os/backtrace.h>
#include <irq_session/connection.h>
#include <io_port_session/connection.h>

#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

#include <util/reconstructible.h>
#include <util/xml_node.h>

#include <acpica/acpica.h>


extern "C" {
#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"
#include "acutils.h"
}

namespace Acpica {
	struct Main;
	struct Statechange;
	class Reportstate;
	struct Heap;
};

#include "util.h"
#include "reporter.h"
#include "fixed.h"


struct Acpica::Statechange
{
	Genode::Signal_handler<Acpica::Statechange> _dispatcher;
	Genode::Attached_rom_dataspace _system_state;
	bool _enable_reset;
	bool _enable_poweroff;

	Statechange(Genode::Env &env, bool reset, bool poweroff)
	:
		_dispatcher(env.ep(), *this, &Statechange::state_changed),
		_system_state(env, "system"),
		_enable_reset(reset), _enable_poweroff(poweroff)
	{
		_system_state.sigh(_dispatcher);

		state_changed();
	}

	void state_changed() {

		_system_state.update();

		if (!_system_state.valid()) return;

		Genode::Xml_node system(_system_state.local_addr<char>(),
		                       _system_state.size());

		typedef Genode::String<32> State;
		State const state = system.attribute_value("state", State());

		if (_enable_poweroff && state == "poweroff") {
			ACPI_STATUS res0 = AcpiEnterSleepStatePrep(5);
			ACPI_STATUS res1 = AcpiEnterSleepState(5);
			Genode::error("system poweroff failed - "
			              "res=", Genode::Hex(res0), ",", Genode::Hex(res1));
			return;
		}

		if (_enable_reset && state == "reset") {
			ACPI_STATUS res = AE_OK;
			try {
				res = AcpiReset();
			} catch (...) { }

			Genode::uint64_t const space_addr = AcpiGbl_FADT.ResetRegister.Address;
			Genode::error("system reset failed - "
			              "err=", res, " "
			              "reset=", !!(AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER), " "
			              "spaceid=", Genode::Hex(AcpiGbl_FADT.ResetRegister.SpaceId), " "
			              "addr=", Genode::Hex(space_addr));
		}
	}
};


struct Acpica::Heap : Genode::Heap
{
	enum {
		MAX_ALLOCATION      = 40000,
		MAX_GENERATION      =   100,
		START_FREE_COUNT    =  8000,
		LOG_FREE_COUNT_STEP =   500,
	};

	struct T
	{
		struct A
		{
			void     *p { nullptr };
			unsigned  s { 0 };
			unsigned  c { 0 };
			unsigned  g { 0 };

			Genode::addr_t bt[15] { 0, 0, 0, 0, 0,
			                        0, 0, 0, 0, 0,
			                        0, 0, 0, 0, 0}; /* backtrace elements */

			A() { }
			A(void *p, unsigned s, unsigned c) : p(p), s(s), c(c) { }

			void print(Genode::Output &out) const
			{
				Genode::print(out, "[", c, "]");
				Genode::print(out, " ", p, " s=", s, " g=", g);
				Genode::print(out, " {");
				for (unsigned i = 0; i < sizeof(bt)/sizeof(*bt); ++i) {
					Genode::print(out, " ", (void *)bt[i]);
				}
				Genode::print(out, " }");
				ACPI_OBJECT_COMMON *o = (ACPI_OBJECT_COMMON *)p;
				Genode::print(out, " ", o->DescriptorType);
				Genode::print(out, " ", o->Type);
				Genode::print(out, " ", o->ReferenceCount);
			}
		};

		unsigned long free_count { 0 };

		A allocations[MAX_ALLOCATION];

		T() { }

		void alloc(void *p, Genode::size_t s)
		{
			if (!p || free_count < START_FREE_COUNT) return;

			bool stored = false;
			for (A &a : allocations) {
				if (a.p != nullptr) continue;

				a = A(p, (unsigned)s, free_count);
				Genode::backtrace(sizeof(a.bt)/sizeof(*a.bt), a.bt);
				stored = true;
				break;
			}
			if (!stored) {
				Genode::error(__PRETTY_FUNCTION__, ": MAX_ALLOCATION (",
				              (int)MAX_ALLOCATION, ") exceeded!");
			}
		}

		void free(void *p)
		{
			if (!p || ++free_count < START_FREE_COUNT) return;

			bool const do_log = (free_count % LOG_FREE_COUNT_STEP == 0);

			if (do_log)
				Genode::error("--------------- free_count=", free_count, " ---------------");

			unsigned sum = 0;
			unsigned index = 0;
			for (A &a : allocations) {
				if (a.p == p) {
					a = A();
				} else {
					if (a.p && ++a.g > MAX_GENERATION && do_log) {
						Genode::warning("[", index, "] ", a);
						sum += a.s;
					}
				}
				++index;
			}

			if (do_log) {
				Genode::error("---------------- sum=", sum, " ------------------------");
				AcpiUtDumpAllocations ((UINT32) -1, nullptr);
			}
		}
	} t;

	using Genode::Heap::Heap;

	bool alloc(Genode::size_t s, void **p) override
	{
		bool const r = Genode::Heap::alloc(s, p);
		t.alloc(*p, s);
		return r;
	}

	void free(void *p, Genode::size_t s) override
	{
		t.free(p);
		/* we keep all allocations for now Genode::Heap::free(p, s); */
	}
};


struct Acpica::Main
{
	Genode::Env  &env;
	Acpica::Heap  heap { env.ram(), env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Acpica::Main>          sci_irq;
	Genode::Constructible<Genode::Irq_connection> sci_conn;

	Acpica::Reportstate * report { nullptr };

	unsigned unchanged_state_count { 0 };
	unsigned unchanged_state_max;

	static struct Irq_handler {
		UINT32 irq;
		ACPI_OSD_HANDLER handler;
		void *context;
	} irq_handler;

	void init_acpica(Acpica::Wait_acpi_ready, Acpica::Act_as_acpi_drv);

	Main(Genode::Env &env)
	:
		env(env),
		sci_irq(env.ep(), *this, &Main::acpi_irq),
		unchanged_state_max(config.xml().attribute_value("update_unchanged", 20U))
	{
		bool const enable_reset    = config.xml().attribute_value("reset", false);
		bool const enable_poweroff = config.xml().attribute_value("poweroff", false);
		bool const enable_report   = config.xml().attribute_value("report", false);
		bool const enable_ready    = config.xml().attribute_value("acpi_ready", false);
		bool const act_as_acpi_drv = config.xml().attribute_value("act_as_acpi_drv", false);

		if (enable_report)
			report = new (heap) Acpica::Reportstate(env);

		init_acpica(Wait_acpi_ready{enable_ready},
		            Act_as_acpi_drv{act_as_acpi_drv});

		if (enable_report)
			report->enable();

		if (enable_reset || enable_poweroff)
			new (heap) Acpica::Statechange(env, enable_reset, enable_poweroff);

		/* setup IRQ */
		if (!irq_handler.handler) {
			Genode::warning("no IRQ handling available");
			return;
		}

		sci_conn.construct(env, irq_handler.irq);

		Genode::log("SCI IRQ: ", irq_handler.irq);

		sci_conn->sigh(sci_irq);
		sci_conn->ack_irq();

		if (!enable_ready)
			return;

		/* we are ready - signal it via changing system state */
		static Genode::Reporter _system_rom(env, "system", "acpi_ready");
		_system_rom.enabled(true);
		Genode::Reporter::Xml_generator xml(_system_rom, [&] () {
			xml.attribute("state", "acpi_ready");
		});
	}

	void acpi_irq()
	{
		if (!irq_handler.handler)
			return;

		UINT32 res = irq_handler.handler(irq_handler.context);

		sci_conn->ack_irq();

		AcpiOsWaitEventsComplete();

		if (report) {
			bool const changed = report->generate_report();

			if (unchanged_state_max) {
				if (changed)
					unchanged_state_count = 0;
				else
					unchanged_state_count ++;

				if (unchanged_state_count >= unchanged_state_max) {
					Genode::log("generate report because of ",
					            unchanged_state_count, " irqs without state "
					            "changes");
					report->generate_report(true);
					unchanged_state_count = 0;
				}
			}
		}

		if (res == ACPI_INTERRUPT_HANDLED)
			return;
	}
};

#include "ac.h"
#include "lid.h"
#include "sb.h"
#include "ec.h"
#include "bridge.h"

ACPI_STATUS init_pic_mode()
{
	ACPI_OBJECT_LIST arguments;
	ACPI_OBJECT      argument;

	arguments.Count = 1;
	arguments.Pointer = &argument;

	enum { PIC = 0, APIC = 1, SAPIC = 2};

	argument.Type = ACPI_TYPE_INTEGER;
	argument.Integer.Value = APIC;

	return AcpiEvaluateObject(ACPI_ROOT_OBJECT, ACPI_STRING("_PIC"),
	                          &arguments, nullptr);
}

ACPI_STATUS Bridge::detect(ACPI_HANDLE bridge, UINT32, void * m,
                           void **return_bridge)
{
	Acpica::Main * main = reinterpret_cast<Acpica::Main *>(m);
	Bridge * dev_obj = new (main->heap) Bridge(main->report, bridge);

	if (*return_bridge == (void *)PCI_ROOT_HID_STRING)
		Genode::log("detected - bridge - PCI root bridge");
	if (*return_bridge == (void *)PCI_EXPRESS_ROOT_HID_STRING)
		Genode::log("detected - bridge - PCIE root bridge");

	*return_bridge = dev_obj;

	return AE_OK;
}

void Acpica::Main::init_acpica(Wait_acpi_ready wait_acpi_ready,
                               Act_as_acpi_drv act_as_acpi_drv)
{
	Acpica::init(env, heap, wait_acpi_ready, act_as_acpi_drv);

	/* enable debugging: */
	/* AcpiDbgLevel |= ACPI_LV_IO | ACPI_LV_INTERRUPTS | ACPI_LV_INIT_NAMES; */

	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (status != AE_OK) {
		Genode::error("AcpiInitializeSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeTables(nullptr, 0, true);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeTables failed, status=", status);
		return;
	}

	status = AcpiLoadTables();
	if (status != AE_OK) {
		Genode::error("AcpiLoadTables failed, status=", status);
		return;
	}

	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiEnableSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_NO_DEVICE_INIT);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (no devices) failed, status=", status);
		return;
	}

	/* set APIC mode */
	status = init_pic_mode();
	if (status != AE_OK) {
		Genode::error("Setting PIC mode failed, status=", status);
		return;
	}

	/* Embedded controller */
	status = AcpiGetDevices(ACPI_STRING("PNP0C09"), Ec::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (full init) failed, status=", status);
		return;
	}

	status = AcpiUpdateAllGpes();
	if (status != AE_OK) {
		Genode::error("AcpiUpdateAllGpes failed, status=", status);
		return;
	}

	status = AcpiEnableAllRuntimeGpes();
	if (status != AE_OK) {
		Genode::error("AcpiEnableAllRuntimeGpes failed, status=", status);
		return;
	}

	/* note: ACPI_EVENT_PMTIMER claimed by nova kernel - not usable by us */
	Fixed * acpi_fixed = new (heap) Fixed(report);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
	                                      Fixed::handle_power_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - power button registration - error=", status);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
	                                      Fixed::handle_sleep_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - sleep button registration - error=", status);


	/* AC Adapters and Power Source Objects */
	status = AcpiGetDevices(ACPI_STRING("ACPI0003"), Ac::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (ACPI0003) failed, status=", status);
		return;
	}

	/* Smart battery control devices */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0A"), Battery::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0A) failed, status=", status);
		return;
	}

	/* LID device */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0D"), Lid::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0D) failed, status=", status);
		return;
	}

	if (act_as_acpi_drv.enabled) {
		/* lookup PCI root bridge */
		void * pci_bridge = (void *)PCI_ROOT_HID_STRING;
		status = AcpiGetDevices(ACPI_STRING(PCI_ROOT_HID_STRING), Bridge::detect,
		                        this, &pci_bridge);
		if (status != AE_OK || pci_bridge == (void *)PCI_ROOT_HID_STRING)
			pci_bridge = nullptr;

		/* lookup PCI Express root bridge */
		void * pcie_bridge = (void *)PCI_EXPRESS_ROOT_HID_STRING;
		status = AcpiGetDevices(ACPI_STRING(PCI_EXPRESS_ROOT_HID_STRING),
		                        Bridge::detect, this, &pcie_bridge);
		if (status != AE_OK || pcie_bridge == (void *)PCI_EXPRESS_ROOT_HID_STRING)
			pcie_bridge = nullptr;

		if (pcie_bridge && pci_bridge)
			Genode::log("PCI and PCIE root bridge found - using PCIE for IRQ "
			            "routing information");

		Bridge *bridge = pcie_bridge ? reinterpret_cast<Bridge *>(pcie_bridge)
		                             : reinterpret_cast<Bridge *>(pci_bridge);

		/* Generate report for platform driver */
		Acpica::generate_report(env, bridge);
	}

	/* Tell PCI backend to use platform_drv for PCI device access from now on */
	Acpica::use_platform_drv();
}


struct Acpica::Main::Irq_handler Acpica::Main::irq_handler;


ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler,
                                          void *context)
{
	Acpica::Main::irq_handler.irq = irq;
	Acpica::Main::irq_handler.handler = handler;
	Acpica::Main::irq_handler.context = context;
	return AE_OK;
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Acpica::Main main(env);
}
