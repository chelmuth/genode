#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/mutex.h>
#include <base/thread.h>
#include <libc/component.h>
#include <timer_session/connection.h>
#include <util/list.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace Echo_server {
	using namespace Genode;

	class Server_main;
}

class Echo_server::Server_main
{
	private:
		Env& _env;
		pthread_t _server_thread;

		Timer::Connection _timer { _env };
		Genode::Signal_handler<Server_main> _timer_handler { _env.ep(), *this, &Server_main::_handle_timer };

		static void *connection_handler(void *arg);
		static void *start_server(void* envp);

		struct Connection {
			volatile int socket { -1 };
			volatile bool done  { false };
			pthread_t thread { 0 };
			Genode::Env& env;

			Connection(Genode::Env& env) : env(env) { }
		};
		class Startup_connection_thread_failed : Genode::Exception { };
		class Null_pointer_exception : Genode::Exception { };

		void _handle_timer()
		{
			log("still alive used_ram=", _env.pd().used_ram());
		}
	public:
		Server_main(Env& env)
			: _env(env)
		{
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(10*1000*1000);
			Libc::with_libc([&] () {
				pthread_create(&_server_thread, 0, start_server, &env);
			});
		}
};

/*
 * This will handle connection for each client
 */
void *Echo_server::Server_main::connection_handler(void *arg)
{
	Connection *conn = reinterpret_cast<Connection *>(arg);

	char buffer[4096] { };
	ssize_t const bytes_read = read(conn->socket, buffer, sizeof(buffer));
	if (bytes_read < 0) {
		warning("read() returned with ", bytes_read, " (errno=", errno, ")");
	} else if (bytes_read == 0) {
		log("EOF received on socket ", conn->socket);
	} else {
		std::string data(50000, 'a');
		data.append(buffer, bytes_read);
		ssize_t const bytes_written = write(conn->socket, data.data(), data.size());
		if (bytes_written < 0) {
			warning("write() returned with ", bytes_written, " (errno=", errno, ")");
		} else if (static_cast<size_t>(bytes_written) != data.size()) {
			warning("write() truncated. written=", bytes_written, " data size=", data.size());
		}
	}

	if (false) {
		/*Timer::Connection timer(conn->env);
		timer.msleep(500);*/
		usleep(500*1000);
	}
	if (close(conn->socket) < 0) {
		warning("close() failed (errno=", errno, ")");
	}

	conn->done = true;
	pthread_exit(nullptr);

	return 0;
}

void *Echo_server::Server_main::start_server(void* envp)
{
	Genode::Env& env = *(reinterpret_cast<Genode::Env*>(envp));
	Genode::Attached_rom_dataspace config { env, "config" };
	int tcp_port = config.xml().attribute_value("server_port", 8899u);

	log("Server thread started on port ", tcp_port);
	int listen_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sd == -1) {
		error("socket creation failed");
		return nullptr;
	}

	sockaddr_in sockaddr;
	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons (tcp_port);
	sockaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(listen_sd, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
		error("bind to port ", tcp_port, " failed");
		return nullptr;
	}

	if (listen(listen_sd, 1)) {
		error("listen failed");
		return nullptr;
	}
	std::vector<Connection *> connections;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);
	size_t conn_id = 0;
	while (true) {
		++conn_id;
//		log("waiting for connection ", conn_id);
		Connection *new_conn = new Connection(env);
		if (new_conn == nullptr) {
			error("alloc connection object failed");
			throw Null_pointer_exception();
		}
		new_conn->socket = accept(listen_sd, nullptr, nullptr);//&addr, &len);
		if (new_conn->socket == -1) {
			error("accept failed with errno=", errno);
			delete new_conn;
			continue;
		}
		log("connection ", conn_id, " accepted on socket ", new_conn->socket);
		pthread_t thread_id = 0;
		if (pthread_create(&thread_id, nullptr, connection_handler, new_conn)) {
			error("pthread_create failed (errno=", errno, ")");
			throw Startup_connection_thread_failed();
		}
		new_conn->thread = thread_id;
		connections.push_back(new_conn);

		/* garbage collect every 10 connections */
		if ((conn_id % 3) == 0) {
			auto iterator = connections.begin();
			while (iterator != connections.end()) {
				Connection *conn = *iterator;
				if (conn->done) {
					int const ret = pthread_join(conn->thread, nullptr);
					if (ret) {
						warning("pthread_join returned ", ret, " (errno=", errno, ")");
					}
//					log("thread joined");
					iterator = connections.erase(iterator);
					delete conn;
				} else {
					++iterator;
				}
			}
			log("connections.size=", connections.size());
		}
	}

	return nullptr;
}

void Libc::Component::construct(Libc::Env &env)
{
	static Echo_server::Server_main main(env);
}
