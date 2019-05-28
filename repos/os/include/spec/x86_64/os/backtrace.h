/*
 * \brief   Backtrace helper utility
 * \date    2015-09-18
 * \author  Christian Prochaska
 * \author  Stefan Kalkowski
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__OS__BACKTRACE_H_
#define _INCLUDE__SPEC__X86_64__OS__BACKTRACE_H_

#include <base/stdint.h>
#include <base/log.h>

namespace Genode {
	void inline backtrace() __attribute__((always_inline));
	void inline backtrace(unsigned, addr_t[]) __attribute__((always_inline));
}

/**
 * Print frame pointer based backtrace
 *
 * To use this function compile your code with the -fno-omit-frame-pointer GCC
 * option.
 */
void inline Genode::backtrace()
{
	Genode::addr_t * fp;

	asm volatile ("movq %%rbp, %0" : "=r"(fp) : :);

	while (fp && *(fp + 1)) {
		Genode::log(Hex(*(fp + 1)));
		fp = (Genode::addr_t*)*fp;
	}
}

void inline Genode::backtrace(unsigned const num_frames, Genode::addr_t bt[])
{
	Genode::addr_t * fp;

	asm volatile ("movq %%rbp, %0" : "=r"(fp) : :);

	unsigned num = 0;
	while (num < num_frames && fp && *(fp + 1)) {
		bt[num] = *(fp + 1);
		fp = (Genode::addr_t*)*fp;
		++num;
	}
}

#endif /* _INCLUDE__SPEC__X86_64__OS__BACKTRACE_H_ */
