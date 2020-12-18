
/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <libc/component.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <util/list.h>

/* libc includes */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

namespace Test_vfs_rom_pthread {
	using namespace Genode;
	class Thread_list_element;
	class Main;
	class Test;

	using Thread_list = Genode::List<Thread_list_element>;

	static char const* TEST_DATA_FILENAME  { "/ro/test-data.bin" };
	enum {
		BUF_SIZE = 4*1024,
		MAX_THREADS = 45
	};
}

class Test_vfs_rom_pthread::Thread_list_element : public Thread_list::Element
{
	private:
		pthread_t _value { 0 };
	public:
		Thread_list_element(pthread_t const value) : _value(value) { }
		pthread_t value() const { return _value; }
};

class Test_vfs_rom_pthread::Test
{
	private:

		Env                    &_env;
		Heap                    _heap { _env.ram(), _env.rm() };
		pthread_attr_t          _worker_settings;
		Thread_list             _threads { };

		static void compare_test_files()
		{
			/* test values */
			char test_data_1[BUF_SIZE] { };
			char test_data_2[BUF_SIZE] { };

			int test_data_file_1 { open(TEST_DATA_FILENAME, O_RDONLY) };
			if (test_data_file_1 == -1) {
				error("Cannot open test data file 1: ", TEST_DATA_FILENAME);
				exit(1);
			}
			int test_data_file_2 { open(TEST_DATA_FILENAME, O_RDONLY) };
			if (test_data_file_2 == -1) {
				error("Cannot open test data file 2: ", TEST_DATA_FILENAME);
				exit(1);
			}

			size_t total_received_bytes { 0 };
			while (true) {
				auto const test_data_num_1 { read(test_data_file_1, test_data_1, BUF_SIZE) };
				auto const test_data_num_2 { read(test_data_file_2, test_data_2, BUF_SIZE) };
				if (test_data_num_1 != test_data_num_2) {
					error("Error test_data_num_1 != test_data_num_2");
					error("total_received_bytes=", total_received_bytes, " test_data_num_1=", test_data_num_1, " test_data_num_2=", test_data_num_2);
					while(true);
				}
				if (test_data_num_1) {
					/* compare the two test data sets */
					auto const diff_to_test_data { ::memcmp(test_data_1, test_data_2, test_data_num_1) };
					if ((0 != diff_to_test_data)) {
						error("the two test data sets are not equal. diff_to_test_data=", diff_to_test_data);
						error("total_received_bytes=", total_received_bytes, " test_data_num_1=", test_data_num_1);
						while(true);
					}
				}
				total_received_bytes += test_data_num_1;
				if (test_data_num_1 == 0) {
					break;
				}
			}

			close(test_data_file_1);
			close(test_data_file_2);
		}

		static void *handle_output_data(void*)
		{
			while (true)
				compare_test_files();
			pthread_exit(NULL);
			return NULL;
		}

		void _init_pthread_attr()
		{
			Libc::with_libc([&] () {
				if (0 != pthread_attr_init(&_worker_settings)) {
					error("error setting thread settings");
					exit(1);
				}
				if (0 != pthread_attr_setdetachstate(&_worker_settings, PTHREAD_CREATE_JOINABLE)) {
					error("error setting thread settings");
					exit(1);
				}
			});
		}

		void _start_thread(pthread_t *thread)
		{
			Libc::with_libc([&] () {
				if (0 != pthread_create(thread, &_worker_settings, handle_output_data, nullptr)) {
					error("error opening worker thread");
					exit(1);
				}
			});
		}

		void _stop_thread(pthread_t thread)
		{
			Libc::with_libc([&] () {
				auto const ret_receiver = pthread_join(thread, nullptr);
				if (0 != ret_receiver) {
					warning("pthread_join unexpectedly returned "
					        "with ", ret_receiver, " (errno=", errno, ")");
				}
			});
		}

	public:

		Test(Env &env) : _env(env) { }

		~Test()
		{
			while (_threads.first() != nullptr) {
				auto element = _threads.first();
				_threads.remove(element);
				Genode::destroy(_heap, element);
			}
		}

		void start_threads(unsigned num_threads)
		{
			log("starting ", num_threads, "  threads");
			_init_pthread_attr();

			for (unsigned i = 0; i < num_threads; ++i) {
				pthread_t t { 0 };
				_start_thread(&t);
				auto element { new (_heap) Thread_list_element { t } };
				_threads.insert(element);
			}
		}

		void stop_threads()
		{
			log("stopping threads ");
			while (_threads.first() != nullptr) {
				auto element = _threads.first();
				_stop_thread(element->value());
				_threads.remove(element);
				Genode::destroy(_heap, element);
			}
			Libc::with_libc([&] () {
				pthread_attr_destroy(&_worker_settings);
			});
			log("threads stopped");
		}
};

class Test_vfs_rom_pthread::Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config { _env, "config" };

	public:

		Main(Env &env) : _env(env)
		{
			auto const max_iterations { _config.xml().attribute_value("iterations", 1u) };
			log("test started with ", max_iterations, " iterations");
			unsigned num_threads { MAX_THREADS };
			for (unsigned i = 0; i < max_iterations; ++i) {
				log("--- test iteration ", i, " started ---");
				Test test(_env);
				test.start_threads(num_threads);
				if (num_threads < MAX_THREADS) {
					num_threads++;
				} else {
					num_threads = 1;
				}
				test.stop_threads();
			}
			log("--- test succeeded ---");
		}

		~Main() = default;
};


void Libc::Component::construct(Libc::Env& env) { static Test_vfs_rom_pthread::Main main(env); }
