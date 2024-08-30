/*
 * \brief  Multi-core packet-stream stress test (server)
 * \author Christian Helmuth
 * \date   2023-09-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <packet_stream_tx/rpc_object.h>
#include <timer_session/connection.h>
#include <libc/component.h>

#include <queue>

#include "session.h"

using namespace Genode;

namespace Test
{
	class Rpc_object;
	class Component_base;
	class Component;
	class Root;
	class Main;
}


class Test::Rpc_object : public Genode::Rpc_object<Test::Session, Test::Rpc_object>
{
	protected:

		Packet_stream_tx::Rpc_object<Tx> _tx;

	public:

		Rpc_object(Region_map &rm, Dataspace_capability tx_ds, Rpc_entrypoint &ep)
		: _tx(tx_ds, rm, ep) { }

		Capability<Tx> _tx_cap() { return _tx.cap(); }

		Tx::Sink *tx_sink() { return _tx.sink(); }
};


class Test::Component_base
{
	protected:

		Env &_env;
		Ram_dataspace_capability _rq_ds;

	public:

		Component_base(Env &env, Genode::size_t tx_size)
		:
			_env(env),
			_rq_ds(_env.ram().alloc(tx_size))
		{ }

		virtual ~Component_base() { _env.ram().free(_rq_ds); }
};


struct Test::Component : public Test::Component_base,
                         public Test::Rpc_object
{
	Env &_env;
	Heap &_heap;

	Timer::Connection _timer { _env };

	Signal_handler<Component> _timer_handler { _env.ep(), *this, &Component::_handle_timer };

	unsigned long _num_acks     = 0;
	unsigned long _num_requests = 0;

	std::queue<Test::Packet_descriptor> _packets { };

	void _handle_timer()
	{
		log("requests/acks = ", _num_requests, "/", _num_acks);
	}

	void process()
	{
		/* incoming requests */
		while (tx_sink()->packet_avail()) {
			Test::Packet_descriptor packet { tx_sink()->get_packet() };
			_packets.push(packet);

			_num_requests += 1;
		}

		/* outgoing acknowledgements */
		while (tx_sink()->ready_to_ack() && !_packets.empty()) {
			tx_sink()->acknowledge_packet(_packets.front());
			_packets.pop();

			_num_acks += 1;
		}
	}

	Component(Env &env, Heap &heap, Genode::size_t size,
	          Signal_context_capability process_handler)
	:
		Component_base(env, size),
		Test::Rpc_object(env.rm(), _rq_ds, env.ep().rpc_ep()),
		_env(env) , _heap(heap)
	{
		_tx.sigh_packet_avail(process_handler);
		_tx.sigh_ready_to_ack(process_handler);

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(5 * 1000 * 1000);
	}
};


class Test::Root : public Root_component<Test::Component>
{
	private:

		Env &_env;
		Heap &_heap;

		Component *_session { };

		Signal_handler<Root> _process_handler { _env.ep(), *this, &Root::_process };

		void _process() { _session->process(); }

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

	protected:

		Component *_create_session(const char *args) override
		{
			Genode::size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			Genode::size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			Genode::size_t session_size =
			Genode::max((Genode::size_t)4096, sizeof(Test::Component) + sizeof(Allocator_avl));
			if (ram_quota < session_size)
				throw Service_denied();

			if (tx_buf_size > ram_quota - session_size) {
				error("ram_quota insufficient: provided ram_quota: ", ram_quota,
				      " required ram_quota: ", tx_buf_size + session_size);

				throw Service_denied();
			}

			_session = new (_heap) Component(_env, _heap, tx_buf_size, _process_handler);

			return _session;
		}

	public:

		Root(Env &env, Heap &heap)
		:
			Root_component(&env.ep().rpc_ep(), &heap),
			_env(env), _heap(heap)
		{
			_env.parent().announce(_env.ep().manage(*this));
		}

};


struct Test::Main
{
	Env &_env;

	Heap _heap{_env.ram(), _env.rm()};

	Timer::Connection _timer{_env};

	Attached_rom_dataspace _config{_env, "config"};

	Test::Root _root{_env, _heap};

	Main(Env &env) : _env(env) { }

};


void Libc::Component::construct(Libc::Env &env)
{
	env.exec_static_constructors();

	static Test::Main main{env};
}
