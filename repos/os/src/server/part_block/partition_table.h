/*
 * \brief  Partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__PARTITION_TABLE_H_
#define _PART_BLOCK__PARTITION_TABLE_H_

#include "block.h"
#include "fsprobe.h"
#include "types.h"

namespace Block {
	struct Partition;
	class  Partition_table;
}


struct Block::Partition : Noncopyable
{
	block_number_t lba;     /* logical block address on device */
	block_count_t  sectors; /* number of sectors in patitions */

	Fs::Type fs_type { };

	uint8_t  mbr_type { 0 };

	using Uuid = String<40>;
	Uuid guid     { };
	Uuid gpt_type { };

	using Name = String<72>; /* use GPT name antry length */
	Name name { };

	Partition(block_number_t lba,
	          block_count_t  sectors,
	          Fs::Type       fs_type,
	          uint8_t        mbr_type)
	:
		lba(lba), sectors(sectors), fs_type(fs_type),
		mbr_type(mbr_type)
	{ }

	Partition(block_number_t lba,
	          block_count_t  sectors,
	          Fs::Type       fs_type,
	          Uuid const &guid, Uuid const &gpt_type,
	          Name const &name)
	:
		lba(lba), sectors(sectors), fs_type(fs_type),
		guid(guid), gpt_type(gpt_type), name(name)
	{ }
};


class Block::Partition_table : Interface, Noncopyable
{
	protected:

		Sync_read::Handler  &_handler;
		Allocator           &_alloc;
		Session::Info const  _info;

	public:

		Partition_table(Sync_read::Handler &handler,
		                Allocator          &alloc,
		                Session::Info       info)
		: _handler(handler), _alloc(alloc), _info(info) { }

		virtual Partition &partition(long num) = 0;

		virtual bool parse() = 0;

		virtual void generate_report(Xml_generator &xml) const = 0;
};

#endif /* _PART_BLOCK__PARTITION_TABLE_H_ */
