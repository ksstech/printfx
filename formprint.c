// formprint.c - Copyright (c) 2014-24 Andre M. Maree/KSS Technologies (Pty) Ltd.

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
    if (flag & FLAG_CB_PRE_CL)		putcharX(CHR_LF, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_PRE_SPC)    	putcharX(CHR_SPACE, configSTDIO_UART_CHAN) ;
void vLogPrePend(uint32_t flag) {
	if (flag & FLAG_CB_FG_MASK)		vANSIattrib(formMASK(flag, FLAG_CB_FG_MASK, 29, 30), 0);
	if (flag & FLAG_CB_BG_MASK)		vANSIattrib(formMASK(flag, FLAG_CB_BG_MASK, 25, 40), 0);
}

/*
 *	vLogPostPend - process all options to postpend info
 *	\brief	append all the character as per the flag in the specific sequence/priority
 *	\param	pCB - pointer to the circular buffer control structure
 * 			flag	- control buffer formatting flag
 *	\return	none
 */
    if (flag & FLAG_CB_POST_COMMA)	putcharX(CHR_COMMA, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_SPC)	putcharX(CHR_SPACE, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_TAB)	putcharX(CHR_TAB, configSTDIO_UART_CHAN) ;
    if (flag & FLAG_CB_POST_CL)		putcharX(CHR_LF, configSTDIO_UART_CHAN) ;
void vLogPostPend(u32_t flag) {
	if (flag & FLAG_CB_FG_MASK)		vANSIattrib(formMASK(FLAG_CB_FG_WHITE, FLAG_CB_FG_MASK, 29, 30), 0);
	if (flag & FLAG_CB_BG_MASK)		vANSIattrib(formMASK(FLAG_CB_BG_BLACK, FLAG_CB_BG_MASK, 25, 40), 0);
}

void vLogPrintf(u32_t flag, const char * format, ...) {
	if (flag) vLogPrePend(flag);
	va_list args;
    va_start(args, format);
    vprintfx(format, args);
    va_end(args);
    if (flag) vLogPostPend(flag);
}

int xI8ArrayPrint(const char * pHeader, u8_t * pArray, int ArraySize) {
	int iRV = 0;
	if (pHeader) iRV += printfx(pHeader);
	for (int Idx = 0; Idx < ArraySize; ++Idx)
		iRV += printfx("  #%d=%d", Idx, pArray[Idx]);
	iRV += printfx(strCRLF);
	return iRV;
}
