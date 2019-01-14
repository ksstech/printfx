/*
 * Copyright 2014-18 Andre M Maree / KSS Technologies (Pty) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * x<snf>printf -  set of routines to replace equivalent printf functionality
 * Date			Ver		Author	Comments
 * 2015/01/17	2.00	AMM		Change output functionality to use pointer to a routine
 * 2015/01/23	2.10	AMM		Fixed length return to exclude the terminating '\000'
 * 2015/03/20	2.20	AMM		Moved wrapper function for circular buffer xcbprintf here
 * 								Rearranged conditionals to group wrapper functions where belong
 * 2015/05/03	2.30	AMM		simplified format string parsing structure
 * 								incorporated all flags in psXPC structure
 * 								removed need to pass all parameters to sub-functions
 * 								Added octal decoding
 * 2015/08/28	2.31	AMM		Added Binary string, IP & MAC address support.
 * 2015/08/30	2.32	AMM		Added debug style dump with variable 8/16/32/64 bit value groupings
 * 								Multiple separator options, none/absolute/relative address display
 * 2015/10/01	2.40	AMM		Added date, time and date/time/zone format selectors
 * 2015/11/01	2.41	AMM		Added relative time & date modifier
 * 2015/11/21	2.42	AMM		Added alternative date/time separators "--T' -> "// " and "::[.Z]" -> "hm[sm]"
 * 2016/01/30	2.50	AMM		Added exponential format conversion specifier(s)
 * 2017/01/05	2.60	AMM		Complete change in handling of output re/direction
 * 								Circular buffer support replace with a simpler linear buffer (circular functionality to be added)
 * 								Added v[snf]printf functionality
 *
 * History:
 * --------
 *	Originally used source as per George Menie and Daniel D Miller.
 *	Converted to be used with the TI compiler for the CC3200
 *	Started complete rewrite mid 2014 when need for retargeting via ITM, UART or RTT was required.
 */

#include	"x_config.h"								// brings x_time.h with xTime_GMTime() function
#include	"x_printf.h"
#include	"x_debug.h"									// need ASSERT, bring x_printf with
#include	"x_string_general.h"						// xinstring function
#include	"x_errors_events.h"
#include	"x_retarget.h"

#include	"hal_nvic.h"

#include	<stdlib.h>
#include	<string.h>
#include	<math.h>									// isnan()
#include	<float.h>									// DBL_MIN/MAX

#define	debugFLAG				(0x4000)

#define	debugPARAM				(debugFLAG & 0x4000)
#define	debugRESULT				(debugFLAG & 0x8000)

// ######################## Character and value translation & rounding tables ######################

const char vPrintStr1[] = {
		'X',									// hex formatted 'x' or 'X' values, always there
#if		(xpfSUPPORT_HEXDUMP == 1)
		'B', 'H', 'W',							// byte, half and word UC/LC formatting
#endif
#if		(xpfSUPPORT_MAC_ADDR == 1)
		'M',									// MAC address UC/LC
#endif
#if		(xpfSUPPORT_IEEE754 == 1)
		'E', 'G',
#endif
#if		(xpfSUPPORT_POINTER == 1)
		'P',
#endif
		'\0' } ;								// terminated

static	const char hexchars[] = "0123456789ABCDEF" ;

static	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
	0.5, 			0.05, 			0.005, 			0.0005,			0.00005, 			0.000005, 			0.0000005, 			0.00000005,
	0.000000005,	0.0000000005, 	0.00000000005, 	0.000000000005,	0.0000000000005, 	0.00000000000005, 	0.000000000000005, 	0.0000000000000005
} ;

// ############################# Foundation character and string output ############################

/*
 * vPrintChar
 * \brief	output a single character using/via the preselected function
 * 			before output verify against specified width
 * 			adjust character count (if required) & remaining width
 * \param	psXPC - pointer to xprintf control structure to be referenced/updated
 * 			c - char to be output
 * \return	1 or 0 based on whether char was '\000'
 */
void	vPrintChar(xpc_t * psXPC, char cChr) {
	if ((psXPC->f.maxlen == 0) || (psXPC->f.curlen < psXPC->f.maxlen)) {
#if 	(xpfSUPPORT_FILTER_NUL == 1)
		if (cChr == CHR_NUL) {
			return ;
		}
#endif
		if (psXPC->handler(psXPC, cChr) == cChr) {
			psXPC->f.curlen++ ;							// adjust the count
		}
	}
}

/*
 * xPrintChars()
 * \brief	perform a RAW string output to the selected "stream"
 * \brief	Does not perform ANY padding, justification or length checking
 * \param	psXPC - pointer to xprintf_t structure controlling the operation
 * 			string - pointer to the string to be output
 * \return	number of ACTUAL characters output.
 */
int32_t	xPrintChars (xpc_t * psXPC, char * pStr) {
	int32_t	len = 0 ;
	while (*pStr) {
		vPrintChar(psXPC, *pStr++) ;
		++len ;
	}
	return len ;
}

/*
 * vPrintString
 * \brief	perform formatted output of a string to the preselected device/string/buffer/file
 * \param	psXPC - pointer to xprintf_t structure controlling the operation
 * 			string - pointer to the string to be output
 * \return	number of ACTUAL characters output.
 */
void	vPrintString (xpc_t * psXPC, char * pStr) {
	int32_t Len = 0 ;
	pStr = (pStr == NULL) ? (char *) STRING_NULL :
			INRANGE_MEM(pStr) ? pStr : (char *) STRING_OOR ;
	for (char * ptr = pStr; *ptr ; ++ptr) {
		Len++ ;											// calculate actual string length
	}
	int32_t PadLen ;
	if (psXPC->f.minwid > Len) {
		PadLen = psXPC->f.minwid - Len ;				// calc required number of padding chars
	} else {
		PadLen = 0 ;
	}
	// handle padding on left (ie right justified)
	int32_t padchar = psXPC->f.pad0 ? CHR_0 : CHR_SPACE ;
	if ((psXPC->f.ljust == 0) && PadLen) {				// If right justified & we must pad
		while (PadLen--) {
			vPrintChar(psXPC, padchar) ;				// do left pad
		}
	}
	// output the actual string
	psXPC->f.precision = psXPC->f.precision ? psXPC->f.precision : Len ;
	while (*pStr && psXPC->f.precision--) {
		vPrintChar(psXPC, *pStr++) ;
	}
	// handle padding on right (ie left justified)
	if ((psXPC->f.ljust == 1) && PadLen) {				// If left justified & we must pad
		while (PadLen--) {
			vPrintChar(psXPC, padchar) ;				// do right pad
		}
	}
}

/*
 * cPrintNibbleToChar()
 * \brief		returns a pointer to the hexadecimal character representing the value of the low order nibble
 * 				Based on the Flag, the pointer will be adjusted for UpperCase (if required)
 * \param[in]	Val = value in low order nibble, to be converted
 * \return		pointer to the correct character
 */
char	cPrintNibbleToChar(xpc_t * psXPC, uint8_t Value) {
	IF_myASSERT(debugPARAM, Value < 0x10) ;
	char cChr = hexchars[Value] ;
	if ((psXPC->f.Ucase == 0) && (Value > 9)) {
		cChr += 0x20 ;									// convert to lower case
	}
	return cChr ;
}

/*
 * xPrintXxx() convert uint64_t value to a formatted string
 * \param	psXPC - pointer to control structure
 * 			ullVal - uint64_t value to convert & output
 * 			pBuffer - pointer to buffer for converted string storage
 * 			BufSize - available (remaining) space in buffer
 * \return	number of actual characters output (incl leading '-' and/or ' ' and/or '0' as possibly added)
 * \comment		Honour & interpret the following modifier flags
 * 				'`'		Group digits in 3 digits groups to left of decimal '.'
 * 				'-'		Left align the individual numbers between the '.'
 * 				'+'		Force a '+' or '-' sign on the left
 * 				'0'		Zero pad to the left of the value to fill the field
 */
int32_t	xPrintXxx(xpc_t * psXPC, uint64_t ullVal, char * pBuffer, size_t BufSize) {
	IF_myASSERT(debugPARAM, INRANGE_SRAM(pBuffer) && (BufSize > 0)) ;
	int32_t	Len, iTemp, Count ;
	// Set pointer to end of buffer
	char * pTemp = pBuffer + BufSize - 1 ;
	// convert to string starting at end of buffer from Least (R) to Most (L) significant digits
	if (ullVal) {
		Len = Count = 0 ;
		while (ullVal) {
		// calculate the next remainder ie digit
			iTemp = ullVal % psXPC->f.nbase ;
			*pTemp-- = cPrintNibbleToChar(psXPC, iTemp) ;
			Len++ ;
			ullVal /= psXPC->f.nbase ;
		// handle digit grouping, if required
			if (ullVal && psXPC->f.group) {
				if ((++Count % 3) == 0) {
					*pTemp-- = CHR_COMMA ;
					Len++ ;
					Count = 0 ;
				}
			}
		}
	} else {
		*pTemp-- = CHR_0 ;
		Len = 1 ;
	}
// First check if ANY form of padding required
	if (!psXPC->f.ljust) {								// right justified (ie pad left) ???
	/* this section ONLY when value is RIGHT justified.
	 * For ' ' padding format is [       -xxxxx]
	 * whilst '0' padding it is  [-0000000xxxxx] */
		Count = (psXPC->f.minwid > Len) ? psXPC->f.minwid - Len : 0 ;
	// If we are padding with ' ' and a signed is required, do that first
		if ((psXPC->f.pad0 == 0) && (psXPC->f.negvalue || psXPC->f.plus)) {	// If a sign is required
			*pTemp-- = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;				// start by prepend of '+' or '-'
			Count-- ;
			Len++ ;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXPC->f.pad0 ? CHR_0 : CHR_SPACE ;// define applicable padding char
			Len += Count ;								// Now do the actual padding
			while (Count--) {
				*pTemp-- = iTemp ; ;
			}
		}
	// If we are padding with ' ' AND a sign is required (-value or +requested), do that first
		if (psXPC->f.pad0 && (psXPC->f.negvalue || psXPC->f.plus)) {		// If a signed is required
			if (*(pTemp+1) == CHR_SPACE || *(pTemp+1) == CHR_0) {
				pTemp++ ;								// set overwrite last with '+' or '-'
			} else {
				Len++ ;									// set to add extra '+' or '-'
			}
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;
		}
	} else {
		if (psXPC->f.negvalue || psXPC->f.plus) {		// If a sign is required
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;	// just prepend '+' or '-'
			Len++ ;
		}
	}
	return Len ;
}

void	vPrintX64(xpc_t * psXPC, uint64_t Value) {
	char 	Buffer[xpfMAX_LEN_X64] ;
	Buffer[xpfMAX_LEN_X64 - 1] = CHR_NUL ;				// terminate the buffer, single value built R to L
// then do conversion and print
	int32_t Len = xPrintXxx(psXPC, Value, Buffer, xpfMAX_LEN_X64 - 1) ;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_X64 - 1 - Len)) ;
}

/*
 * vPrintF64()
 * \brief	convert double value based on flags supplied and output via control structure
 * \param[in]	psXPC - pointer to xprintf control structure
 * \param[in]	dbl - double value to be converted
 * \return		none
 *
 * References:
 * 	http://git.musl-libc.org/cgit/musl/blob/src/stdio/vfprintf.c?h=v1.1.6
 */
void	vPrintF64(xpc_t * psXPC, double f64Val) {
	char	Buffer[xpfMAX_LEN_F64] ;
	int32_t idx, Exponent, Len = 0 ;
	psXPC->f.negvalue	= f64Val < 0.0 ? 1 : 0 ;		// set negvalue if < 0.0
	f64Val				*= psXPC->f.negvalue ? -1.0 : 1.0 ;	// convert to positive number
	xpf_t		xpf ;
	xpf.limits			= psXPC->f.limits ;				// save original flags
	xpf.flags			= psXPC->f.flags ;
	x64_t x64Value 		= { 0 } ;
// if exponential format requested, calculate the exponent
	Exponent = 0 ;
	if (f64Val != 0.0) {								// if not 0 and...
		if (psXPC->f.form != xpfFORMAT_1_F) {			// any of "eEgG" specified ?
			x64Value.f64	= f64Val ;
			while (x64Value.f64 > 10.0) {				// if number is greater that 10.0
				x64Value.f64 /= 10.0 ;					// re-adjust and
				Exponent += 1 ;							// update exponent
			}
			while (x64Value.f64 < 1.0) {				// similarly, if number smaller than 1.0
				x64Value.f64 *= 10.0 ;					// re-adjust upwards and
				Exponent	-= 1 ;						// update exponent
			}
		}
	}

// if 'g' or 'G' specified check the exponent range and select applicable mode.
	if (psXPC->f.form == xpfFORMAT_0_G) {
		idx = (psXPC->f.precision == 0) ? 1 : psXPC->f.precision ;
		if ((idx > Exponent) && (Exponent >= -4)) {
			psXPC->f.form = xpfFORMAT_1_F ;			// force to fixed point format
			psXPC->f.precision = idx - (Exponent + 1) ;
		} else {
			psXPC->f.form = xpfFORMAT_2_E ;			// force to exponential format
			psXPC->f.precision = idx - 1 ;
		}
	}
// based on the format to be used, change the value if exponent format
	if (psXPC->f.form == xpfFORMAT_2_E) {
		f64Val = x64Value.f64 ;							// change to exponent adjusted value
	}
// do rounding based on precision, only if addition of rounding value will NOT cause overflow.
	if (f64Val < (DBL_MAX - round_nums[psXPC->f.precision])) {
		f64Val += round_nums[psXPC->f.precision] ;		// round by adding .5LSB to the value
	}
// building R to L, ensure buffer NULL-term
	Buffer[xpfMAX_LEN_F64 - 1] = CHR_NUL ;

// If required, handle the exponent
	if (psXPC->f.form == xpfFORMAT_2_E) {
		psXPC->f.minwid	= 2 ;
		psXPC->f.pad0		= 1 ;						// MUST left pad with '0'
		psXPC->f.signed_val= 1 ;
		psXPC->f.ljust		= 0 ;
		psXPC->f.negvalue	= (Exponent < 0) ? 1 : 0 ;
		Exponent *= psXPC->f.negvalue ? -1LL : 1LL ;
		Len += xPrintXxx(psXPC, Exponent, Buffer, (xpfMAX_LEN_F64 - 1)) ;
		*(Buffer + (xpfMAX_LEN_F64 - 2 - Len++))	= psXPC->f.Ucase ? 'E' : 'e' ;
	}

// convert the fractional component, if required.
	if (psXPC->f.precision > 0)	{					// Any decimal digits requested?
	// determine the multiplicant to be used to convert the fractional portion into a whole number
		uint64_t	mult ;
		for (idx = 0, mult = 1 ; idx < psXPC->f.precision ; idx++) {
			mult *= 10 ;
		}
	// extract fractional component as integer from double
		x64Value.f64		= f64Val - (uint64_t) f64Val ;		// STEP 1 - Isolate fraction as double
		x64Value.f64		= x64Value.f64 * (double) mult ;	// STEP 2 -convert relevant fraction to integer
		x64Value.u64		= (uint64_t) x64Value.f64 ;		// STEP the relevant integer portion, discard remaining fraction
		if (isnan(x64Value.f64)) {
			strcpy(Buffer + xpfMAX_LEN_F64 - sizeof("{NaN}"), "{NaN}") ;
			Len += 5 ;
			goto done ;
		}
	// now do the decimal (fractional) portion
		psXPC->f.minwid		= psXPC->f.precision ;
		psXPC->f.pad0		= 1 ;						// MUST left pad with '0'
		psXPC->f.group		= 0 ;						// cannot group in fractional
		psXPC->f.signed_val	= 0 ;						// always unsigned value
		psXPC->f.negvalue	= 0 ;
		psXPC->f.plus		= 0 ;						// no leading +/- before fractional part
		Len += xPrintXxx(psXPC, x64Value.u64, Buffer, (xpfMAX_LEN_F64 - 1) - Len) ;
	// handle the separator
		*(Buffer + (xpfMAX_LEN_F64 - 2 - Len++))	= CHR_FULLSTOP ;
	}

// extract and convert the whole number portions
	x64Value.u64		= f64Val ;
	psXPC->f.limits		= xpf.limits ;					// restore original flags
	psXPC->f.flags		= xpf.flags ;
// adjust minwid to do padding (if required) based on string length after adding whole number
	psXPC->f.minwid		= (psXPC->f.minwid > Len) ? psXPC->f.minwid - Len : 0 ;
	Len += xPrintXxx(psXPC, x64Value.u64, Buffer, (xpfMAX_LEN_F64 - 1) - Len) ;
done:
	psXPC->f.limits		= xpf.limits ;					// restore original flags
	psXPC->f.flags		= xpf.flags ;
	psXPC->f.precision	= 0 ;							// enable full string to be output (subject to minwid padding on right)
// then send the formatted output to the correct stream
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_F64 - 1 - Len)) ;
}

/*
 * vPrintHexU8()
 * \brief		write char value as 2 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to xprintf control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU8(xpc_t * psXPC, uint8_t Value) {
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value >> 4)) ;
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value & 0x0F)) ;
}

/*
 * vPrintHexU16()
 * \brief		write uint16_t value as 4 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to xprintf control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU16(xpc_t * psXPC, uint16_t Value) {
	vPrintHexU8(psXPC, (Value >> 8) & 0x000000FF) ;
	vPrintHexU8(psXPC, Value & 0x000000FF) ;
}

/*
 * vPrintHexU32()
 * \brief		write uint32_t value as 8 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to xprintf control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU32(xpc_t * psXPC, uint32_t Value) {
	vPrintHexU16(psXPC, (Value >> 16) & 0x0000FFFF) ;
	vPrintHexU16(psXPC, Value & 0x0000FFFF) ;
}

/*
 * vPrintHexU64()
 * \brief		write uint64_t value as 16 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to xprintf control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU64(xpc_t * psXPC, uint64_t Value) {
	vPrintHexU32(psXPC, (Value >> 32) & 0xFFFFFFFFULL) ;
	vPrintHexU32(psXPC, Value & 0xFFFFFFFFULL) ;
}

/**
 * vPrintHexValues() - write series of char values as hex chars to the buffer, always NULL terminated
 * @param psXPC
 * @param Num		number of bytes to print
 * @param pStr		pointer to bytes to print
 * @comment			Use the following modifier flags
 *					'`'	select grouping separators ':' (byte) '-' (short) ' ' (word) '|' (dword)
 *					'#' via alt_form select reverse order (little vs big endian) interpretation
 */
void	vPrintHexValues(xpc_t * psXPC, int32_t Num, char * pStr) {
	IF_myASSERT(debugPARAM, (Num > 0) && INRANGE_MEM(pStr)) ;
	int32_t	Size = 1 << psXPC->f.size ;
	if (psXPC->f.alt_form) {							// '#' specified to invert order ?
		pStr += Num - Size ;							// working backwards so point to last
	}

	x64_t	x64Val ;
	int32_t	Idx	= 0 ;
	while (Idx < Num) {
		switch (psXPC->f.size) {
		case 0:	x64Val.u8[0] = *pStr ; 					vPrintHexU8(psXPC, x64Val.u8[0]) ; 		break ;
		case 1:	x64Val.u16[0] = *((uint16_t *) pStr) ;	vPrintHexU16(psXPC, x64Val.u16[0]) ;	break ;
		case 2:	x64Val.u32[0] = *((uint32_t *) pStr) ;	vPrintHexU32(psXPC, x64Val.u32[0]) ;	break ;
		case 3:	x64Val.u64 = *((uint64_t *) pStr) ;		vPrintHexU64(psXPC, x64Val.u64) ; 		break ;
		}
	// step to the next 8/16/32/64 bit value
		if (psXPC->f.alt_form) {						// '#' specified to invert order ?
			pStr -= Size ;								// update source pointer backwards
		} else {
			pStr += Size ;								// update source pointer forwards
		}
		Idx += Size ;
		if (Idx >= Num) {
			break ;
		}
	// now handle the grouping separator(s)
		if (psXPC->f.form == xpfFORMAT_1_F) {
			vPrintChar(psXPC, CHR_COLON) ;
		} else if (psXPC->f.form == xpfFORMAT_3) {
			if ((Idx % 8) == 0) {
				vPrintChar(psXPC, CHR_VERT_BAR) ;
			} else if ((Idx % 4) == 0) {
				vPrintChar(psXPC, CHR_SPACE) ;
			} else if ((Idx % 2) == 0) {
				vPrintChar(psXPC, CHR_COLON) ;
			} else {
				vPrintChar(psXPC, CHR_MINUS) ;
			}
		}
	}
}

// ############################### Proprietary extensions to printf() ##############################

/*
 * vPrintPointer() - display a 32 bit hexadecimal number as address
 * \brief		Currently 64 bit addresses not supported ...
 * \param[in]	psXPC - pointer to print control structure
 * \param[in]	uint32_t - address to be printed
 * \param[out]	psXPC - updated based on characters displayed
 * \return		none
 */
void	vPrintPointer(xpc_t * psXPC, uint32_t Address) {
	xPrintChars(psXPC, (char *) "0x") ;
	vPrintHexU32(psXPC, Address) ;
}

seconds_t xPrintCalcSeconds(xpc_t * psXPC, TSZ_t * psTSZ, struct tm * psTM) {
	// Get seconds value to use... (adjust for TZ if required/allowed)
	seconds_t	Seconds ;
	if ((psTSZ->pTZ != NULL) &&							// TZ info available
		(psXPC->f.plus == 1) &&							// display of TZ info requested
		(psXPC->f.abs_rel == 0) &&						// not working with relative time
		(psXPC->f.alt_form == 0)) {						// not asking for alternative form output
		Seconds = xTime_CalcLocalTimeSeconds(psTSZ) ;
	} else {
		Seconds = xTimeStampAsSeconds(psTSZ->usecs) ;
	}

// convert seconds into components
	if (psTM) {
		xTime_GMTime(Seconds, psTM, psXPC->f.abs_rel) ;
	}
	return Seconds ;
}

void	vPrintTimeUSec(xpc_t * psXPC, uint64_t uSecs) {
	struct	tm 	sTM ;
	xTime_GMTime(xTimeStampAsSeconds(uSecs), &sTM, psXPC->f.abs_rel) ;
	psXPC->f.plus	= 0 ;								// ensure no '+' sign printed
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	psXPC->f.minwid = 2 ;
	size_t	Len ;
	char	Buffer[xpfMAX_LEN_TIME] ;
	// Part 1: hours
	if (sTM.tm_hour > 0 || psXPC->f.pad0 || psXPC->f.mday_ok) {
		Len = xPrintXxx(psXPC, (uint64_t) sTM.tm_hour, Buffer, 2) ;
		Buffer[Len++] = (psXPC->f.form == xpfFORMAT_3) ? CHR_h :  CHR_COLON ;
		psXPC->f.hour_ok = 1 ;
		psXPC->f.pad0	= 1 ;
	} else {
		Len = 0 ;
	}
	// Part 2: minutes
	if (sTM.tm_min > 0 || psXPC->f.pad0 || psXPC->f.hour_ok) {
		Len += xPrintXxx(psXPC, (uint64_t) sTM.tm_min, &Buffer[Len], 2) ;
		Buffer[Len++] = (psXPC->f.form == xpfFORMAT_3) ? CHR_m :  CHR_COLON ;
		psXPC->f.min_ok = 1 ;
		psXPC->f.pad0	= 1 ;
	}
	// Part 3: seconds
	Len += xPrintXxx(psXPC, (uint64_t) sTM.tm_sec, &Buffer[Len], 2) ;
	psXPC->f.sec_ok = 1 ;							// not required, just to be correct

	// Part 4: 'Z': ".x[xxxxx]
	uSecs %= MICROS_IN_SECOND ;
	uSecs /= xpfTIME_FRAC_SEC_DIVISOR ;
	if (psXPC->f.alt_form == 0) {
		Buffer[Len++] = (psXPC->f.form == xpfFORMAT_3) ? CHR_s :  CHR_FULLSTOP ;
		psXPC->f.pad0		= 1 ;
		psXPC->f.signed_val= 0 ;						// unsigned value
		psXPC->f.minwid	= xpfMAX_DIGITS_FRAC_SEC ;
		Len += xPrintXxx(psXPC, uSecs, Buffer + Len, xpfMAX_DIGITS_FRAC_SEC) ;
	}

	// converted L to R, so terminate
	Buffer[Len] = CHR_NUL ;
	// then send the formatted output to the correct stream
	psXPC->f.precision	= 0 ;							// enable full string
	psXPC->f.minwid	= 0 ;
	vPrintString(psXPC, Buffer) ;
}

/**
 * vPrintTime() - print time (using TZ info to adjust) with uSec resolution
 */
void	vPrintTime(xpc_t * psXPC, TSZ_t * psTSZ) {
	vPrintTimeUSec(psXPC, xTimeMakeTimestamp(xPrintCalcSeconds(psXPC, psTSZ, NULL), psTSZ->usecs % MICROS_IN_SECOND)) ;
}

size_t	xPrintDate_Year(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 0 ;
	size_t Len = xPrintXxx(psXPC, (int64_t) (psTM->tm_year + (psXPC->f.abs_rel ? 0 : YEAR_BASE_MIN)), pBuffer, 4) ;
	if (psXPC->f.alt_form == 0) {						// no extra ' ' at end for alt_form
		*(pBuffer + Len++) = (psXPC->f.form == xpfFORMAT_3) ? CHR_FWDSLASH : CHR_MINUS ;
	}
	psXPC->f.year_ok	= 1 ;
	psXPC->f.pad0		= 1 ;
	return Len ;
}

size_t	xPrintDate_Month(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 2 ;
	size_t Len = xPrintXxx(psXPC, (int64_t) (psTM->tm_mon + (psXPC->f.abs_rel ? 0 : 1)), pBuffer, 2) ;
	if (psXPC->f.alt_form) {
		*(pBuffer + Len++) = CHR_SPACE ;
	} else {
		*(pBuffer + Len++) = (psXPC->f.form == xpfFORMAT_3) ? CHR_FWDSLASH : CHR_MINUS ;
	}
	psXPC->f.mon_ok	= 1 ;
	psXPC->f.pad0	= 1 ;
	return Len ;
}

size_t	xPrintDate_Day(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid = 2 ;
	size_t Len = xPrintXxx(psXPC, (int64_t) psTM->tm_mday, pBuffer, 2) ;
	if (psXPC->f.alt_form) {
		*(pBuffer + Len++) = CHR_SPACE ;
	} else {
		*(pBuffer + Len++) = ((psXPC->f.form == xpfFORMAT_3) && psXPC->f.abs_rel) ? CHR_d :
							(psXPC->f.form == xpfFORMAT_3) ? CHR_SPACE : CHR_T ;
	}
	psXPC->f.mday_ok	= 1 ;
	psXPC->f.pad0		= 1 ;
	return Len ;
}

void	vPrintDateUSec(xpc_t * psXPC, uint64_t uSecs) {
	struct tm 	sTM ;
	seconds_t Seconds ;
	xTime_GMTime(Seconds = xTimeStampAsSeconds(uSecs), &sTM, psXPC->f.abs_rel) ;
	psXPC->f.plus	= 0 ;								// ensure no '+' sign printed
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	psXPC->f.pad0	= psXPC->f.abs_rel ? 0 : 1 ;
	size_t	Len ;
	char	Buffer[xpfMAX_LEN_DATE] ;
	// Part 1: day of week (optional)
	if (psXPC->f.alt_form) {							// "Sun, "
		Len = xstrncpy(Buffer, (char *) xTime_GetDayName(sTM.tm_wday), 3) ;
		Len += xstrncpy(&Buffer[Len], (char *) ", ", 2) ;
	} else {
		Len = 0 ;
	}

	// Part 2:
	if (psXPC->f.alt_form) {							// "Sun, 10 "
		Len += xPrintDate_Day(psXPC, &sTM, &Buffer[Len]) ;
	} else {
		if (sTM.tm_year > 0 || psXPC->f.pad0) {
			Len += xPrintDate_Year(psXPC, &sTM, &Buffer[Len]) ;
		}
	}

	// Part 3:
	if (psXPC->f.alt_form) {							// "Sun, 10 Sep "
		Len += xstrncpy(&Buffer[Len], (char *) xTime_GetMonthName(sTM.tm_mon), 3) ;
		Len += xstrncpy(&Buffer[Len], (char *) " ", 1) ;
	} else {
		if (sTM.tm_mon > 0 || psXPC->f.pad0 || psXPC->f.year_ok) {
			Len += xPrintDate_Month(psXPC, &sTM, &Buffer[Len]) ;
		}
	}

	// Part 4:
	if (psXPC->f.alt_form) {							// "Sun, 10 Sep 2017"
		Len += xPrintDate_Year(psXPC, &sTM, &Buffer[Len]) ;
	} else {
		if (Seconds >= SECONDS_IN_DAY || psXPC->f.pad0 || psXPC->f.mon_ok) {
			Len += xPrintDate_Day(psXPC, &sTM, &Buffer[Len]) ;
		}
	}

// converted L to R, so terminate
	Buffer[Len] = CHR_NUL ;
// then send the formatted output to the correct stream
	psXPC->f.precision	= 0 ;							// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer) ;
}

/*
 * vPrintDate()
 * \brief		Prints to date in POSIX format to destination
 * \brief		CRITICAL : This function absorbs 2 parameters on the stack
 * \param[in]	psXPC - pointer to print control structure
 * \param[in]	Secs - Seconds in epoch format
 * \param[out]	none
 * \return		none
 * \comment		Use the following modifier flags
 *				'`'		select between 2 different separator sets being
 *				'/' or '-' (years) '/' or '-' (months) 'T' or ' ' (days)
 * 				'!'		Treat time value as abs_rel and not epoch seconds
 * 		Norm 1	1970/01/01T00:00:00Z
 * 		Norm 2	1970-01-01 00:00:00
 * 		Altform Mon, 01 Jan 1970 00:00:00 GMT
 */
void	vPrintDate(xpc_t * psXPC, TSZ_t * psTSZ) {
	seconds_t Seconds = xPrintCalcSeconds(psXPC, psTSZ, NULL) ;
	uint64_t TStamp = xTimeMakeTimestamp(Seconds, psTSZ->usecs % MICROS_IN_SECOND) ;
	vPrintDateUSec(psXPC, TStamp) ;
}

void	vPrintDateTimeUSec(xpc_t * psXPC, uint64_t uSecs) {
	vPrintDateUSec(psXPC, uSecs) ;
	vPrintTimeUSec(psXPC, uSecs) ;
	if (psXPC->f.no_zone == 0 && psXPC->f.abs_rel == 0) {
		vPrintChar(psXPC, CHR_Z) ;						// If not coming from vPrintDateTimeZone() AND not relative time
	}
}

/*
 * vPrintDateTimeZone()
 * \brief		Prints to Date, Time [& TZ] info in POSIX format to destination
 * \brief		CRITICAL : This function absorbs 3 parameters on the stack
 * \param[in]	psXPC - pointer to print control structure
 * \param[in]	psTSZ - pointer to SystemTime structure
 * \param[out]	none
 * \return		none
 * \comment		Use the following modifier flags
 *				'`'		select between ':' & 'h' and
 * 				'!'		Treat time value as abs_rel and not epoch seconds
 */
void	vPrintDateTimeZone(xpc_t * psXPC, TSZ_t * psTSZ) {
	uint32_t flags		= psXPC->f.flags ;				// save current flags, primarily f.plus
	psXPC->f.no_zone	= 1 ;
	vPrintDateTimeUSec(psXPC, xTimeMakeTimestamp(xPrintCalcSeconds(psXPC, psTSZ, NULL), psTSZ->usecs % MICROS_IN_SECOND)) ;
	psXPC->f.flags		= flags ;						// restore the f.plus status
	// now handle the TZ info
	size_t	Len = 0 ;
	char	Buffer[configTIME_MAX_LEN_TZINFO] ;
	if (psXPC->f.abs_rel == 0) {						// ONLY possible if not elapsed time
		if (psTSZ->pTZ == 0 || psXPC->f.plus == 0) {	// If no TZ info supplied or TZ info not wanted...
			Buffer[0]	= CHR_Z ;						// add 'Z' for Zulu/zero time zone
			Len = 1 ;

		} else if (psXPC->f.alt_form == 1) {			// TZ info available but '#' format specified
			Len = xstrncpy(Buffer, (char *) " GMT", 4) ;// show as GMT (ie UTC)

		} else if (psXPC->f.plus) {						// TZ info available & '+x:xx(???)' format requested
			psXPC->f.signed_val	= 1 ;					// TZ hours offset is a signed value
			psXPC->f.plus		= 1 ;					// force display of sign
			psXPC->f.pad0		= 1 ;
			Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3) ;
		// insert separating ':' or 'h'
			Buffer[Len++]	= psXPC->f.form ? CHR_h : CHR_COLON ;
		// add the minutes
			psXPC->f.signed_val	= 0 ;					// TZ offset minutes unsigned
			psXPC->f.plus		= 0 ;
			psXPC->f.pad0		= 1 ;
			Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXPC->f.minwid = 2) ;
#if		(buildTIME_TZTYPE_SELECTED == buildTIME_TZTYPE_POINTER)
			if (psTSZ->pTZ->pcTZName) {					// handle TZ name (as string pointer) if there
				Buffer[Len++]	= CHR_L_ROUND ;			// add separating ' ('
				psXPC->f.minwid = 0 ;					// then complete with the TZ name
				while ((psXPC->f.minwid < configTIME_MAX_LEN_TZNAME) &&
						(psTSZ->pTZ->pcTZName[psXPC->f.minwid] != CHR_NUL)) {
					Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->pcTZName[psXPC->f.minwid] ;
					psXPC->f.minwid++ ;
				}
				Len += psXPC->f.minwid ;
				Buffer[Len++]	= CHR_R_ROUND ;			// and wrap it up...
			}
#elif	(buildTIME_TZTYPE_SELECTED == buildTIME_TZTYPE_FOURCHARS)
			// Now handle the TZ name if there, check to ensure max 4 chars all UCase
			if (xstrverify(&psTSZ->pTZ->tzname[0], CHR_A, CHR_Z, configTIME_MAX_LEN_TZNAME) == erSUCCESS) {
				Buffer[Len++]	= CHR_L_ROUND ;			// add separating ' ('
			// then complete with the TZ name
				psXPC->f.minwid = 0 ;
				while ((psXPC->f.minwid < configTIME_MAX_LEN_TZNAME) &&
						(psTSZ->pTZ->tzname[psXPC->f.minwid] != CHR_NUL) &&
						(psTSZ->pTZ->tzname[psXPC->f.minwid] != CHR_SPACE)) {
					Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->tzname[psXPC->f.minwid] ;
					psXPC->f.minwid++ ;
				}
				Len += psXPC->f.minwid ;
				Buffer[Len++]	= CHR_R_ROUND ;			// and wrap it up...
			}
#elif	(buildTIME_TZTYPE_SELECTED == buildTIME_TZTYPE_RFC3164)
			// nothing added other than the time offset
#endif
		} else {
			// Should never get here...
		}
	}
// terminate string in buffer then out as per normal...
	Buffer[Len]			= CHR_NUL ;						// converted L to R, so add terminating NUL
	psXPC->f.minwid		= 0 ;							// no explicit limit on width
	psXPC->f.precision	= 0 ;							// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer) ;
}

/*
 * vPrintHexDump()
 * \brief		Dumps a block of memory in debug style format. depending on options output can be
 * 				formatted as 8/16/32 or 64 bit variables, optionally with no, absolute or relative address
 * \param[in]	psXPC - pointer to print control structure
 * \param[in]	pStr - pointer to memory starting address
 * \param[in]	Len - Length/size of memory buffer to display
 * \param[out]	none
 * \return		none
 * \comment		Use the following modifier flags
 *				'`'		Grouping of values using ' ' '-' or '|' as separators
 * 				'!'		Use relative address format
 * 				'#'		Use absolute address format
 *						Relative/absolute address prefixed using format '0x12345678:'
 * 				'+'		Add the ASCII char equivalents to the right of the hex output
 */
void	vPrintHexDump(xpc_t * psXPC, uint32_t Len, char * pStr) {
	int32_t	Size = 1 << psXPC->f.size ;
	for (uint32_t Idx = 0; Idx < Len; Idx += xpfHEXDUMP_WIDTH) {
		if (psXPC->f.ljust == 0) {						// display absolute or relative address
			vPrintPointer(psXPC, psXPC->f.abs_rel ? Idx : (uint32_t) (pStr + Idx)) ;
			xPrintChars(psXPC, (char *) ": ") ;
		}

	// then the actual series of values in 8-32 bit groups
		int32_t Width = ((Len - Idx) > xpfHEXDUMP_WIDTH) ? xpfHEXDUMP_WIDTH : Len - Idx ;
		vPrintHexValues(psXPC, Width, pStr + Idx) ;
	// handle values dumped as ASCII chars
		if (psXPC->f.plus == 1) {
		// handle space padding for ASCII dump to line up
			uint32_t Count = ((xpfHEXDUMP_WIDTH - Width) / Size) * ((Size * 2) + (psXPC->f.form ? 1 : 0)) ;
			Count++ ;										// allow for default 1x space
			while (Count--) {
				vPrintChar(psXPC, CHR_SPACE) ;
			}
		// handle same values dumped as ASCII characters
			for (Count = 0; Count < Width; Count++) {
				uint32_t cChr = *(pStr + Idx + Count) ;
				vPrintChar(psXPC, (cChr<CHR_SPACE || cChr==CHR_DEL || cChr==0xFF) ? CHR_FULLSTOP : cChr) ;
			}
			vPrintChar(psXPC, CHR_SPACE) ;
		}
		if (Idx < Len) {
			xPrintChars(psXPC, (char *) "\n") ;
		}
	}
}

/*
 * vPrintBinary()
 * \brief		convert unsigned 32/64 bit value to 1/0 ASCI string
 * 				field width specifier is applied as mask starting from LSB to MSB
 * \param[in]	psXPC - pointer to print control structure
 * \param[in]	ullVal - 64bit value to convert to binary string & display
 * \param[out]	none
 * \return		none
 * \comment		Honour & interpret the following modifier flags
 * 				'`'		Group digits using '|' (bytes) and '-' (nibbles)
 */
void	vPrintBinary(xpc_t * psXPC, uint64_t ullVal) {
	int32_t		len ;
	if (psXPC->f.minwid == 0) {							// not specified
		len = (psXPC->f.llong == 1) ? 64 : 32 ;			// set to 32 or 64 ...
	} else if (psXPC->f.llong == 1) {					// width specified, but check validity...
		len = (psXPC->f.minwid > 64) ? 64 : psXPC->f.minwid ;
	} else {
		len = (psXPC->f.minwid > 32) ? 32 : psXPC->f.minwid ;
	}
	uint64_t mask	= 1ULL << (len - 1) ;
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	while (mask) {
		vPrintChar(psXPC, (ullVal & mask) ? CHR_1 : CHR_0) ;
		mask >>= 1;
	// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
		if (--len && psXPC->f.form) {
			if ((len % 32) == 0) {
				vPrintChar(psXPC, CHR_VERT_BAR) ;		// separate on word boundary
			} else if ((len % 16) == 0) {
				vPrintChar(psXPC, CHR_COLON) ;			// separate on short boundary
			} else if ((len % 8) == 0) {
				vPrintChar(psXPC, CHR_SPACE) ;			// separate on byte boundary
			} else if ((len % 4) == 0) {
				vPrintChar(psXPC, CHR_MINUS) ;			// separate on nibble boundary
			}
		}
	}
}

/*
 * vPrintIpAddress()
 * \brief		Print DOT formatted IP address to destination
 * \param[in]	psXPC - pointer to output & format control structure
 * 				Val - IP address value in HOST format !!!
 * \param[out]	none
 * \return		none
 * \comment		Used the following modifier flags
 * 				'!'		Invert the LSB normal MSB byte order (Network <> Host related)
 * 				'-'		Left align the individual numbers between the '.'
 * 				'0'		Zero pad the individual numbers between the '.'
 */
void	vPrintIpAddress(xpc_t * psXPC, uint32_t Val) {
	psXPC->f.minwid		= psXPC->f.ljust ? 0 : 3 ;
	psXPC->f.signed_val	= 0 ;							// ensure interpreted as unsigned
	psXPC->f.plus		= 0 ;							// and no sign to be shown
	uint8_t * pChr = (uint8_t *) &Val ;
	if (psXPC->f.alt_form) {							// '#' specified to invert order ?
		uint8_t temp ;
		temp		= *pChr ;
		*pChr		= *(pChr+3) ;
		*(pChr+3)	= temp ;
		temp		= *(pChr+1) ;
		*(pChr+1)	= *(pChr+2) ;
		*(pChr+2)	= temp ;
	}
	// building R to L, ensure buffer NULL-term
	char	Buffer[xpfMAX_LEN_IP] ;
	Buffer[xpfMAX_LEN_IP - 1] = CHR_NUL ;

	// then convert the IP address, LSB first
	size_t	Idx, Len ;
	for (Idx = 0, Len = 0; Idx < 4; Idx++) {
		uint64_t u64Val = (uint8_t) pChr[Idx] ;
		Len += xPrintXxx(psXPC, u64Val, Buffer + (xpfMAX_LEN_IP - 5 - Len), 4) ;
		if (Idx < 3) {
			*(Buffer + (xpfMAX_LEN_IP - 1 - ++Len)) = CHR_FULLSTOP ;
		}
	}
	// then send the formatted output to the correct stream
	psXPC->f.precision	= 0 ;							// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_IP - 1 - Len)) ;
}

/* ################################# The HEART of the XPRINTF matter ###############################
 * xprintf - common routine for formatted print functionality
 * \brief	parse the format string and interpret the conversions, flags and modifiers
 * 			extract the parameters variables in correct type format
 * 			call the correct routine(s) to perform conversion, formatting & output
 * \param	psXPC - pointer to structure containing formatting and output destination info
 * 			format - pointer to the formatting string
 * 			vArgs - variable number of arguments
 * \return	void (other than updated info in the original structure passed by reference
 */

int		xPrint(int (handler)(xpc_t *, int), void * pVoid, size_t BufSize, const char *format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_FLASH(handler) && INRANGE_FLASH(format)) ;
	xpc_t	sXPC ;
	sXPC.handler	= handler ;
	sXPC.pVoid		= pVoid ;
	sXPC.f.lengths	= sXPC.f.limits = sXPC.f.flags = 0UL ;
	sXPC.f.maxlen	= (BufSize > xpfMAXLEN_MAXVAL) ? xpfMAXLEN_MAXVAL : BufSize ;

	for (; *format != CHR_NUL; ++format) {
	// start by expecting format indicator
		if (*format == CHR_PERCENT) {
			sXPC.f.flags 	= 0 ;						// set ALL flags to default 0
			sXPC.f.limits	= 0 ;						// reset field specific limits
			sXPC.f.nbase	= BASE10 ;					// override default number base
			++format ;
			if (*format == CHR_NUL) {					// terminating NULL at end of format string...
				break ;
			}
		/* In order for the optional modifiers to work correctly, especially in cases such as HEXDUMP
		 * the modifiers MUST be in correct sequence of interpretation being [ ! # ' * + - % 0 ] */
			int32_t	cFmt ;
			while ((cFmt = xinstring("!#'*+-%0", *format)) != erFAILURE) {
				switch (cFmt) {
				case 0:									// '!' HEXDUMP absolute->relative address
					++format ;							// or DTZ absolute->relative time
					sXPC.f.abs_rel	= 1 ;
					break ;
				case 1:									// '#' DTZ alternative form
					++format ;							// or HEXDUMP swop endian
					sXPC.f.alt_form	= 1 ;				// or IP swop endian
					break ;
				case 2:									// "'" decimal= ',' separated 3 digit grouping, DTZ=alternate format
					++format ;
					sXPC.f.group	= 1 ;
					break ;
				case 3:									// '*' indicate argument will supply field width
					++format ;
					uint32_t U32	= va_arg(vArgs, uint32_t) ;
					IF_myASSERT(debugPARAM, U32 < xpfMINWID_MAXVAL) ;
					sXPC.f.minwid	= U32 ;
					sXPC.f.arg_width= 1 ;
					break ;
				case 4:									// '+' force leading +/- signed
					++format ;							// or HEXDUMP add ASCII char dump
					sXPC.f.plus		= 1 ;				// or Zone info with time
					break ;
				case 5:									// '-' Left justify modifier
					++format ;							// or HEXDUMP remove address pointer
					sXPC.f.ljust	= 1 ;
					break ;
				case 6:									// '%' literal to display
					goto out_lbl ;
				case 7:									// '0 force leading '0's
					++format ;
					sXPC.f.pad0		= 1 ;
					break ;
				default:
					myASSERT(0) ;
				}
			}
		// handle pre and post decimal field width/precision indicators
			if ((*format == CHR_FULLSTOP) || INRANGE(CHR_0, *format, CHR_9, char)) {
				cFmt = 0 ;
				while (1) {
					if (INRANGE(CHR_0, *format, CHR_9, char)) {
						cFmt *= 10 ;
						cFmt += *format - '0' ;
						format++ ;
					} else if (*format == CHR_FULLSTOP) {
						IF_myASSERT(debugPARAM, sXPC.f.radix == 0)	// cannot have 2x radix '.'
						sXPC.f.radix	= 1 ;			// flag radix as provided
						format++ ;						// skip over radix char
						if (cFmt > 0) {
							IF_myASSERT(debugPARAM, sXPC.f.arg_width == 0) ;
						// at this stage we MIGHT have parsed a minwid value, if so verify and store.
							sXPC.f.minwid	= cFmt ;	// Save value parsed (maybe 0) as min_width
							sXPC.f.arg_width= 1 ;		// flag min_width as having been supplied
							cFmt = 0 ;					// reset counter in case of precision following
						}
					} else if (*format == CHR_ASTERISK) {
						format++ ; 						// skip over argument precision char
						IF_myASSERT(debugPARAM, sXPC.f.radix == 1) ;	// Should not have '*' except after radix '.'
						sXPC.f.precision= va_arg(vArgs, uint32_t) ;
						sXPC.f.arg_prec	= 1 ;
						cFmt = 0 ;						// reset counter just in case!!!
					} else {
						break ;
					}
				}
			// Save possible parsed value in cFmt
				if (cFmt > 0) {
					if ((sXPC.f.arg_width == 0) && (sXPC.f.radix == 0)) {
						sXPC.f.minwid	= cFmt ;
					} else if ((sXPC.f.arg_prec == 0) && (sXPC.f.radix == 1)) {
						sXPC.f.precision= cFmt ;
					} else {
						myASSERT(0) ;
					}
				}
			}
		// handle 2x successive 'l' characters, lower case ONLY, form long long value treatment
			if (*format == 'l') {
				++format;
				if (*format == 'l') {
					++format ;
					sXPC.f.llong = 1 ;
				}
			}
		// Check if format character where UC/lc same character control the case of the output
			cFmt = *format ;
			if (xinstring(vPrintStr1, cFmt) != erFAILURE) {
				cFmt |= 0x20 ;							// convert to lower case, but ...
				sXPC.f.Ucase = 1 ;						// indicate as UPPER case requested
			}
		// At this stage the format specifiers used in UC & LC to denote output case has been changed to all lower case
			switch (cFmt) {
#if		(xpfSUPPORT_DATETIME == 1)
				case 'D':								// DATE
					vPrintDate(&sXPC, va_arg(vArgs, TSZ_t *)) ;			// para =  SysTime
					break ;
#endif

#if		(xpfSUPPORT_IP_ADDR == 1)
				case 'I':								// IP address
					vPrintIpAddress(&sXPC, va_arg(vArgs, uint32_t)) ;
					break ;
#endif

#if		(xpfSUPPORT_DATETIME == 1)
				case 'R':								// timestamp (abs or rel)
					vPrintDateTimeUSec(&sXPC, va_arg(vArgs, uint64_t)) ;	// para =  microSeconds
					break ;

				case 'T':								// TIME
					vPrintTime(&sXPC, va_arg(vArgs, TSZ_t *)) ;				// para =  SysTime
					break ;

				case 'Z':								// DATE, TIME, ZONE
					vPrintDateTimeZone(&sXPC, va_arg(vArgs, TSZ_t *)) ;		// para =  SysTime
					break ;
#endif

#if		(xpfSUPPORT_HEXDUMP == 1)
				case 'b':								// HEXDUMP byte sized UC/lc
				case 'h':								// HEXDUMP halfword sized UC/lc
				case 'w':								// HEXDUMP word sized UC/lc
					sXPC.f.form	= sXPC.f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
					sXPC.f.size = (cFmt == 'b') ? xpfSIZING_BYTE : (cFmt == 'h') ? xpfSIZING_SHORT : xpfSIZING_WORD ;
					sXPC.f.size	+= (sXPC.f.llong) ? 1 : 0 ; 			// apply ll modifier to size
				/* In order for formatting to work  the "*" or "." radix specifiers
				 * should not be used. The requirement for a second parameter is implied and assumed */
					uint32_t U32	= va_arg(vArgs, uint32_t) ;				// 32 bit size
					char * pChar	= va_arg(vArgs, char *) ;				// 32 bit address
					if (pChar) {
						vPrintHexDump(&sXPC, U32, pChar) ;
					}
					break ;
#endif

#if		(xpfSUPPORT_MAC_ADDR == 1)
				case 'm':								// MAC address UC/LC format ??:??:??:??:??:??
					sXPC.f.form	= sXPC.f.group ? xpfFORMAT_1_F : xpfFORMAT_0_G ;
					vPrintHexValues(&sXPC, configMAC_ADDRESS_LENGTH, va_arg(vArgs, char *)) ;
					break ;
#endif

				case 'c':								// char passed an uint32_t
					vPrintChar(&sXPC, va_arg(vArgs, int32_t)) ;
					break ;

				case 'd':								// signed decimal "[-]ddddd"
				case 'i':								// signed integer (same as decimal ?)
				case 'o':								// unsigned octal "ddddd"
				case 'u':								// unsigned decimal "ddddd"
				case 'x':								// hex as in "789abcd" UC/LC
				// setup the number base value/form
					sXPC.f.nbase = (cFmt == CHR_x) ? BASE16 : (cFmt == CHR_o) ? BASE08 : BASE10 ;
				// if not working with BASE10, disable grouping
					if (sXPC.f.nbase != BASE10) {
						sXPC.f.group = 0 ;
					}
				// set signed form based on conversion specifier
					sXPC.f.signed_val	= (cFmt == CHR_d || cFmt == CHR_i) ? 1 : 0 ;
				// fetch 64 bit value from stack, or 32 bit & convert to 64 bit
					x64_t	x64Val ;
					if (sXPC.f.signed_val) {
						x64Val.i64		= sXPC.f.llong ? va_arg(vArgs, int64_t) : va_arg(vArgs, int32_t) ;
						sXPC.f.negvalue	= (x64Val.i64 < 0) ? 1 : 0 ;	// set the negvalue form as required.
						x64Val.i64 		*= sXPC.f.negvalue ? -1 : 1 ; // convert the value to unsigned
					} else {
						x64Val.u64	= sXPC.f.llong ? va_arg(vArgs, uint64_t) : va_arg(vArgs, uint32_t) ;
					}
					vPrintX64(&sXPC, x64Val.u64) ;
					break ;

#if	(xpfSUPPORT_IEEE754 == 1)
				case 'e':								// treat same as 'f' for now, need to implement...
					sXPC.f.form++ ;
					/* no break */
				case 'f':								// floating point format
					sXPC.f.form++ ;
					/* no break */
				case 'g':
					sXPC.f.signed_val		= 1 ;		// float always signed value.
					if (sXPC.f.radix) {
						sXPC.f.precision	= (sXPC.f.precision > xpfMAXIMUM_DECIMALS) ? xpfMAXIMUM_DECIMALS : sXPC.f.precision ;
					} else {
						sXPC.f.precision	= xpfDEFAULT_DECIMALS ;
					}
					vPrintF64(&sXPC, va_arg(vArgs, double)) ;
					break ;
#endif

#if	(xpfSUPPORT_POINTER == 1)
				case 'p':								// pointer value UC/lc
					vPrintPointer(&sXPC, va_arg(vArgs, uint32_t)) ;		// no provision for 64 bit pointers (yet)
					break ;
#endif

				case 's':
					vPrintString(&sXPC, va_arg(vArgs, char *)) ;
					break;

#if	(xpfSUPPORT_BINARY == 1)
				case 'J':
					vPrintBinary(&sXPC, sXPC.f.llong ? va_arg(vArgs, uint64_t) : (uint64_t) va_arg(vArgs, uint32_t)) ;
					break ;
#endif

				default:
					vPrintChar(&sXPC, CHR_PERCENT) ;
					vPrintChar(&sXPC, *format) ;
					break ;
			}
			sXPC.f.plus				= 0 ;		// reset this form after printing one value
		} else {
out_lbl:
			vPrintChar(&sXPC, *format) ;
		}
	}  //  for each char  in format string
	return sXPC.f.curlen ;
}

// ##################################### Destination = STRING ######################################

static	int	xPrintToString(xpc_t * psXPC, int cChr) {
	if (psXPC->pStr) {
		*psXPC->pStr++ = cChr ;
	}
	return cChr ;
}

int 	xvsnprintf(char * pBuf, size_t BufSize, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, (pBuf == NULL) || INRANGE_SRAM(pBuf)) ;
	if (pBuf && (BufSize == 1)) {						// buffer specified, but no space ?
		*pBuf = CHR_NUL ;								// yes, terminate
		return 0 ; 										// & return
	}
	int count = xPrint(xPrintToString, pBuf, BufSize, format, vArgs) ;
	if (pBuf && (count == BufSize)) {					// buffer specified and FULL?
		count-- ; 										// yes, adjust the length for terminator
	}
	if (pBuf) {											// buffer specified ?
		pBuf[count] = CHR_NUL ; 						// yes, terminate
	}
	return count ;
}

int 	xsnprintf(char * pBuf, size_t BufSize, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int32_t count = xvsnprintf(pBuf, BufSize, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

int 	xvsprintf(char * pBuf, const char * format, va_list vArgs) { return xvsnprintf(pBuf, xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	xsprintf(char * pBuf, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = xvsnprintf(pBuf, xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = FILE PTR ######################################

static	int	xPrintToFile(xpc_t * psXPC, int cChr) { return fputc(cChr, psXPC->stream) ; }

int 	xvfprintf(FILE * stream, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_SRAM(stream)) ;
	return xPrint(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vArgs) ;
}

int 	xfprintf(FILE * stream, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int count = xvfprintf(stream, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = STDOUT ########################################

int 	xvnprintf(size_t count, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_SRAM(stdout)) ;
	return xPrint(xPrintToFile, stdout, count, format, vArgs) ;
}

int 	xvprintf(const char * format, va_list vArgs) { return xvnprintf(xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	xnprintf(size_t count, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRetVal = xvnprintf(count, format, vArgs) ;
	va_end(vArgs) ;
	return iRetVal ;
}

int 	xprintf(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRetVal = xvnprintf(xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return iRetVal ;
}

// ################################### Destination = HANDLE ########################################

static	int	xPrintToHandle(xpc_t * psXPC, int cChr) {
	char cChar = cChr ;
	ssize_t size = write(psXPC->fd, &cChar, sizeof(cChar)) ;
	return size == 1 ? cChr : size ;
}

int		xvdprintf(int32_t fd, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, fd >= 0) ;
	return xPrint(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vArgs) ;
}

int		xdprintf(int32_t fd, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = xvdprintf(fd, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = DEVICE ########################################

static	int	xPrintToDevice(xpc_t * psXPC, int cChr) { return psXPC->DevPutc(cChr) ; }

int 	vdevprintf(int (* handler)(int ), const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_FLASH(handler)) ;
	return xPrint(xPrintToDevice, handler, xpfMAXLEN_MAXVAL, format, vArgs) ;
}

int 	devprintf(int (* handler)(int ), const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRetVal = xPrint(xPrintToDevice, handler, xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return iRetVal ;
}

/* #################################### Destination : SOCKET #######################################
 * SOCKET directed formatted print support. Problem here is that MSG_MORE is primarily supported on
 * TCP sockets, UDP support officially in LwIP 2.6 but has not been included into ESP-IDF yet. */

static	int	xPrintToSocket(xpc_t * psXPC, int cChr) {
	char cBuf = cChr ;
	if (xNetWrite(psXPC->psSock, &cBuf, sizeof(cBuf)) != sizeof(cBuf)) {
		return EOF ;
	}
	return cChr ;
}

int 	vsocprintf(sock_ctx_t * psSock, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_SRAM(psSock)) ;
	int	OldFlags	= psSock->flags ;
	psSock->flags	|= MSG_MORE ;
	int32_t iRetVal = xPrint(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vArgs) ;
	psSock->flags	= OldFlags ;
	return (psSock->error == 0) ? iRetVal : erFAILURE ;
}

int 	socprintf(sock_ctx_t * psSock, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int count = vsocprintf(psSock, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// #################################### Destination : UBUF #########################################

static	int	xPrintToUBuf(xpc_t * psXPC, int cChr) { return xUBufPutC(psXPC->psUBuf, cChr) ; }

int		vuprintf(ubuf_t * psUBuf, const char * format, va_list vArgs) {
	IF_myASSERT(debugPARAM, INRANGE_SRAM(psUBuf) && INRANGE_MEM(format)) ;
	if (xUBufSpace(psUBuf) == 0) {						// if no space left
		return 0 ;										// return
	}
	return xPrint(xPrintToUBuf, psUBuf, xUBufAvail(psUBuf), format, vArgs) ;
}

int		uprintf(ubuf_t * psUBuf, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = vuprintf(psUBuf, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################## Destination = UART/TELNET ####################################

SemaphoreHandle_t	usartSemaphore = NULL ;

void	cprintf_lock(void) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		if (usartSemaphore == NULL) {
			usartSemaphore = xSemaphoreCreateMutex() ;
		}
		if (halNVIC_CalledFromISR() == 0) {
			xSemaphoreTake(usartSemaphore, portMAX_DELAY) ;
		}
	}
}

void	cprintf_unlock(void) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		if (halNVIC_CalledFromISR() == 0) {
			xSemaphoreGive(usartSemaphore) ;
		}
	}
}

static	int	xPrintToStdout(xpc_t * psXPC, int cChr) { return putchar_stdout(cChr) ; }

int 	vcprintf(const char * format, va_list vArgs) {
	cprintf_lock() ;
	int32_t iRV = xPrint(xPrintToStdout, NULL, xpfMAXLEN_MAXVAL, format, vArgs) ;
	cprintf_unlock() ;
	return iRV ;
}

int 	cprintf(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRetVal = vcprintf(format, vArgs) ;
	va_end(vArgs) ;
	return iRetVal ;
}

/* Output directly to the UART, no Telnet redirection, no FreeRTOS required */
static	int	xPrintToStdoutNoBlock(xpc_t * psXPC, int cChr) { return putchar_stdout_noblock(cChr) ; }

int32_t	cprintf_noblock(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int32_t iRetVal = xPrint(xPrintToStdoutNoBlock, NULL, 128, format, vArgs) ;
	va_end(vArgs) ;
	return iRetVal ;
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
 * 	Alternative is to ensure that the x_printf.obj is specified at the start to be linked with !!
 */
#if		(xpfSUPPORT_ALIASES == 1)		// standard library printf functions
	int		printf(const char * format, ...) __attribute__((alias("xprintf"), unused)) ;
	int		printf_r(const char * format, ...) __attribute__((alias("xprintf"), unused)) ;

	int		vprintf(const char * format, va_list vArgs) __attribute__((alias("xvprintf"), unused)) ;
	int		vprintf_r(const char * format, va_list vArgs) __attribute__((alias("xvprintf"), unused)) ;

	int		sprintf(char * pBuf, const char * format, ...) __attribute__((alias("xsprintf"), unused)) ;
	int		sprintf_r(char * pBuf, const char * format, ...) __attribute__((alias("xsprintf"), unused)) ;

	int		vsprintf(char * pBuf, const char * format, va_list vArgs) __attribute__((alias("xvsprintf"), unused)) ;
	int		vsprintf_r(char * pBuf, const char * format, va_list vArgs) __attribute__((alias("xvsprintf"), unused)) ;

	int 	snprintf(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("xsnprintf"), unused)) ;
	int 	snprintf_r(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("xsnprintf"), unused)) ;

	int 	vsnprintf(char * pBuf, size_t BufSize, const char * format, va_list vArgs) __attribute__((alias("xvsnprintf"), unused)) ;
	int 	vsnprintf_r(char * pBuf, size_t BufSize, const char * format, va_list vArgs) __attribute__((alias("xvsnprintf"), unused)) ;

	int 	fprintf(FILE * stream, const char * format, ...) __attribute__((alias("xfprintf"), unused)) ;
	int 	fprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("xfprintf"), unused)) ;

	int 	vfprintf(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("xvfprintf"), unused)) ;
	int 	vfprintf_r(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("xvfprintf"), unused)) ;

	int 	dprintf(int fd, const char * format, ...) __attribute__((alias("xdprintf"), unused)) ;
	int 	dprintf_r(int fd, const char * format, ...) __attribute__((alias("xdprintf"), unused)) ;

	int 	vdprintf(int fd, const char * format, va_list vArgs) __attribute__((alias("xvdprintf"), unused)) ;
	int 	vdprintf_r(int fd, const char * format, va_list vArgs) __attribute__((alias("xvdprintf"), unused)) ;

	int 	fiprintf(FILE * stream, const char * format, ...) __attribute__((alias("xfprintf"), unused)) ;
	int 	fiprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("xfprintf"), unused)) ;

	int 	vfiprintf(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("xvfprintf"), unused)) ;
	int 	vfiprintf_r(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("xvfprintf"), unused)) ;
#endif

// ##################################### functional tests ##########################################

#define		TEST_INTEGER	0
#define		TEST_STRING		0
#define		TEST_FLOAT		0
#define		TEST_BINARY		0
#define		TEST_ADDRESS	0
#define		TEST_DATETIME	0
#define		TEST_HEXDUMP	0
#define		TEST_WIDTH_PREC	0

void	vPrintfUnitTest(void) {
#if		(TEST_INTEGER == 1)
uint64_t	my_llong = 0x98765432 ;
// Minimums and maximums
	PRINT("\r\nintegers\r\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\r\n") ;
	PRINT("Min/max i8 : %d %d\r\n", INT8_MIN, INT8_MAX) ;
	PRINT("Min/max i16 : %d %d\r\n", INT16_MIN, INT16_MAX) ;
	PRINT("Min/max i32 : %d %d\r\n", INT32_MIN, INT32_MAX) ;
	PRINT("Min/max i64 : %lld %lld\r\n", INT64_MIN, INT64_MAX) ;
	PRINT("Min/max u64 : %llu %llu\r\n", UINT64_MIN, UINT64_MAX) ;

	PRINT("0x%llx , %'lld un/signed long long\r\n", 9876543210ULL, -9876543210LL) ;
	PRINT("0x%llX , %'lld un/signed long long\r\n", 0x0000000076543210ULL, 0x0000000076543210ULL) ;
	PRINT("%'lld , %'llX , %07lld dec-hex-dec(=0 but 7 wide) long long\r\n", my_llong, my_llong, 0ULL) ;
	PRINT("long long: %lld, %llu, 0x%llX, 0x%llx\r\n", -831326121984LL, 831326121984LLU, 831326121984LLU, 831326121984LLU) ;

// left & right padding
	PRINT(" long padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\r\n", 3, 3, 3) ;
	PRINT(" long padding (neg): zero=[%04d], left=[%-+4d], right=[%+4d]\r\n", -3, -3, -3) ;

	PRINT("multiple unsigneds: %u %u %2u %X %u\r\n", 15, 5, 23, 0xb38f, 65535) ;
	PRINT("hex %x = ff, hex 02=%02x\r\n", 0xff, 2) ;		//  hex handling
	PRINT("signed %'d = %'u U = 0x%'X\r\n", -3, -3, -3) ;	//  int formats

	PRINT("octal examples 0xFF = %o , 0x7FFF = %o 0x7FFF7FFF7FFF = %16llo\r\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
	PRINT("octal examples 0xFF = %04o , 0x7FFF = %08o 0x7FFF7FFF7FFF = %016llo\r\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
#endif

#if		(TEST_STRING == 1)
	char 	buf[192] ;
	char	my_string[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ;
	char *	ptr = &my_string[17] ;
	char *	np = NULL ;
	size_t	slen, count ;
	PRINT("\r\nstrings\r\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\r\n") ;
	PRINT("[%d] %s\r\n", xsnprintf(buf, 11, my_string), buf) ;
	PRINT("[%d] %.*s\r\n", 20, 20, my_string) ;
	PRINT("ptr=%s, %s is null pointer, char %c='a'\r\n", ptr, np, 'a');
	PRINT("%d %s(s) with %%\r\n", 0, "message") ;

// test walking string builder
	slen = 0 ;
	slen += xsprintf(buf+slen, "padding (neg): zero=[%04d], ", -3) ;
	slen += xsprintf(buf+slen, "left=[%-4d], ", -3) ;
	slen += xsprintf(buf+slen, "right=[%4d]\r\n", -3) ;
	PRINT("[%d] %s", slen, buf) ;
// left & right justification
	slen = xsprintf(buf, "justify: left=\"%-10s\", right=\"%10s\"\r\n", "left", "right") ;
	PRINT("[len=%d] %s", slen, buf);

	count = 80 ;
	xsnprintf(buf, count, "Only %d buffered bytes should be displayed from this very long string of at least 90 characters", count) ;
	PRINT("%s\r\n", buf) ;
// multiple chars
	xsprintf(buf, "multiple chars: %c %c %c %c\r\n", 'a', 'b', 'c', 'd') ;
	PRINT("%s", buf);
#endif

#if		(TEST_FLOAT == 1)
	float	my_float	= 1000.0 / 9.0 ;
	double	my_double	= 22000.0 / 7.0 ;

	PRINT("\r\nfloat/double\r\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\r\n") ;
	PRINT("DBL MAX=%'.15f MIN=%'.15f\r\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15e MIN=%'.15E\r\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15g MIN=%'.15G\r\n", DBL_MAX, DBL_MIN) ;

	PRINT("float padding (pos): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\r\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\r\n", -my_double, -my_double, -my_double) ;
	PRINT("float padding (pos): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\r\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\r\n", my_double*(-1.0), my_double*(-1.0), my_double*(-1.0)) ;

	PRINT("%'.20f = float(f)\r\n", my_float) ;
	PRINT("%'.20e = float(e)\r\n", my_float) ;
	PRINT("%'.20e = float(e)\r\n", my_float/100.0) ;

	PRINT("%'.7f = double(f)\r\n", my_double) ;
	PRINT("%'.7e = double(e)\r\n", my_double) ;
	PRINT("%'.7E = double(E)\r\n", my_double/1000.00) ;

	PRINT("%'.12g = double(g)\r\n", my_double/10000000.0) ;
	PRINT("%'.12G = double(G)\r\n", my_double*10000.0) ;

	PRINT("%.20f is a double\r\n", 22.0/7.0) ;
	PRINT("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\r\n", 3, -3, 3.0, -3.0, 3, 3.0) ;

	PRINT("multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\r\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;
	PRINT("multiple doubles: %e %.1e %2.0e %.2e %.3e %.2e [%-8.3e]\r\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;

	PRINT("double special cases: %f %.f %.0f %2f %2.f %2.0f\r\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;
	PRINT("double special cases: %e %.e %.0e %2e %2.e %2.0e\r\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;

	PRINT("rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\r\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;
	PRINT("rounding doubles: %.1e %.1e %.3e %.2e [%-8.3e]\r\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;
#endif

#if		(TEST_ADDRESS == 1)
	char MacAdr[6] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 } ;
	PRINT("%I - IP Address (Default)\r\n", 0x01020304UL) ;
	PRINT("%0I - IP Address (PAD0)\r\n", 0x01020304UL) ;
	PRINT("%-I - IP Address (L-Just)\r\n", 0x01020304UL) ;
	PRINT("%#I - IP Address (Rev Default)\r\n", 0x01020304UL) ;
	PRINT("%#0I - IP Address (Rev PAD0)\r\n", 0x01020304UL) ;
	PRINT("%#-I - IP Address (Rev L-Just)\r\n", 0x01020304UL) ;
	PRINT("%m - MAC address (LC)\r\n", &MacAdr[0]) ;
	PRINT("%M - MAC address (UC)\r\n", &MacAdr[0]) ;
	PRINT("%'m - MAC address (LC+sep)\r\n", &MacAdr[0]) ;
	PRINT("%'M - MAC address (UC+sep)\r\n", &MacAdr[0]) ;
#endif

#if		(TEST_BINARY == 1)
	PRINT("%J - Binary 32/32 bit\r\n", 0xF77FA55AUL) ;
	PRINT("%'J - Binary 32/32 bit\r\n", 0xF77FA55AUL) ;
	PRINT("%24J - Binary 24/32 bit\r\n", 0xF77FA55AUL) ;
	PRINT("%'24J - Binary 24/32 bit\r\n", 0xF77FA55AUL) ;
	PRINT("%llJ - Binary 64/64 bit\r\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'llJ - Binary 64/64 bit\r\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%40llJ - Binary 40/64 bit\r\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'40llJ - Binary 40/64 bit\r\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%70llJ - Binary 64/64 bit in 70 width\r\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'70llJ - Binary 64/64 bit in 70 width\r\n", 0xc44c9779F77FA55AULL) ;
#endif

#if		(TEST_DATETIME == 1)
	#if		defined(__TIME__) && defined(__DATE__)
		PRINT("_DATE_TIME_ : %s %s\n", __DATE__, __TIME__) ;
	#endif
	#if		defined(__TIMESTAMP__)
		PRINT("_TIMESTAMP_ : %s\n", __TIMESTAMP__) ;
	#endif
	#if		defined(__TIMESTAMP__ISO__)
		PRINT("TimestampISO: %s\n", __TIMESTAMP__ISO__) ;
	#endif
	PRINT("Normal  (S1): %Z\n", &CurSecs) ;
	PRINT("Elapsed (S1): %!Z\n", &RunSecs) ;
	PRINT("Normal  (S2): %'Z\n", &CurSecs) ;
	PRINT("Elapsed (S2): %!'Z\n", &RunSecs) ;
	PRINT("Normal Alt  : %#Z\n", &CurSecs) ;
//	PRINT("Elapsed Alt: %!#Z\n", &RunSecs) ;		Invalid, CRASH !!!
#endif

#if		(TEST_HEXDUMP == 1)
	uint8_t DumpData[] = "0123456789abcdef0123456789ABCDEF~!@#$%^&*()_+-={}[]:|;'\\<>?,./`01234" ;
	#define DUMPSIZE	(sizeof(DumpData)-1)
//	PRINT("DUMP absolute lc byte\r\n%+b", DUMPSIZE, DumpData) ;
//	PRINT("DUMP absolute lc byte\r\n%'+b", DUMPSIZE, DumpData) ;

//	PRINT("DUMP absolute UC half\r\n%+H", DUMPSIZE, DumpData) ;
//	PRINT("DUMP absolute UC half\r\n%'+H", DUMPSIZE, DumpData) ;

//	PRINT("DUMP relative lc word\r\n%!+w", DUMPSIZE, DumpData) ;
//	PRINT("DUMP relative lc word\r\n%!'+w", DUMPSIZE, DumpData) ;

//	PRINT("DUMP relative UC dword\r\n%!+llW", DUMPSIZE, DumpData) ;
//	PRINT("DUMP relative UC dword\r\n%!'+llW", DUMPSIZE, DumpData) ;
	for (int32_t idx = 0; idx < 16; idx++) {
		PRINT("\r\nDUMP relative lc BYTE %!'+b", idx, DumpData) ;
	}
#endif

#if		(TEST_WIDTH_PREC == 1)
	PRINT("String : Minwid=5  Precis=8  : %*.*s\r\n",  5,  8, "0123456789ABCDEF") ;
	PRINT("String : Minwid=30 Precis=15 : %*.*s\r\n", 30, 15, "0123456789ABCDEF") ;

	double	my_double	= 22000.0 / 7.0 ;
	PRINT("Float  : Variables  5.8  : %*.*f\r\n",  5,  8, my_double) ;
	PRINT("Float  : Specified  5.8  : %5.8f\r\n", my_double) ;
	PRINT("Float  : Variables 30.14 : %*.*f\r\n",  30,  14, my_double) ;
	PRINT("Float  : Specified 30.14 : %30.14f\r\n", my_double) ;
#endif
}
