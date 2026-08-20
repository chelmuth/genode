/*
 * \brief  Verify the dimensions and hashes of a tresor container
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <vfs/root.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/ft_check.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>

using namespace Genode;
using namespace Tresor;

namespace Tresor_check { class Main; }

class Tresor_check::Main : private Vfs::Env::User
{
	private:

		enum State { INIT, CHECK_SB, CHECK_SB_SUCCEEDED };

		Env &_env;
		Heap _heap { _env.ram(), _env.rm() };

		Vfs::Root _vfs_root { _env, _heap, *this };

		Attached_rom_dataspace _config_rom { _env, "config" };

		bool const _vfs_configured = _config_rom.node().with_sub_node("vfs",
			[&] (Node const &config) { _vfs_root.apply_config(config); return true; },
			[&]                      { error("VFS not configured");   return false; });

		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };

		Tresor::Path _path_from_config(auto const &node_name) const
		{
			return _config_rom.node().with_sub_node(node_name,
				[&] (Node const &node) { return node.attribute_value("path", Tresor::Path()); },
				[&]                    { return Tresor::Path(); });
		}

		Tresor::Path const _block_io_path     = _path_from_config("block-io");
		Tresor::Path const _trust_anchor_path = _path_from_config("trust-anchor");

		Tresor::File_handle
			_block_io_file        { _vfs_root, _block_io_path },
			_ta_decrypt_file      { _vfs_root, { _trust_anchor_path, "/decrypt" } },
			_ta_encrypt_file      { _vfs_root, { _trust_anchor_path, "/encrypt" } },
			_ta_generate_key_file { _vfs_root, { _trust_anchor_path, "/generate_key" } },
			_ta_initialize_file   { _vfs_root, { _trust_anchor_path, "/initialize" } },
			_ta_hash_file         { _vfs_root, { _trust_anchor_path, "/hash" } };

		Block_io _block_io { _block_io_file };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		Sb_check _sb_check { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Sb_check::Check _check_superblocks { };

		void _wakeup_back_end_services() { _vfs_root.io().commit(); }

		void _handle_signal()
		{
			while(_sb_check.execute(_check_superblocks, _vbd_check, _ft_check, _block_io, _trust_anchor));
			if (_check_superblocks.complete())
				_env.parent().exit(_check_superblocks.success() ? 0 : -1);
			_wakeup_back_end_services();
		}

		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _sigh.local_submit(); }

	public:

		Main(Env &env) : _env(env) { _handle_signal(); }
};

void Component::construct(Genode::Env &env) { static Tresor_check::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
