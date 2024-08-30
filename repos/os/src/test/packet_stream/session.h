/*
 * \brief  Multi-core packet-stream stress test (session)
 * \author Christian Helmuth
 * \date   2023-09-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__PACKET_STREAM__SESSION_H_
#define _TEST__PACKET_STREAM__SESSION_H_

#include <os/packet_stream.h>
#include <os/packet_allocator.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>

namespace Test
{
	class Packet_allocator;
	class Packet_descriptor;
	class Session;

	using namespace Genode;
}


struct Test::Packet_allocator : Genode::Packet_allocator
{
	enum { DEFAULT_PACKET_SIZE = 4096 };

	Packet_allocator(Allocator *md_alloc)
	: Genode::Packet_allocator(md_alloc, DEFAULT_PACKET_SIZE) { }
};


class Test::Packet_descriptor : public Genode::Packet_descriptor
{
	public:

		enum Alignment { PACKET_ALIGNMENT = 11 };

	private:

		Genode::uint64_t _index { };

	public:

		Packet_descriptor(Genode::off_t offset = 0, Genode::size_t size = 0)
		: Genode::Packet_descriptor(offset, size) { }

		Packet_descriptor(Genode::Packet_descriptor p)
		: Genode::Packet_descriptor(p.offset(), p.size()) { }

		Genode::uint64_t index() const { return _index; }

		void index(Genode::uint64_t index) { _index = index; }
};


struct Test::Session : public Genode::Session
{
	enum { TX_QUEUE_SIZE = 96, CAP_QUOTA = 2, };

	using Tx_policy = Packet_stream_policy<Test::Packet_descriptor, TX_QUEUE_SIZE, TX_QUEUE_SIZE, Genode::uint8_t>;
	using Tx        = Packet_stream_tx::Channel<Tx_policy>;

	static const char *service_name() { return "Test"; }

	virtual ~Session() { }

	virtual Tx *tx_channel() { return nullptr; }

	virtual Tx::Source *tx() { return nullptr; }

	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
	GENODE_RPC_INTERFACE(Rpc_tx_cap);
};

#endif /* _TEST__PACKET_STREAM__SESSION_H_ */
