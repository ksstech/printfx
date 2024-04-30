/*
 * <snf>printfx -  set of routines to replace equivalent printf functionality
 * Copyright (c) 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * https://codereview.stackexchange.com/questions/219994/register-b-conversion-specifier
 *
 */

#include "hal_platform.h"
#include "hal_options.h"
#include "hal_stdio.h"
#include "printfx.h"
#include "socketsX.h"
#include "struct_union.h"
#include "x_string_general.h"		// xinstring function
#include "x_errors_events.h"
#include "x_terminal.h"
#include "x_utilities.h"

#include <math.h>					// isnan()
#include <float.h>					// DBL_MIN/MAX

#ifdef ESP_PLATFORM
	#include "esp_log.h"
	#include "esp32/rom/crc.h"		// ESP32 ROM routine
#else
	#include "crc-barr.h"						// Barr group CRC
#endif

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ###################### control functionality included in xprintf.c ##############################

#define	xpfSUPPORT_MAC_ADDR			1
#define	xpfSUPPORT_IP_ADDR			1
#define	xpfSUPPORT_HEXDUMP			1
#define	xpfSUPPORT_DATETIME			1
#define	xpfSUPPORT_IEEE754			1					// float point support in printfx.c functions
#define	xpfSUPPORT_SCALING			1					// scale number down by 10^[3/6/9/12/15/18]
#define	xpfSUPPORT_SGR				1					// Set Graphics Rendition FG & BG colors only
#define	xpfSUPPORT_URL				1					// URL encoding
#define	xpfSUPPORT_ALIASES			1
#define	xpfSUPPORT_FILTER_NUL		1

// ###################################### Scaling factors ##########################################

#define	K1		1000ULL
#define	M1		1000000ULL
#define	B1		1000000000ULL
#define	T1		1000000000000ULL
#define	q1		1000000000000000ULL
#define	Q1		1000000000000000000ULL

// ####################################### Enumerations ############################################
// ######################## Character and value translation & rounding tables ######################

const u8_t S_bytes[S_XXX] = { sizeof(int), sizeof(char), sizeof(short), sizeof(long), sizeof(long long),
							sizeof(intmax_t), sizeof(size_t), sizeof(ptrdiff_t), sizeof(long double) };

const char vPrintStr1[] = {			// table of characters where lc/UC is applicable
	'B',							// Binary formatted, prepend "0b" or "0B"
	'P',							// pointer formatted, 0x00abcdef or 0X00ABCDEF
	'R',							// Time, absolute/relative, no ZONE info, 64bit/uSec or 32bit/Sec
	'X',							// hex formatted 'x' or 'X' values, always there
	#if	(xpfSUPPORT_IEEE754 == 1)
	'A', 'E', 'F', 'G',				// float hex/exponential/general
	#endif
	'\0'
};

const char hexchars[] = "0123456789ABCDEF";

// ###################################### Public variables #########################################
// ##################################### Private functions #########################################

x64_t x64PrintGetValue(xp_t * psXP) {
	x64_t X64;
	switch(psXP->ctl.uSize) {
	case S_none:
	case S_hh:
	case S_h:
		if (psXP->ctl.bSigned == 1) {
			X64.i64 = va_arg(psXP->vaList, int);
		} else {
			X64.u64 = va_arg(psXP->vaList, unsigned int);
		}
		break;
	case S_l:
		if (psXP->ctl.bSigned == 1) {
			X64.i64 = va_arg(psXP->vaList, i32_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, u32_t);
		}
		break;
	case S_ll:
		if (psXP->ctl.bSigned) {
			X64.i64 = va_arg(psXP->vaList, i64_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, u64_t);
		}
		break;
	case S_z:
		if (psXP->ctl.bSigned) {
			X64.i64 = va_arg(psXP->vaList, ssize_t);
		} else {
			X64.u64 = va_arg(psXP->vaList, size_t);
		}
		break;
	default:
		myASSERT(0);
		X64.u64 = 0ULL;
	}
	return X64;
}

/**
 * @brief		returns a pointer to the hexadecimal character representing the value of the low order nibble
 * 				Based on the Flag, the pointer will be adjusted for UpperCase (if required)
 * @param[in]	Val = value in low order nibble, to be converted
 * @return		pointer to the correct character
 */
char cPrintNibbleToChar(xp_t * psXP, u8_t Value) {
	char cChr = hexchars[Value];
	if (psXP->ctl.bCase == 0 && Value > 9)
		cChr += 0x20;									// convert to lower case
	return cChr;
}

// ############################# Foundation character and string output ############################

/**
 * @brief	output a single character using/via the preselected function
 * 			before output verify against specified width
 * 			adjust character count (if required) & remaining width
 * @param	psXP - pointer to control structure to be referenced/updated
 * 			c - char to be output
 * @return	1 or 0 based on whether char was '\000'
 */
static int xPrintChar(xp_t * psXP, char cChr) {
	int iRV = cChr;
	#if (xpfSUPPORT_FILTER_NUL == 1)
	if (cChr == 0)
		return iRV;
	#endif
	if ((psXP->ctl.maxlen == 0) || (psXP->ctl.curlen < psXP->ctl.maxlen)) {
		iRV = psXP->handler(psXP, cChr);
		if (iRV == cChr) {
			psXP->ctl.curlen++;							// adjust count
		}
	}
	return iRV;
}

/**
 * @brief	perform a RAW string output to the selected "stream"
 * @brief	Does not perform ANY padding, justification or length checking
 * @param	psXP - pointer to structure controlling the operation
 * 			string - pointer to the string to be output
 * @return	number of ACTUAL characters output.
 */
static int xPrintChars(xp_t * psXP, char * pStr) {
	int iRV = 0, cChr;
	while (*pStr) {
		cChr = xPrintChar(psXP, *pStr);
		if (cChr != *pStr++)
			return cChr;
		++iRV;
	}
	return iRV;
}

/**
 * @brief	perform formatted output of a string to the preselected device/string/buffer/file
 * @param	psXP - pointer to structure controlling the operation
 * 			string - pointer to the string to be output
 * @return	number of ACTUAL characters output.
 * @note	
 */
void vPrintString(xp_t * psXP, char * pStr) {
	// determine natural or limited length of string
	size_t uLen;
	if (psXP->ctl.bPrecis && psXP->ctl.bMinWid && (psXP->ctl.Precis < psXP->ctl.MinWid)) {
		uLen = xstrnlen(pStr, psXP->ctl.Precis);
	} else {
		uLen = psXP->ctl.Precis ? psXP->ctl.Precis : strlen(pStr);
	}
	size_t Lpad = 0, Rpad = 0, Tpad = (psXP->ctl.MinWid > uLen) ? (psXP->ctl.MinWid - uLen) : 0;
	u8_t Cpad = psXP->ctl.bPad0 ? CHR_0 : CHR_SPACE;
	if (Tpad) {
		if (psXP->ctl.bAltF) {
			Rpad = Tpad >> 1; Lpad = Tpad - Rpad;
		} else if (psXP->ctl.bLeft) {
			Rpad = Tpad;
		} else {
			Lpad = Tpad;
		}
	}
	for (;Lpad--; xPrintChar(psXP, Cpad));
	for (;uLen-- && *pStr; xPrintChar(psXP, *pStr++));
	for (;Rpad--; xPrintChar(psXP, Cpad));
}

/**
 * xPrintXxx() convert u64_t value to a formatted string (build right to left)
 * @param	psXP - pointer to control structure
 * 			ullVal - u64_t value to convert & output
 * 			pBuffer - pointer to buffer for converted string storage
 * 			BufSize - available (remaining) space in buffer
 * @return	number of actual characters output (incl leading '-' and/or ' ' and/or '0' as possibly added)
 * @note	Honour & interpret the following modifier flags
 * 			'	Group digits in 3 digits groups to left of decimal '.'
 * 			-	Left align the individual numbers between the '.'
 * 			+	Force a '+' or '-' sign on the left
 * 			0	Zero pad to the left of the value to fill the field
 * Protection against buffer overflow based on correct sized buffer being allocated in calling function
 *
 *	(1)		(2)		(0)
 *	12.34Q	12Q34	12,345,678,900,000,000,000
 *	1.234Q	1Q234	1,234,567,890,000,000,000
 *	123.4q	123q4	123,456,789,000,000,000
 *	12.34q	12q34	12,345,678,900,000,000
 *	1.234q	1q234	1,234,567,890,000,000
 *	123.4T	123T4	123,456,789,000,000
 *	12.34T	12T34	12,345,678,900,000
 *	1.234T	1T234	1,234,567,890,000
 *	123.4B	123B4	123,456,789,000
 *	12.34B	12B34	12,345,678,900
 *	1.234B	1B234	1,234,567,890
 *	123.4M	123M4	123,456,789
 *	12.34M	12M34	12,345,678
 *	1.234M	1M234	1,234,567
 *	123,456	123K4	123,456
 *	12,345	12K34	12,345
 *	1,234	1K234	1,234
 *	123		123		123
 */
int	xPrintXxx(xp_t * psXP, u64_t u64Val, char * pBuffer, int BufSize) {
	int	Len = 0;
	int Count, iTemp;
	char * pTemp = pBuffer + BufSize - 1;				// Point to last space in buffer
	if (u64Val) {
		#if	(xpfSUPPORT_SCALING > 0)
		if (psXP->ctl.bAltF) {
			u32_t I, F;
			u8_t ScaleChr;
			if (u64Val >= Q1)		{ F = (u64Val % Q1) / q1; I = u64Val / Q1; ScaleChr = CHR_Q; }
			else if (u64Val >= q1)	{ F = (u64Val % q1) / T1; I = u64Val / q1; ScaleChr = CHR_q; }
			else if (u64Val >= T1)	{ F = (u64Val % T1) / B1; I = u64Val / T1; ScaleChr = CHR_T; }
			else if (u64Val >= B1)	{ F = (u64Val % B1) / M1; I = u64Val / B1; ScaleChr = CHR_B; }
			else if (u64Val >= M1)	{ F = (u64Val % M1) / K1; I = u64Val / M1; ScaleChr = CHR_M; }
			else if (u64Val >= K1)	{ F = u64Val % K1;		  I = u64Val / K1; ScaleChr = CHR_K; }
			else { ScaleChr = 0; }
			if (ScaleChr) {
				if (psXP->ctl.bGroup == 0) {
					*pTemp-- = ScaleChr;
					++Len;
				}
				// calculate & convert the required # of fractional digits
				int Ilen = xDigitsInU32(I, 0);			// 1 (0) -> 3 (999)
				int Flen = xDigitsInU32(F, 0);
				if (Ilen != 2 || Flen != 2) {
					Flen = 4 - Ilen;
					F /= u32pow(psXP->ctl.uBase, 3 - Flen);
				}
				while (Flen--) {
					*pTemp-- = cPrintNibbleToChar(psXP, F % psXP->ctl.uBase);	// digit
					++Len;
					F /= psXP->ctl.uBase;
				}
				*pTemp-- = psXP->ctl.bGroup ? ScaleChr : CHR_FULLSTOP;
				++Len;
				u64Val = (u32_t) I;
			}
		}
		#endif
		// convert to string starting at end of buffer from Least (R) to Most (L) significant digits
		Count = 0;
		while (u64Val) {
			iTemp = u64Val % psXP->ctl.uBase;			// calculate the next remainder ie digit
			*pTemp-- = cPrintNibbleToChar(psXP, iTemp);
			++Len;
			u64Val /= psXP->ctl.uBase;
			if (u64Val && psXP->ctl.bGroup) {				// handle digit grouping, if required
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
	if (psXP->ctl.bLeft == 0) {							// right justified (ie pad left) ???
		/* this section ONLY when value is RIGHT justified.
		 * For ' ' padding format is [       -xxxxx]
		 * whilst '0' padding it is  [-0000000xxxxx]
		 */
		Count = (psXP->ctl.MinWid > Len) ? psXP->ctl.MinWid - Len : 0;
		// If we are padding with ' ' and leading '+' or '-' is required, do that first
		if (psXP->ctl.bPad0 == 0 && (psXP->ctl.bNegVal || psXP->ctl.bPlus)) {	// If a sign is required
			*pTemp-- = psXP->ctl.bNegVal ? CHR_MINUS : CHR_PLUS;	// prepend '+' or '-'
			--Count;
			++Len;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXP->ctl.bPad0 ? CHR_0 : CHR_SPACE;	// define applicable padding char
			Len += Count;								// Now do the actual padding
			while (Count--)
				*pTemp-- = iTemp;
		}
		// If we are padding with '0' AND a sign is required (-value or +requested), do that first
		if (psXP->ctl.bPad0 && (psXP->ctl.bNegVal || psXP->ctl.bPlus)) {	// If +/- sign is required
			if (pTemp[1] == CHR_SPACE || pTemp[1] == CHR_0) {
				++pTemp;								// set overwrite last with '+' or '-'
			} else {
				++Len;									// set to add extra '+' or '-'
			}
			*pTemp = psXP->ctl.bNegVal ? CHR_MINUS : CHR_PLUS;
		}
	} else if (psXP->ctl.bNegVal || psXP->ctl.bPlus) {	// If a sign is required
		*pTemp = psXP->ctl.bNegVal ? CHR_MINUS : CHR_PLUS;
		++Len;
	}
	return Len;
}

/**
 * @brief	convert double value based on flags supplied and output via control structure
 * @param	psXP - pointer to control structure
 * @param	dbl - double value to be converted
 * @return	none
 *
 * References:
 * 	http://git.musl-libc.org/cgit/musl/blob/src/stdio/vfprintf.c?h=v1.1.6
 */
void vPrintF64(xp_t * psXP, double F64) {
	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005, 0.000000005,
		0.0000000005, 0.00000000005, 0.000000000005, 0.0000000000005, 0.00000000000005,
		0.000000000000005, 0.0000000000000005 };
	if (isnan(F64)) {
		vPrintString(psXP, psXP->ctl.bCase ? "NAN" : "nan");
		return;
	} else if (isinf(F64)) {
		vPrintString(psXP, psXP->ctl.bCase ? "INF" : "inf");
		return;
	}
	psXP->ctl.bNegVal = F64 < 0.0 ? 1 : 0;				// set bNegVal if < 0.0
	F64 *= psXP->ctl.bNegVal ? -1.0 : 1.0;				// convert to positive number
	xpc_t sXPC;
	sXPC.limits = psXP->ctl.limits;						// save original flags
	sXPC.flags = psXP->ctl.flags;
	x64_t X64  = { 0 };

	int	Exp = 0;										// if exponential format requested, calculate the exponent
	if (F64 != 0.0) {									// if not 0 and...
		if (psXP->ctl.uForm != form1F) {					// any of "eEgG" specified ?
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
	u8_t AdjForm = psXP->ctl.uForm != form0G ? psXP->ctl.uForm :
		(Exp <- 4 || Exp >= psXP->ctl.Precis) ? form2E : form1F;
	if (AdjForm == form2E)
		F64 = X64.f64;									// change to exponent adjusted value
	if (F64 < (DBL_MAX - round_nums[psXP->ctl.Precis]))	// if addition of rounding value will NOT cause overflow.
		F64 += round_nums[psXP->ctl.Precis];				// round by adding .5LSB to the value
	char Buffer[xpfMAX_LEN_F64];
	Buffer[xpfMAX_LEN_F64 - 1] = 0;						// building R to L, ensure buffer NULL-term

	int Len = 0;
	if (AdjForm == form2E) {							// If required, handle the exponent
		psXP->ctl.MinWid = 2;
		psXP->ctl.bPad0 = 1;								// MUST left pad with '0'
		psXP->ctl.bSigned = 1;
		psXP->ctl.bLeft = 0;
		psXP->ctl.bNegVal = (Exp < 0) ? 1 : 0;
		Exp *= psXP->ctl.bNegVal ? -1LL : 1LL;
		Len += xPrintXxx(psXP, Exp, Buffer, xpfMAX_LEN_F64 - 1);
		Buffer[xpfMAX_LEN_F64-2-Len] = psXP->ctl.bCase ? CHR_E : CHR_e;
		++Len;
	}

	X64.f64	= F64 - (u64_t) F64;						// isolate fraction as double
	X64.f64	= X64.f64 * (double) u64pow(10, psXP->ctl.Precis);	// fraction to integer
	X64.u64	= (u64_t) X64.f64;							// extract integer portion

	if (psXP->ctl.bPrecis) {								// explicit MinWid specified ?
		psXP->ctl.MinWid = psXP->ctl.Precis; 				// yes, stick to it.
	} else if (X64.u64 == 0) {							// process 0 value
		psXP->ctl.MinWid = psXP->ctl.bRadix ? 1 : 0;
	} else {											// process non 0 value
		if (psXP->ctl.bAltF && psXP->ctl.uForm == form0G) {
			psXP->ctl.MinWid = psXP->ctl.Precis;			// keep trailing 0's
		} else {
			u32_t u = u64Trailing0(X64.u64);
			X64.u64 /= u64pow(10, u);
			psXP->ctl.MinWid	= psXP->ctl.Precis - u;		// remove trailing 0's
		}
	}
	if (psXP->ctl.MinWid > 0) {
		psXP->ctl.bPad0 = 1;								// MUST left pad with '0'
		psXP->ctl.bGroup = 0;								// cannot group in fractional
		psXP->ctl.bSigned = 0;							// always unsigned value
		psXP->ctl.bNegVal = 0;							// and never negative
		psXP->ctl.bPlus = 0;								// no leading +/- before fractional part
		Len += xPrintXxx(psXP, X64.u64, Buffer, xpfMAX_LEN_F64-1-Len);
	}
	// process the bRadix = '.'
	if (psXP->ctl.MinWid || psXP->ctl.bRadix) {
		Buffer[xpfMAX_LEN_F64 - 2 - Len] = CHR_FULLSTOP;
		++Len;
	}

	X64.u64	= F64;										// extract and convert the whole number portions
	psXP->ctl.limits = sXPC.limits;						// restore original limits & flags
	psXP->ctl.flags = sXPC.flags;

	// adjust MinWid to do padding (if required) based on string length after adding whole number
	psXP->ctl.MinWid = psXP->ctl.MinWid > Len ? psXP->ctl.MinWid - Len : 0;
	Len += xPrintXxx(psXP, X64.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len);

	psXP->ctl.bPrecis = 1;
	psXP->ctl.Precis = Len;
	psXP->ctl.bMinWid = sXPC.bMinWid;
	psXP->ctl.MinWid = sXPC.MinWid;
	vPrintString(psXP, Buffer + (xpfMAX_LEN_F64 - 1 - Len));
}

void vPrintX64(xp_t * psXP, u64_t Value) {
	char Buffer[xpfMAX_LEN_X64];
	Buffer[xpfMAX_LEN_X64 - 1] = 0;				// terminate the buffer, single value built R to L
	int Len = xPrintXxx(psXP, Value, Buffer, xpfMAX_LEN_X64 - 1);
	vPrintString(psXP, Buffer + (xpfMAX_LEN_X64 - 1 - Len));
}

void vPrintX64array(xp_t * psXP) {
	vs_e eVS = (psXP->ctl.uSize == S_hh) ? vs08B :
				(psXP->ctl.uSize == S_h) ? vs16B :
				(psXP->ctl.uSize == S_l) ? vs32B :
				(psXP->ctl.uSize == S_ll) ? vs64B : vs32B;
	vf_e eVF = psXP->ctl.bFloat ? vfFXX : psXP->ctl.bSigned ? vfIXX : vfUXX;
	cvi_e cvI = xFormSize2Index(eVF, eVS);
	x32_t X32; X32.iX = va_arg(psXP->vaList, int);	// array size
	px_t pX; pX.pv = va_arg(psXP->vaList, void *);	// pointer to 1st element of array
	u32_t savLimits = psXP->ctl.limits;
	u32_t savFlags = psXP->ctl.flags;
	while (X32.iX) {
		x64_t X64 = x64ValueFetch(pX, cvI);
		if (eVF == vfIXX && X64.i64 < 0LL) {
			psXP->ctl.bNegVal = 1;
			X64.i64 *= -1; 	// convert the value to unsigned
		}
		if (psXP->ctl.bFloat) {
			vPrintF64(psXP, X64.f64);
		} else {
			vPrintX64(psXP, X64.u64);
		}
		psXP->ctl.flags = savFlags;
		psXP->ctl.limits = savLimits;
		if (--X32.iX)
			xPrintChar(psXP, CHR_COMMA);
		pX = pxAddrNextWithIndex(pX, cvI);
	}
}

void vPrintPointer(xp_t * psXP, px_t pX) {
	char caBuf[xpfMAX_LEN_PNTR];
	caBuf[xpfMAX_LEN_PNTR-1] = CHR_NUL;
	psXP->ctl.uBase = BASE16;
	psXP->ctl.bNegVal = 0;
	psXP->ctl.bPlus = 0;									// no leading '+'
	psXP->ctl.bAltF = 0;									// disable scaling
	psXP->ctl.bGroup = 0;								// disable 3-digit grouping
	psXP->ctl.bPad0 = 1;
	x64_t X64;
	#if (xpfSIZE_POINTER == 2)
	psXP->ctl.MinWid = 4;
	X64.u64 = (u16_t) pX.pv;
	#elif (xpfSIZE_POINTER == 4)
	psXP->ctl.MinWid = 8;
	X64.u64 = (u32_t) pX.pv;
	#elif (xpfSIZE_POINTER == 8)
	psXP->ctl.MinWid = 16;
	X64.u64 = pX.pv;
	#endif
	int Len = xPrintXxx(psXP, X64.u64, caBuf, xpfMAX_LEN_PNTR - 1);
	memcpy(&caBuf[xpfMAX_LEN_PNTR - 3 - Len], "0x", 2);
	vPrintString(psXP, &caBuf[xpfMAX_LEN_PNTR - 3 - Len]);
}

// ############################# Proprietary extension: hexdump ####################################

/**
 * @brief	write char value as 2 hex chars to the buffer, always NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU8(xp_t * psXP, u8_t Value) {
	xPrintChar(psXP, cPrintNibbleToChar(psXP, Value >> 4));
	xPrintChar(psXP, cPrintNibbleToChar(psXP, Value & 0x0F));
}

/**
 * @brief	write u16_t value as 4 hex chars to the buffer, always NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned short to convert
 * return	none
 */
void vPrintHexU16(xp_t * psXP, u16_t Value) {
	vPrintHexU8(psXP, (Value >> 8) & 0x000000FF);
	vPrintHexU8(psXP, Value & 0x000000FF);
}

/**
 * @brief	write u32_t value as 8 hex chars to the buffer, always NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned word to convert
 * return	none
 */
void vPrintHexU32(xp_t * psXP, u32_t Value) {
	vPrintHexU16(psXP, (Value >> 16) & 0x0000FFFF);
	vPrintHexU16(psXP, Value & 0x0000FFFF);
}

/**
 * @brief	write u64_t value as 16 hex chars to the buffer, always NULL terminated
 * @param	psXP - pointer to control structure
 * @param	Value - unsigned dword to convert
 * @return	none
 */
void vPrintHexU64(xp_t * psXP, u64_t Value) {
	vPrintHexU32(psXP, (Value >> 32) & 0xFFFFFFFFULL);
	vPrintHexU32(psXP, Value & 0xFFFFFFFFULL);
}

/**
 * @brief	write series of char values as hex chars to the buffer, always NULL terminated
 * @param 	psXP
 * @param 	Num - number of bytes to print
 * @param 	pStr - pointer to bytes to print
 * @note	Use the following modifier flags
 *			' select grouping separators ":- |" (byte/half/word/dword)
 *			# select reverse order (little/big endian)
 */
void vPrintHexValues(xp_t * psXP, int Num, char * pStr) {
	int Size = S_bytes[psXP->ctl.uSize];
	if (psXP->ctl.bAltF)									// invert order ?
		pStr += Num - Size;								// working backwards so point to last
	x64_t x64Val;
	int	Idx	= 0;
	while (Idx < Num) {
		switch (Size >> 1) {
		case 0:
			x64Val.x8[0].u8 = *((u8_t *) pStr);
			vPrintHexU8(psXP, x64Val.x8[0].u8);
			break;
		case 1:
			x64Val.x16[0].u16 = *((u16_t *) pStr);
			vPrintHexU16(psXP, x64Val.x16[0].u16);
			break;
		case 2:
			x64Val.x32[0].u32 = *((u32_t *) pStr);
			vPrintHexU32(psXP, x64Val.x32[0].u32);
			break;
		case 4:
			x64Val.u64 = *((u64_t *) pStr);
			vPrintHexU64(psXP, x64Val.u64);
			break;
		default:
			assert(0);
		}
		// step to the next 8/16/32/64 bit value
		if (psXP->ctl.bAltF) {						// invert order ?
			pStr -= Size;								// update source pointer backwards
		} else {
			pStr += Size;								// update source pointer forwards
		}
		Idx += Size;
		if (Idx >= Num)
			break;
		// now handle the grouping separator(s) if any
		if (psXP->ctl.bGroup) {							// separator required?
			xPrintChar(psXP, (psXP->ctl.uForm != form3X) ? CHR_COLON :
							  (Idx % 8) == 0 ? CHR_SPACE :
			  	  	  	  	  (Idx % 4) == 0 ? CHR_VERT_BAR :
				  	  	  	  (Idx % 2) == 0 ? CHR_MINUS : CHR_COLON);
		}
	}
}

/**
 * vPrintHexDump()
 * @brief		Dumps a block of memory in debug style format. depending on options output can be
 * 				formatted as 8/16/32 or 64 bit variables, optionally with no, absolute or relative address
 * @param[in]	psXP - pointer to print control structure
 * @param[in]	pStr - pointer to memory starting address
 * @param[in]	Len - Length/size of memory buffer to display
 * @param[out]	none
 * @return		none
 * @note		Use the following modifier flags
 *				'	Grouping of values using ' ' '-' or '|' as separators
 * 				!	Use relative address format
 * 				#	Use absolute address format
 *					Relative/absolute address prefixed using format '0x12345678:'
 * 				+	Add the ASCII char equivalents to the right of the hex output
 */
void vPrintHexDump(xp_t * psXP, int xLen, char * pStr) {
	xpc_t sXPC = psXP->ctl;
	int iWidth;
	if (psXP->ctl.bMinWid && INRANGE(8, psXP->ctl.MinWid, 64)) iWidth = psXP->ctl.MinWid;
	else {
		#if (includeTERMINAL_CONTROLS == 1)
		terminfo_t sTI;
		vTermGetInfo(&sTI);
		iWidth = sTI.MaxX;
		if (psXP->ctl.bLeft && sTI.MaxX <= (32+6)) {
			psXP->ctl.bLeft = 0;							// little space, disable addr
		} else { 
			iWidth -= psXP->ctl.bLeft ? 0 : 11;			// addr req, decr rem space
		}
		if (!psXP->ctl.bPlus || iWidth > 32) {
			iWidth /= 4;								// ASCII on right = 4 col/char
		} else {
			psXP->ctl.bPlus = 0;							// too little space, disable ASCII
			iWidth /= 3;								// no ASCII on right = 3 col/char
		}
		iWidth -= iWidth % 8;
		#else
		iWidth = xpfHEXDUMP_WIDTH;
		#endif
	}
	for (int Now = 0; Now < xLen; Now += iWidth) {
		if (psXP->ctl.bLeft == 0) {						// display address (absolute/relative)
			vPrintPointer(psXP, (px_t) (psXP->ctl.bRelVal ? (void *) Now : (void *) (pStr + Now)));
			xPrintChars(psXP, (char *) ": ");
			psXP->ctl = sXPC;
		}
		// then the actual series of values in 8-32 bit groups
		int Width = (xLen - Now) > iWidth ? iWidth : xLen - Now;
		vPrintHexValues(psXP, Width, pStr + Now);
		if (psXP->ctl.bPlus) {							// ASCII equivalent requested?
			int Size = S_bytes[psXP->ctl.uSize];
			u32_t Count = (xLen <= iWidth) ? 1 :
				((iWidth - Width) / Size) * (Size * 2 + (psXP->ctl.bGroup ? 1 : 0)) + 1;
			while (Count--)
				xPrintChar(psXP, CHR_SPACE);			// handle space padding for ASCII dump to line up
			for (Count = 0; Count < Width; ++Count) {	// values as ASCII characters
				int cChr = *(pStr + Now + Count);
				bool bFlag = ioB1GET(ioPrintFx);		// 0 = x20 ~ x7E, 1 = x20 ~ xFE
				bFlag = INRANGE(0x20, cChr, 0x7F) ? 0 :
						(INRANGE(0x80, cChr, 0xFE) && bFlag) ? 0 : 1;
				xPrintChar(psXP, bFlag ? CHR_FULLSTOP : cChr);
			}
		}
		if ((Now < xLen) && (xLen > iWidth))
			xPrintChars(psXP, strCRLF);
	}
}

// ############################# Proprietary extensions: date & time ###############################

seconds_t xPrintCalcSeconds(xp_t * psXP, tsz_t * psTSZ, struct tm * psTM) {
	seconds_t	Seconds;
	// Get seconds value to use... (adjust for TZ if required/allowed)
	if (psTSZ->pTZ &&									// TZ info available
		psXP->ctl.bPlus == 1 &&							// TZ info requested
		psXP->ctl.bAltF == 0 &&						// NOT bAltF (always GMT/UTC)
		psXP->ctl.bRelVal == 0) {						// NOT relative time (has no TZ info)
		Seconds = xTimeCalcLocalTimeSeconds(psTSZ);
	} else
		Seconds = xTimeStampAsSeconds(psTSZ->usecs);

	if (psTM)											// convert seconds into components
		xTimeGMTime(Seconds, psTM, psXP->ctl.bRelVal);
	return Seconds;
}

int	xPrintTimeCalcSize(xp_t * psXP, int i32Val) {
	if (!psXP->ctl.bRelVal || psXP->ctl.bPad0 || i32Val > 9)
		return psXP->ctl.MinWid = 2;
	psXP->ctl.MinWid = 1;
	return xDigitsInI32(i32Val, psXP->ctl.bGroup);
}

int	xPrintDate_Year(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	psXP->ctl.MinWid	= 0;
	int Len = xPrintXxx(psXP, (u64_t) (psTM->tm_year + YEAR_BASE_MIN), pBuffer, 4);
	if (psXP->ctl.bAltF == 0)
		pBuffer[Len++] = psXP->ctl.bGroup ? CHR_FWDSLASH : CHR_MINUS;
	return Len;
}

int	xPrintDate_Month(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXP, (u64_t) (psTM->tm_mon + 1), pBuffer, psXP->ctl.MinWid = 2);
	pBuffer[Len++] = psXP->ctl.bAltF ? CHR_SPACE : psXP->ctl.bGroup ? CHR_FWDSLASH : CHR_MINUS;
	return Len;
}

int	xPrintDate_Day(xp_t * psXP, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXP, (u64_t) psTM->tm_mday, pBuffer, xPrintTimeCalcSize(psXP, psTM->tm_mday));
	pBuffer[Len++] = psXP->ctl.bAltF ? CHR_SPACE :
					(psXP->ctl.bGroup && psXP->ctl.bRelVal) ? CHR_d :
					psXP->ctl.bGroup ? CHR_SPACE : CHR_T;
	psXP->ctl.bPad0 = 1;
	return Len;
}

void vPrintDate(xp_t * psXP, struct tm * psTM) {
	int	Len = 0;
	char Buffer[xpfMAX_LEN_DATE];
	if (psXP->ctl.bAltF) {
		Len += xstrncpy(Buffer, xTimeGetDayName(psTM->tm_wday), 3);		// "Sun"
		Len += xstrncpy(Buffer+Len, ", ", 2);							// "Sun, "
		Len += xPrintDate_Day(psXP, psTM, Buffer+Len);					// "Sun, 10 "
		Len += xstrncpy(Buffer+Len, xTimeGetMonthName(psTM->tm_mon), 3);// "Sun, 10 Sep"
		Len += xstrncpy(Buffer+Len, " ", 1);							// "Sun, 10 Sep "
		psXP->ctl.bAltF = 0;
		Len += xPrintDate_Year(psXP, psTM, Buffer+Len);				// "Sun, 10 Sep 2017"
	} else {
		if (psXP->ctl.bRelVal == 0) {					// if epoch value do YEAR+MON
			Len += xPrintDate_Year(psXP, psTM, Buffer+Len);
			Len += xPrintDate_Month(psXP, psTM, Buffer+Len);
		}
		if (psTM->tm_mday || psXP->ctl.bPad0)
			Len += xPrintDate_Day(psXP, psTM, Buffer+Len);
	}
	Buffer[Len] = 0;									// converted L to R, so terminate
	psXP->ctl.limits	= 0;								// enable full string
	vPrintString(psXP, Buffer);
}

void vPrintTime(xp_t * psXP, struct tm * psTM, u32_t uSecs) {
	char Buffer[xpfMAX_LEN_TIME];
	int	Len;
	xpc_t sXPC; memcpy(&sXPC, &psXP->ctl, sizeof(xpc_t));	// save current flags
	psXP->ctl.bAltF = 0;			// disable
	psXP->ctl.bGroup = 0;				// disable
	psXP->ctl.bLeft = 1;				// disable padding (space or 1)
	// Part 1: hours
	if (psTM->tm_hour || psXP->ctl.bPad0) {
		Len = xPrintXxx(psXP, (u64_t) psTM->tm_hour, Buffer, xPrintTimeCalcSize(psXP, psTM->tm_hour));
		Buffer[Len++] = sXPC.bGroup ? CHR_h : CHR_COLON;
		psXP->ctl.bPad0 = 1;
	} else {
		Len = 0;
	}
	// Part 2: minutes
	if (psTM->tm_min || psXP->ctl.bPad0) {
		Len += xPrintXxx(psXP, (u64_t) psTM->tm_min, Buffer+Len, xPrintTimeCalcSize(psXP, psTM->tm_min));
		Buffer[Len++] = sXPC.bGroup ? CHR_m : CHR_COLON;
		psXP->ctl.bPad0 = 1;
	}
	// Part 3: seconds
	Len += xPrintXxx(psXP, (u64_t) psTM->tm_sec, Buffer+Len, xPrintTimeCalcSize(psXP, psTM->tm_sec));
	// Part 4: [.xxxxxx]
	if (psXP->ctl.bRadix && uSecs) {
		Buffer[Len++] = sXPC.bGroup ? CHR_s : CHR_FULLSTOP;
		psXP->ctl.Precis = INRANGE(1, psXP->ctl.Precis, xpfMAX_TIME_FRAC) ? psXP->ctl.Precis : xpfDEF_TIME_FRAC;
		if (psXP->ctl.Precis < xpfMAX_TIME_FRAC)
			uSecs /= u32pow(10, xpfMAX_TIME_FRAC - psXP->ctl.Precis);
		psXP->ctl.bPad0 = 1;								// need leading '0's
		psXP->ctl.bSigned = 0;
		psXP->ctl.bLeft = 0;								// force R-just
		Len += xPrintXxx(psXP, uSecs, Buffer+Len, psXP->ctl.MinWid = psXP->ctl.Precis);
	} else if (sXPC.bGroup) {
		Buffer[Len++] = CHR_s;
	}
	Buffer[Len] = 0;
	memcpy(&psXP->ctl, &sXPC, sizeof(xpc_t));			// restore all flags
	psXP->ctl.Precis = 0;								// was only used for uSec resolution
	if (psXP->ctl.bMinWid && psXP->ctl.MinWid < Len)
		psXP->ctl.MinWid = Len;							// enable full string if longer
	vPrintString(psXP, Buffer);
}

void vPrintZone(xp_t * psXP, tsz_t * psTSZ) {
	int	Len = 0;
	char Buffer[configTIME_MAX_LEN_TZINFO];
	if (psTSZ->pTZ == 0) {								// If no TZ info supplied
		Buffer[Len++] = CHR_Z;

	} else if (psXP->ctl.bAltF) {						// TZ info available but '#' format specified
		Len = xstrncpy(Buffer, (char *) " GMT", 4);	// show as GMT (ie UTC)

	} else {											// TZ info available & '+x:xx(???)' format requested
		#if	(timexTZTYPE_SELECTED == timexTZTYPE_RFC5424)
		if (psXP->ctl.bPlus) {
			psXP->ctl.bSigned = 1;						// TZ hours offset is a signed value
			Len += xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer+Len, psXP->ctl.MinWid = 3);
			Buffer[Len++] = psXP->ctl.bGroup ? CHR_h : CHR_COLON;
			psXP->ctl.bSigned = 0;						// TZ offset minutes unsigned
			psXP->ctl.bPlus = 0;
			Len += xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer+Len, psXP->ctl.MinWid = 2);
		} else {
			Buffer[Len++] = CHR_Z;						// add 'Z' for Zulu/zero time zone
		}
		#elif (timexTZTYPE_SELECTED == timexTZTYPE_POINTER)
		psXP->ctl.bSigned = 1;							// TZ hours offset is a signed value
		psXP->ctl.bPlus = 1;							// force display of sign
		Len = xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXP->ctl.MinWid = 3);
		Buffer[Len++] = psXP->ctl.bGroup ? CHR_h : CHR_COLON;
		psXP->ctl.bSigned = 0;							// TZ offset minutes unsigned
		psXP->ctl.bPlus = 0;
		Len += xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXP->ctl.MinWid = 2);
		if (psTSZ->pTZ->pcTZName) {						// handle TZ name (as string pointer) if there
			Buffer[Len++] = CHR_L_ROUND;
			psXP->ctl.MinWid = 0;						// then complete with the TZ name
			while (psXP->ctl.MinWid < configTIME_MAX_LEN_TZNAME &&
					psTSZ->pTZ->pcTZName[psXP->ctl.MinWid] != 0) {
				Buffer[Len + psXP->ctl.MinWid] = psTSZ->pTZ->pcTZName[psXP->ctl.MinWid];
				psXP->ctl.MinWid++;
			}
			Len += psXP->ctl.MinWid;
			Buffer[Len++] = CHR_R_ROUND;
		}

		#elif (timexTZTYPE_SELECTED == timexTZTYPE_FOURCHARS)
		psXP->ctl.bSigned = 1;							// TZ hours offset is a signed value
		psXP->ctl.bPlus = 1;								// force display of sign
		Len = xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXP->ctl.MinWid = 3);
		Buffer[Len++] = psXP->ctl.bGroup ? CHR_h : CHR_COLON;
		psXP->ctl.bSigned = 0;							// TZ offset minutes unsigned
		psXP->ctl.bPlus = 0;
		Len += xPrintXxx(psXP, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXP->ctl.MinWid = 2);
		// Now handle the TZ name if there, check to ensure max 4 chars all UCase
		if (xstrverify(&psTSZ->pTZ->tzname[0], CHR_A, CHR_Z, configTIME_MAX_LEN_TZNAME) == erSUCCESS) {
			Buffer[Len++] = CHR_L_ROUND;
			// then complete with the TZ name
			psXP->ctl.MinWid = 0;
			while ((psXP->ctl.MinWid < configTIME_MAX_LEN_TZNAME) &&
					(psTSZ->pTZ->tzname[psXP->ctl.MinWid] != 0) &&
					(psTSZ->pTZ->tzname[psXP->ctl.MinWid] != CHR_SPACE)) {
				Buffer[Len + psXP->ctl.MinWid] = psTSZ->pTZ->tzname[psXP->ctl.MinWid];
				psXP->ctl.MinWid++;
			}
			Len += psXP->ctl.MinWid;
			Buffer[Len++] = CHR_R_ROUND;
		}
		#endif
	}
	Buffer[Len] = 0;									// converted L to R, so add terminating NUL
	psXP->ctl.limits = 0;								// enable full string
	vPrintString(psXP, Buffer);
}

// ################################# Proprietary extension: URLs ###################################

/**
 * vPrintURL() -
 * @param psXP
 * @param pString
 */
void vPrintURL(xp_t * psXP, char * pStr) {
	if (halCONFIG_inMEM(pStr)) {
		char cIn;
		while ((cIn = *pStr++) != 0) {
			if (INRANGE(CHR_A, cIn, CHR_Z) ||
				INRANGE(CHR_a, cIn, CHR_z) ||
				INRANGE(CHR_0, cIn, CHR_9) ||
				(cIn == CHR_MINUS || cIn == CHR_FULLSTOP || cIn == CHR_UNDERSCORE || cIn == CHR_TILDE)) {
				xPrintChar(psXP, cIn);
			} else {
				xPrintChar(psXP, CHR_PERCENT);
				xPrintChar(psXP, cPrintNibbleToChar(psXP, cIn >> 4));
				xPrintChar(psXP, cPrintNibbleToChar(psXP, cIn & 0x0F));
			}
		}
	} else {
		vPrintString(psXP, pStr);						// "null" or "pOOR"
	}
}

// ############################## Proprietary extension: IP address ################################

/**
 * vPrintIpAddress()
 * @brief		Print DOT formatted IP address to destination
 * @param[in]	psXP - pointer to output & format control structure
 * 				Val - IP address value in HOST format !!!
 * @param[out]	none
 * @return		none
 * @note		Used the following modifier flags
 * 				'!'		Invert the LSB normal MSB byte order (Network <> Host related)
 * 				'-'		Left align the individual numbers between the '.'
 * 				'0'		Zero pad the individual numbers between the '.'
 */
void vPrintIpAddress(xp_t * psXP, u32_t Val) {
	psXP->ctl.MinWid = psXP->ctl.bLeft ? 0 : 3;
	psXP->ctl.bSigned = 0;								// ensure interpreted as unsigned
	psXP->ctl.bPlus = 0;								// and no sign to be shown
	u8_t * pChr = (u8_t *) &Val;
	if (psXP->ctl.bAltF) {								// '#' specified to invert order ?
		psXP->ctl.bAltF = 0;							// clear so not to confuse xPrintXxx()
		u8_t temp;
		temp = *pChr;
		*pChr = *(pChr+3);
		*(pChr+3) = temp;
		temp = *(pChr+1);
		*(pChr+1) = *(pChr+2);
		*(pChr+2) = temp;
	}
	// building R to L, ensure buffer NULL-term
	char Buffer[xpfMAX_LEN_IP];
	Buffer[xpfMAX_LEN_IP - 1] = 0;

	// then convert the IP address, LSB first
	int	Idx, Len;
	for (Idx = 0, Len = 0; Idx < sizeof(Val); ++Idx) {
		u64_t u64Val = (u8_t) pChr[Idx];
		Len += xPrintXxx(psXP, u64Val, Buffer + xpfMAX_LEN_IP - 5 - Len, 4);
		if (Idx < 3)
			Buffer[xpfMAX_LEN_IP - 1 - ++Len] = CHR_FULLSTOP;
	}
	// then send the formatted output to the correct stream
	psXP->ctl.limits = 0;
	vPrintString(psXP, Buffer + (xpfMAX_LEN_IP - 1 - Len));
}

// ########################### Proprietary extension: SGR attributes ###############################

/**
 * @brief	set starting and ending fore/background colors
 * @param	psXP
 * @param	Val	- u32_t value treated as 4x U8 being SGR color/attribute codes
 * @note	https://docs.lvgl.io/8.4/widgets/core/label.html?highlight=printf%20text%20color
 */
void vPrintSetGraphicRendition(xp_t * psXP, u32_t Val) {
	char Buffer[xpfMAX_LEN_SGR];
	sgr_info_t sSGR = { .u32 = Val };
	switch (psXP->ctl.sgr) {
	case sgrANSI:
		#if (halUSE_CURSOR == 1)						// Optional terminal support
		if (pcTermLocate(Buffer, sSGR.r, sSGR.c) != Buffer) {
			xPrintChars(psXP, Buffer);					// cursor location, 1 relative, row & column
		}
		#endif
		if (pcTermAttrib(Buffer, sSGR.a1, sSGR.a2) != Buffer) {
			xPrintChars(psXP, Buffer);					// attribute[s]
		}
		break;
//	case sgrNONE:
//	case sgrLVGL:
//	case sgrOTHER:
	default:
	}
}

/* ################################# The HEART of the PRINTFX matter ###############################
 * PrintFX - common routine for formatted print functionality
 * @brief	parse the format string and interpret the conversions, flags and modifiers
 * 			extract the parameters variables in correct type format
 * 			call the correct routine(s) to perform conversion, formatting & output
 * @param	psXP - pointer to structure containing formatting and output destination info
 * 			format - pointer to the formatting string
 * @return	void (other than updated info in the original structure passed by reference
 */

int	xPrintFX(xp_t * psXP, const char * fmt) {
	if (fmt == NULL)
		goto exit;
	for (; *fmt != CHR_NUL; ++fmt) {
		if (*fmt == CHR_PERCENT) {						// start by expecting format indicator
			++fmt;
			if (*fmt == CHR_NUL)
				break;
			if (*fmt == CHR_PERCENT)
				goto out_lbl;
			psXP->ctl.flg1 = 0;							// reset internal/dynamic flags
			psXP->ctl.limits = 0;						// reset field specific limits
			psXP->ctl.uBase = BASE10;					// default number base
			int	cFmt;
			x32_t X32 = { 0 };
			// Optional FLAGS must be in correct sequence of interpretation
			while ((cFmt = strchr_i("!#&'*+- 0", *fmt)) != erFAILURE) {
				switch (cFmt) {
				case 0:	psXP->ctl.bRelVal = 1; break;	// !	HEXDUMP/DTZ abs->rel address/time, MAC use ':' separator
				case 1:	psXP->ctl.bAltF = 1; break;	// #	DTZ=GMT format, HEXDUMP/IP swop endian, STRING centre
				case 2: psXP->ctl.bArray = 1; break;		// &	Array size & address provided
				case 3: psXP->ctl.bGroup = 1; break;		// '	"diu" add 3 digit grouping, DTZ, MAC, DUMP select separator set
				case 4:									// *	indicate argument will supply field width
					X32.iX	= va_arg(psXP->vaList, int);
					IF_myASSERT(debugTRACK, psXP->ctl.bMinWid == 0 && X32.iX <= xpfMINWID_MAXVAL);
					psXP->ctl.MinWid = X32.iX;
					psXP->ctl.bMinWid = 1;
					X32.i32 = 0;
					break;
				case 5: psXP->ctl.bPlus = 1; break;		// +	force +/-, HEXDUMP add ASCII, TIME add TZ info
				case 6: psXP->ctl.bLeft = 1; break;		// -	Left justify, HEXDUMP remove address pointer
				case 7:	psXP->ctl.bPadSp = 1; break;		//  	' ' instead of '+'
				case 8:	psXP->ctl.bPad0 = 1; break;		// 0	force leading '0's
				default: assert(0);
				}
				++fmt;
			}

			// Optional WIDTH and PRECISION indicators
			if (*fmt == CHR_FULLSTOP || INRANGE(CHR_1, *fmt, CHR_9)) {
				while (1) {
					if (INRANGE(CHR_0, *fmt, CHR_9)) {
						X32.i32 *= 10;
						X32.i32 += *fmt - CHR_0;
						++fmt;
					} else if (*fmt == CHR_FULLSTOP) {
						if (X32.i32 > 0) {				// save value if previously parsed
							IF_myASSERT(debugTRACK, psXP->ctl.bMinWid == 0 && X32.iX <= xpfMINWID_MAXVAL);
							psXP->ctl.MinWid = X32.iX;
							psXP->ctl.bMinWid = 1;
							X32.i32 = 0;
						}
						IF_myASSERT(debugTRACK, psXP->ctl.bRadix == 0);	// 2x bRadix not allowed
						psXP->ctl.bRadix = 1;
						++fmt;
					} else if (*fmt == CHR_ASTERISK) {
						IF_myASSERT(debugTRACK, psXP->ctl.bRadix == 1 && X32.iX == 0);
						X32.iX	= va_arg(psXP->vaList, int);
						IF_myASSERT(debugTRACK, X32.iX <= xpfPRECIS_MAXVAL);
						psXP->ctl.bPrecis = 1;
						psXP->ctl.Precis = X32.iX;
						X32.i32 = 0;
						++fmt;
					} else {
						break;
					}
				}
				// Save possible parsed value
				if (X32.i32 > 0) {						// if a number was parsed...
					if (psXP->ctl.bRadix == 0 && psXP->ctl.bMinWid == 0) {	// no '.' nor width
						IF_myASSERT(debugTRACK, X32.iX <= xpfMINWID_MAXVAL);
						psXP->ctl.MinWid = X32.iX;		// save value as MinWid
						psXP->ctl.bMinWid = 1;			// set MinWid flag
					} else if (psXP->ctl.bRadix == 1 && psXP->ctl.bPrecis == 0) {	// no '.' nor Precis
						IF_myASSERT(debugTRACK, X32.iX <= xpfPRECIS_MAXVAL);
						psXP->ctl.Precis = X32.iX;		// save value as precision
						psXP->ctl.bPrecis = 1;			// set precision flag
					} else {
						assert(0);
					}
				}
			}

			// Optional SIZE indicators
			cFmt = strchr_i("hljztL", *fmt);
			if (cFmt != erFAILURE) {
				++fmt;
				switch(cFmt) {
				case 0:
					if (*fmt == CHR_h) {				// "hh"
						psXP->ctl.uSize = S_hh;
						++fmt;
					} else {							// 'h'
						psXP->ctl.uSize = S_h;
					}
					break;
				case 1:
					if (*fmt != CHR_l) {
						psXP->ctl.uSize = S_l;			// 'l'
					} else {
						psXP->ctl.uSize = S_ll;			// "ll"
						++fmt;
					}
					break;
				case 2: psXP->ctl.uSize = S_j; break;	// [u]intmax_t[*]
				case 3: psXP->ctl.uSize = S_z; break;	// [s]size_t[*]
				case 4: psXP->ctl.uSize = S_t; break;	// ptrdiff[*]
				case 5: psXP->ctl.uSize = S_L; break;	// long double float (F128)
				default: assert(0);
				}
			}
			IF_myASSERT(debugTRACK, psXP->ctl.uSize < S_XXX);	// rest not yet supported
			
			// Check if format character where UC/lc same character control the case of the output
			cFmt = *fmt;
			if (strchr_i(vPrintStr1, cFmt) != erFAILURE) {
				cFmt |= 0x20;							// convert to lower case, but ...
				psXP->ctl.bCase = 1;						// indicate as UPPER case requested
			}

			x64_t X64;									// default x64 variable
			px_t pX;
			#if	(xpfSUPPORT_DATETIME == 1)
			tsz_t * psTSZ;
			struct tm sTM;
			#endif

			switch (cFmt) {
			#if	(xpfSUPPORT_SGR == 1)
			case CHR_C: vPrintSetGraphicRendition(psXP, va_arg(psXP->vaList, u32_t)); break;
			#endif

			#if	(xpfSUPPORT_IP_ADDR == 1)				// IP address
			case CHR_I: vPrintIpAddress(psXP, va_arg(psXP->vaList, u32_t)); break;
			#endif

			#if	(xpfSUPPORT_DATETIME == 1)
			/* Prints date and/or time in POSIX format
			 * Use the following modifier flags
			 *	'''		select between 2 different separator sets being
			 *			'/' or '-' (years)
			 *			'/' or '-' (months)
			 *			'T' or ' ' (days)
			 *			':' or 'h' (hours)
			 *			':' or 'm' (minutes)
			 *			'.' or 's' (seconds)
			 * 	'!'		Treat time value as elapsed and not epoch [micro] seconds
			 * 	'.'		append 1 -> 6 digit(s) fractional seconds
			 * Norm 1	1970/01/01T00:00:00Z
			 * Norm 2	1970-01-01 00h00m00s
			 * Altform	Mon, 01 Jan 1970 00:00:00 GMT
			 */
			case CHR_D:				// Local TZ date
			case CHR_T:				// Local TZ time
			case CHR_Z: {			// Local TZ DATE+TIME+ZONE
				IF_myASSERT(debugTRACK, !psXP->ctl.bRelVal && !psXP->ctl.bGroup);
				psTSZ = va_arg(psXP->vaList, tsz_t *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(psTSZ));
				psXP->ctl.bPad0 = 1;
				X32.u32 = xTimeStampAsSeconds(psTSZ->usecs);
				xpc_t sXPC = { 0 };
				sXPC.flags = psXP->ctl.flags;
				// If full local time required, add TZ and DST offsets
				if (psXP->ctl.bPlus && psTSZ->pTZ)
					X32.u32 += psTSZ->pTZ->timezone + (int) psTSZ->pTZ->daylight;
				xTimeGMTime(X32.u32, &sTM, psXP->ctl.bRelVal);
				psXP->ctl.bPlus = 0;
				if (cFmt == CHR_D || cFmt == CHR_Z)
					vPrintDate(psXP, &sTM);
				if (cFmt == CHR_T || cFmt == CHR_Z)
					vPrintTime(psXP, &sTM, (u32_t)(psTSZ->usecs % MICROS_IN_SECOND));
				if (cFmt == CHR_Z) {
					psXP->ctl.bPlus = sXPC.bPlus;
					vPrintZone(psXP, psTSZ);
				}
			}
			break;

			case CHR_r: {			// U64 epoch (yr+mth+day) OR relative (days) + TIME
				IF_myASSERT(debugTRACK, !psXP->ctl.bPlus && !psXP->ctl.bPad0 && !psXP->ctl.bGroup);
				if (psXP->ctl.bCase) {					// 64bit value supplied
					X64.u64 = va_arg(psXP->vaList,u64_t);
					X32.u32 = (u32_t) (X64.u64 / MICROS_IN_SECOND);
				} else {								// 32bit value supplied
					X32.u32 = va_arg(psXP->vaList,u32_t);
					X64.u64 = (u64_t) X32.u32 * MICROS_IN_SECOND;
				}
				xTimeGMTime(X32.u32, &sTM, psXP->ctl.bRelVal);
				if (psXP->ctl.bRelVal == 0)
					psXP->ctl.bPad0 = 1;
				if (psXP->ctl.bRelVal == 0 || sTM.tm_mday) {
					u32_t limits = psXP->ctl.limits;
					vPrintDate(psXP, &sTM);
					psXP->ctl.limits = limits;
				}
				vPrintTime(psXP, &sTM, (u32_t) (X64.u64 % MICROS_IN_SECOND));
				if (psXP->ctl.bRelVal == 0)
					xPrintChar(psXP, CHR_Z);
			}
			break;
			#endif

			#if	(xpfSUPPORT_URL == 1)					// para = pointer to string to be encoded
			case CHR_U:
				pX.pc8 = va_arg(psXP->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				vPrintURL(psXP, pX.pc8);
				break;
			#endif

			#if	(xpfSUPPORT_MAC_ADDR == 1)
			/* Formats 6 byte string (0x00 is valid) as a series of hex characters.
			 * default format uses no separators eg. '0123456789AB'
			 * Support the following modifier flags:
			 *  '#' select upper case alternative form
			 *  '!'	select ':' separator between digits
			 */
			case CHR_M:									// MAC address ??:??:??:??:??:??
				IF_myASSERT(debugTRACK, !psXP->ctl.bMinWid && !psXP->ctl.bPrecis);
				psXP->ctl.uSize = S_hh;					// force interpretation as sequence of U8 values
				pX.pc8 = va_arg(psXP->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				vPrintHexValues(psXP, lenMAC_ADDRESS, pX.pc8);
				break;
			#endif

			#if	(xpfSUPPORT_HEXDUMP == 1)
			case CHR_Y:									// HEXDUMP
				IF_myASSERT(debugTRACK, !psXP->ctl.bMinWid && !psXP->ctl.bPrecis);
				/* In order for formatting to work the "*" or "." bRadix specifiers
				 * should not be used. The requirement for a second parameter is implied and assumed */
				X32.iX = va_arg(psXP->vaList, int);
				pX.pc8 = va_arg(psXP->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				psXP->ctl.uForm = form3X;
				vPrintHexDump(psXP, X32.iX, pX.pc8);
				break;
			#endif

			/* convert unsigned 32/64 bit value to 1/0 ASCI string
			 * field width specifier is applied as mask starting from LSB to MSB
			 * '#'	change prepended "0b" to "0B"
			 * '''	Group digits using '|' (bytes) and '-' (nibbles)
			 */
			case CHR_b: {
					X64 = x64PrintGetValue(psXP);
					psXP->ctl.bGroup = 0;
					X32.iX = S_bytes[psXP->ctl.uSize] * BITS_IN_BYTE;
					if (psXP->ctl.MinWid)
						X32.iX = (psXP->ctl.MinWid > X32.iX) ? X32.iX : psXP->ctl.MinWid;
					u64_t mask = 1ULL << (X32.iX - 1);
					xPrintChars(psXP, psXP->ctl.bCase ? "0B" : "0b");
					while (mask) {
						xPrintChar(psXP, (X64.u64 & mask) ? CHR_1 : CHR_0);
						mask >>= 1;
						// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
						if (--X32.iX && psXP->ctl.bGroup && (X32.iX % 4) == 0) {
							xPrintChar(psXP, X32.iX % 32 == 0 ? CHR_VERT_BAR :			// word
											  X32.iX % 16 == 0 ? CHR_COLON :			// short
										  	  X32.iX % 8 == 0 ? CHR_SPACE : CHR_MINUS);	// byte or nibble
						}
					}
				}
				break;

			case CHR_c: xPrintChar(psXP, va_arg(psXP->vaList, int)); break;

			case CHR_d:									// signed decimal "[-]ddddd"
			case CHR_i:									// signed integer (same as decimal ?)
				psXP->ctl.bSigned = 1;
				if (psXP->ctl.bArray) {
					vPrintX64array(psXP);
				} else {
					X64 = x64PrintGetValue(psXP);
					if (X64.i64 < 0LL) {
						psXP->ctl.bNegVal = 1;
						X64.i64 *= -1; 					// convert the value to unsigned
					}
					vPrintX64(psXP, X64.u64);
				}
				break;

			#if	(xpfSUPPORT_IEEE754 == 1)
//			case CHR_a:									// HEX format not yet supported
			case CHR_e: ++psXP->ctl.uForm;				// form = 2
				/* FALLTHRU */ /* no break */
			case CHR_f: ++psXP->ctl.uForm;				// form = 1
				/* FALLTHRU */ /* no break */
			case CHR_g:									// form = 0
				psXP->ctl.bSigned = 1;					// float always signed value.
				/* https://en.cppreference.com/w/c/io/fprintf
				 * https://pubs.opengroup.org/onlinepubs/007908799/xsh/fprintf.html
				 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160
				 */
				if (psXP->ctl.bPrecis) {				// explicit precision specified ?
					psXP->ctl.Precis = psXP->ctl.Precis > xpfMAXIMUM_DECIMALS ? xpfMAXIMUM_DECIMALS : psXP->ctl.Precis;
				} else {
					psXP->ctl.Precis	= xpfDEFAULT_DECIMALS;
				}
				if (psXP->ctl.bArray) {
					psXP->ctl.bFloat = 1;
					vPrintX64array(psXP);
				} else {
					vPrintF64(psXP, va_arg(psXP->vaList, f64_t));
				}
				break;
			#endif

			case CHR_m: pX.pc8 = strerror(errno); goto commonM_S;

			case CHR_n:									// store chars to date at location.
				pX.piX = va_arg(psXP->vaList, int *);
				IF_myASSERT(debugTRACK, halCONFIG_inSRAM(pX.pc8));
				*pX.piX = psXP->ctl.curlen;
				break;

			case CHR_o:									// unsigned octal "ddddd"
			case CHR_x: psXP->ctl.bGroup = 0;				// hex as in "789abcd" UC/LC, disable grouping
				/* FALLTHRU */ /* no break */
			case CHR_u:									// unsigned decimal "ddddd"
				psXP->ctl.bSigned = 0;
				psXP->ctl.uBase = (cFmt == CHR_x) ? BASE16 : 
								 (cFmt == CHR_u) ? BASE10 : BASE08;
 				if (psXP->ctl.bArray) {
					vPrintX64array(psXP);
				} else {
					// Ensure sign-extended bits removed
					int Width = (psXP->ctl.uSize == S_hh) ? 7 :
								(psXP->ctl.uSize == S_h) ? 15 :
								(psXP->ctl.uSize == S_l) ? 31 :
								(psXP->ctl.uSize == S_ll) ? 63 : (BITS_IN_BYTE * sizeof(int)) - 1;
					X64 = x64PrintGetValue(psXP);
					X64.u64 &= BIT_MASK64(0, Width);
					vPrintX64(psXP, X64.u64);
				}
				break;

			case CHR_p:
				pX.pv = va_arg(psXP->vaList, void *);
				vPrintPointer(psXP, pX);
				break;

			case CHR_s: pX.pc8 = va_arg(psXP->vaList, char *);

			commonM_S:
				// Required to avoid crash when wifi message is intercepted and a string pointer parameter
				// is evaluated as out of valid memory address (0xFFFFFFE6). Replace string with "pOOR"
				pX.pc8 = halCONFIG_inMEM(pX.pc8) ? pX.pc8 : (pX.pc8 == NULL) ? strNULL : strOOR;
				vPrintString(psXP, pX.pc8);
				break;

			default:
				/* At this stage we have handled the '%' as assumed, but the next character found is invalid.
				 * Show the '%' we swallowed and then the extra, invalid, character as well */
				xPrintChar(psXP, CHR_PERCENT);
				xPrintChar(psXP, *fmt);
				break;
			}
			psXP->ctl.bPlus = 0;							// reset this form after printing one value
		} else {
out_lbl:
			xPrintChar(psXP, *fmt);
		}
	}
exit:
	return psXP->ctl.curlen;
}

int	xPrintF(int (Hdlr)(xp_t *, int), void * pVoid, size_t Size, const char * fmt, va_list vaList) {
	xp_t sXP;
	sXP.handler = Hdlr;
	sXP.pVoid = pVoid;
	sXP.vaList = vaList;
	// [v]wprintfx() ONLY!! 
	// Move flags mapped onto MSByte of Size into xp_t structure MSByte
	if (Size > xpfMAXLEN_MAXVAL) {
		sXP.ctl.flg2 = Size >> (32 - xpfBITS_REPORT);
		Size &= BIT_MASK32(0, xpfMAXLEN_BITS - 1);
	}
	sXP.ctl.maxlen = Size;
	sXP.ctl.curlen = 0;
	return xPrintFX(&sXP, fmt);
}

// ################################### Destination = STDOUT ########################################

static int xPrintStdOut(xp_t * psXP, int cChr) { return putchar(cChr); }

int vnprintfx(size_t szLen, const char * format, va_list vaList) {
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintStdOut, NULL, szLen, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	return iRV;
}

int nprintfx(size_t szLen, const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintStdOut, NULL, szLen, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	va_end(vaList);
	return iRV;
}

int vprintfx(const char * format, va_list vaList) {
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	return iRV;
}

int printfx(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	va_end(vaList);
	return iRV;
}

#if 0
/*
 * [v[n]]printfx_nolock() - print to stdout without any semaphore locking.
 * 					securing the channel must be done manually
 */
int vnprintfx_nolock(size_t szLen, const char * format, va_list vaList) {
	return xPrintF(xPrintStdOut, NULL, szLen, format, vaList);
}

int vprintfx_nolock(const char * format, va_list vaList) {
	return xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
}

int printfx_nolock(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	int iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return iRV;
}
#endif

// ##################################### Destination = STRING ######################################

static int xPrintToString(xp_t * psXP, int cChr) {
	if (psXP->pStr) *psXP->pStr++ = cChr;
	return cChr;
}

int vsnprintfx(char * pBuf, size_t szBuf, const char * format, va_list vaList) {
	if (szBuf == 1) {									// any "real" space ?
		if (pBuf) *pBuf = 0;							// no, buffer supplied so terminate
		return 0; 										// & return
	}
	int iRV = xPrintF(xPrintToString, pBuf, szBuf, format, vaList);
	if (pBuf) {											// buffer specified ?
		if (iRV == szBuf) --iRV;						// yes, if full make space for terminator
		pBuf[iRV] = 0;									// terminate
	}
	return iRV;
}

int snprintfx(char * pBuf, size_t szBuf, const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	int iRV = vsnprintfx(pBuf, szBuf, format, vaList);
	va_end(vaList);
	return iRV;
}

int vsprintfx(char * pBuf, const char * format, va_list vaList) {
	return vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vaList);
}

int sprintfx(char * pBuf, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int iRV = vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return iRV;
}

/* ################################# Destination - String buffer or STDOUT #########################
 * * Based on the values (pre) initialised for buffer start and size
 * a) walk through the buffer on successive calls, concatenating output; or
 * b) output directly to stdout if buffer pointer/size not initialized.
 * Because the intent in this function is to provide a streamlined method for
 * keeping track of buffer usage over a series of successive printf() type calls
 * no attempt is made to control access to the buffer or output channel as such.
 * It is the responsibility of the calling function to control (un/lock) access.
 */

int	wvprintfx(report_t * psR, const char * pcFormat, va_list vaList) {
	int iRV = 0;
	if (psR && psR->pcBuf && psR->size) {
		IF_myASSERT(debugPARAM, halCONFIG_inSRAM(psR) && halCONFIG_inSRAM(psR->pcBuf));
		iRV = vsnprintfx(psR->pcBuf, psR->Size, pcFormat, vaList);
		if (iRV > 0) {
			IF_myASSERT(debugRESULT, iRV <= psR->size);
			psR->pcBuf += iRV;
			psR->size -= iRV;
		}
	} else {
		BaseType_t btRV = pdTRUE;
		size_t Size = xpfMAXLEN_MAXVAL;
		if (psR) {
			if (psR->sgr)
				Size |= (psR->sgr << 30);
			if (psR->fLock) {
				btRV = xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
				psR->fLock = 0;
			}
		} else {
			btRV = xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
		}
		if (btRV == pdFALSE)		// semaphore not obtained, return....
			goto exit;
		iRV = xPrintF(xPrintStdOut, NULL, Size, pcFormat, vaList);
		if (psR) {
			if (psR->fUnlock) {
				btRV = xRtosSemaphoreGive(&shUARTmux);
				psR->fUnlock = 0;
			}
		} else {
			btRV = xRtosSemaphoreGive(&shUARTmux);
		}
	}
exit:
	return iRV;
}

int	wprintfx(report_t * psR, const char * pcFormat, ...) {
	va_list vaList;
	va_start(vaList, pcFormat);
	int iRV = wvprintfx(psR, pcFormat, vaList);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = FILE PTR ######################################

static int xPrintToFile(xp_t * psXP, int cChr) { return fputc(cChr, psXP->stream); }

int vfprintfx(FILE * stream, const char * format, va_list vaList) {
	return xPrintF(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vaList);
}

int fprintfx(FILE * stream, const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	int count = xPrintF(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return count;
}

// ################################### Destination = HANDLE ########################################

static int xPrintToHandle(xp_t * psXP, int cChr) {
	char cChar = cChr;
	int size = write(psXP->fd, &cChar, sizeof(cChar));
	return (size == 1) ? cChr : size;
}

int	vdprintfx(int fd, const char * format, va_list vaList) {
	return xPrintF(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vaList);
}

int	dprintfx(int fd, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int count = xPrintF(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return count;
}

/* ################################## Destination = UART/TELNET ####################################
 * Output directly to the [possibly redirected] stdout/UART channel
 */

static int xPrintToConsole(xp_t * psXP, int cChr) {
	#if (buildSTDOUT_LEVEL > 0)
	return putchar_cursor(cChr, 0);
	#else
	return esp_rom_printf("%c", cChr);
	#endif
}

int vcprintfx(const char * format, va_list vaList) {
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	return iRV;
}

int cprintfx(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	xRtosSemaphoreTake(&shUARTmux, WPFX_TIMEOUT);
	int iRV = xPrintF(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	xRtosSemaphoreGive(&shUARTmux);
	va_end(vaList);
	return iRV;
}

// ################################### Destination = DEVICE ########################################

static int xPrintToDevice(xp_t * psXP, int cChr) { return psXP->DevPutc(cChr); }

int vdevprintfx(int (* Hdlr)(int ), const char * format, va_list vaList) {
	return xPrintF(xPrintToDevice, Hdlr, xpfMAXLEN_MAXVAL, format, vaList);
}

int devprintfx(int (* Hdlr)(int ), const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	int iRV = xPrintF(xPrintToDevice, Hdlr, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return iRV;
}

/* #################################### Destination : SOCKET #######################################
 * SOCKET directed formatted print support. Problem here is that MSG_MORE is primarily supported on
 * TCP sockets, UDP support officially in LwIP 2.6 but has not been included into ESP-IDF yet. */

static int xPrintToSocket(xp_t * psXP, int cChr) {
	u8_t cBuf = cChr;
	int iRV = xNetSend(psXP->psSock, &cBuf, sizeof(cBuf));
	if (iRV != sizeof(cBuf)) return iRV;
	return cChr;
}

int vsocprintfx(netx_t * psSock, const char * format, va_list vaList) {
	int	Fsav = psSock->flags;
	psSock->flags |= MSG_MORE;
	int iRV = xPrintF(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vaList);
	psSock->flags = Fsav;
	return (psSock->error == 0) ? iRV : erFAILURE;
}

int socprintfx(netx_t * psSock, const char * format, ...) {
	int	Fsav = psSock->flags;
	psSock->flags |= MSG_MORE;
	va_list vaList;
	va_start(vaList, format);
	int iRV = xPrintF(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	psSock->flags = Fsav;
	return (psSock->error == 0) ? iRV : erFAILURE;
}

// #################################### Destination : UBUF #########################################

static int xPrintToUBuf(xp_t * psXP, int cChr) { return xUBufPutC(psXP->psUBuf, cChr); }

int	vuprintfx(ubuf_t * psUBuf, const char * format, va_list vaList) {
	return xPrintF(xPrintToUBuf, psUBuf, xUBufGetSpace(psUBuf), format, vaList);
}

int	uprintfx(ubuf_t * psUBuf, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int count = xPrintF(xPrintToUBuf, psUBuf, xUBufGetSpace(psUBuf), format, vaList);
	va_end(vaList);
	return count;
}

// #################################### Destination : CRC32 ########################################

static int xPrintToCRC32(xp_t * psXP, int cChr) {
	#if defined(ESP_PLATFORM)							// use ROM based CRC lookup table
	u8_t cBuf = cChr;
	*psXP->pU32 = crc32_le(*psXP->pU32, &cBuf, sizeof(cBuf));
	#else												// use fastest of external libraries
	u32_t MsgCRC = crcSlow((u8_t *) pBuf, iRV);
	#endif
	return cChr;
}

int	vcrcprintfx(u32_t * pU32, const char * format, va_list vaList) {
	return xPrintF(xPrintToCRC32, pU32, xpfMAXLEN_MAXVAL, format, vaList);
}

int	crcprintfx(u32_t * pU32, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int count = xPrintF(xPrintToCRC32, pU32, xpfMAXLEN_MAXVAL, format, vaList);
	va_end(vaList);
	return count;
}

// ############################# Aliases for NEW/STDLIB supplied functions #########################

/* To make this work for esp-idf and newlib, the following modules must be removed:
 *		lib_a-[s[sn[fi]]]printf.o
 *		lib_a-v[f[i]]printf.o
 *		lib_a-putchar.o
 *		lib_a-getchar.o
 * using command line
 * 		ar -d /c/Dropbox/devs/ws/z-sdk\libc.a {name}
 *
 * 	Alternative is to ensure that the printfx.obj is specified at the start to be linked with !!
 */

#if (xpfSUPPORT_ALIASES == 1)
	int	printf(const char * format, ...) __attribute__((alias("printfx"), unused));
	int	printf_r(const char * format, ...) __attribute__((alias("printfx"), unused));

	int	vprintf(const char * format, va_list vaList) __attribute__((alias("vprintfx"), unused));
	int	vprintf_r(const char * format, va_list vaList) __attribute__((alias("vprintfx"), unused));

	int	sprintf(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused));
	int	sprintf_r(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused));

	int	vsprintf(char * pBuf, const char * format, va_list vaList) __attribute__((alias("vsprintfx"), unused));
	int	vsprintf_r(char * pBuf, const char * format, va_list vaList) __attribute__((alias("vsprintfx"), unused));

	int snprintf(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused));
	int snprintf_r(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused));

	int vsnprintf(char * pBuf, size_t BufSize, const char * format, va_list vaList) __attribute__((alias("vsnprintfx"), unused));
	int vsnprintf_r(char * pBuf, size_t BufSize, const char * format, va_list vaList) __attribute__((alias("vsnprintfx"), unused));

	int fprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused));
	int fprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused));

	int vfprintf(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused));
	int vfprintf_r(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused));

	int dprintf(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused));
	int dprintf_r(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused));

	int vdprintf(int fd, const char * format, va_list vaList) __attribute__((alias("vdprintfx"), unused));
	int vdprintf_r(int fd, const char * format, va_list vaList) __attribute__((alias("vdprintfx"), unused));

	int fiprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused));
	int fiprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused));

	int vfiprintf(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused));
	int vfiprintf_r(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused));
#endif
