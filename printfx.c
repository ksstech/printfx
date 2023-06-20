/*
 * <snf>printfx -  set of routines to replace equivalent printf functionality
 * Copyright (c) 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 *
 * https://codereview.stackexchange.com/questions/219994/register-b-conversion-specifier
 *
 */

#include <math.h>					// isnan()
#include <float.h>					// DBL_MIN/MAX

#include "printfx.h"

#include "hal_variables.h"
#include "struct_union.h"
#include "hal_usart.h"
#include "socketsX.h"
#include "x_ubuf.h"
#include "x_string_general.h"		// xinstring function
#include "x_errors_events.h"
#include "x_terminal.h"
#include "x_utilities.h"

#ifdef ESP_PLATFORM
	#include "esp_log.h"
	#include "esp32/rom/crc.h"		// ESP32 ROM routine
#else
	#include "crc-barr.h"						// Barr group CRC
#endif

#define	debugFLAG					0xE000

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

//FLOAT "Gg"	"Ff"	"Ee"	None
//Dump	None	':'		'-'		Complex
//Other			'!'				'''
enum { form0G, form1F, form2E, form3X };

enum { S_none, S_hh, S_h, S_l, S_ll, S_j, S_z, S_t, S_L, S_XXX };

// ######################## Character and value translation & rounding tables ######################

const u8_t S_bytes[S_XXX] = { sizeof(int), sizeof(char), sizeof(short), sizeof(long), sizeof(long long),
							sizeof(intmax_t), sizeof(size_t), sizeof(ptrdiff_t), sizeof(long double) };

const char vPrintStr1[] = {			// table of characters where lc/UC is applicable
	'B',							// Binary formatted, prepend "0b" or "0B"
	'P',							// pointer formatted, 0x00abcdef or 0X00ABCDEF
	'X',							// hex formatted 'x' or 'X' values, always there
	#if	(xpfSUPPORT_IEEE754 == 1)
	'A', 'E', 'F', 'G',				// float hex/exponential/general
	#endif
	'\0'
};

const char hexchars[] = "0123456789ABCDEF";

// #################################### local only functions #######################################

x64_t x64PrintGetValue(xpc_t * psXPC) {
	x64_t X64;
	switch(psXPC->f.llong) {
	case S_none:
	case S_hh:
	case S_h:
		if (psXPC->f.signval == 1) {
			X64.i64 = (i64_t) va_arg(psXPC->vaList, int);
		} else {
			X64.u64 = (u64_t) va_arg(psXPC->vaList, unsigned int);
		}
		break;
	case S_l:
		if (psXPC->f.signval == 1) {
			X64.i64 = (i64_t) va_arg(psXPC->vaList, i32_t);
		} else {
			X64.u64 = (u64_t) va_arg(psXPC->vaList, u32_t);
		}
		break;
	case S_ll:
		if (psXPC->f.signval) {
			X64.i64 = va_arg(psXPC->vaList, i64_t);
		} else {
			X64.u64 = va_arg(psXPC->vaList, u64_t);
		}
		break;
	case S_z:
		if (psXPC->f.signval) {
			X64.i64 = (i64_t) va_arg(psXPC->vaList, size_t);
		} else {
			X64.u64 = (u64_t) va_arg(psXPC->vaList, size_t);
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
char cPrintNibbleToChar(xpc_t * psXPC, u8_t Value) {
	char cChr = hexchars[Value];
	if (psXPC->f.Ucase == 0 && Value > 9)
		cChr += 0x20;									// convert to lower case
	return cChr;
}

// ############################# Foundation character and string output ############################

/**
 * @brief	output a single character using/via the preselected function
 * 			before output verify against specified width
 * 			adjust character count (if required) & remaining width
 * @param	psXPC - pointer to control structure to be referenced/updated
 * 			c - char to be output
 * @return	1 or 0 based on whether char was '\000'
 */
static int xPrintChar(xpc_t * psXPC, char cChr) {
	int iRV = cChr;
	#if (xpfSUPPORT_FILTER_NUL == 1)
	if (cChr == 0)
		return iRV;
	#endif
	if ((psXPC->f.maxlen == 0) || (psXPC->f.curlen < psXPC->f.maxlen)) {
		iRV = psXPC->handler(psXPC, cChr);
		if (iRV == cChr) {
			psXPC->f.curlen++;							// adjust count
		}
	}
	return iRV;
}

/**
 * @brief	perform a RAW string output to the selected "stream"
 * @brief	Does not perform ANY padding, justification or length checking
 * @param	psXPC - pointer to structure controlling the operation
 * 			string - pointer to the string to be output
 * @return	number of ACTUAL characters output.
 */
static int xPrintChars(xpc_t * psXPC, char * pStr) {
	int iRV = 0, cChr;
	while (*pStr) {
		cChr = xPrintChar(psXPC, *pStr);
		if (cChr != *pStr++)
			return cChr;
		++iRV;
	}
	return iRV;
}

/**
 * @brief	perform formatted output of a string to the preselected device/string/buffer/file
 * @param	psXPC - pointer to structure controlling the operation
 * 			string - pointer to the string to be output
 * @return	number of ACTUAL characters output.
 */
void vPrintString(xpc_t * psXPC, char * pStr) {
	// determine natural or limited length of string
	size_t uLen;
	if (psXPC->f.arg_prec && psXPC->f.arg_width && (psXPC->f.precis < psXPC->f.minwid)) {
		uLen = xstrnlen(pStr, psXPC->f.precis);
	} else {
		uLen = psXPC->f.precis ? psXPC->f.precis : strlen(pStr);
	}
	size_t Lpad = 0, Rpad = 0, Tpad = (psXPC->f.minwid > uLen) ? (psXPC->f.minwid - uLen) : 0;
	u8_t Cpad = psXPC->f.pad0 ? CHR_0 : CHR_SPACE;
	if (Tpad) {
		if (psXPC->f.alt_form) {
			Rpad = Tpad >> 1; Lpad = Tpad - Rpad;
		} else if (psXPC->f.ljust) {
			Rpad = Tpad;
		} else {
			Lpad = Tpad;
		}
	}
	for (;Lpad--; xPrintChar(psXPC, Cpad));
	for (;uLen-- && *pStr; xPrintChar(psXPC, *pStr++));
	for (;Rpad--; xPrintChar(psXPC, Cpad));
}

/**
 * xPrintXxx() convert u64_t value to a formatted string (build right to left)
 * @param	psXPC - pointer to control structure
 * 			ullVal - u64_t value to convert & output
 * 			pBuffer - pointer to buffer for converted string storage
 * 			BufSize - available (remaining) space in buffer
 * @return	number of actual characters output (incl leading '-' and/or ' ' and/or '0' as possibly added)
 * @comment		Honour & interpret the following modifier flags
 * 				'	Group digits in 3 digits groups to left of decimal '.'
 * 				-	Left align the individual numbers between the '.'
 * 				+	Force a '+' or '-' sign on the left
 * 				0	Zero pad to the left of the value to fill the field
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
int	xPrintXxx(xpc_t * psXPC, u64_t u64Val, char * pBuffer, int BufSize) {
	int	Len = 0;
	int Count, iTemp;
	char * pTemp = pBuffer + BufSize - 1;				// Point to last space in buffer
	if (u64Val) {
		#if	(xpfSUPPORT_SCALING > 0)
		if (psXPC->f.alt_form) {
			unsigned int I, F;
			u8_t ScaleChr;
			if (u64Val >= Q1) {
				F = (u64Val % Q1) / q1;
				I = u64Val / Q1;
				ScaleChr = CHR_Q;
			} else if (u64Val >= q1) {
				F = (u64Val % q1) / T1;
				I = u64Val / q1;
				ScaleChr = CHR_q;
			} else if (u64Val >= T1) {
				F = (u64Val % T1) / B1;
				I = u64Val / T1;
				ScaleChr = CHR_T;
			} else if (u64Val >= B1) {
				F = (u64Val % B1) / M1;
				I = u64Val / B1;
				ScaleChr = CHR_B;
			} else if (u64Val >= M1) {
				F = (u64Val % M1) / K1;
				I = u64Val / M1;
				ScaleChr = CHR_M;
			} else if (u64Val >= K1) {
				F = u64Val % K1;
				I = u64Val / K1;
				ScaleChr = CHR_K;
			} else {
				ScaleChr = 0;
			}
			if (ScaleChr) {
				if (psXPC->f.group == 0) {
					*pTemp-- = ScaleChr;
					++Len;
				}
				// calculate & convert the required # of fractional digits
				int Ilen = xDigitsInU32(I, 0);			// 1 (0) -> 3 (999)
				int Flen = xDigitsInU32(F, 0);
	//			RP("I=%u/%d  F=%u/%d ->", I, Ilen, F, Flen);
				if (Ilen != 2 || Flen != 2) {
					Flen = 4 - Ilen;
					F /= u32pow(psXPC->f.nbase, 3 - Flen);
				}
	//			RP(" %u/%d\n", F, Flen);
				while (Flen--) {
					*pTemp-- = cPrintNibbleToChar(psXPC, F % psXPC->f.nbase);	// digit
					++Len;
					F /= psXPC->f.nbase;
				}
				*pTemp-- = psXPC->f.group ? ScaleChr : CHR_FULLSTOP;
				++Len;
				u64Val = (u32_t) I;
			}
		}
		#endif
		// convert to string starting at end of buffer from Least (R) to Most (L) significant digits
		Count = 0;
		while (u64Val) {
			iTemp = u64Val % psXPC->f.nbase;			// calculate the next remainder ie digit
			*pTemp-- = cPrintNibbleToChar(psXPC, iTemp);
			++Len;
			u64Val /= psXPC->f.nbase;
			if (u64Val && psXPC->f.group) {				// handle digit grouping, if required
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
	if (psXPC->f.ljust == 0) {							// right justified (ie pad left) ???
	/* this section ONLY when value is RIGHT justified.
	 * For ' ' padding format is [       -xxxxx]
	 * whilst '0' padding it is  [-0000000xxxxx]
	 */
		Count = (psXPC->f.minwid > Len) ? psXPC->f.minwid - Len : 0;
		// If we are padding with ' ' and leading '+' or '-' is required, do that first
		if (psXPC->f.pad0 == 0 && (psXPC->f.negvalue || psXPC->f.plus)) {	// If a sign is required
			*pTemp-- = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS;	// prepend '+' or '-'
			--Count;
			++Len;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXPC->f.pad0 ? CHR_0 : CHR_SPACE;	// define applicable padding char
			Len += Count;								// Now do the actual padding
			while (Count--)
				*pTemp-- = iTemp;
		}
		// If we are padding with '0' AND a sign is required (-value or +requested), do that first
		if (psXPC->f.pad0 && (psXPC->f.negvalue || psXPC->f.plus)) {	// If +/- sign is required
			if (pTemp[1] == CHR_SPACE || pTemp[1] == CHR_0)
				++pTemp;								// set overwrite last with '+' or '-'
			else
				++Len;									// set to add extra '+' or '-'
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS;
		}
	} else {
		if (psXPC->f.negvalue || psXPC->f.plus) {		// If a sign is required
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS;
			++Len;
		}
	}
	return Len;
}

void vPrintX64(xpc_t * psXPC, u64_t Value) {
	char Buffer[xpfMAX_LEN_X64];
	Buffer[xpfMAX_LEN_X64 - 1] = 0;				// terminate the buffer, single value built R to L
	int Len = xPrintXxx(psXPC, Value, Buffer, xpfMAX_LEN_X64 - 1);
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_X64 - 1 - Len));
}

/**
 * @brief	convert double value based on flags supplied and output via control structure
 * @param	psXPC - pointer to control structure
 * @param	dbl - double value to be converted
 * @return	none
 *
 * References:
 * 	http://git.musl-libc.org/cgit/musl/blob/src/stdio/vfprintf.c?h=v1.1.6
 */
void vPrintF64(xpc_t * psXPC, double F64) {
	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005, 0.000000005,
		0.0000000005, 0.00000000005, 0.000000000005, 0.0000000000005, 0.00000000000005,
		0.000000000000005, 0.0000000000000005 };

	if (isnan(F64)) {
		vPrintString(psXPC, psXPC->f.Ucase ? "NAN" : "nan");
		return;
	} else if (isinf(F64)) {
		vPrintString(psXPC, psXPC->f.Ucase ? "INF" : "inf");
		return;
	}
	psXPC->f.negvalue	= F64 < 0.0 ? 1 : 0;			// set negvalue if < 0.0
	F64	*= psXPC->f.negvalue ? -1.0 : 1.0;				// convert to positive number
	xpf_t xpf;
	xpf.limits	= psXPC->f.limits;						// save original flags
	xpf.flags	= psXPC->f.flags;
	x64_t X64 	= { 0 };

	int	Exp = 0;										// if exponential format requested, calculate the exponent
	if (F64 != 0.0) {									// if not 0 and...
		if (psXPC->f.form != form1F) {					// any of "eEgG" specified ?
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
	u8_t AdjForm = psXPC->f.form!=form0G ? psXPC->f.form : (Exp<-4 || Exp>=psXPC->f.precis) ? form2E : form1F;
	if (AdjForm == form2E)
		F64 = X64.f64;									// change to exponent adjusted value
	if (F64 < (DBL_MAX - round_nums[psXPC->f.precis])) {// if addition of rounding value will NOT cause overflow.
		F64 += round_nums[psXPC->f.precis];			// round by adding .5LSB to the value
	}
	char Buffer[xpfMAX_LEN_F64];
	Buffer[xpfMAX_LEN_F64 - 1] = 0;						// building R to L, ensure buffer NULL-term

	int Len = 0;
	if (AdjForm == form2E) {							// If required, handle the exponent
		psXPC->f.minwid		= 2;
		psXPC->f.pad0		= 1;						// MUST left pad with '0'
		psXPC->f.signval	= 1;
		psXPC->f.ljust		= 0;
		psXPC->f.negvalue	= (Exp < 0) ? 1 : 0;
		Exp *= psXPC->f.negvalue ? -1LL : 1LL;
		Len += xPrintXxx(psXPC, Exp, Buffer, xpfMAX_LEN_F64 - 1);
		Buffer[xpfMAX_LEN_F64-2-Len] = psXPC->f.Ucase ? CHR_E : CHR_e;
		++Len;
	}

	X64.f64	= F64 - (u64_t) F64;						// isolate fraction as double
	X64.f64	= X64.f64 * (double) u64pow(10, psXPC->f.precis);	// fraction to integer
	X64.u64	= (u64_t) X64.f64;							// extract integer portion

	if (psXPC->f.arg_prec) {							// explicit minwid specified ?
		psXPC->f.minwid	= psXPC->f.precis; 			// yes, stick to it.
	} else if (X64.u64 == 0) {							// process 0 value
		psXPC->f.minwid	= psXPC->f.radix ? 1 : 0;
	} else {											// process non 0 value
		if (psXPC->f.alt_form && psXPC->f.form == form0G) {
			psXPC->f.minwid	= psXPC->f.precis;			// keep trailing 0's
		} else {
			u32_t u = u64Trailing0(X64.u64);
			X64.u64 /= u64pow(10, u);
			psXPC->f.minwid	= psXPC->f.precis - u;		// remove trailing 0's
		}
	}
	if (psXPC->f.minwid > 0) {
		psXPC->f.pad0		= 1;						// MUST left pad with '0'
		psXPC->f.group		= 0;						// cannot group in fractional
		psXPC->f.signval	= 0;						// always unsigned value
		psXPC->f.negvalue	= 0;						// and never negative
		psXPC->f.plus		= 0;						// no leading +/- before fractional part
		Len += xPrintXxx(psXPC, X64.u64, Buffer, xpfMAX_LEN_F64-1-Len);
	}
	// process the radix = '.'
	if (psXPC->f.minwid || psXPC->f.radix) {
		Buffer[xpfMAX_LEN_F64 - 2 - Len] = CHR_FULLSTOP;
		++Len;
	}

	X64.u64	= F64;										// extract and convert the whole number portions
	psXPC->f.limits	= xpf.limits;						// restore original limits & flags
	psXPC->f.flags	= xpf.flags;

	// adjust minwid to do padding (if required) based on string length after adding whole number
	psXPC->f.minwid	= psXPC->f.minwid > Len ? psXPC->f.minwid - Len : 0;
	Len += xPrintXxx(psXPC, X64.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len);

	psXPC->f.arg_prec	= 1;
	psXPC->f.precis		= Len;
	psXPC->f.arg_width	= xpf.arg_width;
	psXPC->f.minwid		= xpf.minwid;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_F64 - 1 - Len));
}

void vPrintPointer(xpc_t * psXPC, px_t pX) {
	char caBuf[xpfMAX_LEN_PNTR];
	caBuf[xpfMAX_LEN_PNTR-1] = CHR_NUL;
	psXPC->f.nbase = BASE16;
	psXPC->f.negvalue = 0;
	psXPC->f.plus = 0;									// no leading '+'
	psXPC->f.alt_form = 0;								// disable scaling
	psXPC->f.group = 0;									// disable 3-digit grouping
	psXPC->f.pad0 = 1;
	x64_t X64;
	#if (xpfSIZE_POINTER == 2)
	psXPC->f.minwid = 4;
	X64.u64 = (u16_t) pX.pv;
	#elif (xpfSIZE_POINTER == 4)
	psXPC->f.minwid = 8;
	X64.u64 = (u32_t) pX.pv;
	#elif (xpfSIZE_POINTER == 8)
	psXPC->f.minwid = 16;
	X64.u64 = pX.pv;
	#endif
	int Len = xPrintXxx(psXPC, X64.u64, caBuf, xpfMAX_LEN_PNTR - 1);
	memcpy(&caBuf[xpfMAX_LEN_PNTR - 3 - Len], "0x", 2);
	vPrintString(psXPC, &caBuf[xpfMAX_LEN_PNTR - 3 - Len]);
}

// ############################# Proprietary extension: hexdump ####################################

/**
 * @brief	write char value as 2 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU8(xpc_t * psXPC, u8_t Value) {
	xPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value >> 4));
	xPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value & 0x0F));
}

/**
 * @brief	write u16_t value as 4 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU16(xpc_t * psXPC, u16_t Value) {
	vPrintHexU8(psXPC, (Value >> 8) & 0x000000FF);
	vPrintHexU8(psXPC, Value & 0x000000FF);
}

/**
 * @brief	write u32_t value as 8 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU32(xpc_t * psXPC, u32_t Value) {
	vPrintHexU16(psXPC, (Value >> 16) & 0x0000FFFF);
	vPrintHexU16(psXPC, Value & 0x0000FFFF);
}

/**
 * @brief	write u64_t value as 16 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU64(xpc_t * psXPC, u64_t Value) {
	vPrintHexU32(psXPC, (Value >> 32) & 0xFFFFFFFFULL);
	vPrintHexU32(psXPC, Value & 0xFFFFFFFFULL);
}

/**
 * @brief	write series of char values as hex chars to the buffer, always NULL terminated
 * @param 	psXPC
 * @param 	Num		number of bytes to print
 * @param 	pStr	pointer to bytes to print
 * @comment			Use the following modifier flags
 *					' select grouping separators ":- |" (byte/half/word/dword)
 *					# select reverse order (little/big endian)
 */
void vPrintHexValues(xpc_t * psXPC, int Num, char * pStr) {
	int Size = S_bytes[psXPC->f.llong];
	if (psXPC->f.alt_form)								// invert order ?
		pStr += Num - Size;								// working backwards so point to last

	x64_t x64Val;
	int	Idx	= 0;
	while (Idx < Num) {
		switch (Size >> 1) {
		case 0:
			x64Val.x8[0].u8 = *((u8_t *) pStr);
			vPrintHexU8(psXPC, x64Val.x8[0].u8);

			break;
		case 1:
			x64Val.x16[0].u16 = *((u16_t *) pStr);
			vPrintHexU16(psXPC, x64Val.x16[0].u16);
			break;
		case 2:
			x64Val.x32[0].u32 = *((u32_t *) pStr);
			vPrintHexU32(psXPC, x64Val.x32[0].u32);
			break;
		case 4:
			x64Val.u64 = *((u64_t *) pStr);
			vPrintHexU64(psXPC, x64Val.u64);
			break;
		default:
			assert(0);
		}
		// step to the next 8/16/32/64 bit value
		if (psXPC->f.alt_form)							// invert order ?
			pStr -= Size;								// update source pointer backwards
		else
			pStr += Size;								// update source pointer forwards
		Idx += Size;
		if (Idx >= Num)
			break;
		// now handle the grouping separator(s) if any
		if (psXPC->f.form == form0G)					// no separator required?
			continue;
		xPrintChar(psXPC, psXPC->f.form == form1F ? CHR_COLON :
						  psXPC->f.form == form2E ? CHR_MINUS :
						  psXPC->f.form == form3X ? ( (Idx % 8) == 0 ? CHR_SPACE :
								  	  	  	  	  	  (Idx % 4) == 0 ? CHR_VERT_BAR :
								  	  	  	  	  	  (Idx % 2) == 0 ? CHR_MINUS : CHR_COLON ) :
								  	  	  	  	  	  CHR_QUESTION);
	}
}

/**
 * vPrintHexDump()
 * @brief		Dumps a block of memory in debug style format. depending on options output can be
 * 				formatted as 8/16/32 or 64 bit variables, optionally with no, absolute or relative address
 * @param[in]	psXPC - pointer to print control structure
 * @param[in]	pStr - pointer to memory starting address
 * @param[in]	Len - Length/size of memory buffer to display
 * @param[out]	none
 * @return		none
 * @comment		Use the following modifier flags
 *				'	Grouping of values using ' ' '-' or '|' as separators
 * 				!	Use relative address format
 * 				#	Use absolute address format
 *					Relative/absolute address prefixed using format '0x12345678:'
 * 				+	Add the ASCII char equivalents to the right of the hex output
 */
void vPrintHexDump(xpc_t * psXPC, int xLen, char * pStr) {
	xpf_t sXPF = psXPC->f;
	for (int Now = 0; Now < xLen; Now += xpfHEXDUMP_WIDTH) {
		if (psXPC->f.ljust == 0) {						// display absolute or relative address
			vPrintPointer(psXPC, (px_t) (psXPC->f.rel_val ? (void *) Now : (void *) (pStr + Now)));
			xPrintChars(psXPC, (char *) ": ");
			psXPC->f.flags = sXPF.flags;
		}
		// then the actual series of values in 8-32 bit groups
		int Width = (xLen - Now) > xpfHEXDUMP_WIDTH ? xpfHEXDUMP_WIDTH : xLen - Now;
		vPrintHexValues(psXPC, Width, pStr + Now);
		if (psXPC->f.plus) {							// ASCII equivalent requested?
			int Size = S_bytes[psXPC->f.llong];
			u32_t Count = (xLen <= xpfHEXDUMP_WIDTH) ? 1 :
					((xpfHEXDUMP_WIDTH - Width) / Size) * (Size*2 + (psXPC->f.form ? 1 : 0)) + 1;
			while (Count--)			// handle space padding for ASCII dump to line up
				xPrintChar(psXPC, CHR_SPACE);
			for (Count = 0; Count < Width; ++Count) {	// values as ASCII characters
				int cChr = *(pStr + Now + Count);
				#if 0				// Device supports characters in range 0x80 to 0xFE
				xPrintChar(psXPC, (cChr < CHR_SPACE || cChr == CHR_DEL || cChr == 0xFF) ? CHR_FULLSTOP : cChr);
				#else				// Device does NOT support ANY characters > 0x7E
				xPrintChar(psXPC, (cChr < CHR_SPACE || cChr >= CHR_DEL) ? CHR_FULLSTOP : cChr);
				#endif
			}
			xPrintChar(psXPC, CHR_SPACE);
		}
		if ((Now < xLen) && (xLen > xpfHEXDUMP_WIDTH))
			xPrintChars(psXPC, strCRLF);
	}
}

// ############################# Proprietary extensions: date & time ###############################

seconds_t xPrintCalcSeconds(xpc_t * psXPC, tsz_t * psTSZ, struct tm * psTM) {
	seconds_t	Seconds;
	// Get seconds value to use... (adjust for TZ if required/allowed)
	if (psTSZ->pTZ &&									// TZ info available
		psXPC->f.plus == 1 &&							// TZ info requested
		psXPC->f.alt_form == 0 &&						// NOT alt_form (always GMT/UTC)
		psXPC->f.rel_val == 0) {						// NOT relative time (has no TZ info)
		Seconds = xTimeCalcLocalTimeSeconds(psTSZ);
	} else
		Seconds = xTimeStampAsSeconds(psTSZ->usecs);

	if (psTM)											// convert seconds into components
		xTimeGMTime(Seconds, psTM, psXPC->f.rel_val);
	return Seconds;
}

int	xPrintTimeCalcSize(xpc_t * psXPC, int i32Val) {
	if (!psXPC->f.rel_val || psXPC->f.pad0 || i32Val > 9)
		return psXPC->f.minwid = 2;
	psXPC->f.minwid = 1;
	return xDigitsInI32(i32Val, psXPC->f.group);
}

int	xPrintDate_Year(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 0;
	int Len = xPrintXxx(psXPC, (u64_t) (psTM->tm_year + YEAR_BASE_MIN), pBuffer, 4);
	if (psXPC->f.alt_form == 0)
		pBuffer[Len++] = (psXPC->f.form == form3X) ? CHR_FWDSLASH : CHR_MINUS;
	return Len;
}

int	xPrintDate_Month(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXPC, (u64_t) (psTM->tm_mon + 1), pBuffer, psXPC->f.minwid = 2);
	pBuffer[Len++] = psXPC->f.alt_form ? CHR_SPACE : (psXPC->f.form == form3X) ? CHR_FWDSLASH : CHR_MINUS;
	return Len;
}

int	xPrintDate_Day(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXPC, (u64_t) psTM->tm_mday, pBuffer, xPrintTimeCalcSize(psXPC, psTM->tm_mday));
	pBuffer[Len++] = psXPC->f.alt_form ? CHR_SPACE :
					(psXPC->f.form == form3X && psXPC->f.rel_val) ? CHR_d :
					(psXPC->f.form == form3X) ? CHR_SPACE : CHR_T;
	psXPC->f.pad0 = 1;
	return Len;
}

void vPrintDate(xpc_t * psXPC, struct tm * psTM) {
	int	Len = 0;
	char Buffer[xpfMAX_LEN_DATE];
	psXPC->f.form = psXPC->f.group ? form3X : form0G;
	if (psXPC->f.alt_form) {
		Len += xstrncpy(Buffer, xTimeGetDayName(psTM->tm_wday), 3);		// "Sun"
		Len += xstrncpy(Buffer+Len, ", ", 2);							// "Sun, "
		Len += xPrintDate_Day(psXPC, psTM, Buffer+Len);					// "Sun, 10 "
		Len += xstrncpy(Buffer+Len, xTimeGetMonthName(psTM->tm_mon), 3);// "Sun, 10 Sep"
		Len += xstrncpy(Buffer+Len, " ", 1);							// "Sun, 10 Sep "
		psXPC->f.alt_form = 0;
		Len += xPrintDate_Year(psXPC, psTM, Buffer+Len);				// "Sun, 10 Sep 2017"
	} else {
		if (psXPC->f.rel_val == 0) {					// if epoch value do YEAR+MON
			Len += xPrintDate_Year(psXPC, psTM, Buffer+Len);
			Len += xPrintDate_Month(psXPC, psTM, Buffer+Len);
		}
		if (psTM->tm_mday || psXPC->f.pad0)
			Len += xPrintDate_Day(psXPC, psTM, Buffer+Len);
	}
	Buffer[Len] = 0;									// converted L to R, so terminate
	psXPC->f.limits	= 0;								// enable full string
	vPrintString(psXPC, Buffer);
}

void vPrintTime(xpc_t * psXPC, struct tm * psTM, u32_t uSecs) {
	char Buffer[xpfMAX_LEN_TIME];
	int	Len;
	psXPC->f.form	= psXPC->f.group ? form3X : form0G;
	// Part 1: hours
	if (psTM->tm_hour || psXPC->f.pad0) {
		Len = xPrintXxx(psXPC, (u64_t) psTM->tm_hour, Buffer, xPrintTimeCalcSize(psXPC, psTM->tm_hour));
		Buffer[Len++] = psXPC->f.form == form3X ? CHR_h : CHR_COLON;
		psXPC->f.pad0 = 1;
	} else {
		Len = 0;
	}
	// Part 2: minutes
	if (psTM->tm_min || psXPC->f.pad0) {
		Len += xPrintXxx(psXPC, (u64_t) psTM->tm_min, Buffer+Len, xPrintTimeCalcSize(psXPC, psTM->tm_min));
		Buffer[Len++]	= psXPC->f.form == form3X ? CHR_m :  CHR_COLON;
		psXPC->f.pad0	= 1;
	}

	// Part 3: seconds
	Len += xPrintXxx(psXPC, (u64_t) psTM->tm_sec, Buffer+Len, xPrintTimeCalcSize(psXPC, psTM->tm_sec));
	// Part 4: [.xxxxxx]
	if (psXPC->f.radix && psXPC->f.alt_form == 0) {
		Buffer[Len++]	= psXPC->f.form == form3X ? CHR_s : CHR_FULLSTOP;
		psXPC->f.precis	= (psXPC->f.precis == 0) ? xpfDEF_TIME_FRAC :
						  (psXPC->f.precis > xpfMAX_TIME_FRAC) ? xpfMAX_TIME_FRAC :
						  psXPC->f.precis;
		if (psXPC->f.precis < xpfMAX_TIME_FRAC)
			uSecs /= u32pow(10, xpfMAX_TIME_FRAC - psXPC->f.precis);
		psXPC->f.pad0	= 1;						// need leading '0's
		psXPC->f.signval= 0;
		psXPC->f.ljust	= 0;						// force R-just
		Len += xPrintXxx(psXPC, uSecs, Buffer+Len, psXPC->f.minwid	= psXPC->f.precis);
	} else if (psXPC->f.form == form3X) {
		Buffer[Len++] = CHR_s;
	}

	Buffer[Len] = 0;
	psXPC->f.limits	= 0;							// enable full string
	vPrintString(psXPC, Buffer);
}

void vPrintZone(xpc_t * psXPC, tsz_t * psTSZ) {
	int	Len = 0;
	char Buffer[configTIME_MAX_LEN_TZINFO];
	if (psTSZ->pTZ == 0) {								// If no TZ info supplied
		Buffer[Len++] = CHR_Z;

	} else if (psXPC->f.alt_form) {						// TZ info available but '#' format specified
		Len = xstrncpy(Buffer, (char *) " GMT", 4);	// show as GMT (ie UTC)

	} else {											// TZ info available & '+x:xx(???)' format requested
		#if	(timexTZTYPE_SELECTED == timexTZTYPE_RFC5424)
		if (psXPC->f.plus) {
			psXPC->f.signval = 1;						// TZ hours offset is a signed value
			Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer+Len, psXPC->f.minwid = 3);
			Buffer[Len++] = psXPC->f.form ? CHR_h : CHR_COLON;
			psXPC->f.signval = 0;						// TZ offset minutes unsigned
			psXPC->f.plus = 0;
			Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer+Len, psXPC->f.minwid = 2);
		} else {
			Buffer[Len++]	= CHR_Z;					// add 'Z' for Zulu/zero time zone
		}
		#elif (timexTZTYPE_SELECTED == timexTZTYPE_POINTER)
		psXPC->f.signval	= 1;						// TZ hours offset is a signed value
		psXPC->f.plus		= 1;						// force display of sign
		Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3);
		Buffer[Len++]	= psXPC->f.form ? CHR_h : CHR_COLON;
		psXPC->f.signval	= 0;						// TZ offset minutes unsigned
		psXPC->f.plus		= 0;
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXPC->f.minwid = 2);
		if (psTSZ->pTZ->pcTZName) {						// handle TZ name (as string pointer) if there
			Buffer[Len++]	= CHR_L_ROUND;
			psXPC->f.minwid = 0;						// then complete with the TZ name
			while (psXPC->f.minwid < configTIME_MAX_LEN_TZNAME &&
					psTSZ->pTZ->pcTZName[psXPC->f.minwid] != 0) {
				Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->pcTZName[psXPC->f.minwid];
				psXPC->f.minwid++;
			}
			Len += psXPC->f.minwid;
			Buffer[Len++]	= CHR_R_ROUND;
		}

		#elif (timexTZTYPE_SELECTED == timexTZTYPE_FOURCHARS)
		psXPC->f.signval	= 1;						// TZ hours offset is a signed value
		psXPC->f.plus		= 1;						// force display of sign
		Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3);
		Buffer[Len++]	= psXPC->f.form ? CHR_h : CHR_COLON;
		psXPC->f.signval	= 0;						// TZ offset minutes unsigned
		psXPC->f.plus		= 0;
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXPC->f.minwid = 2);
		// Now handle the TZ name if there, check to ensure max 4 chars all UCase
		if (xstrverify(&psTSZ->pTZ->tzname[0], CHR_A, CHR_Z, configTIME_MAX_LEN_TZNAME) == erSUCCESS) {
			Buffer[Len++]	= CHR_L_ROUND;
			// then complete with the TZ name
			psXPC->f.minwid = 0;
			while ((psXPC->f.minwid < configTIME_MAX_LEN_TZNAME) &&
					(psTSZ->pTZ->tzname[psXPC->f.minwid] != 0) &&
					(psTSZ->pTZ->tzname[psXPC->f.minwid] != CHR_SPACE)) {
				Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->tzname[psXPC->f.minwid];
				psXPC->f.minwid++;
			}
			Len += psXPC->f.minwid;
			Buffer[Len++]	= CHR_R_ROUND;
		}
		#endif
	}
	Buffer[Len]		= 0;								// converted L to R, so add terminating NUL
	psXPC->f.limits	= 0;								// enable full string
	vPrintString(psXPC, Buffer);
}

// ################################# Proprietary extension: URLs ###################################

/**
 * vPrintURL() -
 * @param psXPC
 * @param pString
 */
void vPrintURL(xpc_t * psXPC, char * pStr) {
	if (halCONFIG_inMEM(pStr)) {
		char cIn;
		while ((cIn = *pStr++) != 0) {
			if (INRANGE(CHR_A, cIn, CHR_Z) ||
				INRANGE(CHR_a, cIn, CHR_z) ||
				INRANGE(CHR_0, cIn, CHR_9) ||
				(cIn == CHR_MINUS || cIn == CHR_FULLSTOP || cIn == CHR_UNDERSCORE || cIn == CHR_TILDE)) {
				xPrintChar(psXPC, cIn);
			} else {
				xPrintChar(psXPC, CHR_PERCENT);
				xPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn >> 4));
				xPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn & 0x0F));
			}
		}
	} else {
		vPrintString(psXPC, pStr);						// "null" or "pOOR"
	}
}

// ############################## Proprietary extension: IP address ################################

/**
 * vPrintIpAddress()
 * @brief		Print DOT formatted IP address to destination
 * @param[in]	psXPC - pointer to output & format control structure
 * 				Val - IP address value in HOST format !!!
 * @param[out]	none
 * @return		none
 * @comment		Used the following modifier flags
 * 				'!'		Invert the LSB normal MSB byte order (Network <> Host related)
 * 				'-'		Left align the individual numbers between the '.'
 * 				'0'		Zero pad the individual numbers between the '.'
 */
void vPrintIpAddress(xpc_t * psXPC, u32_t Val) {
	psXPC->f.minwid		= psXPC->f.ljust ? 0 : 3;
	psXPC->f.signval	= 0;							// ensure interpreted as unsigned
	psXPC->f.plus		= 0;							// and no sign to be shown
	u8_t * pChr = (u8_t *) &Val;
	if (psXPC->f.alt_form) {							// '#' specified to invert order ?
		psXPC->f.alt_form = 0;							// clear so not to confuse xPrintXxx()
		u8_t temp;
		temp		= *pChr;
		*pChr		= *(pChr+3);
		*(pChr+3)	= temp;
		temp		= *(pChr+1);
		*(pChr+1)	= *(pChr+2);
		*(pChr+2)	= temp;
	}
	// building R to L, ensure buffer NULL-term
	char Buffer[xpfMAX_LEN_IP];
	Buffer[xpfMAX_LEN_IP - 1] = 0;

	// then convert the IP address, LSB first
	int	Idx, Len;
	for (Idx = 0, Len = 0; Idx < sizeof(Val); ++Idx) {
		u64_t u64Val = (u8_t) pChr[Idx];
		Len += xPrintXxx(psXPC, u64Val, Buffer + xpfMAX_LEN_IP - 5 - Len, 4);
		if (Idx < 3)
			Buffer[xpfMAX_LEN_IP - 1 - ++Len] = CHR_FULLSTOP;
	}
	// then send the formatted output to the correct stream
	psXPC->f.limits	= 0;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_IP - 1 - Len));
}

// ########################### Proprietary extension: SBR attributes ###############################

/**
 * set starting and ending fore/background colors
 * @param psXPC
 * @param Val	U32 value treated as 4x U8 being SGR color/attribute codes
 *
 * XXX: Since we are using the same command handler to process (UART, HTTP & TNET)
 * requests, and we are embedding colour ESC sequences into the formatted output,
 * and the colour ESC sequences (handled by UART & TNET) are not understood by
 * the HTTP protocol, we must try to filter out the possible output produced by
 * the ESC sequences if the output is going to a socket, one way or another.
 */
void vPrintSetGraphicRendition(xpc_t * psXPC, u32_t Val) {
	char Buffer[xpfMAX_LEN_SGR];
	sgr_info_t sSGR;
	sSGR.u32 = Val;
	if (pcANSIlocate(Buffer, sSGR.c, sSGR.d) != Buffer)
		xPrintChars(psXPC, Buffer);
	if (pcANSIattrib(Buffer, sSGR.a, sSGR.b) != Buffer)
		xPrintChars(psXPC, Buffer);
}

/* ################################# The HEART of the PRINTFX matter ###############################
 * PrintFX - common routine for formatted print functionality
 * @brief	parse the format string and interpret the conversions, flags and modifiers
 * 			extract the parameters variables in correct type format
 * 			call the correct routine(s) to perform conversion, formatting & output
 * @param	psXPC - pointer to structure containing formatting and output destination info
 * 			format - pointer to the formatting string
 * @return	void (other than updated info in the original structure passed by reference
 */

int	xPrintFX(xpc_t * psXPC, const char * fmt) {
	fmt = (fmt == NULL) ? "nullPTR" : (*fmt == 0) ? "nullFMT" : fmt;
	for (; *fmt != 0; ++fmt) {
	// start by expecting format indicator
		if (*fmt == CHR_PERCENT) {
			++fmt;
			if (*fmt == CHR_NUL)		break;
			if (*fmt == CHR_PERCENT)	goto out_lbl;
			psXPC->f.flags = 0;							// set ALL flags to default 0
			psXPC->f.limits	= 0;						// reset field specific limits
			psXPC->f.nbase = BASE10;					// default number base
			int	cFmt;
			x32_t X32 = { 0 };
			// Optional FLAGS must be in correct sequence of interpretation
			while ((cFmt = strchr_i("!#'*+- 0", *fmt)) != erFAILURE) {
				switch (cFmt) {
				case 0:	psXPC->f.rel_val = 1; break;	// !	HEXDUMP/DTZ abs->rel address/time, MAC use ':' separator
				case 1:	psXPC->f.alt_form = 1; break;	// #	DTZ=GMT format, HEXDUMP/IP swop endian, STRING centre
				case 2: psXPC->f.group = 1; break;		// '	"diu" add 3 digit grouping, DTZ, MAC, DUMP select separator set
				case 3:									// *	indicate argument will supply field width
					X32.iX	= va_arg(psXPC->vaList, int);
					IF_myASSERT(debugTRACK, psXPC->f.arg_width == 0 && X32.iX <= xpfMINWID_MAXVAL);
					psXPC->f.minwid = X32.iX;
					psXPC->f.arg_width = 1;
					break;
				case 4: psXPC->f.plus = 1; break;		// +	force +/-, HEXDUMP add ASCII, TIME add TZ info
				case 5: psXPC->f.ljust = 1; break;		// -	Left justify, HEXDUMP remove address pointer
				case 6:	psXPC->f.Pspc = 1; break;		//  	' ' instead of '+'
				case 7:	psXPC->f.pad0 = 1; break;		// 0	force leading '0's
				default: assert(0);
				}
				++fmt;
			}

			// Optional WIDTH and PRECISION indicators
			if (*fmt == CHR_FULLSTOP || INRANGE(CHR_0, *fmt, CHR_9)) {
				X32.i32 = 0;
				while (1) {
					if (INRANGE(CHR_0, *fmt, CHR_9)) {
						X32.i32 *= 10;
						X32.i32 += *fmt - CHR_0;
						++fmt;
					} else if (*fmt == CHR_FULLSTOP) {
						IF_myASSERT(debugTRACK, psXPC->f.radix == 0);	// 2x radix not allowed
						++fmt;
						psXPC->f.radix = 1;
						if (X32.i32 > 0) {
							IF_myASSERT(debugTRACK, psXPC->f.arg_width == 0 && X32.iX <= xpfMINWID_MAXVAL);
							psXPC->f.minwid = X32.iX;
							psXPC->f.arg_width = 1;
							X32.i32 = 0;
						}
					} else if (*fmt == CHR_ASTERISK) {
						IF_myASSERT(debugTRACK, psXPC->f.radix == 1 && X32.iX == 0);
						++fmt;
						X32.iX	= va_arg(psXPC->vaList, int);
						IF_myASSERT(debugTRACK, X32.iX <= xpfPRECIS_MAXVAL);
						psXPC->f.precis = X32.iX;
						psXPC->f.arg_prec = 1;
						X32.i32 = 0;
					} else {
						break;
					}
				}
				// Save possible parsed value
				if (X32.i32 > 0) {
					if (psXPC->f.arg_width == 0 && psXPC->f.radix == 0) {
						IF_myASSERT(debugTRACK, X32.iX <= xpfMINWID_MAXVAL);
						psXPC->f.minwid	= X32.iX;
						psXPC->f.arg_width = 1;
					} else if (psXPC->f.arg_prec == 0 && psXPC->f.radix == 1) {
						IF_myASSERT(debugTRACK, X32.iX <= xpfPRECIS_MAXVAL);
						psXPC->f.precis	= X32.iX;
						psXPC->f.arg_prec = 1;
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
						psXPC->f.llong = S_hh;
						++fmt;
					} else {							// 'h'
						psXPC->f.llong = S_h;
					}
					break;
				case 1:
					if (*fmt != CHR_l) {
						psXPC->f.llong = S_l;			// 'l'
					} else {
						psXPC->f.llong = S_ll;			// "ll"
						++fmt;
					}
					break;
				case 2: psXPC->f.llong = S_j; break;	// [u]intmax_t[*]
				case 3: psXPC->f.llong = S_z; break;	// [s]size_t[*]
				case 4: psXPC->f.llong = S_t; break;	// ptrdiff[*]
				case 5: psXPC->f.llong = S_L; break;	// long double float (F128)
				default: assert(0);
				}
			}
			IF_myASSERT(debugTRACK, psXPC->f.llong < S_XXX);	// rest not yet supported
			// Check if format character where UC/lc same character control the case of the output
			cFmt = *fmt;
			if (strchr_i(vPrintStr1, cFmt) != erFAILURE) {
				cFmt |= 0x20;							// convert to lower case, but ...
				psXPC->f.Ucase = 1;						// indicate as UPPER case requested
			}

			x64_t X64;								// default x64 variable
			px_t pX;
			#if	(xpfSUPPORT_DATETIME == 1)
			tsz_t * psTSZ;
			struct tm sTM;
			#endif

			switch (cFmt) {
			#if	(xpfSUPPORT_SGR == 1)
			case CHR_C: vPrintSetGraphicRendition(psXPC, va_arg(psXPC->vaList, u32_t)); break;
			#endif

			#if	(xpfSUPPORT_IP_ADDR == 1)				// IP address
			case CHR_I: vPrintIpAddress(psXPC, va_arg(psXPC->vaList, u32_t)); break;
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
			case CHR_Z:				// Local TZ DATE+TIME+ZONE
				IF_myASSERT(debugTRACK, !psXPC->f.rel_val && !psXPC->f.group);
				psTSZ = va_arg(psXPC->vaList, tsz_t *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(psTSZ));
				psXPC->f.pad0 = 1;
				X32.u32 = xTimeStampAsSeconds(psTSZ->usecs);
				xpf_t sXPF = { 0 };
				sXPF.flags = psXPC->f.flags;
				// If full local time required, add TZ and DST offsets
				if (psXPC->f.plus && psTSZ->pTZ)
					X32.u32 += psTSZ->pTZ->timezone + (int) psTSZ->pTZ->daylight;
				xTimeGMTime(X32.u32, &sTM, psXPC->f.rel_val);
				psXPC->f.plus = 0;
				if (cFmt == CHR_D || cFmt == CHR_Z)
					vPrintDate(psXPC, &sTM);
				if (cFmt == CHR_T || cFmt == CHR_Z)
					vPrintTime(psXPC, &sTM, (u32_t)(psTSZ->usecs % MICROS_IN_SECOND));
				if (cFmt == CHR_Z) {
					psXPC->f.plus = sXPF.plus;
					vPrintZone(psXPC, psTSZ);
				}
				break;

			case CHR_R:				// U64 epoch (yr+mth+day) OR relative (days) + TIME
				IF_myASSERT(debugTRACK, !psXPC->f.plus && !psXPC->f.pad0 && !psXPC->f.group);
				X64.u64 = va_arg(psXPC->vaList, u64_t);
				if (psXPC->f.alt_form) {
					psXPC->f.group = 1;
					psXPC->f.alt_form = 0;
				}
				X32.u32 = xTimeStampAsSeconds(X64.u64);
				xTimeGMTime(X32.u32, &sTM, psXPC->f.rel_val);
				if (psXPC->f.rel_val == 0)
					psXPC->f.pad0 = 1;
				if (psXPC->f.rel_val == 0 || sTM.tm_mday) {
					u32_t limits = psXPC->f.limits;
					vPrintDate(psXPC, &sTM);
					psXPC->f.limits = limits;
				}
				vPrintTime(psXPC, &sTM, (u32_t) (X64.u64 % MICROS_IN_SECOND));
				if (psXPC->f.rel_val == 0)
					xPrintChar(psXPC, CHR_Z);
				break;
			#endif

			#if	(xpfSUPPORT_URL == 1)					// para = pointer to string to be encoded
			case CHR_U:
				pX.pc8 = va_arg(psXPC->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				vPrintURL(psXPC, pX.pc8);
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
				IF_myASSERT(debugTRACK, !psXPC->f.arg_width && !psXPC->f.arg_prec);
				psXPC->f.llong = S_hh;					// force interpretation as sequence of U8 values
				psXPC->f.form = psXPC->f.group ? form1F : form0G;
				pX.pc8 = va_arg(psXPC->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				vPrintHexValues(psXPC, lenMAC_ADDRESS, pX.pc8);
				break;
			#endif

			#if	(xpfSUPPORT_HEXDUMP == 1)
			case CHR_Y:									// HEXDUMP
				IF_myASSERT(debugTRACK, !psXPC->f.arg_width && !psXPC->f.arg_prec);
				/* In order for formatting to work  the "*" or "." radix specifiers
				 * should not be used. The requirement for a second parameter is implied and assumed */
				psXPC->f.form = psXPC->f.group ? form3X : form0G;
				X32.iX	= va_arg(psXPC->vaList, int);
				pX.pc8	= va_arg(psXPC->vaList, char *);
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(pX.pc8));
				vPrintHexDump(psXPC, X32.iX, pX.pc8);
				break;
			#endif

			/* convert unsigned 32/64 bit value to 1/0 ASCI string
			 * field width specifier is applied as mask starting from LSB to MSB
			 * '#'	change prepended "0b" to "0B"
			 * '''	Group digits using '|' (bytes) and '-' (nibbles)
			 */
			case CHR_b:
				{
					X64 = x64PrintGetValue(psXPC);
					psXPC->f.group = 0;
					X32.iX = S_bytes[psXPC->f.llong] * BITS_IN_BYTE;
					if (psXPC->f.minwid)
						X32.iX = (psXPC->f.minwid > X32.iX) ? X32.iX : psXPC->f.minwid;
					u64_t mask = 1ULL << (X32.iX - 1);
					psXPC->f.form = psXPC->f.group ? form3X : form0G;
					xPrintChars(psXPC, psXPC->f.alt_form ? "0B" : "0b");
					while (mask) {
						xPrintChar(psXPC, (X64.u64 & mask) ? CHR_1 : CHR_0);
						mask >>= 1;
						// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
						if (--X32.iX && psXPC->f.form && (X32.iX % 4) == 0) {
							xPrintChar(psXPC, X32.iX % 32 == 0 ? CHR_VERT_BAR :		// word boundary
											  X32.iX % 16 == 0 ? CHR_COLON :			// short boundary
										  	  X32.iX % 8 == 0 ? CHR_SPACE : CHR_MINUS);// byte boundary or nibble
						}
					}
				}
				break;

			case CHR_c: xPrintChar(psXPC, va_arg(psXPC->vaList, int)); break;

			case CHR_d:									// signed decimal "[-]ddddd"
			case CHR_i:									// signed integer (same as decimal ?)
				psXPC->f.signval = 1;
				X64 = x64PrintGetValue(psXPC);
				if (X64.i64 < 0LL) {
					psXPC->f.negvalue = 1;
					X64.i64 *= -1; 					// convert the value to unsigned
				}
				vPrintX64(psXPC, X64.u64);
				break;

			#if	(xpfSUPPORT_IEEE754 == 1)
//			case CHR_a:									// HEX format not yet supported
			case CHR_e: ++psXPC->f.form;				// form = 2
				/* FALLTHRU */ /* no break */
			case CHR_f:	++psXPC->f.form;				// form = 1
				/* FALLTHRU */ /* no break */
			case CHR_g:									// form = 0
				psXPC->f.signval = 1;					// float always signed value.
				/* https://en.cppreference.com/w/c/io/fprintf
				 * https://pubs.opengroup.org/onlinepubs/007908799/xsh/fprintf.html
				 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160
				 */
				if (psXPC->f.arg_prec) {				// explicit precision specified ?
					psXPC->f.precis = psXPC->f.precis > xpfMAXIMUM_DECIMALS ? xpfMAXIMUM_DECIMALS : psXPC->f.precis;
				} else {
					psXPC->f.precis	= xpfDEFAULT_DECIMALS;
				}
				vPrintF64(psXPC, va_arg(psXPC->vaList, f64_t));
				break;
			#endif

			case CHR_m: pX.pc8 = strerror(errno); goto commonM_S;

			case CHR_n:									// store chars to date at location.
				pX.piX = va_arg(psXPC->vaList, int *);
				IF_myASSERT(debugTRACK, halCONFIG_inSRAM(pX.pc8));
				*pX.piX = psXPC->f.curlen;
				break;

			case CHR_o:									// unsigned octal "ddddd"
			case CHR_x: psXPC->f.group = 0;				// hex as in "789abcd" UC/LC, disable grouping
				/* FALLTHRU */ /* no break */
			case CHR_u:									// unsigned decimal "ddddd"
				psXPC->f.nbase = (cFmt == CHR_x) ? BASE16 : (cFmt == CHR_u) ? BASE10 : BASE08;
				X64 = x64PrintGetValue(psXPC);
				// Ensure sign-extended bits removed
				int Wid = psXPC->f.llong==S_hh ? 7 : psXPC->f.llong==S_h ? 15 : psXPC->f.llong==S_l ? 31 : 63;
				X64.u64 &= BIT_MASK64(0, Wid);
				vPrintX64(psXPC, X64.u64);
				break;

			case CHR_p:
				pX.pv = va_arg(psXPC->vaList, void *);
				vPrintPointer(psXPC, pX);
				break;

			case CHR_s: pX.pc8 = va_arg(psXPC->vaList, char *);

			commonM_S:
				// Required to avoid crash when wifi message is intercepted and a string pointer parameter
				// is evaluated as out of valid memory address (0xFFFFFFE6). Replace string with "pOOR"
				pX.pc8 = halCONFIG_inMEM(pX.pc8) ? pX.pc8 : (pX.pc8 == NULL) ? strNULL : strOOR;
				vPrintString(psXPC, pX.pc8);
				break;

			default:
				/* At this stage we have handled the '%' as assumed, but the next character found is invalid.
				 * Show the '%' we swallowed and then the extra, invalid, character as well */
				xPrintChar(psXPC, CHR_PERCENT);
				xPrintChar(psXPC, *fmt);
				break;
			}
			psXPC->f.plus = 0;							// reset this form after printing one value
		} else {
out_lbl:
			xPrintChar(psXPC, *fmt);
		}
	}
	return psXPC->f.curlen;
}

int	xPrintF(int (Hdlr)(xpc_t *, int), void * pVoid, size_t szBuf, const char * fmt, va_list vaList) {
	xpc_t sXPC;
	sXPC.handler	= Hdlr;
	sXPC.pVoid		= pVoid;
	sXPC.f.maxlen	= (szBuf > xpfMAXLEN_MAXVAL) ? xpfMAXLEN_MAXVAL : szBuf;
	sXPC.f.curlen	= 0;
	sXPC.vaList		= vaList;
	return xPrintFX(&sXPC, fmt);
}

// ##################################### Destination = STRING ######################################

int	xPrintToString(xpc_t * psXPC, int cChr) {
	if (psXPC->pStr)
		*psXPC->pStr++ = cChr;
	return cChr;
}

int vsnprintfx(char * pBuf, size_t szBuf, const char * format, va_list vaList) {
	if (szBuf == 1) {									// any "real" space ?
		if (pBuf)										// no, buffer supplied?
			*pBuf = 0;									// yes, terminate
		return 0; 										// & return
	}
	int iRV = xPrintF(xPrintToString, pBuf, szBuf, format, vaList);
	if (pBuf) {											// buffer specified ?
		if (iRV == szBuf) 								// yes, but full?
			--iRV;										// make space for terminator
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

// ################################### Destination = STDOUT ########################################

SemaphoreHandle_t printfxMux = NULL;

/**
 * Locks the STDOUT semaphore and sets the status/tracking flag
 */
void printfx_lock(void) { xRtosSemaphoreTake(&printfxMux, portMAX_DELAY); }

/**
 * Unlock the STDOUT semaphore and clears the status/tracking flag
 */
void printfx_unlock(void) { xRtosSemaphoreGive(&printfxMux); }

int xPrintStdOut(xpc_t * psXPC, int cChr) {
#if	defined(ESP_PLATFORM)
	return putcharX(cChr, configSTDIO_UART_CHAN);
#else
	return putchar(cChr);
#endif
}

int vnprintfx(size_t szLen, const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xPrintF(xPrintStdOut, NULL, szLen, format, vaList);
	printfx_unlock();
	return iRV;
}

int nprintfx(size_t szLen, const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	printfx_lock();
	int iRV = xPrintF(xPrintStdOut, NULL, szLen, format, vaList);
	printfx_unlock();
	va_end(vaList);
	return iRV;
}

int vprintfx(const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	return iRV;
}

int printfx(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	printfx_lock();
	int iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	va_end(vaList);
	return iRV;
}

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

/* ################################# Destination - String buffer or STDOUT #########################
 * * Based on the values (pre) initialised for buffer start and size
 * a) walk through the buffer on successive calls, concatenating output; or
 * b) output directly to stdout if buffer pointer/size not initialized.
 * Because the intent in this function is to provide a streamlined method for
 * keeping track of buffer usage over a series of successive printf() type calls
 * no attempt is made to control access to the buffer or output channel as such.
 * It is the responsibility of the calling function to control (un/lock) access.
 */

int	wprintfx(report_t * psRprt, const char * pcFormat, ...) {
	va_list vaList;
	va_start(vaList, pcFormat);
	int iRV;
	if (psRprt && psRprt->pcBuf && psRprt->Size) {
		IF_myASSERT(debugPARAM, halCONFIG_inSRAM(psRprt) && halCONFIG_inSRAM(psRprt->pcBuf));
		iRV = vsnprintfx(psRprt->pcBuf, psRprt->Size, pcFormat, vaList);
		if (iRV > 0) {
			IF_myASSERT(debugRESULT, iRV <= psRprt->Size);
			psRprt->pcBuf += iRV;
			psRprt->Size -= iRV;
		}
	} else {
		iRV = xPrintF(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, pcFormat, vaList);
	}
	va_end(vaList);
	return iRV;
}

// ################################### Destination = FILE PTR ######################################

int	xPrintToFile(xpc_t * psXPC, int cChr) { return fputc(cChr, psXPC->stream); }

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

int	xPrintToHandle(xpc_t * psXPC, int cChr) {
	char cChar = cChr;
	int size = write(psXPC->fd, &cChar, sizeof(cChar));
	return size == 1 ? cChr : size;
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

int	xPrintToConsole(xpc_t * psXPC, int cChr) { return putcharRT(cChr); }

int vcprintfx(const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xPrintF(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	return iRV;
}

int cprintfx(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
//	printfx_lock();
	int iRV = xPrintF(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList);
//	printfx_unlock();
	va_end(vaList);
	return iRV;
}

// ################################### Destination = DEVICE ########################################

int	xPrintToDevice(xpc_t * psXPC, int cChr) { return psXPC->DevPutc(cChr); }

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

int	xPrintToSocket(xpc_t * psXPC, int cChr) {
	u8_t cBuf = cChr;
	int iRV = xNetSend(psXPC->psSock, &cBuf, sizeof(cBuf));
	if (iRV != sizeof(cBuf))
		return iRV;
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

int	xPrintToUBuf(xpc_t * psXPC, int cChr) { return xUBufPutC(psXPC->psUBuf, cChr); }

int	vuprintfx(ubuf_t * psUBuf, const char * format, va_list vaList) {
	return xPrintF(xPrintToUBuf, psUBuf, xUBufSpace(psUBuf), format, vaList);
}

int	uprintfx(ubuf_t * psUBuf, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int count = vuprintfx(psUBuf, format, vaList);
	va_end(vaList);
	return count;
}

// #################################### Destination : CRC32 ########################################

int xPrintToCRC32(xpc_t * psXPC, int cChr) {
	#if defined(ESP_PLATFORM)							// use ROM based CRC lookup table
	u8_t cBuf = cChr;
	*psXPC->pU32 = crc32_le(*psXPC->pU32, &cBuf, sizeof(cBuf));
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
