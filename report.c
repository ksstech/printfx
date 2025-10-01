// report.c - Copyright (c) 2025 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"
#include "report.h"

#include "hal_memory.h"
#include "hal_usart.h"
#include "stdioX.h"
#include "struct_union.h"
#include "string_general.h"		// xinstring function
#include "errors_events.h"
#include "utilitiesX.h"

#include "esp_debug_helpers.h"
#include <unistd.h>

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Enumerations ############################################

// ###################################### Public variables #########################################

SemaphoreHandle_t ReportLock = NULL;

// ##################################### Public functions ##########################################

void vReportDebug(report_t * psR) {
	RP("A=%p  H=%p  B=%p  size=x%X  uSGR=%d  bDebug=%d  Xlock=%d  bHdlr=%d  sFM=x%X" strNL, psR->pcAlloc,
	psR->hdlr, psR->pcBuf, psR->size, psR->uSGR, psR->bDebug, psR->XLock, psR->bHdlr, psR->sFM.u32Val);
}

/* ############################ Walking, Talking, Singing and Dancing ##############################
 * Based on the values (pre) initialised for buffer start and size
 * a) walk through the buffer on successive calls, concatenating output; or
 * b) output directly to stdout if buffer pointer/size not initialized; or
 * c) output using specified handler function.
 * Because the intent in this function is to provide a streamlined method for
 * keeping track of buffer usage over a series of successive printf() type calls,
 * the calling function has control un/lock activities using the nolock flag provided.
 */

int	xvReport(report_t * psR, const char * pcFmt, va_list vaList) {
	int iRV = 0;
	report_t sRprt = { .uSGR=sgrANSI, .XLock=sLO_UL };	// remove .bDirect = 1, 
	if (psR == NULL)
		psR = &sRprt;
	IF_myASSERT(debugPARAM, halMemoryRAM(psR));
	if (psR->bHdlr) {									// handler information supplied?
		IF_myASSERT(debugTRACK, halMemoryEXE(psR->hdlr));
		psR->XLock = sNONE;
	} else if (psR->pcBuf && psR->size) {				// pointer & size supplied, output to buffer
		IF_myASSERT(debugPARAM, halMemoryRAM(psR->pcBuf));
		psR->hdlr = xPrintToString;
		psR->XLock = sBUFFER;							// no un/lock required or done
	} else {											// string buffer not configured
		psR->hdlr = xPrintToHandle;
		psR->pcBuf = (char *)STDOUT_FILENO;				// set parameter to STDOUT
		if (psR->bDirect) {
			psR->bSaved = bStdioConsoleGetStatus();
			vStdioConsoleSetStatus(1);
		}
		IF_myASSERT(debugTRACK, psR->size == 0);
	}
	// verify xlock values, handle semaphore accordingly
	BaseType_t btRV = pdFALSE;
	if (psR->XLock == sLO || psR->XLock == sLO_UL) {	// handle sLO & sLO_UL
		if (psR->XLock == sLO)							// sLO is transient
			psR->XLock = sNL;							// mark special sNL stage
		btRV = xRtosSemaphoreCheckCurrent(&ReportLock) ? pdFALSE : xRtosSemaphoreTake(&ReportLock, WPFX_TIMEOUT);
	} else if (psR->XLock == sINV5 || psR->XLock == sINV6) {
		RP("Xlock=%d" strNL, psR->XLock);
		esp_backtrace_print(6);
		assert(0);
	} else {
		// sNONE, sUL, sNL, sBUFFER are OK
	}
	// generate formatted output to specified channel
	iRV = xPrintFX(psR->hdlr, psR->pcBuf, psR->Size, pcFmt, vaList);
	// act on Xlock value, unlock semaphore if required, update pointers if output to buffer
	if (psR->XLock == sUL || psR->XLock == sLO_UL) {
		if (btRV == pdTRUE)								// earlier lock was successful?
			xRtosSemaphoreGive(&ReportLock);			// then unlock
		if (psR->XLock == sUL)							// sUL is transient
			psR->XLock = sNONE;							// clear
	} else if (psR->XLock == sBUFFER) {					// output to string buffer, update members
		if (iRV == psR->size)							// buffer full?
			--iRV;										// step back to make space for terminator
		psR->pcBuf += iRV;								// update buffer pointer
		psR->size -= iRV;								// update available size
		*psR->pcBuf = 0;								// add terminator
	} else {
		// sNONE / sNL. [sLO_UL & sBUFFER handled, sLO->sNL, sUL->sNONE, sINV5 & sINV6 asserted]
	}
	// restore serial console status if required 
	if (psR->bHdlr == 0 && psR->size == 0 && psR->bDirect)
		vStdioConsoleSetStatus(psR->bSaved);
	return iRV;
}

int	xReport(report_t * psR, const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = xvReport(psR, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

int	xReportBitMap(report_t * psR, u32_t V1, u32_t V2, u32_t Mask, const char * const paM[]) {
	if ((V1 == 0 && V2 == 0) || Mask == 0)				// if no bits set in both values, or no bits set in mask
		return 0;										// nothing to do, return....

	int	pos, idx, iFS = 31 - __builtin_clzl(Mask);		// determine index of first bit set
	bool B1, B2, aColor = (psR && psR->uSGR) ? 1 : 0;
	const char * pccTmp, * pFormat = aColor ? "%C%s%C " : "%c%s%c ";
	char caTmp[16];
	int iRV = 0;
	u32_t Col, CurMask;
	repSET(XLock, sLO);									// will be changed to sNL automatically
	for (pos = iFS, idx = iFS, CurMask = (1<<iFS); pos >= 0; CurMask >>= 1, --pos, --idx) {
		if (Mask & CurMask) {
			B1 = V1 & CurMask ? 1 : 0;
			B2 = V2 & CurMask ? 1 : 0;
			if (B1 && B2) {								// No change, was 1 still 1
				Col = aColor ? xpfCOL(attrRESET, 0) : CHR_TILDE;
			} else if (B1) {							// 1 -> 0
				Col = aColor ? xpfCOL(attrRESET, colourFG_RED) : CHR_US;	// CHR_UNDERSCORE
			} else if (B2) {							// 0 -> 1
				Col = aColor ? xpfCOL(attrRESET, colourFG_BLUE) : CHR_RS;	// CHR_CARET
			} else {									// No change, was 0 still 0
				Col = 0xFFFFFFFF;
			}
			if (Col != 0xFFFFFFFF) {					// only show if true or changed
				if (paM && paM[idx]) {					// pointer to string array supplied, entry avail?
					pccTmp = paM[idx];					// yes, use that as label
				} else {								// no,
					snprintfx(caTmp, sizeof(caTmp), "%d/x%X", idx, 1 << idx);
					pccTmp = caTmp;						// create a dynamic "label"
				}
				iRV += xReport(psR, pFormat, Col, pccTmp, 0);	// print string (with colour/char) then reset color
			}
		}
	}
	repSET(XLock, sUL);
	if (iRV) {											// if any output generated append hex value
		iRV += xReport(psR, "(x%0.*X)%s", iFS+2, V2, fmTST(aNL) ? strNL : strNUL);
	} else {
		iRV += xReport(psR, strNUL);					// no output, just unlock
	}
	return iRV;
}
