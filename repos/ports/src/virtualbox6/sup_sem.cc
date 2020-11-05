/*
 * \brief  Genode backend for VirtualBox Suplib
 * \author Norman Feske
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <iprt/err.h>
#include <iprt/semaphore.h>
#include <VBox/sup.h>
#include <SUPLibInternal.h>

/* local includes */
#include <stub_macros.h>

static bool const debug = true;


int SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
	return RTSemEventCreate((PRTSEMEVENT)phEvent);
}


int SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
	Assert (hEvent);

	return RTSemEventDestroy((RTSEMEVENT)hEvent);
}


int SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent) STOP


int SUPSemEventWaitNoResume(PSUPDRVSESSION pSession,
                            SUPSEMEVENT hEvent,
                            uint32_t cMillies) STOP


int SUPSemEventWaitNsAbsIntr(PSUPDRVSESSION pSession,
                             SUPSEMEVENT    hEvent,
                             uint64_t       uNsTimeout) STOP


int SUPSemEventWaitNsRelIntr(PSUPDRVSESSION pSession,
                             SUPSEMEVENT    hEvent,
                             uint64_t       cNsTimeout) STOP


uint32_t SUPSemEventGetResolution(PSUPDRVSESSION pSession) STOP


int SUPSemEventMultiCreate(PSUPDRVSESSION pSession,
                           PSUPSEMEVENTMULTI phEventMulti) STOP


int SUPSemEventMultiClose(PSUPDRVSESSION   pSession,
                          SUPSEMEVENTMULTI hEventMulti) STOP


int SUPSemEventMultiSignal(PSUPDRVSESSION   pSession,
                           SUPSEMEVENTMULTI hEventMulti) STOP


int SUPSemEventMultiReset(PSUPDRVSESSION   pSession,
                          SUPSEMEVENTMULTI hEventMulti) STOP


int SUPSemEventMultiWaitNoResume(PSUPDRVSESSION   pSession,
                                 SUPSEMEVENTMULTI hEventMulti,
                                 uint32_t         cMillies) STOP


int SUPSemEventMultiWaitNsAbsIntr(PSUPDRVSESSION   pSession,
                                  SUPSEMEVENTMULTI hEventMulti,
                                  uint64_t         uNsTimeout) STOP


int SUPSemEventMultiWaitNsRelIntr(PSUPDRVSESSION   pSession,
                                  SUPSEMEVENTMULTI hEventMulti,
                                  uint64_t         cNsTimeout) STOP


uint32_t SUPSemEventMultiGetResolution(PSUPDRVSESSION pSession) STOP
