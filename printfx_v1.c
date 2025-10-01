/*
 * <snf>printfx -  set of routines to replace equivalent printf functionality
 * Copyright (c) 2014-25 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * https://codereview.stackexchange.com/questions/219994/register-b-conversion-specifier
 *
 */

#include "hal_platform.h"
#include "printfx.h"

#include "common-vars.h"
#include "errors_events.h"
#include "FreeRTOS_Support.h"
#include "hal_memory.h"
#include "hal_usart.h"
#include "hal_stdio.h"
#include "stdioX.h"
#include "socketsX.h"
#include "struct_union.h"
#include "string_general.h"		// xinstring function
#include "utilitiesX.h"

#include <math.h>					// isnan()
#include <float.h>					// DBL_MIN/MAX

#ifdef ESP_PLATFORM
	#include "esp_log.h"
	#include "esp_rom_crc.h"
	#include "esp_debug_helpers.h"
#else
	#include "crc-barr.h"						// Barr group CRC
#endif

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ######################## control [optional] functionality included ##############################

#define	xpfSUPPORT_MAC_ADDR			1
#define	xpfSUPPORT_IP_ADDR			1
#define	xpfSUPPORT_HEXDUMP			1
#define	xpfSUPPORT_DATETIME			1
#define	xpfSUPPORT_IEEE754			1					// float point support in printfx.c functions
#define	xpfSUPPORT_SCALING			1					// scale number down by 10^[3/6/9/12/15/18]
#define	xpfSUPPORT_SGR				1					// Set Graphics Rendition FG & BG colors only
#define	xpfSUPPORT_URL				1					// URL encoding
#define xpfSUPPORT_ARRAYS			1					// uses complex vars to achieve

#define	xpfSUPPORT_ALIASES	1

// ###################################### Scaling factors ##########################################

#define	K1		1000ULL
#define	M1		1000000ULL
#define	B1		1000000000ULL
#define	T1		1000000000000ULL
#define	q1		1000000000000000ULL
#define	Q1		1000000000000000000ULL

// ####################################### xpc_t macros ############################################

#define XPC_SAVE(XP)	xpc_t sXPC = { .u64XPC = ((xp_t *)XP)->u64XPC };	// save xpc_t structure
#define XPC_REST(XP)	((xp_t *)XP)->u64XPC = sXPC.u64XPC;					// restore xpc_t structure
#define XPC_FLAG(XP,M)	((xp_t *)XP)->M = sXPC.M		// restore specific member from saved structure

// ####################################### Enumerations ############################################

enum {			//	F64		Hex		Other
	form0G,		//	Gg		None	None
	form1F,		//	Ff		:		!
	form2E,		//	Ee		-		None
	form3X		//	None	Complex	'
};

enum { S_none, S_hh, S_h, S_l, S_ll, S_j, S_z, S_t, S_L, S_XXX };

// ######################## Character and value translation & rounding tables ######################

const u8_t S_bytes[S_XXX] = { sizeof(int), sizeof(char), sizeof(short), sizeof(long), sizeof(long long),
							sizeof(intmax_t), sizeof(size_t), sizeof(ptrdiff_t), sizeof(long double) };
const u8_t S_bits[S_XXX] = { (u8_t)(BITS_IN_BYTE * sizeof(int))-1,(u8_t)7,(u8_t)15,(u8_t)31,(u8_t)63,(u8_t)63,(u8_t)31,(u8_t)31,(u8_t)127};

const char Delim0[2] = { '-', '/' };					// "-/"
const char Delim1[2] = { 'T', ' ' };					// "T "
const char Delim2[2] = { ':', 'h' };					// ":h"
const char Delim3[2] = { ':', 'm' };					// ":m"

const char hexchars[] = "0123456789ABCDEF";
const char vPrintStr1[] = {			// table of characters where lc/UC is applicable
	'B',							// Binary formatted, prepend "0b" or "0B"
	'P',							// pointer formatted, 0x00abcdef or 0X00ABCDEF
	#if	(xpfSUPPORT_DATETIME == 1)
	'R',							// Time, absolute/relative, no ZONE info, 64bit/uSec or 32bit/Sec
	#endif
	'X',							// hex formatted 'x' or 'X' values, always there
	#if	(xpfSUPPORT_IEEE754 == 1)
	'A', 'E', 'F', 'G',				// float hex/exponential/general
	#endif
	'\0'
};

// ###################################### Public variables #########################################

// ##################################### Private functions #########################################

static __attribute__((unused)) void xpDebugFlags(xp_t * psXP) {
	xpc_flg_t * psXPF = &psXP->flg;
	RP(" [SGR=%d  bLT=%d  bGT=%d  bFlt=%d  bArr=%d  bPad0=%d", psXPF->uSGR, psXPF->bLT, psXPF->bGT, psXPF->bFloat, psXPF->bArray, psXPF->bPad0);
	RP("  MW={%d/%d}  Pre={%d/%d}  uSiz=%d  bGrp=%d", psXPF->bMinWid, psXPF->MinWid, psXPF->bPrecis, psXPF->Precis, psXPF->uSize, psXPF->bGroup);
	RP("  uForm=%d  uBase=%d  bRdx=%d  bRel=%d  bNeg=%d", psXPF->uForm, psXPF->uBase, psXPF->bRadix, psXPF->bRelVal, psXPF->bNegVal);
	RP("  bSign=%d  bPlus=%d  bCase=%d  bLeft=%d  bAltF=%d]" strNL, psXPF->bSigned, psXPF->bPlus, psXPF->bCase, psXPF->bLeft, psXPF->bAltF);
}

static __attribute__((unused)) void xpDebugStruct(xp_t * psXP, const char * pcFmt) {
	RP("[H=%p  P=x%X  M=%d  C=%d '%s']" strNL, psXP->hdlr, psXP->pvPara, psXP->MaxLen, psXP->CurLen, pcFmt);
}

static x64_t x64PrintGetValue(xp_t * psXP) {
	x64_t X64;
	switch(psXP->flg.uSize) {
	case S_none:
	case S_hh:
	case S_h: {
		if (psXP->flg.bSigned == 1) {
			X64.i64 = va_arg(psXP->vaList, int);
		} else {
			X64.u64 = va_arg(psXP->vaList, unsigned int);
		}
		break;
	}
	case S_l: {
		if (psXP->flg.bSigned == 1) {
			X64.i64 = va_arg(psXP->vaList, i32_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, u32_t);
		}
		break;
	}
	case S_ll: {
		if (psXP->flg.bSigned) {
			X64.i64 = va_arg(psXP->vaList, i64_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, u64_t);
		}
		break;
	}
	case S_z: {
		if (psXP->flg.bSigned) {
			X64.i64 = va_arg(psXP->vaList, ssize_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, size_t);
		}
		break;
	}
	default:
		IF_myASSERT(debugTRACK, 0);
		X64.u64 = 0ULL;
	}
	if (psXP->flg.bSigned && X64.i64 < 0LL) {			// signed value requested ?
		psXP->flg.bNegVal = 1;							// and value is negative, set the flag
		X64.i64 *= -1; 									// and convert to unsigned
	} else if (psXP->flg.bSigned == 0) {				// unsigned requested ?
		X64.u64 &= BIT_MASK64(0, S_bits[psXP->flg.uSize]);	// Ensure sign-extended bits removed
	}
	return X64;
}

/**
 * @brief		determine the hexadecimal character representing the value of the low order nibble
 * @brief		Based on the Flag, the pointer will be adjusted for UpperCase (if required)
 * @param[in]	psXP - pointer to structure controlling the operation
 * @param[in]	Val = value in low order nibble, to be converted
 * @return		pointer to the correct character
 */
static char cPrintNibbleToChar(xp_t * psXP, u8_t Val) {
	char cChr = hexchars[Val];
	if (psXP->flg.bCase == 0 && Val > 9)
		cChr += 0x20;									// convert to lower case
	return cChr;
}

// #################################### Destination handlers #######################################

int xPrintToString(xp_t * psXP, const char * pcSrc, size_t sSrc) {
	if (halMemoryRAM(psXP->pvPara)) {
		memcpy(psXP->pvPara, pcSrc, sSrc);		// RISKY if insufficient space left in buffer...!!!
		psXP->pvPara += sSrc;
	}
	return sSrc;
}

int xPrintToHandle(xp_t * psXP, const char * pcSrc, size_t sSrc) { return write((int) psXP->pvPara, pcSrc, sSrc); }

int xPrintToConsole(xp_t * psXP, const char * pcSrc, size_t sSrc) {
	int iRV, Count = 0;
	while (Count < sSrc) {
		iRV = uart_tx_chars(configCONSOLE_UART, pcSrc, sSrc);
		if (iRV < 0)
			return iRV;
		Count += iRV;
	}
	return (iRV < 0) ? iRV : sSrc;
}

int xPrintToDevice(xp_t * psXP, const char * pcSrc, size_t sSrc) { return ((int (*)(const char *, size_t))psXP->pvPara)(pcSrc, sSrc); }

int xPrintToSocket(xp_t * psXP, const char * pcSrc, size_t sSrc) { return xNetSend((netx_t *)psXP->pvPara, (u8_t *)pcSrc, sSrc); }

int xPrintToUBuf(xp_t * psXP, const char * pcSrc, size_t sSrc) { return xUBufWrite((ubuf_t *)psXP->pvPara, pcSrc, sSrc); }

int xPrintToCRC32(xp_t * psXP, const char * pcSrc, size_t sSrc) {
	unsigned int * pU32 = psXP->pvPara;
	*pU32 = esp_rom_crc32_le(*pU32, (u8_t const *)pcSrc, sSrc);
	return sSrc;
}

// ############################# Foundation character and string output ############################

/**
 * @brief	perform a RAW string output to the selected "stream"
 * @brief	Does not perform ANY padding, justification or length checking
 * @param	psXP - pointer to structure controlling the operation
 * @param	pcSrc - pointer to the character array to be output
 * @param	sSrc number of characters to output
 * @return	number of ACTUAL characters output.
 * @note	Changes CurLen directly
 */
static int xPrintChars(xp_t * psXP, const char * pcSrc, size_t sSrc) {
	int sMax = psXP->MaxLen ? (psXP->MaxLen - psXP->CurLen) : xpfMAXLEN_MAXVAL;
	sMax = (sSrc <= sMax) ? sSrc : sMax;
	if (sMax > 0) {										// some space ?
		sMax = psXP->hdlr(psXP, pcSrc, sMax);			// process character(s)
		if (sMax > 0)									// if successful
			psXP->CurLen += sMax;						// adjust count
//		RP("[%.*s/%d] ", psXP->CurLen, psXP->pvPara - psXP->CurLen, psXP->CurLen);
		RP("%.*s", sSrc, pcSrc);
	}
	if (sMax != sSrc) {
		if (halMemoryRAM(psXP->pvPara))
			RP("'%.*s' ", psXP->CurLen, psXP->pvPara - psXP->CurLen);
		RP("Partial/error '%.*s' (%d vs %d)", sSrc, pcSrc, sMax, sSrc);
		esp_backtrace_print(8);
		vShowSpinWait();
	}
	return sMax;
}

/**
 * @brief	output a single character via xPrintChars()
 * @param	psXP - pointer to control structure to be referenced/updated
 * @param	iChr - char to be output
 * @note	see xPrintChars() for uses and changes
 * @return	result from xPrintChars()
 */
static int xPrintChar(xp_t * psXP, int iChr) {
	char cChr = iChr;
	return xPrintChars(psXP, &cChr, sizeof(cChr));
}

/**
 * @brief	perform formatted output of a string to the preselected device/string/buffer/file
 * @param	psXP - pointer to structure controlling the operation
 * @param	pcSrc - pointer to the string to be output
 * @note	Uses bPrecis Precis bMinWid MinWid bPad0 bAltF bLeft
 * @note	Changes CurLen indirectly through xPrintChar
 * @return	number of ACTUAL characters output.
 */
static void vPrintStringJustified(xp_t * psXP, char * pcSrc) {
	// determine natural or limited length of string
	size_t uLen;
	if (psXP->flg.bPrecis && psXP->flg.bMinWid && (psXP->flg.Precis <= psXP->flg.MinWid)) {
		uLen = xstrnlen(pcSrc, psXP->flg.Precis);
	} else {
		uLen = psXP->flg.Precis ? psXP->flg.Precis : strlen(pcSrc);
	}
	// Determine total padding required.
	size_t Tpad = (psXP->flg.MinWid > uLen) ? (psXP->flg.MinWid - uLen) : 0;
	// Split total padding between left & right sides
	size_t Lpad = 0, Rpad = 0;
	if (Tpad) {
		if (psXP->flg.bAltF) {		// Centre string?
			Lpad = Tpad >> 1;		// possibly smaller windows on left
			Rpad = Tpad - Lpad;		// possibly larger window on right
		} else if (psXP->flg.bLeft) {
			Rpad = Tpad;
		} else {
			Lpad = Tpad;
		}
	}
	char cPad = psXP->flg.bPad0 ? CHR_0 : CHR_SPACE;
	for (;Lpad--; xPrintChars(psXP, &cPad, 1));			/* output Left pad characters */
	for (;uLen-- && *pcSrc; ++pcSrc) {
		char cAdj = psXP->flg.bGT ? tolower(*pcSrc) : psXP->flg.bLT ? toupper(*pcSrc) : *pcSrc;
		xPrintChars(psXP, &cAdj, 1);
	}
	for (;Rpad--; xPrintChars(psXP, &cPad, 1));			/* output Right pad characters */
}

/**
 * @brief	Convert u64_t value to a formatted string (build right to left)
 * @param	psXP - pointer to control structure
 * @param	U64 - u64_t value to convert & output
 * @param	pBuffer - pointer to buffer for converted string storage
 * @param	BufSize - available (remaining) space in buffer
 * @return	number of actual characters output (incl leading '-' and/or ' ' and/or '0' as possibly added)
 * @note	Honour & interpret the following modifiers
 * @note	'#' Enable scaling in SI units
 * @note	''' If scaling select bGroup = 0/1 columns below.
 * @note		If not scaling group digits in 3's (3rd column below)
 * @note	'-' Left align the individual numbers between the '.'
 * @note	'+' Force a '+' or '-' sign on the left
 * @note	'0' Zero pad to the left of the value to fill the field
 * @note	Uses bAltF bGroup uBase bLeft bPad0 bNegVal bPlus MinWid
 * @note	Changes nothing
 * @note	Buffer overflow if incorrect BufSize allocated in calling function
*/
static int	xPrintValueJustified(xp_t * psXP, u64_t U64, char * pBuffer, int BufSize) {
	int	Len = 0;
	int Count, iTemp;
	char * pTemp = pBuffer + BufSize - 1;				// Point to last space in buffer
	if (U64) {
		#if	(xpfSUPPORT_SCALING > 0)
		/*	bGroup=0	bGRoup=1	No scaling specified
		 *	12.34Q		12Q34		12,345,678,900,000,000,000
		 *	1.234Q		1Q234		1,234,567,890,000,000,000
		 *	123.4q		123q4		123,456,789,000,000,000
		 *	12.34q		12q34		12,345,678,900,000,000
		 *	1.234q		1q234		1,234,567,890,000,000
		 *	123.4T		123T4		123,456,789,000,000
		 *	12.34T		12T34		12,345,678,900,000
		 *	1.234T		1T234		1,234,567,890,000
		 *	123.4B		123B4		123,456,789,000
		 *	12.34B		12B34		12,345,678,900
		 *	1.234B		1B234		1,234,567,890
		 *	123.4M		123M4		123,456,789
		 *	12.34M		12M34		12,345,678
		 *	1.234M		1M234		1,234,567
		 *	123.4K		123K4		123,456
		 *	12.34K		12K34		12,345
		 *	1.234K		1K234		1,234
		 *	123			123			123
		*/
		if (psXP->flg.bAltF) {
			u32_t I, F;
			u8_t ScaleChr;
			if (U64 >= Q1)		{ F = U64 % Q1; I = U64 / Q1; ScaleChr = CHR_Q; }
			else if (U64 >= q1)	{ F = U64 % q1; I = U64 / q1; ScaleChr = CHR_q; }
			else if (U64 >= T1)	{ F = U64 % T1; I = U64 / T1; ScaleChr = CHR_T; }
			else if (U64 >= B1)	{ F = U64 % B1; I = U64 / B1; ScaleChr = CHR_B; }
			else if (U64 >= M1)	{ F = U64 % M1; I = U64 / M1; ScaleChr = CHR_M; }
			else if (U64 >= K1)	{ F = U64 % K1; I = U64 / K1; ScaleChr = CHR_K; }
			else { ScaleChr = 0; }
			if (ScaleChr) {
				if (psXP->flg.bGroup == 0) {
					*pTemp-- = ScaleChr;
					++Len;
				}
				// calculate & convert the required # of fractional digits
				int Ilen = xDigitsInU32(I, 0);			// 1 (0) -> 3 (999)
				int Flen = xDigitsInU32(F, 0);
				// if field width has been specified, use that as base, adjusted for scaling character
				#define xpMIN_SCALED_WIDTH	5		// 3?1 / 2?2 / 1?3
				int MaxDigit = (psXP->flg.bMinWid && (psXP->flg.MinWid > xpMIN_SCALED_WIDTH)) ? psXP->flg.MinWid : xpMIN_SCALED_WIDTH;
				MaxDigit -= psXP->flg.bGroup ? 1 : 2;	// lose 1 (?) or 2 (.?) positions
				if ((Ilen + Flen) > MaxDigit) {			// if sum of I+F digits > available space
					F /= u32pow(psXP->flg.uBase, Flen - (MaxDigit - Ilen));
					Flen = MaxDigit - Ilen;				// adjust Flen to what remains
				}
				while (Flen--) {
					*pTemp-- = cPrintNibbleToChar(psXP, F % psXP->flg.uBase);	// digit
					++Len;
					F /= psXP->flg.uBase;
				}
				*pTemp-- = psXP->flg.bGroup ? ScaleChr : CHR_FULLSTOP;
				++Len;
				U64 = I;
			}
		}
		#endif
		// convert to string starting at end of buffer from R/Least -> L/Most significant digits
		Count = 0;
		while (U64) {
			iTemp = U64 % psXP->flg.uBase;			// calculate the next remainder ie digit
			*pTemp-- = cPrintNibbleToChar(psXP, iTemp);
			++Len;
			U64 /= psXP->flg.uBase;
			if (U64 && psXP->flg.bGroup) {			// handle digit grouping, if required
				if ((++Count % 3) == 0) {
					*pTemp-- = CHR_COMMA;
					++Len;
					Count = 0;
				}
			}
		}
	} else {
		*pTemp-- = CHR_0;
		Len = 1;
	}

	// First check if ANY form of padding required
	if (psXP->flg.bLeft == 0) {							// right justified (ie pad left) ???
		/* this section ONLY when value is RIGHT justified.
		 * For ' ' padding format is [       -xxxxx]
		 * whilst '0' padding it is  [-0000000xxxxx]
		 */
		Count = (psXP->flg.MinWid > Len) ? psXP->flg.MinWid - Len : 0;
		// If we are padding with ' ' and leading '+' or '-' is required, do that first
		if (psXP->flg.bPad0 == 0 && (psXP->flg.bNegVal || psXP->flg.bPlus)) {	// If a sign is required
			*pTemp-- = psXP->flg.bNegVal ? CHR_MINUS : CHR_PLUS;	// prepend '+' or '-'
			--Count;
			++Len;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXP->flg.bPad0 ? CHR_0 : CHR_SPACE;	// define applicable padding char
			Len += Count;								// Now do the actual padding
			while (Count--)
				*pTemp-- = iTemp;
		}
		// If we are padding with '0' AND a sign is required (-value or +requested), do that first
		if (psXP->flg.bPad0 && (psXP->flg.bNegVal || psXP->flg.bPlus)) {	// If +/- sign is required
			if (pTemp[1] == CHR_SPACE || pTemp[1] == CHR_0) {
				++pTemp;								// set overwrite last with '+' or '-'
			} else {
				++Len;									// set to add extra '+' or '-'
			}
			*pTemp = psXP->flg.bNegVal ? CHR_MINUS : CHR_PLUS;
		}
	} else if (psXP->flg.bNegVal || psXP->flg.bPlus) {	// If a sign is required
		*pTemp = psXP->flg.bNegVal ? CHR_MINUS : CHR_PLUS;
		++Len;
	}
	return Len;
}

/**
 * @brief
 * @param	psXP
 * @param	Value
 * @note	Uses vPrintStringJustified()
 * @note
*/
static void vPrintX64(xp_t * psXP, u64_t Value) {
	char Buffer[xpfMAX_LEN_X64];
	Buffer[xpfMAX_LEN_X64 - 1] = 0;				// terminate the buffer, single value built R to L
	int Len = xPrintValueJustified(psXP, Value, Buffer, xpfMAX_LEN_X64 - 1);
	vPrintStringJustified(psXP, Buffer + (xpfMAX_LEN_X64 - 1 - Len));
}

/**
 * @brief
 * @param
 * @note	Also called internally from HexDump for [optional] address
 * @note	Uses NONE
 * @note	Changes NONE
 * @note	Uses vPrintStringJustified()
 * @return
*/
static void vPrintPointer(xp_t * psXP, px_t pX) {
	char caBuf[xpfMAX_LEN_PNTR];
	caBuf[xpfMAX_LEN_PNTR-1] = CHR_NUL;
	XPC_SAVE(psXP);
	psXP->flg.uBase = BASE16;
	psXP->flg.bPad0 = 1;
	psXP->flg.bNegVal = 0;
	psXP->flg.bPlus = 0;								// no leading '+'
	psXP->flg.bAltF = 0;								// disable scaling
	psXP->flg.bGroup = 0;								// disable 3-digit grouping
	x64_t X64;
	#if (xpfSIZE_POINTER == 2)
		psXP->flg.bMinWid = psXP->flg.bPrecis = 1;
		psXP->flg.MinWid = psXP->flg.Precis = 4;
		X64.u64 = (u16_t) pX.pv;
	#elif (xpfSIZE_POINTER == 4)
		psXP->flg.bMinWid = psXP->flg.bPrecis = 1;
		psXP->flg.MinWid = psXP->flg.Precis = 8;
		X64.u64 = (u32_t) pX.pv;
	#elif (xpfSIZE_POINTER == 8)
		psXP->flg.bMinWid = psXP->flg.bPrecis = 1;
		psXP->flg.MinWid = psXP->flg.Precis = 16;
		X64.u64 = pX.pv;
	#endif
	int Len = xPrintValueJustified(psXP, X64.u64, caBuf, xpfMAX_LEN_PNTR - 1);
	memcpy(&caBuf[xpfMAX_LEN_PNTR - 3 - Len], psXP->flg.bCase ? "0X" : "0x", 2);	// prepend
	Len += 2;
	psXP->flg.MinWid += 2;
	psXP->flg.Precis += 2;
	vPrintStringJustified(psXP, &caBuf[xpfMAX_LEN_PNTR - Len - 1]);
	XPC_REST(psXP);
}

// ############################## Optional float/doublesupport #####################################
#if	(xpfSUPPORT_IEEE754 == 1)
/**
 * @brief	convert double value based on flags supplied and output via control structure
 * @param	psXP pointer to control structure
 * @param	F64 double value to be converted
 * @note	Uses bCase bNegVal uForm Precis
 * @note	Changes bPrecis Precis
 * @note	Uses vPrintStringJustified()
 * References:
 * http://git.musl-libc.org/cgit/musl/blob/src/stdio/vfprintf.c?h=v1.1.6
 * https://en.cppreference.com/w/c/io/fprintf
 * https://pubs.opengroup.org/onlinepubs/007908799/xsh/fprintf.html
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160
 */
static void vPrintF64(xp_t * psXP, double F64) {
	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005, 0.000000005,
		0.0000000005, 0.00000000005, 0.000000000005, 0.0000000000005, 0.00000000000005,
		0.000000000000005, 0.0000000000000005 };
	if (isnan(F64)) {
		vPrintStringJustified(psXP, psXP->flg.bCase ? "NAN" : "nan");
		return;
	} else if (isinf(F64)) {
		vPrintStringJustified(psXP, psXP->flg.bCase ? "INF" : "inf");
		return;
	}
	psXP->flg.bNegVal = F64 < 0.0 ? 1 : 0;				// set bNegVal if < 0.0
	F64 *= psXP->flg.bNegVal ? -1.0 : 1.0;				// convert to positive number
	XPC_SAVE(psXP);
	x64_t X64  = { 0 };

	int	Exp = 0;										// if exponential format requested, calculate the exponent
	if (F64 != 0.0) {									// if not 0 and...
		if (psXP->flg.uForm != form1F) {					// any of "eEgG" specified ?
			X64.f64	= F64;
			while (X64.f64 > 10.0) {					// if number is greater that 10.0
				X64.f64 /= 10.0;						// re-adjust and
				Exp += 1;								// update exponent
			}
			while (X64.f64 < 1.0) {						// similarly, if number smaller than 1.0
				X64.f64 *= 10.0;						// re-adjust upwards and
				Exp	-= 1;								// update exponent
			}
		}
	}
	u8_t AdjForm = psXP->flg.uForm != form0G ? psXP->flg.uForm :
					(Exp <- 4 || Exp >= psXP->flg.Precis) ? form2E : form1F;
	if (AdjForm == form2E)
		F64 = X64.f64;									// change to exponent adjusted value
	if (F64 < (DBL_MAX - round_nums[psXP->flg.Precis]))	// if addition of rounding value will NOT cause overflow.
		F64 += round_nums[psXP->flg.Precis];			// round by adding .5LSB to the value
	char Buffer[xpfMAX_LEN_F64];
	Buffer[xpfMAX_LEN_F64 - 1] = 0;						// building R to L, ensure buffer NULL-term

	int Len = 0;
	if (AdjForm == form2E) {							// If required, handle the exponent
		psXP->flg.MinWid = 2;
		psXP->flg.bPad0 = 1;							// MUST left pad with '0'
		psXP->flg.bSigned = 1;
		psXP->flg.bLeft = 0;
		psXP->flg.bNegVal = (Exp < 0) ? 1 : 0;
		Exp *= psXP->flg.bNegVal ? -1LL : 1LL;
		Len += xPrintValueJustified(psXP, Exp, Buffer, xpfMAX_LEN_F64 - 1);
		Buffer[xpfMAX_LEN_F64-2-Len] = psXP->flg.bCase ? CHR_E : CHR_e;
		++Len;
	}

	X64.f64	= F64 - (u64_t) F64;						// isolate fraction as double
	X64.f64	= X64.f64 * (double) u64pow(10, psXP->flg.Precis);	// fraction to integer
	X64.u64	= (u64_t) X64.f64;							// extract integer portion

	if (psXP->flg.bPrecis) {							// explicit MinWid specified ?
		psXP->flg.MinWid = psXP->flg.Precis; 			// yes, stick to it.
	} else if (X64.u64 == 0) {							// process 0 value
		psXP->flg.MinWid = psXP->flg.bRadix ? 1 : 0;
	} else {											// process non 0 value
		if (psXP->flg.bAltF && psXP->flg.uForm == form0G) {
			psXP->flg.MinWid = psXP->flg.Precis;		// keep trailing 0's
		} else {
			u32_t u = u64Trailing0(X64.u64);
			X64.u64 /= u64pow(10, u);
			psXP->flg.MinWid = psXP->flg.Precis - u;	// remove trailing 0's
		}
	}
	if (psXP->flg.MinWid > 0) {
		psXP->flg.bPad0 = 1;							// MUST left pad with '0'
		psXP->flg.bGroup = 0;							// cannot group in fractional
		psXP->flg.bSigned = 0;							// always unsigned value
		psXP->flg.bNegVal = 0;							// and never negative
		psXP->flg.bPlus = 0;							// no leading +/- before fractional part
		Len += xPrintValueJustified(psXP, X64.u64, Buffer, xpfMAX_LEN_F64-1-Len);
	}
	// process the bRadix = '.'
	if (psXP->flg.MinWid || psXP->flg.bRadix) {
		Buffer[xpfMAX_LEN_F64 - 2 - Len] = CHR_FULLSTOP;
		++Len;
	}

	X64.u64	= F64;										// extract and convert the whole number portions
	// adjust MinWid to do padding (if required) based on string length after adding whole number
	XPC_REST(psXP);
	psXP->flg.MinWid = psXP->flg.MinWid > Len ? psXP->flg.MinWid - Len : 0;
	Len += xPrintValueJustified(psXP, X64.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len);
	XPC_REST(psXP);
	psXP->flg.bPrecis = 1;
	psXP->flg.Precis = Len;
	vPrintStringJustified(psXP, Buffer + (xpfMAX_LEN_F64 - 1 - Len));
}
#endif

// ############################## Proprietary extension: Arrays ####################################
#if (xpfSUPPORT_ARRAYS > 0)
/**
 * @brief	Generate array of values separated by ','
 * @param	psXP
 * @note	Handles 8/16/32/64 bit values, un/signed/float
 * @note	Uses uSize bFloat bSigned 
 * @note	Changes CurLen indirectly through xPrintValueJustified() and vPrintStringJustified()
 * @note	Uses vPrintStringJustified()
*/
static void vPrintX64array(xp_t * psXP) {
	vs_e eVS = (psXP->flg.uSize == S_hh) ? vs08B :
				(psXP->flg.uSize == S_h) ? vs16B :
				(psXP->flg.uSize == S_l) ? vs32B :
				(psXP->flg.uSize == S_ll) ? vs64B : vs32B;
	vf_e eVF = psXP->flg.bFloat ? vfFXX : psXP->flg.bSigned ? vfIXX : vfUXX;
	cvi_e cvI = xFormSize2Index(eVF, eVS);
	x32_t X32; X32.iX = va_arg(psXP->vaList, int);	// number of array elements
	px_t pX; pX.pv = va_arg(psXP->vaList, void *);	// pointer to 1st element of array
	XPC_SAVE(psXP);
	while (X32.iX) {
		x64_t X64 = x64ValueFetch(pX, cvI);
		if (eVF == vfIXX && X64.i64 < 0LL) {
			psXP->flg.bNegVal = 1;
			X64.i64 *= -1; 	// convert the value to unsigned
		}
		if (psXP->flg.bFloat) {
			#if	(xpfSUPPORT_IEEE754 == 1)
				vPrintF64(psXP, X64.f64);
			#else
				assert(0);
			#endif
		} else {
			vPrintX64(psXP, X64.u64);
		}
		XPC_REST(psXP);
		if (--X32.iX)
			xPrintChar(psXP, CHR_COMMA);
		pX = pxAddrNextWithIndex(pX, cvI);
	}
}
#endif

// ############################# Proprietary extension: hexdump ####################################
#if	(xpfSUPPORT_HEXDUMP == 1)
/**
 * @brief	convert u8_t to 2 hex chars in buffer, NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned byte to convert
 * @return	none
 */
static void vPrintHexU8(xp_t * psXP, u8_t Value) {
	xPrintChar(psXP, cPrintNibbleToChar(psXP, Value >> 4));
	xPrintChar(psXP, cPrintNibbleToChar(psXP, Value & 0x0F));
}

/**
 * @brief	convert u16_t to 4 hex chars in buffer, NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned short to convert
 * @return	none
 */
static void vPrintHexU16(xp_t * psXP, u16_t Value) {
	vPrintHexU8(psXP, (Value >> 8) & 0x000000FF);
	vPrintHexU8(psXP, Value & 0x000000FF);
}

/**
 * @brief	convert u32_t to 4 hex chars in buffer, NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned word to convert
 * @return	none
 */
static void vPrintHexU32(xp_t * psXP, u32_t Value) {
	vPrintHexU16(psXP, (Value >> 16) & 0x0000FFFF);
	vPrintHexU16(psXP, Value & 0x0000FFFF);
}

/**
 * @brief	convert u64_t to 8 hex chars in buffer, NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned dword to convert
 * @return	none
 */
static void vPrintHexU64(xp_t * psXP, u64_t Value) {
	vPrintHexU32(psXP, (Value >> 32) & 0xFFFFFFFFULL);
	vPrintHexU32(psXP, Value & 0xFFFFFFFFULL);
}

/**
 * @brief	convert values to hex chars in buffer, NULL terminated
 * @param 	psXP
 * @param 	Num number of bytes to print
 * @param 	pcSrc pointer to bytes to print
 * @note	Use the following modifier flags
 *			' select grouping separators ":- |" (byte/half/word/dword)
 *			# select reverse order (little/big endian)
 *			Uses uSize bAltF bGroup uForm
 */
static void vPrintHexValues(xp_t * psXP, int Num, char * pcSrc) {
	int s = S_bytes[psXP->flg.uSize];
	if (psXP->flg.bAltF)								// invert order ?
		pcSrc += Num - s;								// working backwards so point to last
	x64_t x64Val;
	int	Idx	= 0;
	while (Idx < Num) {
		switch (s >> 1) {
		case 0:
			x64Val.x8[0].u8 = *((u8_t *) pcSrc);
			vPrintHexU8(psXP, x64Val.x8[0].u8);
			break;
		case 1:
			x64Val.x16[0].u16 = *((u16_t *) pcSrc);
			vPrintHexU16(psXP, x64Val.x16[0].u16);
			break;
		case 2:
			x64Val.x32[0].u32 = *((u32_t *) pcSrc);
			vPrintHexU32(psXP, x64Val.x32[0].u32);
			break;
		case 4:
			x64Val.u64 = *((u64_t *) pcSrc);
			vPrintHexU64(psXP, x64Val.u64);
			break;
		default:
			assert(0);
		}
		// step to the next 8/16/32/64 bit value
		if (psXP->flg.bAltF) {							// invert order ?
			pcSrc -= s;									// update source pointer backwards
		} else {
			pcSrc += s;									// update source pointer forwards
		}
		Idx += s;
		if (Idx >= Num)
			break;
		// now handle the grouping separator(s) if any
		if (psXP->flg.bGroup) {							// separator required?
			char cSep = psXP->flg.uForm != form3X ? CHR_COLON : (Idx % 8) == 0 ? CHR_SPACE : (Idx % 4) == 0 ? CHR_VERT_BAR : (Idx % 2) == 0 ? CHR_MINUS : CHR_COLON;
			xPrintChars(psXP, &cSep, 1);
		}
	}
}

/**
 * @brief		Dumps a block of memory in debug style format. depending on options output can be
 * @brief		formatted as 8/16/32 or 64 bit variables, optionally with no, absolute or relative address
 * @param[in]	psXP pointer to print control structure
 * @param[in]	pcSrc pointer to memory starting address
 * @param[in]	Len Length of memory buffer to display
 * @param[out]	none
 * @note		Use the following modifier flags
 * @note		'	Grouping of values using ' ' '-' or '|' as separators
 * @note		!	Use relative address format
 * @note		#	Use absolute address format
 * @note			Relative/absolute address prefixed using format '0x12345678:'
 * @note		+	Add the ASCII char equivalents to the right of the hex output
 * @return		none
 */
static void vPrintHexDump(xp_t * psXP, int sSrc, char * pcSrc) {
	xpc_t sXPC = { .u64XPC = psXP->u64XPC };
	int iWidth;
	if (psXP->flg.bMinWid && INRANGE(8, psXP->flg.MinWid, 64)) {
		iWidth = psXP->flg.MinWid;
	} else {
		iWidth = xStdioGetMaxColX(NULL);
		if (psXP->flg.bLeft && iWidth <= (32+6)) {
			psXP->flg.bLeft = 0;						// little space, disable addr
		} else { 
			iWidth -= psXP->flg.bLeft ? 0 : 11;			// addr req, decr rem space
		}
		if (psXP->flg.bPlus == 0 || iWidth > 32) {
			iWidth /= 4;								// ASCII on right = 4 col/char
		} else {
			psXP->flg.bPlus = 0;						// too little space, disable ASCII
			iWidth /= 3;								// no ASCII on right = 3 col/char
		}
		iWidth -= iWidth % 8;
	}
	if (iWidth > xpfMAXWIDTH_HEXDUMP)
		iWidth = xpfMAXWIDTH_HEXDUMP;
	for (int Now = 0; Now < sSrc; Now += iWidth) {
		if (psXP->flg.bLeft == 0) {						// display address (absolute/relative)
			vPrintPointer(psXP, (px_t) (psXP->flg.bRelVal ? (void *) Now : (void *) (pcSrc + Now)));
			xPrintChars(psXP, ": ", 2);
			psXP->u64XPC = sXPC.u64XPC;
		}
		// then the actual series of values in 8-32 bit groups
		int Width = (sSrc - Now) > iWidth ? iWidth : sSrc - Now;
		vPrintHexValues(psXP, Width, pcSrc + Now);
		if (psXP->flg.bPlus) {							// ASCII equivalent requested?
			int s = S_bytes[psXP->flg.uSize];
			u32_t Count = (sSrc <= iWidth) ? 1 :
				((iWidth - Width) / s) * (s * 2 + (psXP->flg.bGroup ? 1 : 0)) + 1;
			while (Count--)
				xPrintChar(psXP, CHR_SPACE);			// handle space padding for ASCII dump to line up
			for (Count = 0; Count < Width; ++Count) {	// values as ASCII characters
				int cChr = *(pcSrc + Now + Count);
				// theoretically support up to 0xFF, but loses some characters idp.py and Serial
				xPrintChar(psXP, (cChr < 0x20 || cChr == 0x7F || cChr == 0xFF) ? CHR_FULLSTOP : cChr);
			}
		}
		if ((Now < sSrc) && (sSrc > iWidth))			// data left to be dumped & 1+ lines of output
			xPrintChars(psXP, strNL, sizeof(strNL)-1);	// do NL for this line.
	}
}
#endif

// ############################# Proprietary extensions: date & time ###############################
#if	(xpfSUPPORT_DATETIME == 1)
/**
 * @brief	Calculate # of local time seconds (uses TSZ info if available)
 * @note	Uses bPlus bAltF bRelVal
 * @note	Changes nothing
 * @note	If psTM supplied will populate structure accordingly
 * @return	seconds (epoch or relative)
 */
static  __attribute__((unused)) seconds_t xPrintCalcSeconds(xp_t * psXP, tsz_t * psTSZ, struct tm * psTM) {
	seconds_t Seconds;
	// Get seconds value to use... (adjust for TZ if required/allowed)
	if (psTSZ->pTZ &&									// TZ info available
		psXP->flg.bPlus == 1 &&							// TZ info requested
		psXP->flg.bAltF == 0 &&							// NOT bAltF (ALT form always GMT/UTC)
		psXP->flg.bRelVal == 0) {						// NOT relative time (has no TZ info)
		Seconds = xTimeCalcLocalTimeSeconds(psTSZ);
	} else {
		Seconds = xTimeStampSeconds(psTSZ->usecs);
	}
	if (psTM)											// convert seconds into components
		xTimeGMTime(Seconds, psTM, psXP->flg.bRelVal);
	return Seconds;
}

/**
 * @brief	Calulate buffer space required to format print a value
 * @note	Uses bRelVal bPad0 bGroup
 * @note	Changes MinWid
 * @return	Number of digits if value formatted print
 */
static int	xPrintTimeCalcSize(xp_t * psXP, u32_t uVal) {		// REVIEW to make generic for any value...
	int Len = xDigitsInU32(uVal, psXP->flg.bGroup);
	if (psXP->flg.bNegVal)
		++Len;
	if ((uVal < 10) && psXP->flg.bPad0)
		++Len;
	return psXP->flg.MinWid = Len;
}

/**
 * @brief	Formats YEAR value into buffer
 * @note	Uses bAltF bGroup
 * @note	Changes MinWid
 * @return	Number of characters stored ie length of generated output
 */
static int	xPrintDate_Year(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	psXP->flg.MinWid = 0;
	int Len = xPrintValueJustified(psXP, (u64_t) (psTM->tm_year + YEAR_BASE_MIN), pBuffer, 4);
	if (psXP->flg.bAltF == 0)
		pBuffer[Len++] = Delim0[psXP->flg.bGroup];
	return Len;
}

/**
 * @brief	Formats MONTH value into buffer
 * @note	Uses bAltF bGroup
 * @note	Changes MinWid
 * @return	Number of characters stored ie length of generated output
 */
static int	xPrintDate_Month(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	int Len = xPrintValueJustified(psXP, (u64_t) (psTM->tm_mon + 1), pBuffer, psXP->flg.MinWid = 2);
	pBuffer[Len++] = psXP->flg.bAltF ? CHR_SPACE : Delim0[psXP->flg.bGroup];
	return Len;
}

/**
 * @brief	Formats DAY value into buffer
 * @note	Uses bAltF bGroup bRelVal 
 * @note	Changes bPad0
 * @return	Number of characters stored ie length of generated output
 */
static int	xPrintDate_Day(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	int Len = xPrintValueJustified(psXP, (u64_t) psTM->tm_mday, pBuffer, xPrintTimeCalcSize(psXP, psTM->tm_mday));
	pBuffer[Len++] = psXP->flg.bAltF ? CHR_SPACE :
					(psXP->flg.bRelVal && psXP->flg.bGroup) ? CHR_d : Delim1[psXP->flg.bGroup];
	psXP->flg.bPad0 = 1;
	return Len;
}

/**
 * @brief	Converts epoch date component to text format as specified
 * @param	psXP ponter to control structure
 * @param	psTM pointer to date & time component structure
 * @note	Uses bAltF bRelVal bPad0
 * @note	Changes bAltF MinWid bPrecis
 * @note	Uses vPrintStringJustified()
*/
static void vPrintDate(xp_t * psXP, struct tm * psTM) {
	int	Len = 0;
	char Buffer[xpfMAX_LEN_DATE];
	if (psXP->flg.bAltF) {
		Len += xstrncpy(Buffer, xTimeGetDayName(psTM->tm_wday), 3);		// "Sun"
		Len += xstrncpy(Buffer+Len, ", ", 2);							// "Sun, "
		Len += xPrintDate_Day(psXP, psTM, Buffer+Len);					// "Sun, 10 "
		Len += xstrncpy(Buffer+Len, xTimeGetMonthName(psTM->tm_mon), 3);// "Sun, 10 Sep"
		Len += xstrncpy(Buffer+Len, " ", 1);							// "Sun, 10 Sep "
		psXP->flg.bAltF = 0;
		Len += xPrintDate_Year(psXP, psTM, Buffer+Len);					// "Sun, 10 Sep 2017"
	} else {
		if (psXP->flg.bRelVal == 0) {					// if epoch value do YEAR+MON
			Len += xPrintDate_Year(psXP, psTM, Buffer+Len);
			Len += xPrintDate_Month(psXP, psTM, Buffer+Len);
		}
		if (psTM->tm_mday || psXP->flg.bPad0) {
			Len += xPrintDate_Day(psXP, psTM, Buffer+Len);
		}
	}
	Buffer[Len] = 0;									// converted L to R, so terminate
	psXP->flg.Precis = 0;								// enable full string
	vPrintStringJustified(psXP, Buffer);
}

/**
 * @brief	Converts epoch time component to text format as specified
 * @param	psXP ponter to control structure
 * @param	psTM pointer to date & time component structure
 * @note	Uses bPad0 bGroup bRadix Precis
 * @note	Changes bPad0 bLeft bGroup
 * @note	Uses vPrintStringJustified()
*/
static void vPrintTime(xp_t * psXP, struct tm * psTM, u32_t uSecs) {
	char Buffer[xpfMAX_LEN_TIME];
	int	Len;
	XPC_SAVE(psXP);
	// Part 1: hours
	if (psTM->tm_mday || psTM->tm_hour || psXP->flg.bPad0) {
		Len = xPrintValueJustified(psXP, (u64_t) psTM->tm_hour, Buffer, xPrintTimeCalcSize(psXP, psTM->tm_hour));
		Buffer[Len++] = Delim2[sXPC.flg.bGroup];
		psXP->flg.bPad0 = 1;
		psXP->flg.bNegVal = 0;		// disable possible second '-'
	} else {
		Len = 0;
	}

	// Part 2: minutes
	if (psTM->tm_min || psXP->flg.bPad0) {
		Len += xPrintValueJustified(psXP, (u64_t) psTM->tm_min, Buffer+Len, xPrintTimeCalcSize(psXP, psTM->tm_min));
		Buffer[Len++] = Delim3[sXPC.flg.bGroup];
		psXP->flg.bPad0 = 1;
		psXP->flg.bNegVal = 0;		// disable possible second '-'
	}

	// Part 3: seconds
	Len += xPrintValueJustified(psXP, (u64_t) psTM->tm_sec, Buffer+Len, xPrintTimeCalcSize(psXP, psTM->tm_sec));

	// Part 4: [.xxxxxx]
	if (psXP->flg.Precis > xpfMAX_TIME_FRAC)
		psXP->flg.Precis = xpfMAX_TIME_FRAC;
	if (psXP->flg.bRadix || psXP->flg.Precis)
		Buffer[Len++] = CHR_FULLSTOP;
	if (psXP->flg.Precis) {
		if (psXP->flg.Precis < xpfMAX_TIME_FRAC)
			uSecs /= u32pow(10, xpfMAX_TIME_FRAC - psXP->flg.Precis);
		psXP->flg.bPad0 = 1;							// need leading '0's
		psXP->flg.bLeft = 0;							// force R-just
		psXP->flg.bGroup = 0;
		psXP->flg.bNegVal = 0;							// fractions can only be positive
		Len += xPrintValueJustified(psXP, uSecs, Buffer+Len, psXP->flg.MinWid = psXP->flg.Precis);
	}
	if (sXPC.flg.bGroup)
		Buffer[Len++] = CHR_s;
	Buffer[Len] = 0;

	XPC_REST(psXP);
	// if MinWid specified and value smaller than string length override MinWid to enable full string
	if (psXP->flg.bMinWid && psXP->flg.MinWid < Len)
		psXP->flg.MinWid = Len;
	psXP->flg.Precis = 0;								// Remove restriction on max string length
	vPrintStringJustified(psXP, Buffer);
}

/**
 * @brief	
 * @note	Uses vPrintStringJustified
 */
static void vPrintZone(xp_t * psXP, tsz_t * psTSZ) {
	int	Len = 0;
	char Buffer[configTIME_MAX_LEN_TZINFO];
	if (psTSZ->pTZ == 0) {								// If no TZ info supplied
		Buffer[Len++] = CHR_Z;

	} else if (psXP->flg.bAltF) {						// TZ info available but '#' format specified
		Len = xstrncpy(Buffer, (char *) " GMT", 4);	// show as GMT (ie UTC)

	} else {											// TZ info available & '+x:xx(???)' format requested
		#if	(timexTZTYPE_SELECTED == timexTZTYPE_RFC5424)
		if (psXP->flg.bPlus) {
			psXP->flg.bSigned = 1;						// TZ hours offset is a signed value
			Len += xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer+Len, psXP->flg.MinWid = 3);
			Buffer[Len++] = Delim2[psXP->flg.bGroup];
			psXP->flg.bSigned = 0;						// TZ offset minutes unsigned
			psXP->flg.bPlus = 0;
			Len += xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer+Len, psXP->flg.MinWid = 2);
		} else {
			Buffer[Len++] = CHR_Z;						// add 'Z' for Zulu/zero time zone
		}
		#elif (timexTZTYPE_SELECTED == timexTZTYPE_POINTER)
		psXP->flg.bSigned = 1;							// TZ hours offset is a signed value
		psXP->flg.bPlus = 1;							// force display of sign
		Len = xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXP->flg.MinWid = 3);
		Buffer[Len++] = Delim2[psXP->flg.bGroup];
		psXP->flg.bSigned = 0;							// TZ offset minutes unsigned
		psXP->flg.bPlus = 0;
		Len += xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXP->flg.MinWid = 2);
		if (psTSZ->pTZ->pcTZName) {						// handle TZ name (as string pointer) if there
			Buffer[Len++] = CHR_L_ROUND;
			psXP->flg.MinWid = 0;						// then complete with the TZ name
			while (psXP->flg.MinWid < configTIME_MAX_LEN_TZNAME &&
					psTSZ->pTZ->pcTZName[psXP->flg.MinWid] != 0) {
				Buffer[Len + psXP->flg.MinWid] = psTSZ->pTZ->pcTZName[psXP->flg.MinWid];
				psXP->flg.MinWid++;
			}
			Len += psXP->flg.MinWid;
			Buffer[Len++] = CHR_R_ROUND;
		}
		#elif (timexTZTYPE_SELECTED == timexTZTYPE_FOURCHARS)
		psXP->flg.bSigned = 1;							// TZ hours offset is a signed value
		psXP->flg.bPlus = 1;							// force display of sign
		Len = xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXP->flg.MinWid = 3);
		Buffer[Len++] = Delim2[psXP->flg.bGroup];
		psXP->flg.bSigned = 0;							// TZ offset minutes unsigned
		psXP->flg.bPlus = 0;
		Len += xPrintValueJustified(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXP->flg.MinWid = 2);
		// Now handle the TZ name if there, check to ensure max 4 chars all UCase
		if (xstrverify(&psTSZ->pTZ->tzname[0], CHR_A, CHR_Z, configTIME_MAX_LEN_TZNAME) == erSUCCESS) {
			Buffer[Len++] = CHR_L_ROUND;
			// then complete with the TZ name
			psXP->flg.MinWid = 0;
			while ((psXP->flg.MinWid < configTIME_MAX_LEN_TZNAME) &&
					(psTSZ->pTZ->tzname[psXP->flg.MinWid] != 0) &&
					(psTSZ->pTZ->tzname[psXP->flg.MinWid] != CHR_SPACE)) {
				Buffer[Len + psXP->flg.MinWid] = psTSZ->pTZ->tzname[psXP->flg.MinWid];
				psXP->flg.MinWid++;
			}
			Len += psXP->flg.MinWid;
			Buffer[Len++] = CHR_R_ROUND;
		}
		#endif
	}
	Buffer[Len] = 0;									// converted L to R, so add terminating NUL
	psXP->flg.MinWid = 0;								// enable full string
	vPrintStringJustified(psXP, Buffer);
}
#endif

// ################################# Proprietary extension: URLs ###################################
#if	(xpfSUPPORT_URL == 1)
/**
 * @brief	generate URL 
 * @param	psXP
 * @param	pcSrc
 * @note	Uses vPrintStringJustified
 */
static void vPrintURL(xp_t * psXP, char * pcSrc) {
	if (halMemoryANY(pcSrc)) {
		char cIn;
		while ((cIn = *pcSrc++) != 0) {
			if (isalnum(cIn) || strchr_i("-._~", cIn)) {
				xPrintChar(psXP, cIn);
			} else {
				xPrintChar(psXP, CHR_PERCENT);
				xPrintChar(psXP, cPrintNibbleToChar(psXP, cIn >> 4));
				xPrintChar(psXP, cPrintNibbleToChar(psXP, cIn & 0x0F));
			}
		}
	} else {
		vPrintStringJustified(psXP, pcSrc);						// "null" or "pOOR"
	}
}
#endif

/** ################################# The HEART of the PRINTFX matter ###############################
 * @brief	parse the format string and interpret the conversions, flags and modifiers
 * @param[in]	Hdlr pointer to output handler
 * @param[in]	pvPara optional parameter for some handlers
 * @param[in]	Size bitmapped flags (top XPC_BITS_XFER bits) and size (bottom xpfMINWID_BITS bits)
 * @param[in]	pcFmt pointer to the formatting string
 * @param[in]	vaList variable argument list structure
 * @return	number of characters output
 */

 int xPrintFX(int (Hdlr)(xp_t *, const char *, size_t), void * pvPara, size_t Size, const char * pcFmt, va_list vaList) {
	if (pcFmt == NULL)
		return 0;
	x32_t X32 = { 0 };
	xp_t sXP = { 0 };
	x64_t X64;										// default x64 variable
	px_t pX;
	int	cFmt;
#if	(xpfSUPPORT_DATETIME == 1)
	tsz_t * psTSZ;
	struct tm sTM;
#endif
#if	(xpfSUPPORT_SGR == 1)
	char Buffer[xpfMAX_LEN_SGR];
	sgr_info_t sSGR; 
#endif
// init common structure used throughout
	sXP.hdlr = Hdlr;
	sXP.pvPara = pvPara;
	sXP.vaList = vaList;
	// Cannot check for/change Size being 0 at this point since
	// 0 is used by [v]snprintfx() to calc number of output characters to be generated.
	if (Size > xpfMAXLEN_MAXVAL)
		sXP.val.flg2 = Size >> (32 - XPC_BITS_XFER);
	sXP.MaxLen = Size & xpfMAXLEN_MAXVAL;
	xpDebugStruct(&sXP, pcFmt);
	for (; *pcFmt != CHR_NUL; ++pcFmt) {
		if (*pcFmt != CHR_PERCENT) {					// current character NOT a '%'
			cFmt = strchr_i(pcFmt, CHR_PERCENT);		// check position of following '%', if any...
			if (cFmt == erFAILURE)						// no following '%' ?
				cFmt = strlen(pcFmt);					// determine length of string
			assert(cFmt > 0);
			xPrintChars(&sXP, pcFmt, cFmt);				// output character(s)
			pcFmt += cFmt;								// step to end
			continue;
		}
		++pcFmt;										// skip over '%'
		if (*pcFmt == CHR_PERCENT) {					// second '%' so "%%"
			xPrintChars(&sXP, pcFmt, 1);				// output single '%'
			continue;
		}
		if (*pcFmt == CHR_NUL)							// if at end of format string
			break;										// give up....
		sXP.flg.MinWid = sXP.flg.Precis = 0;			// clear MinWid & Precis
		sXP.val.flg1 = 0;								// reset internal/dynamic flags
		sXP.flg.uBase = BASE10;							// default number base
		// Optional FLAGS must be in correct sequence of interpretation
		while ((cFmt = strchr_i("!#&'*+-0><", *pcFmt)) != erFAILURE) {
			switch (cFmt) {
				case 0:	sXP.flg.bRelVal = 1; break;		// !	HEXDUMP/DTZ abs->rel address/time, MAC use ':' separator
				case 1:	sXP.flg.bAltF = 1; break;		// #	DTZ=GMT format, HEXDUMP/IP swop endian, STRING centre
				case 2: sXP.flg.bArray = 1; break;		// &	Array address ( and length) provided
				case 3: sXP.flg.bGroup = 1; break;		// '	"diu" add 3 digit grouping, DTZ, MAC, DUMP select separator set
				case 4: {								// *	indicate argument will supply field width
					X32.iX	= va_arg(sXP.vaList, int);
					IF_myASSERT(debugTRACK, sXP.flg.bMinWid == 0 && X32.iX <= xpfMINWID_MAXVAL);
					sXP.flg.MinWid = X32.iX;
					sXP.flg.bMinWid = 1;
					X32.i32 = 0;
					break;
				}
				case 5: sXP.flg.bPlus = 1; break;		// +	force +/-, HEXDUMP add ASCII, TIME add TZ info
				case 6: sXP.flg.bLeft = 1; break;		// -	Left justify, HEXDUMP remove address pointer
				case 7:	sXP.flg.bPad0 = 1; break;		// 0	force leading '0's
				case 8: sXP.flg.bGT = 1; break;			// >	to LC
				case 9: sXP.flg.bLT = 1; break;			// >	to UC
				default: assert(0);
			}
			++pcFmt;
		}

		// Optional WIDTH and PRECISION indicators
		if (*pcFmt == CHR_FULLSTOP || INRANGE(CHR_1, *pcFmt, CHR_9)) {
			while (1) {
				if (INRANGE(CHR_0, *pcFmt, CHR_9)) {
					X32.u32 *= 10;
					X32.u32 += *pcFmt - CHR_0;
				} else if (*pcFmt == CHR_FULLSTOP) {
					if (X32.u32 > 0) {					// save value if previously parsed
						IF_myASSERT(debugTRACK, sXP.flg.bMinWid == 0 && X32.u32 <= xpfMINWID_MAXVAL);
						sXP.flg.MinWid = X32.u32;
						sXP.flg.bMinWid = 1;
						X32.u32 = 0;
					}
					IF_myASSERT(debugTRACK, sXP.flg.bRadix == 0);	// 2x bRadix not allowed
					sXP.flg.bRadix = 1;
				} else if (*pcFmt == CHR_ASTERISK) {
					IF_myASSERT(debugTRACK, sXP.flg.bRadix == 1 && X32.u32 == 0);
					X32.u32 = va_arg(sXP.vaList, unsigned int);
					IF_myASSERT(debugTRACK, X32.u32 <= xpfPRECIS_MAXVAL);
					sXP.flg.Precis = X32.u32;
					sXP.flg.bPrecis = 1;
					X32.u32 = 0;
				} else {
					break;
				}
				++pcFmt;
			}
			// Save possible parsed value
			if (X32.u32 > 0) {							// if a number was parsed...
				if (sXP.flg.bRadix == 0 && sXP.flg.bMinWid == 0) {			// no '.' nor width
					IF_myASSERT(debugTRACK, X32.u32 <= xpfMINWID_MAXVAL);
					sXP.flg.MinWid = X32.u32;			// save value as MinWid
					sXP.flg.bMinWid = 1;				// set MinWid flag
				} else if (sXP.flg.bRadix == 1 && sXP.flg.bPrecis == 0) {	// '.' but no Precis
					IF_myASSERT(debugTRACK, X32.u32 <= xpfPRECIS_MAXVAL);
					sXP.flg.Precis = X32.u32;			// save value as precision
					sXP.flg.bPrecis = 1;				// set precision flag
				} else {
					assert(0);
				}
			}
		}

		// Optional SIZE indicators
		cFmt = strchr_i("hljztL", *pcFmt);
		if (cFmt != erFAILURE) {
			++pcFmt;
			switch(cFmt) {
				case 0: {
					if (*pcFmt == CHR_h) {				// "hh"
						sXP.flg.uSize = S_hh;
						++pcFmt;
					} else {							// 'h'
						sXP.flg.uSize = S_h;
					}
					break;
				}
				case 1: {
					if (*pcFmt != CHR_l) {
						sXP.flg.uSize = S_l;			// 'l'
					} else {
						sXP.flg.uSize = S_ll;			// "ll"
						++pcFmt;
					}
					break;
				}
				case 2: sXP.flg.uSize = S_j; break;		// [u]intmax_t[*]
				case 3: sXP.flg.uSize = S_z; break;		// [s]size_t[*]
				case 4: sXP.flg.uSize = S_t; break;		// ptrdiff[*]
				case 5: sXP.flg.uSize = S_L; break;		// long double float (F128)
				default: assert(0);
			}
		}
		IF_myASSERT(debugTRACK, sXP.flg.uSize < S_XXX);	// rest not yet supported
			
		// Check if format character where UC/lc same character control the case of the output
		cFmt = *pcFmt;
		if (strchr_i(vPrintStr1, cFmt) != erFAILURE) {
			cFmt |= 0x20;								// convert to lower case, but ...
			sXP.flg.bCase = 1;							// indicate as UPPER case requested
		}

		switch (cFmt) {
			#if	(xpfSUPPORT_SGR == 1)
			case CHR_C: {
				sSGR.u32 = va_arg(sXP.vaList, u32_t);	// ensure removed from stack
				if (sXP.flg.uSGR == sgrANSI) {
					if (sSGR.rowcol && pcTermLocate(Buffer, sSGR.r, sSGR.c) != Buffer)
						xPrintChars(&sXP, Buffer, strlen(Buffer));		// cursor location, 1 relative row & column
					if (pcTermAttrib(Buffer, sSGR.a1, sSGR.a2) != Buffer)
						xPrintChars(&sXP, Buffer, strlen(Buffer));		// attribute[s]
				}
				break;
			}
			#endif

			#if	(xpfSUPPORT_IP_ADDR == 1)				// IP address
			case CHR_I: {
				IF_myASSERT(debugTRACK, !sXP.flg.bPrecis && !sXP.flg.Precis && !sXP.flg.bPlus);
				sXP.flg.MinWid = sXP.flg.bLeft ? 0 : 3;
				X32.u32 = va_arg(sXP.vaList, u32_t);
				u8_t temp;
				u8_t * pChr = (u8_t *) &X32;
				if (sXP.flg.bAltF) {					// '#' specified to invert order ?
					temp = *pChr;		*pChr = *(pChr+3);		*(pChr+3) = temp;
					temp = *(pChr+1);	*(pChr+1) = *(pChr+2);	*(pChr+2) = temp;
					sXP.flg.bAltF = 0;					// clear so not to confuse xPrintValueJustified()
				}
				char Buffer[xpfMAX_LEN_IP];
				Buffer[xpfMAX_LEN_IP - 1] = 0;			// building R to L, ensure buffer NULL-term
				int	Idx, Len;
				for (Idx = 0, Len = 0; Idx < sizeof(u32_t); ++Idx) {	// then convert the IP address, LSB first
					X64.u64 = (u8_t) pChr[Idx];
					Len += xPrintValueJustified(&sXP, X64.u64, Buffer + xpfMAX_LEN_IP - 5 - Len, 4);
					if (Idx < 3)
						Buffer[xpfMAX_LEN_IP - 1 - ++Len] = CHR_FULLSTOP;
				}
				// then send the formatted output to the correct stream
				vPrintStringJustified(&sXP, Buffer + (xpfMAX_LEN_IP - 1 - Len));
				break;
			}
			#endif

			#if	(xpfSUPPORT_DATETIME == 1)
			/* Prints date and/or time in POSIX format
			 * Use the following modifiers
			 *	'	select between 2 different separator sets being
			 *		'/' or '-' (years)
			 *		'/' or '-' (months)
			 *		'T' or ' ' (days)
			 *		':' or 'h' (hours)
			 *		':' or 'm' (minutes)
			 *		'.' or 's' (seconds)
			 * 	!	Treat value as relative/elapsed and not epoch
			 * 	.	Append 1 -> 6 digit(s) fractional seconds
			 *	+	Convert to local time, add TZ offset and DSt if available
			 *	#	Select the alternative format
			 * Norm 1	1970/01/01T00:00:00Z
			 * Norm 2	1970-01-01 00h00m00s
			 * Altform	Mon, 01 Jan 1970 00:00:00 GMT
			 */
			case CHR_D:				// Local TZ date
			case CHR_T:				// Local TZ time
			case CHR_Z: {			// Local TZ DATE+TIME+ZONE
				IF_myASSERT(debugTRACK, sXP.flg.bRelVal == 0);
				psTSZ = va_arg(sXP.vaList, tsz_t *);	// retrieve TSX pointer parameter
				IF_myASSERT(debugTRACK, halMemoryRAM(psTSZ));
				X32.u32 = xTimeStampSeconds(psTSZ->usecs);				// convert to u32_t epoch value
				if (sXP.flg.bPlus && psTSZ->pTZ)		// If full local time required, add TZ and DST offsets
					X32.u32 += psTSZ->pTZ->timezone + (int) psTSZ->pTZ->daylight;
				xTimeGMTime(X32.u32, &sTM, sXP.flg.bRelVal);				// Convert to component values
				sXP.flg.bPad0 = 1;						// Need 0 on date & time, want to restore same
				XPC_SAVE(&sXP);
				sXP.flg.bPlus = 0;						// only for DTZone, need to restore status later..
				if (cFmt == CHR_D || cFmt == CHR_Z) {
					vPrintDate(&sXP, &sTM);				// bPad0=1, bPlus=0
					XPC_REST(&sXP);						// bAltF changed
				}
				if (cFmt == CHR_T || cFmt == CHR_Z) {
					vPrintTime(&sXP, &sTM, (u32_t)(psTSZ->usecs % MICROS_IN_SECOND));
				}
				if (cFmt == CHR_Z) {
					XPC_REST(&sXP);
					vPrintZone(&sXP, psTSZ);
				}
				break;
			}

			case CHR_r: {			// U32 epoch (yr+mth+day) OR relative (days) + TIME
				IF_myASSERT(debugTRACK, sXP.flg.bPlus == 0);
				sXP.flg.bSigned = sXP.flg.bRelVal ? 1 : 0;		// Relative values signed
				sXP.flg.uSize = sXP.flg.bCase ? S_ll : S_l;		// 'R' = 64bit, 'r' = 32bit
				X64 = x64PrintGetValue(&sXP);
				X32.u32 = (u32_t) sXP.flg.bCase ? (X64.u64 / MICROS_IN_SECOND) : X64.u64;
				xTimeGMTime(X32.u32, &sTM, sXP.flg.bRelVal);
				if (sXP.flg.bRelVal == 0)				// absolute (not relative) value
					sXP.flg.bPad0 = 1;					// must pad
				if (sXP.flg.bRelVal == 0 || sTM.tm_mday) {
					XPC_SAVE(&sXP);
					vPrintDate(&sXP, &sTM);
                    XPC_FLAG(&sXP,val.flg1);
                    XPC_FLAG(&sXP,flg.Precis);
					sXP.flg.bNegVal = 0;				// disable possible second '-'
				}
				X32.u32 = sXP.flg.bCase ? X64.u64 % MICROS_IN_SECOND : 0;
				vPrintTime(&sXP, &sTM, X32.u32);
				break;
			}
			#endif

			#if	(xpfSUPPORT_URL == 1)					// para = pointer to string to be encoded
			case CHR_U: {
				pX.pc8 = va_arg(sXP.vaList, char *);
				IF_myASSERT(debugTRACK, halMemoryANY(pX.pc8));
				vPrintURL(&sXP, pX.pc8);
				break;
			}
			#endif

			#if	(xpfSUPPORT_MAC_ADDR == 1)
			/* Formats 6 byte string (0x00 is valid) as a series of hex characters.
			 * default format uses no separators eg. '0123456789AB'
			 * Support the following modifiers:
			 *  '#' select upper case alternative form
			 *  '!'	select ':' separator between digits
			 */
			case CHR_M:	{								// MAC address ??:??:??:??:??:??
				IF_myASSERT(debugTRACK, sXP.flg.bMinWid == 0 && sXP.flg.bPrecis == 0);
				sXP.flg.uSize = S_hh;					// force interpretation as sequence of U8 values
				pX.pc8 = va_arg(sXP.vaList, char *);
				IF_myASSERT(debugTRACK, halMemoryANY(pX.pc8));
				vPrintHexValues(&sXP, lenMAC_ADDRESS, pX.pc8);
				break;
			}
			#endif

			#if	(xpfSUPPORT_HEXDUMP == 1)
			case CHR_Y: {									// HEXDUMP
				// retrieve implied/hidden size parameter if not specified...
				X32.iX = sXP.flg.bPrecis ? sXP.flg.Precis : va_arg(sXP.vaList, int);
				pX.pc8 = va_arg(sXP.vaList, char *);	// retrieve the pointer to data
				IF_myASSERT(debugTRACK, halMemoryANY(pX.pc8));
				sXP.flg.uForm = form3X;
				vPrintHexDump(&sXP, X32.iX, pX.pc8);
				break;
			}
			#endif

			/* convert unsigned 32/64 bit value to 1/0 ASCII string
			 * field width specifier is applied as mask starting from LSB to MSB
			 * '#'	change prepended "0b" to "0B"
			 * '''	Group digits using '|' (bytes) and '-' (nibbles)
			 */
			case CHR_b: {
				//IF_myASSERT(debugTRACK, sXP.flg.bSigned == 0 && sXP.flg.bPad0 == 0);
				X64 = x64PrintGetValue(&sXP);
				X32.iX = S_bytes[sXP.flg.uSize] * BITS_IN_BYTE;
				if (sXP.flg.MinWid)
					X32.iX = (sXP.flg.MinWid > X32.iX) ? X32.iX : sXP.flg.MinWid;
				u64_t mask = 1ULL << (X32.iX - 1);
				xPrintChars(&sXP, sXP.flg.bCase ? "0B" : "0b", 2);
				
				while (mask) {
					xPrintChar(&sXP, (X64.u64 & mask) ? CHR_1 : CHR_0);
					mask >>= 1;
					// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
					if (--X32.iX && sXP.flg.bGroup && (X32.iX % 4) == 0) {
						xPrintChar(&sXP, X32.iX % 32 == 0 ? CHR_VERT_BAR :			// word
										  X32.iX % 16 == 0 ? CHR_COLON :			// short
									  	  X32.iX % 8 == 0 ? CHR_SPACE : CHR_MINUS);	// byte or nibble
					}
				}
				break;
			}

			case CHR_c: {
				xPrintChar(&sXP, va_arg(sXP.vaList, int));
				break;
			}

			case CHR_d:									// signed decimal "[-]ddddd"
			case CHR_i: {								// signed integer (same as decimal ?)
				sXP.flg.bSigned = 1;
				#if (xpfSUPPORT_ARRAYS > 0)
				if (sXP.flg.bArray) {
					IF_EXEC_1(xpfSUPPORT_ARRAYS, vPrintX64array, &sXP);
				} else
				#endif
				{
					X64 = x64PrintGetValue(&sXP);
					if (X64.i64 < 0LL) {
						sXP.flg.bNegVal = 1;
						X64.i64 *= -1; 					// convert the value to unsigned
					}
					vPrintX64(&sXP, X64.u64);
				}
				break;
			}
		
			#if	(xpfSUPPORT_IEEE754 == 1)
//			case CHR_a:									// HEX format not yet supported
			case CHR_e: ++sXP.flg.uForm;				// form = 2
				/* FALLTHRU */ /* no break */
			case CHR_f: ++sXP.flg.uForm;				// form = 1
				/* FALLTHRU */ /* no break */
			case CHR_g: {								// form = 0 (or 1 or 2)
				sXP.flg.bSigned = 1;					// float always signed value.
				if (sXP.flg.bPrecis) {					// explicit precision specified ?
					sXP.flg.Precis = sXP.flg.Precis > xpfMAXIMUM_DECIMALS ? xpfMAXIMUM_DECIMALS : sXP.flg.Precis;
				} else {
					sXP.flg.Precis = xpfDEFAULT_DECIMALS;
				}
				#if (xpfSUPPORT_ARRAYS > 0)
				if (sXP.flg.bArray) {
					sXP.flg.bFloat = 1;
					vPrintX64array(&sXP);
				} else
				#endif
				{
					vPrintF64(&sXP, va_arg(sXP.vaList, f64_t));
				}
				break;
			}
			#endif

			case CHR_m: {
				pX.pc8 = strerror(errno);
				goto commonM_S;
			}

			case CHR_n: {								// store chars to date at location.
				pX.piX = va_arg(sXP.vaList, int *);
				IF_myASSERT(debugTRACK, halMemorySRAM(pX.pc8));
				*pX.piX = sXP.CurLen;
				break;
			}

			case CHR_o:									// unsigned octal "ddddd"
			case CHR_x: sXP.flg.bGroup = 0;				// hex as in "789abcd" UC/LC, disable grouping
				/* FALLTHRU */ /* no break */
			case CHR_u: {								// unsigned decimal "ddddd"
				IF_myASSERT(debugTRACK, sXP.flg.bSigned == 0);
				sXP.flg.uBase = (cFmt == CHR_x) ? BASE16 : (cFmt == CHR_u) ? BASE10 : BASE08;
				#if (xpfSUPPORT_ARRAYS > 0)
 					if (sXP.flg.bArray) {
						vPrintX64array(&sXP);
					} else
				#endif
				{
					X64 = x64PrintGetValue(&sXP);
					vPrintX64(&sXP, X64.u64);
				}
				break;
			}

			case CHR_p: {
				pX.pv = va_arg(sXP.vaList, void *);
				vPrintPointer(&sXP, pX);
				break;
			}

			case CHR_s: {
				pX.pc8 = va_arg(sXP.vaList, char *);
			commonM_S:
				// Required to avoid crash when wifi message is intercepted and a string pointer parameter
				// is evaluated as out of valid memory address (0xFFFFFFE6). Replace string with "pOOR"
				pX.pc8 = halMemoryANY(pX.pc8) ? pX.pc8 : pX.pc8 ? strOOR : strNULL;
				vPrintStringJustified(&sXP, pX.pc8);
				break;
			}

			default: {
				/* At this stage we have handled the '%' as assumed, but the next character found is invalid.
				 * Show the '%' we swallowed and then the extra, invalid, character as well */
				xPrintChar(&sXP, CHR_PERCENT);
				xPrintChar(&sXP, *pcFmt);
				break;
			}
		}
		sXP.flg.bPlus = 0;							// reset this form after printing one value
	}
	return sXP.CurLen;
}

// ##################################### Destination = STRING ######################################

int vsnprintfx(char * pBuf, size_t sBuf, const char * pcFmt, va_list vaList) {
	int iRV = 0;
	if (sBuf != 1) {
		iRV = xPrintFX(xPrintToString, pBuf, sBuf, pcFmt, vaList);
		if (iRV == sBuf)								// buffer full ...
			--iRV;										// make space for terminator
	}
	if (pBuf)											// only if actually specified
		pBuf[iRV] = 0;									// terminate
	return iRV;
}

int vsprintfx(char * pBuf, const char * pcFmt, va_list vaList) { return vsnprintfx(pBuf, 0, pcFmt, vaList); }

int snprintfx(char * pBuf, size_t sBuf, const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vsnprintfx(pBuf, sBuf, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

int sprintfx(char * pBuf, const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vsnprintfx(pBuf, 0, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = STDOUT ########################################

int vprintfx(const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToHandle, (void *) STDOUT_FILENO, 0, pcFmt, vaList); }

int printfx(const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vprintfx(pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = FILE PTR ######################################

int vfprintfx(FILE * stream, const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToHandle, (void *) fileno(stream), 0, pcFmt, vaList); }

int fprintfx(FILE * stream, const char * pcFmt, ...) {		
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vfprintfx(stream, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = CONSOLE #######################################

 int vcprintfx(const char * pcFmt, va_list vaList) {
	BaseType_t btRV = halUartLock(WPFX_TIMEOUT);
	int iRV = xPrintFX(xPrintToConsole, 0, 0, pcFmt, vaList);
	if (btRV == pdTRUE)
		halUartUnLock();
	return iRV;
}

int cprintfx(const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vcprintfx(pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = HANDLE ########################################

int	vdprintfx(int fd, const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToHandle, (void *) fd, 0, pcFmt, vaList); }

int	dprintfx(int fd, const char * pcFmt, ...) {
	va_list	vaList;
	va_start(vaList, pcFmt);
	int iRV = vdprintfx(fd, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = DEVICE ########################################

int vdevprintfx(int (* Hdlr)(int), const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToDevice, (void *) Hdlr, 0, pcFmt, vaList); }

int devprintfx(int (* Hdlr)(int), const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vdevprintfx(Hdlr, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

/* #################################### Destination : SOCKET #######################################
 * SOCKET directed formatted print support. Problem here is that MSG_MORE is primarily supported on
 * TCP sockets, UDP support officially in LwIP 2.6 but has not been included into ESP-IDF yet. */

int vsocprintfx(netx_t * psSock, const char * pcFmt, va_list vaList) {
	int	Fsav = psSock->flags;							// save the current socket flags
	psSock->flags |= MSG_MORE;
	int iRV = xPrintFX(xPrintToSocket, (void *) psSock, 0, pcFmt, vaList);
	psSock->flags = Fsav;								// restore socket flags
	return (psSock->error == 0) ? iRV : erFAILURE;
}

int socprintfx(netx_t * psSock, const char * pcFmt, ...) {
	va_list vaList;
	va_start(vaList, pcFmt);
	int iRV = vsocprintfx(psSock, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// #################################### Destination : UBUF #########################################

int	vuprintfx(ubuf_t * psUBuf, const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToUBuf, (void *) psUBuf, xUBufGetSpace(psUBuf), pcFmt, vaList); }

int	uprintfx(ubuf_t * psUBuf, const char * pcFmt, ...) {
	va_list	vaList;
	va_start(vaList, pcFmt);
	int iRV = vuprintfx(psUBuf, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// #################################### Destination : CRC32 ########################################

int	vcrcprintfx(u32_t * pU32, const char * pcFmt, va_list vaList) { return xPrintFX(xPrintToCRC32, (void *) pU32, 0, pcFmt, vaList); }

int	crcprintfx(u32_t * pU32, const char * pcFmt, ...) {
	va_list	vaList;
	va_start(vaList, pcFmt);
	int iRV = vcrcprintfx(pU32, pcFmt, vaList);
	va_end(vaList);
	return iRV;
}

// ############################# Aliases for NEW/STDLIB supplied functions #########################

/* To make this work for esp-idf and newlib, the following modules must be removed:
 *		lib_a-[s[sn[fi]]]printf.o
 *		lib_a-v[f[i]]printf.o
 *		lib_a-??tchar.o (put & get)
 * using command line
 * 		ar -d /c/Dropbox/devs/ws/z-sdk\libc.a {name}
 *
 * 	Alternative is to ensure that the printfx.obj is specified at the start to be linked with !!
 */

#if (xpfSUPPORT_ALIASES == 1)
	int	printf(const char * pcFmt, ...) __attribute__((alias("printfx"), unused));
	int	printf_r(const char * pcFmt, ...) __attribute__((alias("printfx"), unused));

	int	vprintf(const char * pcFmt, va_list vaList) __attribute__((alias("vprintfx"), unused));
	int	vprintf_r(const char * pcFmt, va_list vaList) __attribute__((alias("vprintfx"), unused));

	int	sprintf(char * pBuf, const char * pcFmt, ...) __attribute__((alias("sprintfx"), unused));
	int	sprintf_r(char * pBuf, const char * pcFmt, ...) __attribute__((alias("sprintfx"), unused));

	int	vsprintf(char * pBuf, const char * pcFmt, va_list vaList) __attribute__((alias("vsprintfx"), unused));
	int	vsprintf_r(char * pBuf, const char * pcFmt, va_list vaList) __attribute__((alias("vsprintfx"), unused));

	int snprintf(char * pBuf, size_t BufSize, const char * pcFmt, ...) __attribute__((alias("snprintfx"), unused));
	int snprintf_r(char * pBuf, size_t BufSize, const char * pcFmt, ...) __attribute__((alias("snprintfx"), unused));

	int vsnprintf(char * pBuf, size_t BufSize, const char * pcFmt, va_list vaList) __attribute__((alias("vsnprintfx"), unused));
	int vsnprintf_r(char * pBuf, size_t BufSize, const char * pcFmt, va_list vaList) __attribute__((alias("vsnprintfx"), unused));

	int fprintf(FILE * stream, const char * pcFmt, ...) __attribute__((alias("fprintfx"), unused));
	int fprintf_r(FILE * stream, const char * pcFmt, ...) __attribute__((alias("fprintfx"), unused));

	int vfprintf(FILE * stream, const char * pcFmt, va_list vaList) __attribute__((alias("vfprintfx"), unused));
	int vfprintf_r(FILE * stream, const char * pcFmt, va_list vaList) __attribute__((alias("vfprintfx"), unused));

	int dprintf(int fd, const char * pcFmt, ...) __attribute__((alias("dprintfx"), unused));
	int dprintf_r(int fd, const char * pcFmt, ...) __attribute__((alias("dprintfx"), unused));

	int vdprintf(int fd, const char * pcFmt, va_list vaList) __attribute__((alias("vdprintfx"), unused));
	int vdprintf_r(int fd, const char * pcFmt, va_list vaList) __attribute__((alias("vdprintfx"), unused));

	int fiprintf(FILE * stream, const char * pcFmt, ...) __attribute__((alias("fprintfx"), unused));
	int fiprintf_r(FILE * stream, const char * pcFmt, ...) __attribute__((alias("fprintfx"), unused));

	int vfiprintf(FILE * stream, const char * pcFmt, va_list vaList) __attribute__((alias("vfprintfx"), unused));
	int vfiprintf_r(FILE * stream, const char * pcFmt, va_list vaList) __attribute__((alias("vfprintfx"), unused));
#endif
