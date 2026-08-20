/*
 * \brief  Integration of the Tresor block encryption
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
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/vbd_initializer.h>

using namespace Genode;
using namespace Tresor;

namespace Tresor_init { class Main; }

class Tresor_init::Main : private Vfs::Env::User, private Crypto_key_files_interface
{
	private:

		struct Crypto_key
		{
			Key_id const key_id;
			Tresor::File_handle encrypt_file, decrypt_file;

			Crypto_key(Vfs::Env &env, Tresor::Path const &dir, Key_id const key_id)
			:
				key_id(key_id),
				encrypt_file(env, { dir, "/", key_id, "/encrypt" }),
				decrypt_file(env, { dir, "/", key_id, "/decrypt" })
			{ }
		};

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };
		Attached_rom_dataspace _config_rom { _env, "config" };
		Vfs::Root _vfs_env = _config_rom.node().with_sub_node("vfs",
			[&] (Node const &config) -> Vfs::Root {
				return { _env, _heap, config, *this }; },
			[&] () -> Vfs::Root {
				error("VFS not configured");
				return { _env, _heap, Node() }; });
		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };
		Superblock_configuration _sb_config { _config_rom.node() };

		Tresor::Path _path_from_config(auto const &node_name) const
		{
			return _config_rom.node().with_sub_node(node_name,
				[&] (Node const &node) { return node.attribute_value("path", Tresor::Path()); },
				[&]                    { return Tresor::Path(); });
		}

		Tresor::Path const _crypto_path       = _path_from_config("crypto");
		Tresor::Path const _block_io_path     = _path_from_config("block-io");
		Tresor::Path const _trust_anchor_path = _path_from_config("trust-anchor");

		Tresor::File_handle
			_block_io_file          { _vfs_env, _block_io_path },
			_crypto_add_key_file    { _vfs_env, { _crypto_path, "/add_key" } },
			_crypto_remove_key_file { _vfs_env, { _crypto_path, "/remove_key" } },
			_ta_decrypt_file        { _vfs_env, { _trust_anchor_path, "/decrypt" } },
			_ta_encrypt_file        { _vfs_env, { _trust_anchor_path, "/encrypt" } },
			_ta_generate_key_file   { _vfs_env, { _trust_anchor_path, "/generate_key" } },
			_ta_initialize_file     { _vfs_env, { _trust_anchor_path, "/initialize" } },
			_ta_hash_file           { _vfs_env, { _trust_anchor_path, "/hash" } };

		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Constructible<Crypto_key> _crypto_keys[2] { };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		Sb_initializer::Initialize _init_superblocks { {_sb_config, _pba_alloc} };
		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			while(_sb_initializer.execute(_init_superblocks, _block_io, _trust_anchor, _vbd_initializer, _ft_initializer));
			if (_init_superblocks.complete())
				_env.parent().exit(_init_superblocks.success() ? 0 : -1);
			_wakeup_back_end_services();
		}

		/********************************
		 ** Crypto_key_files_interface **
		 ********************************/

		void add_crypto_key(Key_id key_id) override
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (!key.constructed()) {
					key.construct(_vfs_env, Tresor::Path { _crypto_path, "/keys" }, key_id);
					return;
				}
			ASSERT_NEVER_REACHED;
		}

		void remove_crypto_key(Key_id key_id) override
		{
			_crypto_key(key_id).destruct();
		}

		Vfs::File_handle &encrypt_file(Key_id key_id) override { return _crypto_key(key_id)->encrypt_file; }
		Vfs::File_handle &decrypt_file(Key_id key_id) override { return _crypto_key(key_id)->decrypt_file; }

		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _sigh.local_submit(); }

	public:

		Main(Env &env) : _env(env) { _handle_signal(); }
};

void Component::construct(Genode::Env &env) { static Tresor_init::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
