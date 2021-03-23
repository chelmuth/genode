/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <timer_session/connection.h>

/* Libc includes */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace TcpTest
{
	class Runner;
};

class TcpTest::Runner
{
	private:
		Libc::Env&                      _env;
		Genode::Heap                    _heap { _env.ram(), _env.rm() };
		Timer::Connection               _timer { _env };
		Genode::Attached_rom_dataspace  _config { _env, "config" };

	public:
		Runner(Libc::Env& env)
			: _env(env)
		{
			Genode::log("Test starting...");

			Libc::with_libc([&] () {
				echo_test(1024 * 1024, 16);
			});
			
			Genode::log("Test succeded");
		}

	private:
		void echo_test(int buffer_count, int rounds)
		{
			Genode::String<16> server_ip = _config.xml().attribute_value("server_ip", Genode::String<16>());
			uint16_t server_port = _config.xml().attribute_value("server_port", 5000U);

			if (server_ip == "") {
				Genode::error("Config attribute 'server_ip' not set");
				throw Genode::Exception();
			}

			int socket_fd;
			sockaddr_in server_address;

			socket_fd = socket(AF_INET, SOCK_STREAM, 0);
			if (socket_fd < 0) {
				Genode::error("Socket creation failed");
				throw Genode::Exception();
			}

			memset(&server_address, 0, sizeof(server_address)); 
			server_address.sin_family = AF_INET; 
			server_address.sin_addr.s_addr = inet_addr(server_ip.string()); 
			server_address.sin_port = htons(server_port); 

			if (connect(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) != 0) {
				Genode::error("Connection failed");
				return;
			}

			const int buffer_length = buffer_count;
			char* out_buffer = nullptr;
			if (!_heap.alloc(buffer_length, (void**)&out_buffer)) {
				Genode::error("Alloc failed");
				throw Genode::Exception();
			}
			char* in_buffer = nullptr;
			if (!_heap.alloc(buffer_length, (void**)&in_buffer)) {
				Genode::error("Alloc failed");
				throw Genode::Exception();
			}

			int start_time = _timer.elapsed_ms();

			for (int j = 0; j < rounds; j++) {
				memset(out_buffer, 'a', buffer_length);
				for (int i = 0; i < buffer_length; i += 1024) {
					out_buffer[i] = '\n';
				}

				if (write(socket_fd, out_buffer, buffer_length) < 0) {
					Genode::error("Write failed");
					throw Genode::Exception();
				}

				memset(in_buffer, 0, buffer_length);
				int count = 0;
				while (count < buffer_length) {
					int bytes = read(socket_fd, in_buffer + count, buffer_length - count);
					if (bytes < 0) {
						Genode::error("Read failed");
						throw Genode::Exception();
					}
					count += bytes;
				}

				if (memcmp(out_buffer, in_buffer, buffer_length) != 0) {
					Genode::error("Data mismatch");
					throw Genode::Exception();
				}

				Genode::log("Round ", j, " done");
			}

			int end_time = _timer.elapsed_ms();
			int diff_time = end_time - start_time;
			Genode::log("Time: ", diff_time, "ms");

			close(socket_fd);

			_heap.free(out_buffer, buffer_length);
			_heap.free(in_buffer, buffer_length);

			Genode::log("Run succeeded");
		}
};

void Libc::Component::construct(Libc::Env &env)
{
	static TcpTest::Runner inst(env);
}
