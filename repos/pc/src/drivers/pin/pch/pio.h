/*
 * \brief  Device interface of the Intel 500 PCH Tigerlake GPIO driver
 * \author Alexander Boettcher
 * \date   2023-07-07
 *
 * Specification looked into
 * - Intel 500 Series Chipset Family On-Package Platform Controller Hub
 *   Datasheet, Volume 2 of 2
 *   Revision 002, February 2023
 *   Chapter 27 - General Purpose I/O (GPIO)
 * - Linux kernel
 */

#ifndef _PIO_H_
#define _PIO_H_

/* Genode includes */
#include <platform_session/device.h>

/* local includes */
#include <types.h>


namespace Pio_driver { struct Pio; }


struct Pio_driver::Pio
{
	/*
	 * Spec refers often to "same as PAD_CFG_DW0_GPPC_F_0"
	 */
	struct Cfg_gppc_f_0 : Register<32> {
		struct Gpio_rx_state      : Bitfield< 1, 1> { };

		struct Gpio_tx_disable    : Bitfield< 8, 1> { };
		struct Gpio_rx_disable    : Bitfield< 9, 1> { };

		struct Pad_mode           : Bitfield<10, 3> { };

		struct Gpio_input_nmi     : Bitfield<17, 1> { };
		struct Gpio_input_smi     : Bitfield<18, 1> { };
		struct Gpio_input_sci     : Bitfield<19, 1> { };
		struct Gpio_input_ioxapic : Bitfield<20, 1> { };

		struct Rx_invert          : Bitfield<23, 1> { };
		struct Rx_level_edge_cfg  : Bitfield<25, 2> {
			enum { LEVEL = 0, EDGE = 1, DISABLE = 2, EDGES = 3 };
		};
	};

	/* by now identical used by us, nevertheless use name as in specification */
	typedef Cfg_gppc_f_0 Cfg_gppc_h_0;
	typedef Cfg_gppc_f_0 Cfg_gppc_b_0;
	typedef Cfg_gppc_f_0 Cfg_gpp_r_0;
	typedef Cfg_gppc_f_0 Cfg_gpd_0;

	struct Common
	{
		template <unsigned COUNT, unsigned START = 0, typename F>
		bool with_index(Pin_id const &id, F const &fn) const
		{
			if ((id.index.value < START) || (id.index.value >= START + COUNT))
				return false;

			/* 4 * 32bit registers per index */
			auto const index = (id.index.value - START) * 4;

			fn(index);

			return true;
		}

		template <typename Reg>
		void configure_irq(Pin_id const &id, Attr const &attr,
		                   auto const &read_register, auto const &write_register)
		{
			auto const &irq       = attr.irq_trigger.value;
			auto const &irq_route = attr.irq_trigger.route;

			if (attr.output()) {
				error(id, " PIN output not supported");
				return;
			}

			auto raw = read_register();

			/* intel_gpio_set_gpio_mode */
			Reg::Pad_mode::set(raw, 0 /* GPIO */);

			Reg::Gpio_input_ioxapic::set(raw, (irq_route == Irq_trigger::Route::IOXAPIC) ? 1 : 0);
			Reg::Gpio_input_sci    ::set(raw, (irq_route == Irq_trigger::Route::SCI)     ? 1 : 0);

			/*
				SMI and NMI bits are read only according to spec
				Reg::Gpio_input_smi    ::set(raw, 0);
				Reg::Gpio_input_nmi    ::set(raw, 0);
			*/

			/* set to input */
			Reg::Gpio_rx_disable::set(raw, 0);
			Reg::Gpio_tx_disable::set(raw, 1);

			if (irq == Irq_trigger::LOW || irq == Irq_trigger::HIGH) {
				Reg::Rx_level_edge_cfg::set(raw, Reg::Rx_level_edge_cfg::LEVEL);
				Reg::Rx_invert        ::set(raw, irq == Irq_trigger::LOW);
			} else
			if (irq == Irq_trigger::RISING || irq == Irq_trigger::FALLING) {
				Reg::Rx_level_edge_cfg::set(raw, Reg::Rx_level_edge_cfg::EDGE);
				Reg::Rx_invert        ::set(raw, irq == Irq_trigger::FALLING);
			}
			else
			if (irq == Irq_trigger::EDGES) {
				Reg::Rx_level_edge_cfg::set(raw, Reg::Rx_level_edge_cfg::EDGES);
				Reg::Rx_invert        ::set(raw, 0);
			} else {
				Reg::Rx_level_edge_cfg::set(raw, Reg::Rx_level_edge_cfg::DISABLE);
				Reg::Rx_invert        ::set(raw, 0);
			}

			write_register(raw);
		}

		template <typename Reg>
		void dump_cfg_state(Pin_id const &id, auto const &read_register) const
		{
			auto const raw = read_register();

			auto const rx         = Reg::Gpio_rx_state::get(raw)   ? " rx pending," : "";
			auto const rx_disable = Reg::Gpio_rx_disable::get(raw) ? " rx disabled," : "";
			auto const tx_disable = Reg::Gpio_tx_disable::get(raw) ? " tx disabled," : "";
			auto const pad_mode   = (Reg::Pad_mode::get(raw) == 0) ? " GPIO," : " Function";

			auto const nmi = Reg::Gpio_input_nmi::get(raw) ? " NMI," : "";
			auto const smi = Reg::Gpio_input_smi::get(raw) ? " SMI," : "";
			auto const sci = Reg::Gpio_input_sci::get(raw) ? " SCI," : "";
			auto const ioa = Reg::Gpio_input_ioxapic::get(raw) ? " IOxAPIC," : "";

			auto const inv    = Reg::Rx_invert::get(raw) ? "inverted," : "";
			auto const levedg = (Reg::Rx_level_edge_cfg::get(raw) == 0) ? "level" :
			                    (Reg::Rx_level_edge_cfg::get(raw) == 1) ? "edge"  :
			                    (Reg::Rx_level_edge_cfg::get(raw) == 2) ? "disabled"  :
			                    (Reg::Rx_level_edge_cfg::get(raw) == 3) ? "edge rising or falling"
			                                                            : "unknown";

			log("state ", id, ": ", rx, rx_disable, tx_disable, pad_mode,
			    (Reg::Pad_mode::get(raw) != 0) ? String<4>(Reg::Pad_mode::get(raw), ",") : String<4>(""),
			    nmi, smi, sci, ioa, inv,
			    " cfg ", levedg);
		}
	};

	struct Community0 : Platform::Device::Mmio, Common
	{
		struct Irq_status_gpp_b : Register <0x100, 32> { };
		struct Irq_status_gpp_t : Register <0x104, 32> { };
		struct Irq_status_gpp_a : Register <0x108, 32> { };

		struct Irq_enable_gpp_b : Register <0x120, 32> { };
		struct Irq_enable_gpp_t : Register <0x124, 32> { };
		struct Irq_enable_gpp_a : Register <0x128, 32> { };

		enum {
			CFG_GPPC_B_COUNT = 24,
			CFG_GPPC_A_COUNT = 24,

			CFG_GPPC_T_COUNT =  2, CFG_GPPC_T_START = 2,
		};

		struct Cfg_gppc_b  : Register_array<0x700, 32,  CFG_GPPC_B_COUNT * 4, 32> { };
		struct Cfg_gppc_t  : Register_array<0x8c0, 32,  CFG_GPPC_T_COUNT * 4, 32> { };
		struct Cfg_gppc_a  : Register_array<0x9a0, 32,  CFG_GPPC_A_COUNT * 4, 32> { };

		template <typename F>
		bool with_cfg(Pin_id const &id, F const &fn)
		{
			switch (id.gpp.value) {
			case Pio_driver::Gpp::B:
				return with_index<CFG_GPPC_B_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_b>(index); },
					   [&](auto raw) {        write<Cfg_gppc_b>(raw, index); });
				});
			case Pio_driver::Gpp::T:
				return with_index<CFG_GPPC_T_COUNT, CFG_GPPC_T_START>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_t>(index); },
					   [&](auto raw) {        write<Cfg_gppc_t>(raw, index); });
				});
			case Pio_driver::Gpp::A:
				return with_index<CFG_GPPC_A_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_a>(index); },
					   [&](auto raw) {        write<Cfg_gppc_a>(raw, index); });
				});
			default:
				return false;
			}

			return false;
		}

		bool configure(Pin_id const &id, Attr const &attr)
		{
			return with_cfg(id, [&](auto const &read_reg, auto const &write_reg) {
				configure_irq<Cfg_gppc_b_0>(id, attr, read_reg, write_reg);
			});
		}

		bool state(Pin_id const &id) const
		{
			bool     ok  = false;
			unsigned raw = 0;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::B:
				ok = with_index<CFG_GPPC_B_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_b> (index);
				});
				break;
			case Pio_driver::Gpp::T:
				ok = with_index<CFG_GPPC_T_COUNT, CFG_GPPC_T_START>(id, [&](auto const index) {
					raw = read<Cfg_gppc_t> (index);
				});
				break;
			case Pio_driver::Gpp::A:
				ok = with_index<CFG_GPPC_A_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_a> (index);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				warning("state of ", id, " unknown");
				return false;
			}

			return !!Cfg_gppc_b_0::Gpio_rx_state::get(raw);
		}

		bool clear_irq_status(Pin_id const &id)
		{
			auto const clear = 1u << id.index.value;
			bool       ok    = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::B:
				ok = with_index<CFG_GPPC_B_COUNT>(id, [&](auto const) {
					write<Irq_status_gpp_b>(clear);
				});
				break;
			case Pio_driver::Gpp::T:
				ok = with_index<CFG_GPPC_T_COUNT, CFG_GPPC_T_START>(id, [&](auto const) {
					write<Irq_status_gpp_t>(clear);
				});
				break;
			case Pio_driver::Gpp::A:
				ok = with_index<CFG_GPPC_A_COUNT>(id, [&](auto const) {
					write<Irq_status_gpp_a>(clear);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return true;
		}

		bool irq_enabled(Pin_id const &id, bool const enabled)
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::B:
				ok = with_index<CFG_GPPC_B_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_b>();
				});
				break;
			case Pio_driver::Gpp::T:
				ok = with_index<CFG_GPPC_T_COUNT, CFG_GPPC_T_START>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_t>();
				});
				break;
			case Pio_driver::Gpp::A:
				ok = with_index<CFG_GPPC_A_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_a>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			if (enabled)
				raw |=  (1u << id.index.value);
			else
				raw &= ~(1u << id.index.value);

			switch (id.gpp.value) {
			case Pio_driver::Gpp::B: write<Irq_enable_gpp_b>(raw); break;
			case Pio_driver::Gpp::T: write<Irq_enable_gpp_t>(raw); break;
			case Pio_driver::Gpp::A: write<Irq_enable_gpp_a>(raw); break;
			default:
				return false;
			}

			return true;
		}

		bool irq_pending(Pin_id const &id) const
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::B:
				ok = with_index<CFG_GPPC_B_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gpp_b>();
				});
				break;
			case Pio_driver::Gpp::T:
				ok = with_index<CFG_GPPC_T_COUNT, CFG_GPPC_T_START>(id, [&](auto const) {
					raw = read<Irq_status_gpp_t>();
				});
				break;
			case Pio_driver::Gpp::A:
				ok = with_index<CFG_GPPC_A_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gpp_a>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return !!(raw & (1u << id.index.value));
		}

		Community0(Platform::Device &dev, Platform::Device::Mmio::Index index)
		: Platform::Device::Mmio(dev, index) { }
	};

	struct Community1 : Platform::Device::Mmio, Common
	{
		struct Irq_status_gpp_s : Register <0x100, 32> { };
		struct Irq_status_gpp_h : Register <0x104, 32> { };
		struct Irq_status_gpp_d : Register <0x108, 32> { };
		struct Irq_status_gpp_u : Register <0x10c, 32> { };

		struct Irq_enable_gpp_s : Register <0x120, 32> { };
		struct Irq_enable_gpp_h : Register <0x124, 32> { };
		struct Irq_enable_gpp_d : Register <0x128, 32> { };
		struct Irq_enable_gpp_u : Register <0x12c, 32> { };

		enum {
			CFG_GPP_S_COUNT  =  8,
			CFG_GPPC_H_COUNT = 24,
			CFG_GPPC_D_COUNT = 20,

			CFG_GPPC_U_START = 4, CFG_GPPC_U_COUNT = 2
		};

		struct Cfg_gpp_s  : Register_array<0x700, 32,  CFG_GPP_S_COUNT * 4, 32> { };
		struct Cfg_gppc_h : Register_array<0x780, 32, CFG_GPPC_H_COUNT * 4, 32> { };
		struct Cfg_gppc_d : Register_array<0x900, 32, CFG_GPPC_D_COUNT * 4, 32> { };
		struct Cfg_gppc_u : Register_array<0xa90, 32, CFG_GPPC_U_COUNT * 4, 32> { };

		Community1(Platform::Device &device, Platform::Device::Mmio::Index index)
		: Platform::Device::Mmio(device, index) { }

		template <typename F>
		bool with_cfg(Pin_id const &id, F const &fn)
		{
			switch (id.gpp.value) {
			case Pio_driver::Gpp::S:
				return with_index<CFG_GPP_S_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gpp_s>(index); },
					   [&](auto raw) {        write<Cfg_gpp_s>(raw, index); });
				});
			case Pio_driver::Gpp::H:
				return with_index<CFG_GPPC_H_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_h>(index); },
					   [&](auto raw) {        write<Cfg_gppc_h>(raw, index); });
				});
			case Pio_driver::Gpp::D:
				return with_index<CFG_GPPC_D_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_d>(index); },
					   [&](auto raw) {        write<Cfg_gppc_d>(raw, index); });
				});
			case Pio_driver::Gpp::U:
				return with_index<CFG_GPPC_U_COUNT, CFG_GPPC_U_START>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_u>(index); },
					   [&](auto raw) {        write<Cfg_gppc_u>(raw, index); });
				});
			default:
				return false;
			}

			return false;
		}

		bool configure(Pin_id const &id, Attr const &attr)
		{
			return with_cfg(id, [&](auto const &read_reg, auto const &write_reg) {
				configure_irq<Cfg_gppc_h_0>(id, attr, read_reg, write_reg);
			});
		}

		bool state(Pin_id const &id) const
		{
			bool     ok  = false;
			unsigned raw = 0;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::S:
				ok = with_index<CFG_GPP_S_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gpp_s> (index);
				});
				break;
			case Pio_driver::Gpp::H:
				ok = with_index<CFG_GPPC_H_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_h> (index);
				});
				break;
			case Pio_driver::Gpp::D:
				ok = with_index<CFG_GPPC_D_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_d> (index);
				});
				break;
			case Pio_driver::Gpp::U:
				ok = with_index<CFG_GPPC_U_COUNT, CFG_GPPC_U_START>(id, [&](auto const index) {
					raw = read<Cfg_gppc_u> (index);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				warning("state of ", id, " unknown");
				return false;
			}

			return !!Cfg_gppc_h_0::Gpio_rx_state::get(raw);
		}

		bool clear_irq_status(Pin_id const &id)
		{
			auto const clear = 1u << id.index.value;
			bool       ok    = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::S:
				ok = with_index<CFG_GPP_S_COUNT>(id, [&](auto const) {
					write<Irq_status_gpp_s>(clear);
				});
				break;
			case Pio_driver::Gpp::H:
				ok = with_index<CFG_GPPC_H_COUNT>(id, [&](auto const) {
					write<Irq_status_gpp_h>(clear);
				});
				break;
			case Pio_driver::Gpp::D:
				ok = with_index<CFG_GPPC_D_COUNT>(id, [&](auto const) {
					write<Irq_status_gpp_d>(clear);
				});
				break;
			case Pio_driver::Gpp::U:
				ok = with_index<CFG_GPPC_U_COUNT, CFG_GPPC_U_START>(id, [&](auto const) {
					write<Irq_status_gpp_u>(clear);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return true;
		}

		bool irq_enabled(Pin_id const &id, bool const enabled)
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::S:
				ok = with_index<CFG_GPP_S_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_s>();
				});
				break;
			case Pio_driver::Gpp::H:
				ok = with_index<CFG_GPPC_H_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_h>();
				});
				break;
			case Pio_driver::Gpp::D:
				ok = with_index<CFG_GPPC_D_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_d>();
				});
				break;
			case Pio_driver::Gpp::U:
				ok = with_index<CFG_GPPC_U_COUNT, CFG_GPPC_U_START>(id, [&](auto const) {
					raw = read<Irq_enable_gpp_u>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			if (enabled)
				raw |=  (1u << id.index.value);
			else
				raw &= ~(1u << id.index.value);

			switch (id.gpp.value) {
			case Pio_driver::Gpp::S: write<Irq_enable_gpp_s>(raw); break;
			case Pio_driver::Gpp::H: write<Irq_enable_gpp_h>(raw); break;
			case Pio_driver::Gpp::D: write<Irq_enable_gpp_d>(raw); break;
			case Pio_driver::Gpp::U: write<Irq_enable_gpp_u>(raw); break;
			default:
				return false;
			}

			return true;
		}

		bool irq_pending(Pin_id const &id) const
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::S:
				ok = with_index<CFG_GPP_S_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gpp_s>();
				});
				break;
			case Pio_driver::Gpp::H:
				ok = with_index<CFG_GPPC_H_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gpp_h>();
				});
				break;
			case Pio_driver::Gpp::D:
				ok = with_index<CFG_GPPC_D_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gpp_d>();
				});
				break;
			case Pio_driver::Gpp::U:
				ok = with_index<CFG_GPPC_U_COUNT, CFG_GPPC_U_START>(id, [&](auto const) {
					raw = read<Irq_status_gpp_u>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return !!(raw & (1u << id.index.value));
		}
	};

	struct Community2 : Platform::Device::Mmio, Common
	{
		struct Irq_status_dsw_0 : Register <0x100, 32> { };

		struct Irq_enable_dsw_0 : Register <0x120, 32> { };

		enum { CFG_GPD_COUNT = 12 };

		struct Cfg_gpd : Register_array<0x700, 32, CFG_GPD_COUNT * 4, 32> { };

		Community2(Platform::Device &dev, Platform::Device::Mmio::Index index)
		: Platform::Device::Mmio(dev, index) { }

		template <typename F>
		bool with_cfg(Pin_id const &id, F const &fn)
		{
			return with_index<CFG_GPD_COUNT>(id, [&](auto const index) {
				fn([&]()         { return read <Cfg_gpd>(index); },
				   [&](auto raw) {        write<Cfg_gpd>(raw, index); });
			});
		}

		bool configure(Pin_id const &id, Attr const &attr)
		{
			return with_cfg(id, [&](auto const &read_reg, auto const &write_reg) {
				configure_irq<Cfg_gpd_0>(id, attr, read_reg, write_reg);
			});
		}

		bool state(Pin_id const &id) const
		{
			unsigned raw = 0;

			bool const ok = with_index<CFG_GPD_COUNT>(id, [&](auto const index) {
				raw = read<Cfg_gpd> (index);
			});

			if (!ok) {
				warning("state of ", id, " unknown");
				return false;
			}

			return !!Cfg_gpd_0::Gpio_rx_state::get(raw);
		}

		bool clear_irq_status(Pin_id const &id)
		{
			bool const ok = with_index<CFG_GPD_COUNT>(id, [&](auto) {
				auto const clear = 1u << id.index.value;
				write<Irq_status_dsw_0>(clear);
			});

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return true;
		}

		bool irq_enabled(Pin_id const &id, bool const enabled)
		{
			unsigned raw = 0;

			bool const ok = with_index<CFG_GPD_COUNT>(id, [&](auto) {
				raw = read<Irq_enable_dsw_0>();
			});

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			if (enabled)
				raw |=  (1u << id.index.value);
			else
				raw &= ~(1u << id.index.value);

			write<Irq_enable_dsw_0>(raw);

			return true;
		}

		bool irq_pending(Pin_id const &id) const
		{
			unsigned raw = 0;

			bool const ok = with_index<CFG_GPD_COUNT>(id, [&](auto const) {
				raw = read<Irq_status_dsw_0>();
			});

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return !!(raw & (1u << id.index.value));
		}
	};

	struct Community4 : Platform::Device::Mmio, Common
	{
		struct Irq_status_gppc_c : Register <0x100, 32> { };
		struct Irq_status_gppc_f : Register <0x104, 32> { };
		struct Irq_status_gppc_e : Register <0x10c, 32> { };

		struct Irq_enable_gppc_c : Register <0x120, 32> { };
		struct Irq_enable_gppc_f : Register <0x124, 32> { };
		struct Irq_enable_gppc_e : Register <0x12c, 32> { };

		/* all GPPs - C, F, E - have same count in this coummunity4 */
		enum { CFG_GPPC_CFE_COUNT = 24 };

		struct Cfg_gppc_c : Register_array<0x700, 32, CFG_GPPC_CFE_COUNT * 4, 32> { };
		struct Cfg_gppc_f : Register_array<0x880, 32, CFG_GPPC_CFE_COUNT * 4, 32> { };
		struct Cfg_gppc_e : Register_array<0xa70, 32, CFG_GPPC_CFE_COUNT * 4, 32> { };

		Community4(Platform::Device &dev, Platform::Device::Mmio::Index index)
		: Platform::Device::Mmio(dev, index) { }

		template <typename F>
		bool with_cfg(Pin_id const &id, F const &fn)
		{
			switch (id.gpp.value) {
			case Pio_driver::Gpp::C:
				return with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_c>(index); },
					   [&](auto raw) {        write<Cfg_gppc_c>(raw, index); });
				});
			case Pio_driver::Gpp::E:
				return with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_e>(index); },
					   [&](auto raw) {        write<Cfg_gppc_e>(raw, index); });
				});
			case Pio_driver::Gpp::F:
				return with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gppc_f>(index); },
					   [&](auto raw) {        write<Cfg_gppc_f>(raw, index); });
				});
			default:
				return false;
			}

			return false;
		}

		bool configure(Pin_id const &id, Attr const &attr)
		{
			return with_cfg(id, [&](auto const &read_reg, auto const &write_reg) {
				configure_irq<Cfg_gppc_f_0>(id, attr, read_reg, write_reg);
			});
		}

		bool state(Pin_id const &id) const
		{
			bool     ok  = false;
			unsigned raw = 0;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::C:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_c> (index);
				});
				break;
			case Pio_driver::Gpp::E:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_e> (index);
				});
				break;
			case Pio_driver::Gpp::F:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gppc_f> (index);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				warning("state of ", id, " unknown");
				return false;
			}

			return !!Cfg_gppc_f_0::Gpio_rx_state::get(raw);
		}

		bool clear_irq_status(Pin_id const &id)
		{
			auto const clear = 1u << id.index.value;
			bool       ok    = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::C:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto) {
					 write<Irq_status_gppc_c>(clear);
				});
				break;
			case Pio_driver::Gpp::E:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto) {
					 write<Irq_status_gppc_e>(clear);
				});
				break;
			case Pio_driver::Gpp::F:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto) {
					write<Irq_status_gppc_f>(clear);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return true;
		}

		bool irq_enabled(Pin_id const &id, bool const enabled)
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::C:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gppc_c>();
				});
				break;
			case Pio_driver::Gpp::E:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gppc_e>();
				});
				break;
			case Pio_driver::Gpp::F:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_enable_gppc_f>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			if (enabled)
				raw |=  (1u << id.index.value);
			else
				raw &= ~(1u << id.index.value);

			switch (id.gpp.value) {
			case Pio_driver::Gpp::C: write<Irq_enable_gppc_c>(raw); break;
			case Pio_driver::Gpp::E: write<Irq_enable_gppc_e>(raw); break;
			case Pio_driver::Gpp::F: write<Irq_enable_gppc_f>(raw); break;
			default:
				return false;
			}

			return true;
		}

		bool irq_pending(Pin_id const &id) const
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::C:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gppc_c>();
				});
				break;
			case Pio_driver::Gpp::E:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gppc_e>();
				});
				break;
			case Pio_driver::Gpp::F:
				ok = with_index<CFG_GPPC_CFE_COUNT>(id, [&](auto const) {
					raw = read<Irq_status_gppc_f>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return !!(raw & (1u << id.index.value));
		}
	};

	struct Community5 : Platform::Device::Mmio, Common
	{
		struct Irq_status_gpp_r : Register <0x100, 32> { };

		struct Irq_enable_gpp_r : Register <0x120, 32> { };

		enum { CFG_GPP_R_COUNT = 8 };

		struct Cfg_gpp_r : Register_array<0x700, 32, CFG_GPP_R_COUNT * 4, 32> { };

		template <typename F>
		bool with_cfg(Pin_id const &id, F const &fn)
		{
			switch (id.gpp.value) {
			case Pio_driver::Gpp::R:
				return with_index<CFG_GPP_R_COUNT>(id, [&](auto const index) {
					fn([&]()         { return read <Cfg_gpp_r>(index); },
					   [&](auto raw) {        write<Cfg_gpp_r>(raw, index); });
				});
			default:
				return false;
			}

			return false;
		}

		bool configure(Pin_id const &id, Attr const &attr)
		{
			return with_cfg(id, [&](auto const &read_reg, auto const &write_reg) {
				configure_irq<Cfg_gpp_r_0>(id, attr, read_reg, write_reg);
			});
		}

		bool state(Pin_id const &id) const
		{
			bool     ok  = false;
			unsigned raw = 0;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::R:
				ok = with_index<CFG_GPP_R_COUNT>(id, [&](auto const index) {
					raw = read<Cfg_gpp_r> (index);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				warning("state of ", id, " unknown");
				return false;
			}

			return !!Cfg_gpp_r_0::Gpio_rx_state::get(raw);
		}

		bool clear_irq_status(Pin_id const &id)
		{
			auto const clear = 1u << id.index.value;
			bool       ok    = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::R:
				ok = with_index<CFG_GPP_R_COUNT>(id, [&](auto) {
					 write<Irq_status_gpp_r>(clear);
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return true;
		}

		bool irq_enabled(Pin_id const &id, bool const enabled)
		{
			unsigned raw = 0;
			bool     ok  = false;

			switch (id.gpp.value) {
			case Pio_driver::Gpp::R:
				ok = with_index<CFG_GPP_R_COUNT>(id, [&](auto) {
					raw = read<Irq_enable_gpp_r>();
				});
				break;
			default:
				break;
			}

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			if (enabled)
				raw |=  (1u << id.index.value);
			else
				raw &= ~(1u << id.index.value);

			switch (id.gpp.value) {
			case Pio_driver::Gpp::R: write<Irq_enable_gpp_r>(raw); break;
			default:
				return false;
			}

			return true;
		}

		bool irq_pending(Pin_id const &id) const
		{
			unsigned raw = 0;

			bool const ok = with_index<CFG_GPP_R_COUNT>(id, [&](auto const) {
				raw = read<Irq_status_gpp_r>();
			});

			if (!ok) {
				error(__func__, ": ", id, " unhandled");
				return false;
			}

			return !!(raw & (1u << id.index.value));
		}

		Community5(Platform::Device &dev, Platform::Device::Mmio::Index index)
		: Platform::Device::Mmio(dev, index) { }
	};

	Community5 _community5;
	Community4 _community4;
	Community2 _community2;
	Community1 _community1;
	Community0 _community0;

	template <typename PIO, typename FN>
	static bool _with_community(PIO &pio, Pin_id const &id, FN const &fn)
	{
		if (id.community.value == 2) {
			error("community 2 not supported");
			return false;
		}

		if (id.community.value == 0) fn(pio._community0); else
		if (id.community.value == 1) fn(pio._community1); else
		if (id.community.value == 2) fn(pio._community2); else
		if (id.community.value == 4) fn(pio._community4); else
		if (id.community.value == 5) fn(pio._community5); else
			return false;

		return true;
	}

	Pio(Platform::Device &device)
	:
		_community5 (device, Platform::Device::Mmio::Index(0)),
		_community4 (device, Platform::Device::Mmio::Index(1)),
		_community2 (device, Platform::Device::Mmio::Index(1)), /* XXX - wrong */
		_community1 (device, Platform::Device::Mmio::Index(2)),
		_community0 (device, Platform::Device::Mmio::Index(3))
	{ }

	void configure(Pin_id const &id, Attr const &attr)
	{
		bool       ok      = false;
		bool const handled = _with_community(*this, id, [&](auto &community) {
			ok = community.configure(id, attr); });

		if (!handled || !ok)
			warning(__func__, " ", id, " output=", attr.output(),
			        " not handled");
	}

	bool state(Pin_id const &id) const
	{
		bool result = false;

		bool const handled = _with_community(*this, id, [&] (auto const &community) {
			result = community.state(id); });

		if (!handled)
			warning(__func__, " ", id, " not handled");

		return result;
	}

	void state(Pin_id const &id, Pin::Level /* level */)
	{
		Genode::error(__func__, " called ", id, " level ");
	}

	void clear_irq_status(Pin_id const &id)
	{
		bool ok = false;

		bool const handled = _with_community(*this, id, [&] (auto &community) {
			ok = community.clear_irq_status(id); });

		if (!handled && !ok)
			warning(__func__, " ", id, " not handled");
	}

	void irq_enabled(Pin_id const &id, bool const enabled)
	{
		bool ok = false;

		bool const handled = _with_community(*this, id, [&] (auto &community) {
			ok = community.irq_enabled(id, enabled); });

		if (!handled && !ok)
			warning(__func__, " ", id, " not handled");
	}

	bool irq_pending(Pin_id const &id) const
	{
		bool pending = false;

		bool const handled = _with_community(*this, id, [&] (auto &community) {
			pending = community.irq_pending(id); });

		if (!handled)
			warning(__func__, " ", id, " not handled");

		return pending;
	}
};

#endif /* _PIO_H_ */
