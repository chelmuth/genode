/*
 * \brief  VFS file-system back-end interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_H_
#define _INCLUDE__VFS__FILE_SYSTEM_H_

#include <vfs/directory_service.h>
#include <vfs/types.h>

namespace Genode::Vfs { class File_system; }


class Genode::Vfs::File_system : public Directory_service
{
	public:

		struct Factory : Interface
		{
			/**
			 * Create and return a new file-system
			 */
			virtual File_system *create(Vfs::Env &env, Parent_fs &, Node const &) = 0;
		};

		/**
		 * File-system identity used for updating the union fs via 'List_model'
		 */
		struct Ident
		{
			String<100> string;

			/**
			 * Return file-system ident string from node type and attribute
			 *
			 * This function composes identity from the note type and
			 * attributes. Sub nodes are not part of the identity.
			 */
			inline static Ident from_node(Node const &node);

		} const _ident;

	private:

		/*
		 * Noncopyable
		 */
		File_system(File_system const &);
		File_system &operator = (File_system const &);

	public:

		/**
		 * Our next sibling within the same 'Union_file_system'
		 * Construct fs with its identity defined by node type and attributes
		 */
		struct File_system *next = nullptr;

		/**
		 * Construct fs with specified identity string
		 */
		File_system(Ident const &ident) : _ident(ident) { }

		/**
		 * Construct fs with its identity defined by node type and attributes
		 */
		File_system(Node const &node) : File_system(Ident::from_node(node)) { }

		/**
		 * Adjust to configuration changes
		 */
		virtual Progress update(Node const &, Factory &) { return STALLED; }

		/**
		 * Return the file-system type
		 */
		virtual char const *type() = 0;
};


Genode::Vfs::File_system::Ident
Genode::Vfs::File_system::Ident::from_node(Node const &node)
{
	char buf[decltype(string)::capacity()] { };

	return Generator::generate(Byte_range_ptr(buf, sizeof(buf)),
	                           node.type(), [&] (Generator &g) {
		g.node_attributes(node);
	}).convert<Ident>(
		[&] (size_t len) {
			len = max(len, 3u) - 3u;  /* omit HID end marker and line breaks */
			return Ident { { Cstring(buf, len) } };
		},
		[&] (Buffer_error) {
			warning("dropping attributes for VFS identity of: ", node);
			return Ident { node.type() };
	});
}

#endif /* _INCLUDE__VFS__FILE_SYSTEM_H_ */
