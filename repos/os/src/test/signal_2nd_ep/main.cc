/*
 * \brief  Test for handling signals in secondary entrypoint
 * \author Christian Helmuth
 * \date   2016-09-07
 */

/* Genode includes */
#include <base/env.h>
#include <base/component.h>
#include <base/log.h>
#include <timer_session/connection.h>


namespace Test {
	using Genode::log;

	struct Go;
	struct Go_rpc;
	struct Go_rpc_object;
	struct Ep;
	struct Main;
}


struct Test::Go { virtual void go() = 0; };


struct Test::Go_rpc : Test::Go
{
	GENODE_RPC(Rpc_go, void, go);
	GENODE_RPC_INTERFACE(Rpc_go);
};


struct Test::Go_rpc_object : Genode::Rpc_object<Go_rpc, Go_rpc_object>
{
	Go &ep;

	Go_rpc_object(Go &ep) : ep(ep) { }

	void go() override { ep.go(); }
};


struct Test::Ep : Genode::Entrypoint, Test::Go
{
	Timer::Connection          timer;
	Genode::Signal_handler<Ep> sigh {*this, *this, &Ep::handle_timer};
	unsigned                   count { 0 };

	Ep(Genode::Env &env)
	: Genode::Entrypoint(env, 4*1024*sizeof(long), "signal_handler_ep")
	{
		log("2nd ep constructed");
	}

	void handle_timer()
	{
		log("timeout ", ++count);
	}

	void go() override
	{
		timer.sigh(sigh);
		timer.trigger_periodic(100*1000);

		log("secondary ep starts signal handling");
	}
};


struct Test::Main
{
	Genode::Env &env;
	Ep           secondary_ep { env };

	Main(Genode::Env &env) : env(env)
	{
		Go_rpc_object go(secondary_ep);
		Genode::Capability<Go_rpc> go_cap = secondary_ep.manage(go);
		go_cap.call<Go_rpc::Rpc_go>();
		secondary_ep.dissolve(go);

		log("primary ep enters RPC dispatcher");
	}
};


Genode::size_t Component::stack_size()      { return 4*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Test::Main inst(env); }
