/*
 * \brief  Common libc-internal types
 * \author Norman Feske
 * \date   2019-09-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__TYPES_H_
#define _LIBC__INTERNAL__TYPES_H_

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

namespace Libc {

	using namespace Genode;

	typedef Genode::uint64_t uint64_t;
	typedef String<64> Binary_name;

	struct Trace_scope
	{
		char const *func;
		char const *id;
		Trace_scope(char const *func, char const *id)
		: func(func), id(id)
		{
			log("ENTER ", func, "() ", id);
		}

		~Trace_scope()
		{
			log("LEAVE ", func, "() ", id);
		}
	};
}

#define TRACE_SCOPE(x) Libc::Trace_scope x { __func__, #x }

#endif /* _LIBC__INTERNAL__TYPES_H_ */
