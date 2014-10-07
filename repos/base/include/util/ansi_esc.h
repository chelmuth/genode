/*
 * \brief  ANSI escape sequences (e.g., colors)
 * \author Christian Helmuth
 * \date   2014-10-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ANSI_ESC_H_
#define _INCLUDE__UTIL__ANSI_ESC_H_

#define ANSI_ESC_RESET      "\033[00m"
#define ANSI_ESC_BLACK      "\033[30m"
#define ANSI_ESC_RED        "\033[31m"
#define ANSI_ESC_GREEN      "\033[32m"
#define ANSI_ESC_YELLOW     "\033[33m"
#define ANSI_ESC_BLUE       "\033[34m"
#define ANSI_ESC_MAGENTA    "\033[35m"
#define ANSI_ESC_CYAN       "\033[36m"
#define ANSI_ESC_WHITE      "\033[37m"
#define ANSI_ESC_BG_BLACK   "\033[40m"
#define ANSI_ESC_BG_RED     "\033[41m"
#define ANSI_ESC_BG_GREEN   "\033[42m"
#define ANSI_ESC_BG_YELLOW  "\033[43m"
#define ANSI_ESC_BG_BLUE    "\033[44m"
#define ANSI_ESC_BG_MAGENTA "\033[45m"
#define ANSI_ESC_BG_CYAN    "\033[46m"
#define ANSI_ESC_BG_WHITE   "\033[47m"

#endif /* _INCLUDE__UTIL__ANSI_ESC_H_ */
