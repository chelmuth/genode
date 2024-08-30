/*
 * \brief  Multi-core packet-stream stress test (client)
 * \author Christian Helmuth
 * \date   2023-09-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/attached_rom_dataspace.h>
#include <base/connection.h>
#include <base/heap.h>
#include <base/rpc_client.h>
#include <timer_session/connection.h>
#include <packet_stream_tx/client.h>
#include <libc/component.h>

#include "session.h"

using namespace Genode;


namespace Test
{
	class Client;
	class Connection;
	class Main;
}


struct Test::Connection : Genode::Connection<Session>, Rpc_client<Test::Session>
{
	Packet_stream_tx::Client<Tx> _tx;

	Connection(Env &env, Range_allocator *tx_block_alloc,
	           Genode::size_t tx_buf_size = 512 * 1024, const char *label = "")
	:
		Genode::Connection<Session>(env, label, Ram_quota{8 * 4096 + tx_buf_size},
		                            Args("tx_buf_size=", tx_buf_size)),
		Rpc_client<Session>(cap()),
		_tx(call<Rpc_tx_cap>(), env.rm(), *tx_block_alloc)
	{ }

	virtual Tx *tx_channel() override { return &_tx; }
	virtual Tx::Source *tx() override { return _tx.source(); }
};


struct Test::Main
{
	enum {
		PACKET_SIZE = Test::Packet_allocator::DEFAULT_PACKET_SIZE,
		TX_BUFFER = 64 * 1024 * 1024
	};

	Env &_env;

	Heap _heap {_env.ram(), _env.rm() };

	Attached_rom_dataspace _config{_env, "config"};

	Test::Packet_allocator _alloc { &_heap };

	Test::Connection _session {_env, &_alloc, TX_BUFFER };

	::Signal_handler<Main> _timer_handler  { _env.ep(), *this, &Main::_handle_timer };
	::Signal_handler<Main> _packet_handler { _env.ep(), *this, &Main::_handle_packets };

	Timer::Connection _timer { _env };

	unsigned const _max_pending = Test::Session::TX_QUEUE_SIZE;

	unsigned _next_packet_index = 0;
	unsigned _next_ack_index    = 0;
	unsigned _num_pending       = 0;

	unsigned long _num_acks      = 0;
	unsigned long _num_requests  = 0;
	unsigned long _prev_num_acks = 1; /* initially differ from _num_acks */

	void _handle_timer()
	{
		if (_num_acks == _prev_num_acks)
			error("no acknowledgements received");
		_prev_num_acks = _num_acks;

		log("requests/acks = ", _num_requests, "/", _num_acks, " pending = ", _num_pending);
	}

	void _handle_packets()
	{
		/* incoming acknowledgements */
		while (_session.tx()->ack_avail()) {
			Test::Packet_descriptor packet { _session.tx()->get_acked_packet() };

			if (packet.index() != _next_ack_index)
				error("packets do not arrive in order");

			_session.tx()->release_packet(packet);

			_num_acks    += 1;
			_num_pending -= 1;

			_next_ack_index = (_next_ack_index + 1) % _max_pending;
		}

		/* outgoing requests */
		for (unsigned i = 0; _session.tx()->ready_to_submit() && i < _max_pending; ++i) {
			try {
				Test::Packet_descriptor packet { _session.tx()->alloc_packet(PACKET_SIZE) };
				packet.index(_next_packet_index);

				/* waste some time */
				for (unsigned i = 0; i < 1000; ++i)
					asm volatile ("" ::: "memory");

				_session.tx()->submit_packet(packet);
			} catch (...) {
				error("allocation of packet fails");
			}

			_num_requests += 1;
			_num_pending  += 1;

			_next_packet_index = (_next_packet_index + 1) % _max_pending;
		}
	}

	Main(Env &env) : _env(env)
	{
		_session.tx_channel()->sigh_ack_avail(_packet_handler);
		_session.tx_channel()->sigh_ready_to_submit(_packet_handler);

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(5 * 1000 * 1000);

		_handle_packets();
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc( [&]() { static Test::Main main(env); });
}
