// report.c - Copyright (c) 2025 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"
#include "report.h"
#include "hal_memory.h"
#include "struct_union.h"
#include "string_general.h"		// xinstring function
#include "errors_events.h"
#include "utilitiesX.h"

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Enumerations ############################################

// ###################################### Public variables #########################################

// ##################################### Public functions ##########################################

inline __attribute__((always_inline)) BaseType_t xPrintFxSaveLock(report_t * psR) {
	psR->sNoLock = psR->fNoLock;						// save noLock flag
	psR->fNoLock = 1; 									// force to non-locking (in future)
	return halUartLock(WPFX_TIMEOUT); 					// lock the channel
}

inline __attribute__((always_inline)) BaseType_t xPrintFxRestoreUnLock(report_t * psR) {
	BaseType_t bRV = halUartUnLock(); 					// unlock the channel
	psR->fNoLock = psR->sNoLock;						// restore noLock flag 
	return bRV;
}

/* ############################ Walking, Talking, Singing and Dancing ##############################
 * * Based on the values (pre) initialised for buffer start and size
 * a) walk through the buffer on successive calls, concatenating output; or
 * b) output directly to stdout if buffer pointer/size not initialized.
 * Because the intent in this function is to provide a streamlined method for
 * keeping track of buffer usage over a series of successive printf() type calls,
 * the calling function has control un/lock activities using the nolock flag provided.
 */

int	vreport(report_t * psR, const char * pcFmt, va_list vaList) {
	report_t sRprt;
	int iRV = 0;
	if (psR == NULL) {
		memset(&sRprt, 0, sizeof(report_t));
		psR = &sRprt;
		psR->uSGR = sgrANSI;
	}
	IF_myASSERT(debugPARAM, halMemoryRAM(psR));
	if (psR->pcBuf && psR->size) {						/* pointer & size supplied, output to buffer */
		IF_myASSERT(debugTRACK, halMemoryRAM(psR->pcBuf));		/* verify buffer is in RAM */
		iRV = vsnprintfx(psR->pcBuf, psR->Size, pcFmt, vaList);	/* generate output to buffer */
		if (iRV > 0) {									/* if anything written */
			IF_myASSERT(debugRESULT, iRV <= psR->size);
			psR->pcBuf += iRV;							/* update buffer pointer */
			psR->size -= iRV;							/* available size */
		}
	} else {
		/* Set default output handler */
		if (psR->putc == NULL) { psR->putc = xPrintToHandle; psR->pvArg = (void *)STDOUT_FILENO; }
//		if (psR->putc == NULL) { psR->putc = xPrintToFile; psR->pvArg = (void *) stdout; }
		if (psR->size == 0)
			psR->size = xpfMAXLEN_MAXVAL;
		BaseType_t btRV = pdFALSE;
		if (psR->fNoLock == 0)							/* lock requested? */
			btRV = halUartLock(WPFX_TIMEOUT);			/* yes, lock */
		iRV = xPrintF(psR->putc, psR->pvArg, psR->Size, pcFmt, vaList);
		if (psR->fNoLock == 0 && btRV == pdTRUE)		/* lock requested & was successful*/
			halUartUnLock();							/* yes, unlock */
	}
	return iRV;
}

int	report(report_t * psR, const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vreport(psR, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}
