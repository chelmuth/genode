/*
 * \brief  Suplib implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_H_
#define _SUP_H_

/* VirtualBox includes */
#include <VBox/sup.h>

namespace Sup {
	using namespace Genode;

	struct Cpu_count    { unsigned value; };
	struct Cpu_freq_khz { unsigned value; };
}

#endif /* _SUP_H_ */
