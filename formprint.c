/*
 * Copyright 2014-21 AM Maree/KSS Technologies (Pty) Ltd.
 *
 * formprint.c
 *
 */

#include	"hal_config.h"
#include 	"formprint.h"
#include	"printfx.h"
#include	"x_terminal.h"

#define	formMASK(f,m,p,a)	(((f & m) >> p) + a)

/*
 *	vLogPrePend - process all options to prepend info
 *	\brief	prepend all the characters as per the flag in the specific sequence
 *	\param	pCB - pointer to the circular buffer control structure
 * 			flag	- control buffer formatting flag
 *	\return	none
 */
void	vLogPrePend(uint32_t flag) {
	if (flag & FLAG_CB_FG_MASK)		vANSIattrib(formMASK(flag, FLAG_CB_FG_MASK, 29, 30), 0) ;
	if (flag & FLAG_CB_BG_MASK)		vANSIattrib(formMASK(flag, FLAG_CB_BG_MASK, 25, 40), 0) ;
    if (flag & FLAG_CB_PRE_CL)		putcx(CHR_LF, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_PRE_SPC)    	putcx(CHR_SPACE, configSTDIO_UART_CHAN) ;
}

/*
 *	vLogPostPend - process all options to postpend info
 *	\brief	append all the character as per the flag in the specific sequence/priority
 *	\param	pCB - pointer to the circular buffer control structure
 * 			flag	- control buffer formatting flag
 *	\return	none
 */
void	vLogPostPend(uint32_t flag) {
	if (flag & FLAG_CB_FG_MASK)		vANSIattrib(formMASK(FLAG_CB_FG_WHITE, FLAG_CB_FG_MASK, 29, 30), 0) ;
	if (flag & FLAG_CB_BG_MASK)		vANSIattrib(formMASK(FLAG_CB_BG_BLACK, FLAG_CB_BG_MASK, 25, 40), 0) ;
    if (flag & FLAG_CB_POST_COMMA)	putcx(CHR_COMMA, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_SPC)	putcx(CHR_SPACE, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_TAB)	putcx(CHR_TAB, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_CL)		putcx(CHR_LF, configSTDIO_UART_CHAN) ;
}

void 	vLogPrintf(uint32_t flag, const char * format, ...) {
	if (flag)						vLogPrePend(flag) ;
	va_list args ;
    va_start(args, format) ;
    vprintfx(format, args) ;
    va_end(args) ;
    if (flag)						vLogPostPend(flag) ;
}

int32_t	xI8ArrayPrint(const char * pHeader, uint8_t * pArray, int32_t ArraySize) {
	int32_t iRV = 0 ;
	if (pHeader)
		iRV += PRINT(pHeader) ;
	for (int32_t Idx = 0; Idx < ArraySize; ++Idx)
		iRV += PRINT("  #%d=%d", Idx, pArray[Idx]) ;
	iRV += PRINT("\n") ;
	return iRV ;
}
