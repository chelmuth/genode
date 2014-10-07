/*
 * \brief  Interface of the printf back end
 * \author Norman Feske
 * \date   2006-04-08
 */

/*
 * Copyright (C) 2006-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PRINTF_H_
#define _INCLUDE__BASE__PRINTF_H_

#include <stdarg.h>
#include <util/ansi_esc.h>

namespace Genode {

	/**
	 * Output format string to LOG session
	 */
	void  printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

	void vprintf(const char *format, va_list) __attribute__((format(printf, 1, 0)));
}

/**
 * Remove colored output from release version
 */
#ifdef GENODE_RELEASE
#define ESC_LOG
#define ESC_DBG
#define ESC_INF
#define ESC_WRN
#define ESC_ERR
#define ESC_END
#else
#define ESC_LOG ANSI_ESC_YELLOW
#define ESC_DBG ANSI_ESC_YELLOW
#define ESC_INF ANSI_ESC_GREEN
#define ESC_WRN ANSI_ESC_BLUE
#define ESC_ERR ANSI_ESC_RED
#define ESC_END ANSI_ESC_RESET
#endif /* GENODE_RELEASE */

/**
 * Suppress debug messages in release version
 */
#ifdef GENODE_RELEASE
#define DO_PDBG false
#define DO_PWRN false
#else
#define DO_PDBG true
#define DO_PWRN true
#endif /* GENODE_RELEASE */

/**
 * Print debug message with function name
 *
 * We're using heavy CPP wizardry here to prevent compiler errors after macro
 * expansion. Each macro works as follows:
 *
 * - Support one format string plus zero or more arguments.
 * - Put all static strings (including the format string) in the first argument
 *   of the call to printf() and let the compiler merge them.
 * - Append the function name (magic static string variable) as first argument.
 * - (Optionally) append the arguments to the macro with ", ##__VA_ARGS__". CPP
 *   only appends the comma and arguments if __VA__ARGS__ is not empty,
 *   otherwise nothing (not even the comma) is appended.
 */
#define PDBG(fmt, ...) \
	do { \
		if (DO_PDBG) \
			Genode::printf("%s: " ESC_DBG fmt ESC_END "\n", \
			               __PRETTY_FUNCTION__, ##__VA_ARGS__ ); \
	} while (0)

/**
 * Print log message
 */
#define PLOG(fmt, ...) \
	Genode::printf(ESC_LOG fmt ESC_END "\n", ##__VA_ARGS__ )

/**
 * Print status-information message
 */
#define PINF(fmt, ...) \
	Genode::printf(ESC_INF fmt ESC_END "\n", ##__VA_ARGS__ )

/**
 * Print warning message
 */
#define PWRN(fmt, ...) \
	do { \
		if (DO_PWRN) \
			Genode::printf(ESC_WRN fmt ESC_END "\n", ##__VA_ARGS__ ); \
	} while (0)

/**
 * Print error message
 */
#define PERR(fmt, ...) \
	Genode::printf(ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__ )

#endif /* _INCLUDE__BASE__PRINTF_H_ */
