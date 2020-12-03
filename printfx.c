/*
  * Copyright 2014-20 Andre M Maree / KSS Technologies (Pty) Ltd.
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
 * <snf>printfx -  set of routines to replace equivalent printf functionality
 *
 * History:
 * --------
 *	Originally used source as per George Menie and Daniel D Miller.
 */


#include	"printfx.h"
#include	"FreeRTOS_Support.h"
#include	"x_string_general.h"						// xinstring function
#include	"x_errors_events.h"
#include	"x_values_to_string.h"
#include	"x_sockets.h"
#include	"x_ubuf.h"
#include	"x_stdio.h"
#include	"x_struct_union.h"
#include	"x_utilities.h"

#if		defined(ESP_PLATFORM)
	#include	"hal_config.h"
	#include	"hal_debug.h"							// need ASSERT
	#include	"hal_nvic.h"
#else
	#include	"alt_config.h"
#endif

#include	<string.h>
#include	<math.h>									// isnan()
#include	<float.h>									// DBL_MIN/MAX

#define	debugFLAG					(0xC000)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ######################## Character and value translation & rounding tables ######################

const char vPrintStr1[] = {						// table of characters where lc/UC is applicable
		'X',									// hex formatted 'x' or 'X' values, always there
#if		(xpfSUPPORT_HEXDUMP == 1)
		'B', 'H', 'W',							// byte, half and word UC/LC formatting
#endif
#if		(xpfSUPPORT_MAC_ADDR == 1)
		'M',									// MAC address UC/LC
#endif
#if		(xpfSUPPORT_IEEE754 == 1)
		'E', 'G',								// float exponential of general
#endif
#if		(xpfSUPPORT_POINTER == 1)
		'P',									// Pointer lc/0x or UC/0X
#endif
		'\0' } ;								// terminated

static	const char hexchars[] = "0123456789ABCDEF" ;

static	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
	0.5, 			0.05, 			0.005, 			0.0005,			0.00005, 			0.000005, 			0.0000005, 			0.00000005,
	0.000000005,	0.0000000005, 	0.00000000005, 	0.000000000005,	0.0000000000005, 	0.00000000000005, 	0.000000000000005, 	0.0000000000000005
} ;

// ############################## private function variables #######################################

SemaphoreHandle_t	printfxMux = NULL, cprintfxMux = NULL ;

// ############################# Foundation character and string output ############################

/**
 * vPrintChar
 * \brief	output a single character using/via the preselected function
 * 			before output verify against specified width
 * 			adjust character count (if required) & remaining width
 * \param	psXPC - pointer to PrintFX control structure to be referenced/updated
 * 			c - char to be output
 * \return	1 or 0 based on whether char was '\000'
 */
void	vPrintChar(xpc_t * psXPC, char cChr) {
#if 	(xpfSUPPORT_FILTER_NUL == 1)
		if (cChr == CHR_NUL)
			return ;
#endif
	if (psXPC->f.maxlen == 0 || psXPC->f.curlen < psXPC->f.maxlen) {
		if (psXPC->handler(psXPC, cChr) == cChr)		// successfully written ?
			psXPC->f.curlen++ ;							// yes, adjust count
	}
}

/**
 * xPrintChars()
 * \brief	perform a RAW string output to the selected "stream"
 * \brief	Does not perform ANY padding, justification or length checking
 * \param	psXPC - pointer to PrintFX structure controlling the operation
 * 			string - pointer to the string to be output
 * \return	number of ACTUAL characters output.
 */
int32_t	xPrintChars (xpc_t * psXPC, char * pStr) {
	int32_t len ;
	for (len = 0; *pStr; vPrintChar(psXPC, *pStr++), ++len) ;
	return len ;
}

/**
 * vPrintString
 * \brief	perform formatted output of a string to the preselected device/string/buffer/file
 * \param	psXPC - pointer to PrintFX structure controlling the operation
 * 			string - pointer to the string to be output
 * \return	number of ACTUAL characters output.
 */
void	vPrintString (xpc_t * psXPC, char * pStr) {
// Required to avoid crash when wifi message is intercepted and a string pointer parameter
// is evaluated as out of valid memory address (0xFFFFFFE6). Replace string with "pOOR"
// Following line was used to test for the details of the anomaly
//	if (pStr != NULL && INRANGE_MEM(pStr) == false) ets_printf("pOOR 0x%08x\n", pStr) ;
	pStr = INRANGE_MEM(pStr) ? pStr : pStr == NULL ? STRING_NULL : STRING_OOR ;
	int32_t Len = xstrlen(pStr) ;
	int32_t PadLen = psXPC->f.minwid > Len ? psXPC->f.minwid - Len : 0 ;	// calc required number of padding chars
	int32_t PadChr = psXPC->f.pad0 ? CHR_0 : CHR_SPACE ;
	if (psXPC->f.ljust == 0 && PadLen)					// If right justified & we must pad
		while (PadLen--)	vPrintChar(psXPC, PadChr) ;	// do left pad

	uint32_t precis = psXPC->f.precis ? psXPC->f.precis : Len ;
	while (*pStr && precis--)vPrintChar(psXPC, *pStr++) ;

	if (psXPC->f.ljust && PadLen)						// If left justified & we must pad
		while (PadLen--)	vPrintChar(psXPC, PadChr) ;	// do right pad
}

/**
 * cPrintNibbleToChar()
 * \brief		returns a pointer to the hexadecimal character representing the value of the low order nibble
 * 				Based on the Flag, the pointer will be adjusted for UpperCase (if required)
 * \param[in]	Val = value in low order nibble, to be converted
 * \return		pointer to the correct character
 */
char	cPrintNibbleToChar(xpc_t * psXPC, uint8_t Value) {
	char cChr = hexchars[Value] ;
	if (psXPC->f.Ucase == 0 && Value > 9)
		cChr += 0x20 ;									// convert to lower case
	return cChr ;
}

/**
 * xPrintXxx() convert uint64_t value to a formatted string (right to left, L <- R)
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
 * Protection against buffer overflow based on correct sized buffer being allocated in calling function
 */
int32_t	xPrintXxx(xpc_t * psXPC, uint64_t ullVal, char * pBuffer, size_t BufSize) {
	int32_t	Len = 0, Count = 0, iTemp = 0 ;
	char * pTemp = pBuffer + BufSize - 1 ;				// Point to last space in buffer
	if (ullVal) {
#if		(xpfSUPPORT_SCALING == 1)
		uint8_t	ScaleChr = CHR_NUL ;
		if (psXPC->f.alt_form) {
			if (ullVal > 10000000000000ULL) {
				ullVal		/= 1000000000000ULL ;
				ScaleChr	= CHR_T ;
			} else if (ullVal > 10000000000ULL) {
				ullVal		/= 1000000000ULL ;
				ScaleChr	= CHR_B ;
			} else if (ullVal > 10000000ULL) {
				ullVal		/= 1000000ULL ;
				ScaleChr	= CHR_M ;
			} else if (ullVal > 10000ULL) {
				ullVal		/= 1000ULL ;
				ScaleChr	= CHR_K ;
			}
			if (ScaleChr != CHR_NUL) {
				*pTemp-- = ScaleChr ;
				++Len ;
			}
		}
#endif
		// convert to string starting at end of buffer from Least (R) to Most (L) significant digits
		while (ullVal) {
			iTemp	= ullVal % psXPC->f.nbase ;			// calculate the next remainder ie digit
			*pTemp-- = cPrintNibbleToChar(psXPC, iTemp) ;
			++Len ;
			ullVal	/= psXPC->f.nbase ;
			if (ullVal && psXPC->f.group) {				// handle digit grouping, if required
				if ((++Count % 3) == 0) {
					*pTemp-- = CHR_COMMA ;
					++Len ;
					Count = 0 ;
				}
			}
		}
	} else {
		*pTemp-- = CHR_0 ;
		Len = 1 ;
	}

	// First check if ANY form of padding required
	if (psXPC->f.ljust == 0) {							// right justified (ie pad left) ???
	/* this section ONLY when value is RIGHT justified.
	 * For ' ' padding format is [       -xxxxx]
	 * whilst '0' padding it is  [-0000000xxxxx]
	 */
		Count = (psXPC->f.minwid > Len) ? psXPC->f.minwid - Len : 0 ;
	// If we are padding with ' ' and leading '+' or '-' is required, do that first
		if (psXPC->f.pad0 == 0 && (psXPC->f.negvalue || psXPC->f.plus)) {	// If a sign is required
			*pTemp-- = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;			// start by prepend of '+' or '-'
			--Count ;
			++Len ;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXPC->f.pad0 ? CHR_0 : CHR_SPACE ;	// define applicable padding char
			Len += Count ;								// Now do the actual padding
			while (Count--) *pTemp-- = iTemp ;
		}
	// If we are padding with '0' AND a sign is required (-value or +requested), do that first
		if (psXPC->f.pad0 && (psXPC->f.negvalue || psXPC->f.plus)) {	// If +/- sign is required
			if (pTemp[1] == CHR_SPACE || pTemp[1] == CHR_0) {
				++pTemp ;								// set overwrite last with '+' or '-'
			} else {
				++Len ;									// set to add extra '+' or '-'
			}
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;
		}
	} else {
		if (psXPC->f.negvalue || psXPC->f.plus) {		// If a sign is required
			*pTemp = psXPC->f.negvalue ? CHR_MINUS : CHR_PLUS ;	// just prepend '+' or '-'
			++Len ;
		}
	}
	return Len ;
}

void	vPrintX64(xpc_t * psXPC, uint64_t Value) {
	char 	Buffer[xpfMAX_LEN_X64] ;
	Buffer[xpfMAX_LEN_X64 - 1] = CHR_NUL ;				// terminate the buffer, single value built R to L
	int32_t Len = xPrintXxx(psXPC, Value, Buffer, xpfMAX_LEN_X64 - 1) ;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_X64 - 1 - Len)) ;
}

/**
 * vPrintF64()
 * \brief	convert double value based on flags supplied and output via control structure
 * \param[in]	psXPC - pointer to PrintFX control structure
 * \param[in]	dbl - double value to be converted
 * \return		none
 *
 * References:
 * 	http://git.musl-libc.org/cgit/musl/blob/src/stdio/vfprintf.c?h=v1.1.6
 */
void	vPrintF64(xpc_t * psXPC, double f64Val) {
	if (isnan(f64Val)) {
		vPrintString(psXPC, (char *)"{NaN}") ;
		return ;
	}
	char	Buffer[xpfMAX_LEN_F64] ;
	int32_t Len = 0 ;
	psXPC->f.negvalue	= f64Val < 0.0 ? 1 : 0 ;		// set negvalue if < 0.0
	f64Val				*= psXPC->f.negvalue ? -1.0 : 1.0 ;	// convert to positive number
	xpf_t	xpf ;
	xpf.limits			= psXPC->f.limits ;				// save original flags
	xpf.flags			= psXPC->f.flags ;
	xpf.precis			= psXPC->f.precis ;
	x64_t x64Value 		= { 0 } ;

	int32_t	Exponent = 0 ;								// if exponential format requested, calculate the exponent
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

	if (psXPC->f.form == xpfFORMAT_0_G) {				// if 'gG' specified check exponent range and select applicable mode.
		// if explicit precision is '0' take it as '1' (https://pubs.opengroup.org/onlinepubs/007908799/xsh/fprintf.html)
		psXPC->f.precis = psXPC->f.precis ? psXPC->f.precis : 1 ;
		if (Exponent < -4 || Exponent >= psXPC->f.precis) {
			psXPC->f.form = xpfFORMAT_2_E ;				// force 'eE' format
		} else {
			psXPC->f.form = xpfFORMAT_1_F ;				// force 'f' format
		}
	}

	if (psXPC->f.form == xpfFORMAT_2_E)					// based on format change value if exponent format
		f64Val = x64Value.f64 ;							// change to exponent adjusted value

	if (f64Val < (DBL_MAX - round_nums[psXPC->f.precis]))	// if addition of rounding value will NOT cause overflow.
		f64Val += round_nums[psXPC->f.precis] ;			// round by adding .5LSB to the value

	Buffer[xpfMAX_LEN_F64 - 1] = CHR_NUL ;				// building R to L, ensure buffer NULL-term

	if (psXPC->f.form == xpfFORMAT_2_E) {				// If required, handle the exponent
		psXPC->f.minwid		= 2 ;
		psXPC->f.pad0		= 1 ;						// MUST left pad with '0'
		psXPC->f.signval	= 1 ;
		psXPC->f.ljust		= 0 ;
		psXPC->f.negvalue	= (Exponent < 0) ? 1 : 0 ;
		Exponent *= psXPC->f.negvalue ? -1LL : 1LL ;
		Len += xPrintXxx(psXPC, Exponent, Buffer, xpfMAX_LEN_F64 - 1) ;
		Buffer[xpfMAX_LEN_F64-2 -Len++] = psXPC->f.Ucase ? 'E' : 'e' ;
	}

	if (psXPC->f.precis > 0)	{						// if decimal digits requested, convert fractional portion?
		uint64_t	m ;									// calc multiplicant to convert fractional portion to whole number
		int32_t		i ;
		x64Value.f64	= f64Val - (uint64_t)f64Val ;	// isolate fraction as double
		for (i = 0, m = 1 ; i < psXPC->f.precis ; ++i)
			m *= 10 ;
		x64Value.f64	= x64Value.f64 * (double)m ;	// convert fraction to integer [+fraction]
		x64Value.u64	= (uint64_t)x64Value.f64 ;		// extract integer portion, discard remaining fraction of "fraction"

		if (x64Value.u64 || psXPC->f.radix) {			// if fraction<>0 or radix specified, something to be displayed...
			psXPC->f.minwid		= psXPC->f.precis ;	// do the (now decimal) fractional portion
			psXPC->f.pad0		= 1 ;					// MUST left pad with '0'
			psXPC->f.group		= 0 ;					// cannot group in fractional
			psXPC->f.signval	= 0 ;					// always unsigned value
			psXPC->f.negvalue	= 0 ;					// and never negative
			psXPC->f.plus		= 0 ;					// no leading +/- before fractional part
			Len += xPrintXxx(psXPC, x64Value.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len) ;
			Buffer[xpfMAX_LEN_F64-2 -Len++] = CHR_FULLSTOP ;	// handle the separator
		}
	}

	x64Value.u64	= f64Val ;							// extract and convert the whole number portions
	psXPC->f.limits	= xpf.limits ;						// restore original limits & flags
	psXPC->f.flags	= xpf.flags ;

	// adjust minwid to do padding (if required) based on string length after adding whole number
	psXPC->f.minwid	= psXPC->f.minwid > Len ? psXPC->f.minwid - Len : 0 ;
	Len += xPrintXxx(psXPC, x64Value.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len) ;
	psXPC->f.limits	= xpf.limits ;						// restore original limits & flags
	psXPC->f.flags	= xpf.flags ;
	psXPC->f.precis	= 0 ;								// enable full string to be output (subject to minwid padding on right)
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_F64 - 1 - Len)) ;
	psXPC->f.precis	= xpf.precis ;
}

/**
 * vPrintHexU8()
 * \brief		write char value as 2 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to PrintFX control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU8(xpc_t * psXPC, uint8_t Value) {
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value >> 4)) ;
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value & 0x0F)) ;
}

/**
 * vPrintHexU16()
 * \brief		write uint16_t value as 4 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to PrintFX control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU16(xpc_t * psXPC, uint16_t Value) {
	vPrintHexU8(psXPC, (Value >> 8) & 0x000000FF) ;
	vPrintHexU8(psXPC, Value & 0x000000FF) ;
}

/**
 * vPrintHexU32()
 * \brief		write uint32_t value as 8 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to PrintFX control structure
 * \param[in]	Value - unsigned byte to convert
 * return		none
 */
void	vPrintHexU32(xpc_t * psXPC, uint32_t Value) {
	vPrintHexU16(psXPC, (Value >> 16) & 0x0000FFFF) ;
	vPrintHexU16(psXPC, Value & 0x0000FFFF) ;
}

/**
 * vPrintHexU64()
 * \brief		write uint64_t value as 16 hex chars to the buffer, always NULL terminated
 * \param[in]	psXPC - pointer to PrintFX control structure
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
	int32_t	Size = 1 << psXPC->f.size ;
	if (psXPC->f.alt_form)								// '#' specified to invert order ?
		pStr += Num - Size ;							// working backwards so point to last

	x64_t	x64Val ;
	int32_t	Idx	= 0 ;
	while (Idx < Num) {
		switch (psXPC->f.size) {
		case 0:	x64Val.u8[0]  = *((uint8_t *) pStr) ; 	vPrintHexU8(psXPC, x64Val.u8[0]) ; 		break ;
		case 1:	x64Val.u16[0] = *((uint16_t *) pStr) ;	vPrintHexU16(psXPC, x64Val.u16[0]) ;	break ;
		case 2:	x64Val.u32[0] = *((uint32_t *) pStr) ;	vPrintHexU32(psXPC, x64Val.u32[0]) ;	break ;
		case 3:	x64Val.u64	  = *((uint64_t *) pStr) ;	vPrintHexU64(psXPC, x64Val.u64) ; 		break ;
		}
		// step to the next 8/16/32/64 bit value
		if (psXPC->f.alt_form)							// '#' specified to invert order ?
			pStr -= Size ;								// update source pointer backwards
		else
			pStr += Size ;								// update source pointer forwards
		Idx += Size ;
		if (Idx >= Num) {
			break ;
		}
		// now handle the grouping separator(s) if any
		if (psXPC->f.form == xpfFORMAT_0_G)				// no separator required?
			continue ;
		vPrintChar(psXPC, psXPC->f.form == xpfFORMAT_1_F ? CHR_COLON :
						  psXPC->f.form == xpfFORMAT_2_E ? CHR_MINUS :
						  psXPC->f.form == xpfFORMAT_3 ? (
							(Idx % 8) == 0 ? CHR_SPACE :
							(Idx % 4) == 0 ? CHR_VERT_BAR :
							(Idx % 2) == 0 ? CHR_MINUS : CHR_COLON ) : '?') ;
	}
}

// ############################### Proprietary extensions to printf() ##############################

/**
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
	seconds_t	Seconds ;
	// Get seconds value to use... (adjust for TZ if required/allowed)
	if (psTSZ->pTZ &&									// TZ info available
		psXPC->f.plus == 1 &&							// TZ info requested
		psXPC->f.alt_form == 0 &&						// NOT alt_form (always GMT/UTC)
		psXPC->f.rel_val == 0) {						// NOT relative time (has no TZ info)
		Seconds = xTimeCalcLocalTimeSeconds(psTSZ) ;
	} else
		Seconds = xTimeStampAsSeconds(psTSZ->usecs) ;

	if (psTM)											// convert seconds into components
		xTimeGMTime(Seconds, psTM, psXPC->f.rel_val) ;
	return Seconds ;
}

uint32_t u32pow(uint32_t base, uint32_t exp) {
	uint32_t res ;
	for(res = 1; exp > 0; res *= base, --exp) ;
	return res ;
size_t	xPrintDate_Year(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 0 ;
	size_t Len = xPrintXxx(psXPC, (uint64_t) (psTM->tm_year + YEAR_BASE_MIN), pBuffer, 4) ;
	if (psXPC->f.alt_form == 0)							// no extra ' ' at end for alt_form
		pBuffer[Len++] = psXPC->f.form == xpfFORMAT_3 ? CHR_FWDSLASH : CHR_MINUS ;
	return Len ;
}

size_t	xPrintDate_Month(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 2 ;
	size_t Len = xPrintXxx(psXPC, (uint64_t) (psTM->tm_mon + 1), pBuffer, 2) ;
	pBuffer[Len++] = psXPC->f.alt_form ? CHR_SPACE :
					(psXPC->f.form == xpfFORMAT_3) ? CHR_FWDSLASH : CHR_MINUS ;
	return Len ;
}

size_t	xPrintDate_Day(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	size_t szBuf ;
	if (psXPC->f.rel_val) {
		psXPC->f.minwid	= 1 ;
		szBuf = xDigitsInU32(psTM->tm_mday, psXPC->f.group) ;
	} else {
		psXPC->f.minwid	= 2 ;
		szBuf = 2 ;
	}
	size_t Len = xPrintXxx(psXPC, (uint64_t) psTM->tm_mday, pBuffer, szBuf) ;
	pBuffer[Len++] = psXPC->f.alt_form ? CHR_SPACE :
					(psXPC->f.form == xpfFORMAT_3 && psXPC->f.rel_val) ? CHR_d :
					(psXPC->f.form == xpfFORMAT_3) ? CHR_SPACE : CHR_T ;
	psXPC->f.pad0	= 1 ;
	return Len ;
}

void	vPrintDate(xpc_t * psXPC, struct tm * psTM) {
	size_t	Len = 0 ;
	char	Buffer[xpfMAX_LEN_DATE] ;
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	if (psXPC->f.alt_form) {
		Len += xstrncpy(Buffer, xTimeGetDayName(psTM->tm_wday), 3) ;		// "Sun"
		Len += xstrncpy(Buffer + Len, ", ", 2) ;							// "Sun, "
		Len += xPrintDate_Day(psXPC, psTM, Buffer + Len) ;					// "Sun, 10 "
		Len += xstrncpy(Buffer + Len, xTimeGetMonthName(psTM->tm_mon), 3) ;	// "Sun, 10 Sep"
		Len += xstrncpy(Buffer + Len, " ", 1) ;								// "Sun, 10 Sep "
		Len += xPrintDate_Year(psXPC, psTM, Buffer + Len) ;					// "Sun, 10 Sep 2017"
	} else {
		if (psXPC->f.rel_val == 0) {					// if epoch value do YEAR+MON
			Len += xPrintDate_Year(psXPC, psTM, Buffer + Len) ;
			Len += xPrintDate_Month(psXPC, psTM, Buffer + Len) ;
		}
		if (psTM->tm_mday || psXPC->f.pad0)
			Len += xPrintDate_Day(psXPC, psTM, Buffer + Len) ;
	}
	Buffer[Len] = CHR_NUL ;								// converted L to R, so terminate
	psXPC->f.limits	= 0 ;								// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer) ;
}

void	vPrintTimeUSec(xpc_t * psXPC, uint64_t uSecs) {
	struct	tm 	sTM ;
	xTimeGMTime(xTimeStampAsSeconds(uSecs), &sTM, psXPC->f.abs_rel) ;
	xpf_t xpf ;
	xpf.precis	= psXPC->f.precis ;
	xpf.minwid	= psXPC->f.minwid ;
	psXPC->f.plus	= 0 ;								// ensure no '+' sign printed
void	vPrintTime(xpc_t * psXPC, struct tm * psTM, uint32_t uSecs) {
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	psXPC->f.minwid = 2 ;
	size_t	Len ;
	char	Buffer[xpfMAX_LEN_TIME] ;
	// Part 1: hours
	if (psTM->tm_hour || psXPC->f.pad0) {
		Len = xPrintXxx(psXPC, (uint64_t) psTM->tm_hour, Buffer, 2) ;
		Buffer[Len++]	= (psXPC->f.form == xpfFORMAT_3) ? CHR_h :  CHR_COLON ;
		psXPC->f.pad0	= 1 ;
	} else
		Len = 0 ;

	// Part 2: minutes
	if (psTM->tm_min || psXPC->f.pad0) {
		Len += xPrintXxx(psXPC, (uint64_t) psTM->tm_min, Buffer+Len, 2) ;
		Buffer[Len++]	= (psXPC->f.form == xpfFORMAT_3) ? CHR_m :  CHR_COLON ;
		psXPC->f.pad0	= 1 ;
	}

	// Part 3: seconds
	Len += xPrintXxx(psXPC, (uint64_t) psTM->tm_sec, Buffer+Len, 2) ;

	// Part 4: [.xxxxxx]
	if (psXPC->f.radix && psXPC->f.alt_form == 0) {
		Buffer[Len++]	= (psXPC->f.form == xpfFORMAT_3) ? CHR_s :  CHR_FULLSTOP ;
		psXPC->f.precis	= psXPC->f.precis == 0 ? 3 : psXPC->f.precis > 6 ? 6 : psXPC->f.precis ;
		if (psXPC->f.precis < 6)
			uSecs /= u32pow(10, 6 - psXPC->f.precis) ;
		psXPC->f.pad0	= 1 ;						// need leading '0's
		psXPC->f.signval= 0 ;
		psXPC->f.ljust	= 0 ;						// force R-just
		psXPC->f.minwid	= psXPC->f.precis ;
		Len += xPrintXxx(psXPC, uSecs, Buffer+Len, psXPC->f.precis) ;
	}

	Buffer[Len] = CHR_NUL ;
	psXPC->f.limits	= 0 ;							// enable full string
	vPrintString(psXPC, Buffer) ;
}


void	vPrintDateUSec(xpc_t * psXPC, uint64_t uSecs) {
	struct tm 	sTM ;
	seconds_t Seconds ;
	xTimeGMTime(Seconds = xTimeStampAsSeconds(uSecs), &sTM, psXPC->f.abs_rel) ;
	psXPC->f.plus	= 0 ;								// ensure no '+' sign printed
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	psXPC->f.pad0	= psXPC->f.abs_rel ? 0 : 1 ;
	size_t	Len ;
	char	Buffer[xpfMAX_LEN_DATE] ;
	if (psXPC->f.alt_form && psXPC->f.abs_rel) 			// cannot have Alt form with relative value
		psXPC->f.alt_form = 0 ;							// force to normal format output.

	if (psXPC->f.alt_form) {
		Len = xstrncpy(Buffer, (char *) xTimeGetDayName(sTM.tm_wday), 3) ;			// "Sun"
		Len += xstrncpy(&Buffer[Len], (char *) ", ", 2) ;							// "Sun, "
		Len += xPrintDate_Day(psXPC, &sTM, &Buffer[Len]) ;							// "Sun, 10 "
		Len += xstrncpy(&Buffer[Len], (char *) xTimeGetMonthName(sTM.tm_mon), 3) ;	// "Sun, 10 Sep"
		Len += xstrncpy(&Buffer[Len], (char *) " ", 1) ;							// "Sun, 10 Sep "
		Len += xPrintDate_Year(psXPC, &sTM, &Buffer[Len]) ;							// "Sun, 10 Sep 2017"
	} else {
		Len = 0 ;
		if (sTM.tm_year > 0 || psXPC->f.pad0)
			Len += xPrintDate_Year(psXPC, &sTM, &Buffer[Len]) ;
		if (sTM.tm_mon > 0 || psXPC->f.pad0 || psXPC->f.year_ok)
			Len += xPrintDate_Month(psXPC, &sTM, &Buffer[Len]) ;
		if (Seconds >= SECONDS_IN_DAY || psXPC->f.pad0 || psXPC->f.mon_ok)
			Len += xPrintDate_Day(psXPC, &sTM, &Buffer[Len]) ;
	}
	// converted L to R, so terminate
	Buffer[Len] = CHR_NUL ;
	// then send the formatted output to the correct stream
	uint32_t precis = psXPC->f.precis ;
	psXPC->f.precis	= 0 ;								// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer) ;
	psXPC->f.precis = precis ;
}

void	vPrintDateTimeUSec(xpc_t * psXPC, uint64_t uSecs) {
	vPrintDateUSec(psXPC, uSecs) ;
	vPrintTimeUSec(psXPC, uSecs) ;
	if (psXPC->f.no_zone == 0 && psXPC->f.abs_rel == 0)
		vPrintChar(psXPC, CHR_Z) ;						// If not coming from vPrintDateTimeZone() AND not relative time
}

/**
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
			psXPC->f.signval	= 1 ;					// TZ hours offset is a signed value
			psXPC->f.plus		= 1 ;					// force display of sign
			psXPC->f.pad0		= 1 ;
			Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3) ;
		// insert separating ':' or 'h'
			Buffer[Len++]	= psXPC->f.form ? CHR_h : CHR_COLON ;
		// add the minutes
			psXPC->f.signval	= 0 ;					// TZ offset minutes unsigned
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
			IF_myASSERT(debugTRACK, 0) ;
		}
	}
// terminate string in buffer then out as per normal...
	Buffer[Len]		= CHR_NUL ;							// converted L to R, so add terminating NUL
	psXPC->f.minwid	= 0 ;								// no explicit limit on width
	psXPC->f.precis	= 0 ;								// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer) ;
}

/**
 * vPrintURL() -
 * @param psXPC
 * @param pString
 */
void	vPrintURL(xpc_t * psXPC, char * pStr) {
	if (INRANGE_MEM(pStr)) {
		char cIn ;
		while ((cIn = *pStr++) != CHR_NUL) {
			if (INRANGE(CHR_A, cIn, CHR_Z, char) ||
				INRANGE(CHR_a, cIn, CHR_z, char) ||
				INRANGE(CHR_0, cIn, CHR_9, char) ||
				cIn == CHR_MINUS || cIn == CHR_FULLSTOP || cIn == CHR_UNDERSCORE || cIn == CHR_TILDE) {
				vPrintChar(psXPC, cIn) ;
			} else {
				vPrintChar(psXPC, CHR_PERCENT) ;
				vPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn >> 4)) ;
				vPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn & 0x0F)) ;
			}
		}
	} else
		vPrintString(psXPC, pStr) ;						// "null" or "pOOR"
}

/**
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
	uint32_t	Count ;
	for (uint32_t Idx = 0; Idx < Len; Idx += xpfHEXDUMP_WIDTH) {
		if (psXPC->f.ljust == 0) {						// display absolute or relative address
			vPrintPointer(psXPC, psXPC->f.abs_rel ? Idx : (uint32_t) (pStr + Idx)) ;
			xPrintChars(psXPC, (char *) ": ") ;
		}
		// then the actual series of values in 8-32 bit groups
		int32_t Width = ((Len - Idx) > xpfHEXDUMP_WIDTH) ? xpfHEXDUMP_WIDTH : Len - Idx ;
		vPrintHexValues(psXPC, Width, pStr + Idx) ;
		if (psXPC->f.plus == 1) {						// handle values dumped as ASCII chars
		// handle space padding for ASCII dump to line up
			Count = (Len > xpfHEXDUMP_WIDTH) ? ((xpfHEXDUMP_WIDTH - Width) / Size) * ((Size * 2) + (psXPC->f.form ? 1 : 0)) + 1 : 1 ;
			while (Count--)	vPrintChar(psXPC, CHR_SPACE) ;
			// handle same values dumped as ASCII characters
			for (Count = 0; Count < Width; ++Count) {
				uint32_t cChr = *(pStr + Idx + Count) ;
			#if 0										// Device supports characters in range 0x80 to 0xFE
				vPrintChar(psXPC, (cChr < CHR_SPACE || cChr == CHR_DEL || cChr == 0xFF) ? CHR_FULLSTOP : cChr) ;
			#else										// Device does NOT support ANY characters > 0x7E
				vPrintChar(psXPC, (cChr < CHR_SPACE || cChr >= CHR_DEL) ? CHR_FULLSTOP : cChr) ;
			#endif
			}
			vPrintChar(psXPC, CHR_SPACE) ;
		}
		if (Idx < Len && Len > xpfHEXDUMP_WIDTH)
			xPrintChars(psXPC, (char *) "\n") ;
	}
}

/**
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
	if (psXPC->f.minwid == 0)							// not specified
		len = (psXPC->f.llong == 1) ? 64 : 32 ;			// set to 32 or 64 ...
	else if (psXPC->f.llong == 1)						// width specified, but check validity...
		len = (psXPC->f.minwid > 64) ? 64 : psXPC->f.minwid ;
	else
		len = (psXPC->f.minwid > 32) ? 32 : psXPC->f.minwid ;
	uint64_t mask	= 1ULL << (len - 1) ;
	psXPC->f.form	= psXPC->f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
	while (mask) {
		vPrintChar(psXPC, (ullVal & mask) ? CHR_1 : CHR_0) ;
		mask >>= 1;
	// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
		if (--len && psXPC->f.form) {
			vPrintChar(psXPC, len % 32 == 0 ? CHR_VERT_BAR :	// word boundary
							  len % 16 == 0 ? CHR_COLON :		// short boundary
							  len % 8 == 0 ? CHR_SPACE :		// byte boundary
							  len % 4 == 0 ? CHR_MINUS : '?') ;	// nibble
		}
	}
}

/**
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
	psXPC->f.signval	= 0 ;							// ensure interpreted as unsigned
	psXPC->f.plus		= 0 ;							// and no sign to be shown
	uint8_t * pChr = (uint8_t *) &Val ;
	if (psXPC->f.alt_form) {							// '#' specified to invert order ?
		psXPC->f.alt_form = 0 ;							// clear so not to confuse xPrintXxx()
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
	for (Idx = 0, Len = 0; Idx < sizeof(Val); ++Idx) {
		uint64_t u64Val = (uint8_t) pChr[Idx] ;
		Len += xPrintXxx(psXPC, u64Val, Buffer + xpfMAX_LEN_IP - 5 - Len, 4) ;
		if (Idx < 3)
			Buffer[xpfMAX_LEN_IP - 1 - ++Len] = CHR_FULLSTOP ;
	}
	// then send the formatted output to the correct stream
	psXPC->f.precis	= 0 ;							// enable full string (subject to minwid)
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_IP - 1 - Len)) ;
}

/**
 * vPrintSetGraphicRendition() - set starting and ending fore/background colors
 * @param psXPC
 * @param Val	U32 value treated as 4x U8 being SGR color/attribute codes
 */
void	vPrintSetGraphicRendition(xpc_t * psXPC, uint32_t Val) {
	char * 	pTmp, Buffer[xpfMAX_LEN_SGR] ;
	sgr_info_t sSGR ;
	sSGR.u32 = Val ;
	pTmp	= Buffer ;
	*pTmp++	= CHR_ESC ;
	*pTmp++	= CHR_L_SQUARE ;
	pTmp	+= xU32ToDecStr(sSGR.a, pTmp) ;
	for (int32_t Idx = 2; sSGR.u8[Idx] && Idx >= 0; --Idx) {
		*pTmp++	= CHR_SEMICOLON ;
		pTmp	+= xU32ToDecStr(sSGR.u8[Idx], pTmp) ;
	}
	*pTmp++ = CHR_m ;
	*pTmp	= CHR_NUL ;									// terminate
	xPrintChars(psXPC, Buffer) ;
}

/* ################################# The HEART of the PRINTFX matter ###############################
 * PrintFX - common routine for formatted print functionality
 * \brief	parse the format string and interpret the conversions, flags and modifiers
 * 			extract the parameters variables in correct type format
 * 			call the correct routine(s) to perform conversion, formatting & output
 * \param	psXPC - pointer to structure containing formatting and output destination info
 * 			format - pointer to the formatting string
 * 			vArgs - variable number of arguments
 * \return	void (other than updated info in the original structure passed by reference
 */

int		PrintFX(int (handler)(xpc_t *, int), void * pVoid, size_t BufSize, const char * format, va_list vArgs) {
//	IF_myASSERT(debugPARAM, INRANGE_MEM(handler) && INRANGE_MEM(format)) ;
	xpc_t	sXPC ;
	sXPC.handler	= handler ;
	sXPC.pVoid		= pVoid ;
	sXPC.f.maxlen	= (BufSize > xpfMAXLEN_MAXVAL) ? xpfMAXLEN_MAXVAL : BufSize ;
	sXPC.f.curlen	= 0 ;

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
					if (U32 >= xpfMINWID_MAXVAL) {		// if value out of range, limit...
						U32 = xpfMINWID_MAXVAL - 1 ;
					}
					sXPC.f.minwid	= U32 ;
					sXPC.f.arg_width= 1 ;
					break ;
				case 4:									// '+' force leading +/- signed
					++format ;							// or HEXDUMP add ASCII char dump, TIME add TZ info
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
						cFmt += *format - CHR_0 ;
						++format ;
					} else if (*format == CHR_FULLSTOP) {
						if (sXPC.f.radix == 0) {		// cannot have 2x radix '.'
							++format ;					// skip over radix char
							sXPC.f.radix	= 1 ;		// flag radix as provided
							if (cFmt > 0) {
								if (sXPC.f.arg_width == 0) {
									// at this stage we MIGHT have parsed a minwid value, if so verify and store.
									sXPC.f.minwid	= cFmt ;	// Save value parsed (maybe 0) as min_width
									sXPC.f.arg_width= 1 ;	// flag min_width as having been supplied
									cFmt = 0 ;			// reset counter in case of precision following
								} else {
									IF_myASSERT(debugTRACK, 0) ;
								}
							}
						} else {
							IF_myASSERT(debugTRACK, 0) ;
						}
					} else if (*format == CHR_ASTERISK) {
						if (sXPC.f.radix) {				// '*' only allowed after .
							++format ; 					// skip over argument precision char
							sXPC.f.precis= va_arg(vArgs, uint32_t) ;
							sXPC.f.arg_prec	= 1 ;
							cFmt = 0 ;					// reset counter just in case
						} else {
							IF_myASSERT(debugTRACK, 0) ;
						}
					} else {
						break ;
					}
				}
				// Save possible parsed value in cFmt
				if (cFmt > 0) {
					if (sXPC.f.arg_width == 0 && sXPC.f.radix == 0) {
						sXPC.f.minwid	= cFmt > xpfMINWID_MAXVAL ? xpfMINWID_MAXVAL : cFmt ;
					} else if (sXPC.f.arg_prec == 0 && sXPC.f.radix == 1) {
						sXPC.f.precis	= cFmt > xpfPRECIS_MAXVAL ? xpfPRECIS_MAXVAL : cFmt ;
					} else {
						IF_myASSERT(debugTRACK, 0) ;
					}
				}
			}
			// handle 2x successive 'l' characters, lower case ONLY, form long long value treatment
			if (*format == CHR_l) {
				++format;
				if (*format == CHR_l) {
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
#if		(xpfSUPPORT_SGR == 1)
			/* XXX: Since we are using the same command handler to process (UART, HTTP & TNET)
			 * requests, and we are embedding colour ESC sequences into the formatted output,
			 * and the colour ESC sequences (handled by UART & TNET) are not understood by
			 * the HTTP protocol, we must try to filter out the possible output produced by
			 * the ESC sequences if the output is going to a socket, one way or another.
			 */
			case CHR_C:
				vPrintSetGraphicRendition(&sXPC, va_arg(vArgs, uint32_t)) ;
				break ;
#endif

#if		(xpfSUPPORT_DATETIME == 1)
			case CHR_C: vPrintSetGraphicRendition(&sXPC, va_arg(vArgs, uint32_t)) ;		break ;
#endif

#if		(xpfSUPPORT_IP_ADDR == 1)						// IP address
			case CHR_I: vPrintIpAddress(&sXPC, va_arg(vArgs, uint32_t)) ;	break ;
#endif

#if		(xpfSUPPORT_BINARY == 1)
			case CHR_J: vPrintBinary(&sXPC, sXPC.f.llong ? va_arg(vArgs, uint64_t) : (uint64_t) va_arg(vArgs, uint32_t)) ;	break ;
#endif

#if		(xpfSUPPORT_DATETIME == 1)
			/* Prints date and/or time in POSIX format
			 * Use the following modifier flags
			 *	'`'		select between 2 different separator sets being
			 *			'/' or '-' (years)
			 *			'/' or '-' (months)
			 *			'T' or ' ' (days)
			 *			':' or 'h' (hours and minutes)
			 *			'.' or 's' (seconds)
			 * 	'!'		Treat time value as elapsed and not epoch [micro] seconds
			 * 	'.'		append 1 -> 6 digit(s) fractional seconds
			 * Norm 1	1970/01/01T00:00:00Z
			 * Norm 2	1970-01-01 00h00m00s
			 * Altform	Mon, 01 Jan 1970 00:00:00 GMT
			 */
				break ;

			case CHR_R:				// U64 epoch (yr+mth+day) OR relative (days) + TIME
				IF_myASSERT(debugTRACK, sXPC.f.alt_form == 0 && sXPC.f.plus == 0 && sXPC.f.pad0 == 0 && sXPC.f.group == 0) ;
				if (sXPC.f.rel_val == 0)
					sXPC.f.pad0 = 1 ;
				vPrintDateTime(&sXPC, va_arg(vArgs, uint64_t)) ;
				break ;
#endif
			case CHR_T:				// psTSZ epoch TIME
				IF_myASSERT(debugTRACK, sXPC.f.rel_val == 0 && sXPC.f.group == 0) ;
				sXPC.f.pad0		= 1 ;
				psTSZ = va_arg(vArgs, TSZ_t *) ;
				xPrintCalcSeconds(&sXPC, psTSZ, &sTM) ;
				vPrintTime(&sXPC, &sTM, (uint32_t) (psTSZ->usecs % MICROS_IN_SECOND)) ;
				vPrintZone(&sXPC, psTSZ) ;

			case CHR_Z:				// psTSZ epoch DATE+TIME+ZONE
				IF_myASSERT(debugTRACK, sXPC.f.rel_val == 0 && sXPC.f.plus == 0 && sXPC.f.group == 0) ;
				sXPC.f.pad0		= 1 ;
				sXPC.f.no_zone	= 1 ;
				uint32_t flags	= sXPC.f.flags ;
				psTSZ = va_arg(vArgs, TSZ_t *) ;
				vPrintDateTime(&sXPC, xTimeMakeTimestamp(xPrintCalcSeconds(&sXPC, psTSZ, NULL), psTSZ->usecs % MICROS_IN_SECOND)) ;
				sXPC.f.flags	= flags ;
				vPrintZone(&sXPC, psTSZ) ;
				break ;

#if		(xpfSUPPORT_DATETIME == 1)
			case CHR_r:				// U32->U64 epoch (yr+mth+day) or relative (days) + TIME
				IF_myASSERT(debugTRACK, sXPC.f.alt_form == 0 && sXPC.f.plus == 0 && sXPC.f.pad0 == 0 && sXPC.f.radix == 0 && sXPC.f.group == 0) ;
				sXPC.f.pad0		= 1 ;
				vPrintDateTime(&sXPC, (uint64_t) va_arg(vArgs, uint32_t) * MILLION) ;
				break ;
#endif

#if		(xpfSUPPORT_URL == 1)							// para = pointer to string to be encoded
			case CHR_U: vPrintURL(&sXPC, va_arg(vArgs, char *)) ;	break ;
#endif

#if		(xpfSUPPORT_HEXDUMP == 1)
			case CHR_b:									// HEXDUMP byte sized UC/lc
			case CHR_h:									// HEXDUMP halfword sized UC/lc
			case CHR_w:									// HEXDUMP word sized UC/lc
					sXPC.f.form	= sXPC.f.group ? xpfFORMAT_3 : xpfFORMAT_0_G ;
					sXPC.f.size = cFmt == 'b' ? xpfSIZING_BYTE : (cFmt == 'h') ? xpfSIZING_SHORT : xpfSIZING_WORD ;
					sXPC.f.size	+= (sXPC.f.llong) ? 1 : 0 ; 			// apply ll modifier to size
				/* In order for formatting to work  the "*" or "." radix specifiers
				 * should not be used. The requirement for a second parameter is implied and assumed */
					uint32_t U32	= va_arg(vArgs, uint32_t) ;				// 32 bit size
					char * pChar	= va_arg(vArgs, char *) ;				// 32 bit address
					if (pChar)
						vPrintHexDump(&sXPC, U32, pChar) ;
					break ;
#endif

#if		(xpfSUPPORT_MAC_ADDR == 1)
			/* Formats 6 byte string (0x00 is valid) as a series of hex characters.
			 * defualt format uses no separators eg. '0123456789AB'
			 * Support the following modifier flags:
			 *  '!'	select ':' separator between digits
			 */
			case CHR_m:									// MAC address UC/LC format ??:??:??:??:??:??
				sXPC.f.alt_form	= 0 ;
				sXPC.f.size		= 0 ;
				sXPC.f.llong	= 0 ;					// force interpretation as sequence of U8 values
				sXPC.f.form		= sXPC.f.group ? xpfFORMAT_1_F : xpfFORMAT_0_G ;
				vPrintHexValues(&sXPC, configMAC_ADDRESS_LENGTH, va_arg(vArgs, char *)) ;
				break ;
#endif

			case CHR_c: vPrintChar(&sXPC, va_arg(vArgs, int32_t)) ;		break ;

			case CHR_d:									// signed decimal "[-]ddddd"
			case CHR_i:									// signed integer (same as decimal ?)
			case CHR_o:									// unsigned octal "ddddd"
			case CHR_u:									// unsigned decimal "ddddd"
			case CHR_x:									// hex as in "789abcd" UC/LC
				x64Val.u64	= sXPC.f.llong ? va_arg(vArgs, uint64_t) : va_arg(vArgs, uint32_t) ;
				if (cFmt == CHR_d || cFmt == CHR_i) {
					sXPC.f.signval = 1 ;
					if (x64Val.i64 < 0)	{
						sXPC.f.negvalue	= 1 ;
						x64Val.i64 		*= -1 ; 		// convert the value to unsigned
					}
				}
				sXPC.f.nbase = (cFmt == CHR_x) ? BASE16 : (cFmt == CHR_o) ? BASE08 : BASE10 ;
				if (sXPC.f.nbase != BASE10)				// if not BASE10
					sXPC.f.group = 0 ;					// disable grouping
				vPrintX64(&sXPC, x64Val.u64) ;
				break ;

#if		(xpfSUPPORT_IEEE754 == 1)
			case CHR_e:									// treat same as 'f' for now, need to implement...
				sXPC.f.form++ ;
				/* FALLTHRU */ /* no break */
			case CHR_f:									// floating point format
				sXPC.f.form++ ;
				/* FALLTHRU */ /* no break */
			case CHR_g:
				sXPC.f.signval	= 1 ;					// float always signed value.
				if (sXPC.f.radix) {
					sXPC.f.precis	= (sXPC.f.precis > xpfMAXIMUM_DECIMALS) ? xpfMAXIMUM_DECIMALS : sXPC.f.precis ;
				} else {
					sXPC.f.precis	= xpfDEFAULT_DECIMALS ;
				}
				myASSERT(sXPC.f.alt_form == 0) ;		// MUST not use '#' modifier
				vPrintF64(&sXPC, va_arg(vArgs, double)) ;
				break ;
#endif

#endif

#if		(xpfSUPPORT_DATETIME == 1)
				break ;
#endif

			case CHR_s:	vPrintString(&sXPC, va_arg(vArgs, char *)) ;	break;

			default:
				/* At this stage we have handled the '%' as assumed, but the next character found is invalid.
				 * Show the '%' we swallowed and then the extra, invalid, character as well */
				vPrintChar(&sXPC, CHR_PERCENT) ;
				vPrintChar(&sXPC, *format) ;
				break ;
			}
			sXPC.f.plus				= 0 ;		// reset this form after printing one value
		} else {
out_lbl:
			vPrintChar(&sXPC, *format) ;
		}
	}
	return sXPC.f.curlen ;
}

// ##################################### Destination = STRING ######################################

int		xPrintToString(xpc_t * psXPC, int cChr) {
	if (psXPC->pStr)
		*psXPC->pStr++ = cChr ;
	return cChr ;
}

int 	vsnprintfx(char * pBuf, size_t BufSize, const char * format, va_list vArgs) {
	if (pBuf && (BufSize == 1)) {						// buffer specified, but no space ?
		*pBuf = CHR_NUL ;								// yes, terminate
		return 0 ; 										// & return
	}
	int iRV = PrintFX(xPrintToString, pBuf, BufSize, format, vArgs) ;
	if (pBuf && (iRV == BufSize))						// buffer specified and FULL?
		iRV-- ; 										// yes, adjust the length for terminator
	if (pBuf)											// buffer specified ?
		pBuf[iRV] = CHR_NUL ; 							// yes, terminate
	return iRV ;
}

int 	snprintfx(char * pBuf, size_t BufSize, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int32_t count = vsnprintfx(pBuf, BufSize, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

int 	vsprintfx(char * pBuf, const char * format, va_list vArgs) { return vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	sprintfx(char * pBuf, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = STDOUT ########################################

void	printfx_lock(void) { xRtosSemaphoreTake(&printfxMux, portMAX_DELAY) ; }

void	printfx_unlock(void) { xRtosSemaphoreGive(&printfxMux) ; }

int		xPrintStdOut(xpc_t * psXPC, int cChr) { return x_uputc(configSTDIO_UART_CHAN, cChr) ; }

int 	vnprintfx(size_t count, const char * format, va_list vArgs) {
	printfx_lock() ;
	int32_t iRV = PrintFX(xPrintStdOut, stdout, count, format, vArgs) ;
	printfx_unlock() ;
	return iRV ;
}

int 	vprintfx(const char * format, va_list vArgs) { return vnprintfx(xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	nprintfx(size_t count, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRV = vnprintfx(count, format, vArgs) ;
	va_end(vArgs) ;
	return iRV ;
}

int 	printfx(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRV = vnprintfx(xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return iRV ;
}

/*
 * vnprintfx_nolock() - print to stdout without any semaphore locking.
 * 					securing the channel must be done manually
 */
int 	vnprintfx_nolock(size_t count, const char * format, va_list vArgs) { return PrintFX(xPrintStdOut, stdout, count, format, vArgs) ; }

int 	printfx_nolock(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRV = PrintFX(xPrintStdOut, stdout, xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return iRV ;
}

/* ################################## Destination = UART/TELNET ####################################
 * Output directly to the [possibly redirected] stdout/UART channel
 */

int		xPrintToStdout(xpc_t * psXPC, int cChr) { return x_putchar(cChr) ; }

int 	vcprintfx(const char * format, va_list vArgs) {
	xRtosSemaphoreTake(&cprintfxMux, portMAX_DELAY) ;
	int iRV = PrintFX(xPrintToStdout, NULL, xpfMAXLEN_MAXVAL, format, vArgs) ;
	xRtosSemaphoreGive(&cprintfxMux) ;
	return iRV ;
}

int 	cprintfx(const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRV = vcprintfx(format, vArgs) ;
	va_end(vArgs) ;
	return iRV ;
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

int		wsnprintfx(char ** ppcBuf, size_t * pSize, const char * pcFormat, ...) {
	va_list vArgs ;
	va_start(vArgs, pcFormat) ;
	int32_t iRV ;
	if (*ppcBuf && (*pSize > 1)) {
		iRV = vsnprintfx(*ppcBuf, *pSize, pcFormat, vArgs) ;
		if (iRV > 0) {
			*ppcBuf	+= iRV ;
			*pSize	-= iRV ;
		}
	} else {
		iRV = vnprintfx_nolock(xpfMAXLEN_MAXVAL, pcFormat, vArgs) ;
	}
	va_end(vArgs) ;
	return iRV ;
}

// ################################### Destination = FILE PTR ######################################

int		xPrintToFile(xpc_t * psXPC, int cChr) { return fputc(cChr, psXPC->stream) ; }

int 	vfprintfx(FILE * stream, const char * format, va_list vArgs) { return PrintFX(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	fprintfx(FILE * stream, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int count = vfprintfx(stream, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = HANDLE ########################################

int		xPrintToHandle(xpc_t * psXPC, int cChr) {
	char cChar = cChr ;
	ssize_t size = write(psXPC->fd, &cChar, sizeof(cChar)) ;
	return size == 1 ? cChr : size ;
}

int		vdprintfx(int32_t fd, const char * format, va_list vArgs) { return PrintFX(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vArgs) ; }

int		dprintfx(int32_t fd, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = vdprintfx(fd, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// ################################### Destination = DEVICE ########################################

int		xPrintToDevice(xpc_t * psXPC, int cChr) { return psXPC->DevPutc(cChr) ; }

int 	vdevprintfx(int (* handler)(int ), const char * format, va_list vArgs) { return PrintFX(xPrintToDevice, handler, xpfMAXLEN_MAXVAL, format, vArgs) ; }

int 	devprintfx(int (* handler)(int ), const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int iRV = PrintFX(xPrintToDevice, handler, xpfMAXLEN_MAXVAL, format, vArgs) ;
	va_end(vArgs) ;
	return iRV ;
}

/* #################################### Destination : SOCKET #######################################
 * SOCKET directed formatted print support. Problem here is that MSG_MORE is primarily supported on
 * TCP sockets, UDP support officially in LwIP 2.6 but has not been included into ESP-IDF yet. */

int		xPrintToSocket(xpc_t * psXPC, int cChr) {
	char cBuf = cChr ;
	if (xNetWrite(psXPC->psSock, &cBuf, sizeof(cBuf)))
		return EOF ;
	return cChr ;
}

int 	vsocprintfx(netx_t * psSock, const char * format, va_list vArgs) {
	int	Fsav	= psSock->flags ;
	psSock->flags	|= MSG_MORE ;
	int32_t iRV = PrintFX(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vArgs) ;
	psSock->flags	= Fsav ;
	return (psSock->error == 0) ? iRV : erFAILURE ;
}

int 	socprintfx(netx_t * psSock, const char * format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	int count = vsocprintfx(psSock, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
}

// #################################### Destination : UBUF #########################################

int		xPrintToUBuf(xpc_t * psXPC, int cChr) { return xUBufPutC(psXPC->psUBuf, cChr) ; }

int		vuprintfx(ubuf_t * psUBuf, const char * format, va_list vArgs) { return PrintFX(xPrintToUBuf, psUBuf, xUBufSpace(psUBuf), format, vArgs) ; }

int		uprintfx(ubuf_t * psUBuf, const char * format, ...) {
	va_list	vArgs ;
	va_start(vArgs, format) ;
	int count = vuprintfx(psUBuf, format, vArgs) ;
	va_end(vArgs) ;
	return count ;
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
#if		(xpfSUPPORT_ALIASES == 1)		// standard library [?]printf functions
	int		printf(const char * format, ...) __attribute__((alias("printfx"), unused)) ;
	int		printf_r(const char * format, ...) __attribute__((alias("printfx"), unused)) ;

	int		vprintf(const char * format, va_list vArgs) __attribute__((alias("vprintfx"), unused)) ;
	int		vprintf_r(const char * format, va_list vArgs) __attribute__((alias("vprintfx"), unused)) ;

	int		sprintf(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused)) ;
	int		sprintf_r(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused)) ;

	int		vsprintf(char * pBuf, const char * format, va_list vArgs) __attribute__((alias("vsprintfx"), unused)) ;
	int		vsprintf_r(char * pBuf, const char * format, va_list vArgs) __attribute__((alias("vsprintfx"), unused)) ;

	int 	snprintf(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused)) ;
	int 	snprintf_r(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused)) ;

	int 	vsnprintf(char * pBuf, size_t BufSize, const char * format, va_list vArgs) __attribute__((alias("vsnprintfx"), unused)) ;
	int 	vsnprintf_r(char * pBuf, size_t BufSize, const char * format, va_list vArgs) __attribute__((alias("vsnprintfx"), unused)) ;

	int 	fprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;
	int 	fprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;

	int 	vfprintf(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("vfprintfx"), unused)) ;
	int 	vfprintf_r(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("vfprintfx"), unused)) ;

	int 	dprintf(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused)) ;
	int 	dprintf_r(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused)) ;

	int 	vdprintf(int fd, const char * format, va_list vArgs) __attribute__((alias("vdprintfx"), unused)) ;
	int 	vdprintf_r(int fd, const char * format, va_list vArgs) __attribute__((alias("vdprintfx"), unused)) ;

	int 	fiprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;
	int 	fiprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;

	int 	vfiprintf(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("vfprintfx"), unused)) ;
	int 	vfiprintf_r(FILE * stream, const char * format, va_list vArgs) __attribute__((alias("vfprintfx"), unused)) ;
#endif

// ##################################### functional tests ##########################################

#define		TEST_INTEGER	1
#define		TEST_STRING		1
#define		TEST_FLOAT		1
#define		TEST_BINARY		1
#define		TEST_ADDRESS	1
#define		TEST_DATETIME	1
#define		TEST_HEXDUMP	1
#define		TEST_WIDTH_PREC	1

void	vPrintfUnitTest(void) {
#if		(TEST_INTEGER == 1)
uint64_t	my_llong = 0x98765432 ;
// Minimums and maximums
	PRINT("\nintegers\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("Min/max i8 : %d %d\n", INT8_MIN, INT8_MAX) ;
	PRINT("Min/max i16 : %d %d\n", INT16_MIN, INT16_MAX) ;
	PRINT("Min/max i32 : %d %d\n", INT32_MIN, INT32_MAX) ;
	PRINT("Min/max i64 : %lld %lld\n", INT64_MIN, INT64_MAX) ;
	PRINT("Min/max u64 : %llu %llu\n", UINT64_MIN, UINT64_MAX) ;

	PRINT("0x%llx , %'lld un/signed long long\n", 9876543210ULL, -9876543210LL) ;
	PRINT("0x%llX , %'lld un/signed long long\n", 0x0000000076543210ULL, 0x0000000076543210ULL) ;
	PRINT("%'lld , %'llX , %07lld dec-hex-dec(=0 but 7 wide) long long\n", my_llong, my_llong, 0ULL) ;
	PRINT("long long: %lld, %llu, 0x%llX, 0x%llx\n", -831326121984LL, 831326121984LLU, 831326121984LLU, 831326121984LLU) ;

// left & right padding
	PRINT(" long padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\n", 3, 3, 3) ;
	PRINT(" long padding (neg): zero=[%04d], left=[%-+4d], right=[%+4d]\n", -3, -3, -3) ;

	PRINT("multiple unsigneds: %u %u %2u %X %u\n", 15, 5, 23, 0xb38f, 65535) ;
	PRINT("hex %x = ff, hex 02=%02x\n", 0xff, 2) ;		//  hex handling
	PRINT("signed %'d = %'u U = 0x%'X\n", -3, -3, -3) ;	//  int formats

	PRINT("octal examples 0xFF = %o , 0x7FFF = %o 0x7FFF7FFF7FFF = %16llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
	PRINT("octal examples 0xFF = %04o , 0x7FFF = %08o 0x7FFF7FFF7FFF = %016llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
#endif

#if		(TEST_STRING == 1)
	char 	buf[192] ;
	char	my_string[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ;
	char *	ptr = &my_string[17] ;
	char *	np = NULL ;
	size_t	slen, count ;
	PRINT("\nstrings\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("[%d] %s\n", snprintfx(buf, 11, my_string), buf) ;
	PRINT("[%d] %.*s\n", 20, 20, my_string) ;
	PRINT("ptr=%s, %s is null pointer, char %c='a'\n", ptr, np, 'a');
	PRINT("%d %s(s) with %%\n", 0, "message") ;

// test walking string builder
	slen = 0 ;
	slen += sprintfx(buf+slen, "padding (neg): zero=[%04d], ", -3) ;
	slen += sprintfx(buf+slen, "left=[%-4d], ", -3) ;
	slen += sprintfx(buf+slen, "right=[%4d]\n", -3) ;
	PRINT("[%d] %s", slen, buf) ;
// left & right justification
	slen = sprintfx(buf, "justify: left=\"%-10s\", right=\"%10s\"\n", "left", "right") ;
	PRINT("[len=%d] %s", slen, buf);

	count = 80 ;
	snprintfx(buf, count, "Only %d buffered bytes should be displayed from this very long string of at least 90 characters", count) ;
	PRINT("%s\n", buf) ;
// multiple chars
	sprintfx(buf, "multiple chars: %c %c %c %c\n", 'a', 'b', 'c', 'd') ;
	PRINT("%s", buf);
#endif

#if		(TEST_FLOAT == 1)
	float	my_float	= 1000.0 / 9.0 ;
	double	my_double	= 22000.0 / 7.0 ;

	PRINT("\nfloat/double\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("DBL MAX=%'.15f MIN=%'.15f\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15e MIN=%'.15E\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15g MIN=%'.15G\n", DBL_MAX, DBL_MIN) ;

	PRINT("float padding (pos): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", -my_double, -my_double, -my_double) ;
	PRINT("float padding (pos): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double*(-1.0), my_double*(-1.0), my_double*(-1.0)) ;

	PRINT("%'.20f = float(f)\n", my_float) ;
	PRINT("%'.20e = float(e)\n", my_float) ;
	PRINT("%'.20e = float(e)\n", my_float/100.0) ;

	PRINT("%'.7f = double(f)\n", my_double) ;
	PRINT("%'.7e = double(e)\n", my_double) ;
	PRINT("%'.7E = double(E)\n", my_double/1000.00) ;

	PRINT("%'.12g = double(g)\n", my_double/10000000.0) ;
	PRINT("%'.12G = double(G)\n", my_double*10000.0) ;

	PRINT("%.20f is a double\n", 22.0/7.0) ;
	PRINT("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\n", 3, -3, 3.0, -3.0, 3, 3.0) ;

	PRINT("multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;
	PRINT("multiple doubles: %e %.1e %2.0e %.2e %.3e %.2e [%-8.3e]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;

	PRINT("double special cases: %f %.f %.0f %2f %2.f %2.0f\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;
	PRINT("double special cases: %e %.e %.0e %2e %2.e %2.0e\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;

	PRINT("rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;
	PRINT("rounding doubles: %.1e %.1e %.3e %.2e [%-8.3e]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;

	PRINT ("%g  %g  %g  %g  %g  %g  %g  %g\n", 0.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001) ;
	PRINT ("%g  %g  %g  %g  %g  %g  %g  %g\n", 1.1, 10.01, 100.001, 1000.0001, 10000.00001, 100000.000001, 1000000.0000001, 10000000.00000001) ;
	double dVal ;
	int32_t Width, Precis ;
	for(Width=0, Precis=0, dVal=1234567.7654321; Width < 8 && Precis < 8; ++Width, ++Precis)
		PRINT ("%*.*g  ", Width, Precis, dVal) ;
	PRINT("\n") ;
#endif

#if		(TEST_ADDRESS == 1)
	char MacAdr[6] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 } ;
	PRINT("%I - IP Address (Default)\n", 0x01020304UL) ;
	PRINT("%0I - IP Address (PAD0)\n", 0x01020304UL) ;
	PRINT("%-I - IP Address (L-Just)\n", 0x01020304UL) ;
	PRINT("%#I - IP Address (Rev Default)\n", 0x01020304UL) ;
	PRINT("%#0I - IP Address (Rev PAD0)\n", 0x01020304UL) ;
	PRINT("%#-I - IP Address (Rev L-Just)\n", 0x01020304UL) ;
	PRINT("%m - MAC address (LC)\n", &MacAdr[0]) ;
	PRINT("%M - MAC address (UC)\n", &MacAdr[0]) ;
	PRINT("%'m - MAC address (LC+sep)\n", &MacAdr[0]) ;
	PRINT("%'M - MAC address (UC+sep)\n", &MacAdr[0]) ;
#endif

#if		(TEST_BINARY == 1)
	PRINT("%J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%'J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%'24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
#endif

#if		(TEST_DATETIME == 1)
	#if		defined(__TIME__) && defined(__DATE__)
		PRINT("_DATE_ _TIME_ : %s %s\n", __DATE__, __TIME__) ;
	#endif
	#if		defined(__TIMESTAMP__)
		PRINT("_TIMESTAMP_ : %s\n", __TIMESTAMP__) ;
	#endif
	#if		defined(__TIMESTAMP__ISO__)
		PRINT("_TIMESTAMP_ISO_ : %s\n", __TIMESTAMP__ISO__) ;
	#endif
	PRINT("Normal  (S1): %Z\n", &sTSZ) ;
	PRINT("Normal  (S2): %'Z\n", &sTSZ) ;
	PRINT("Normal Alt  : %#Z\n", &sTSZ) ;

	PRINT("Elapsed S1      : %!R\n", RunTime) ;
	PRINT("Elapsed S2      : %!`R\n", RunTime) ;
	PRINT("Elapsed S1 x3uS : %!.R\n", RunTime) ;
	PRINT("Elapsed S2 x6uS : %!`.6R\n", RunTime) ;

	seconds_t	Seconds ;
	uint64_t uSecs ;
	struct	tm 	sTM ;
	for(uint64_t i = 0; i <= 1000; i += 50) {
		printfx("#%llu", i) ;
		uSecs = i * 86398999000ULL ;
		Seconds = xTimeStampAsSeconds(uSecs) ;
		xTimeGMTime(Seconds, &sTM, 1) ;
		printfx("  %u / %d  ->  %!.R", Seconds, sTM.tm_mday, uSecs) ;

		uSecs = i * 86400000000ULL ;
		Seconds = xTimeStampAsSeconds(uSecs) ;
		xTimeGMTime(Seconds, &sTM, 1) ;
		printfx("  %u / %d  ->  %!.R", Seconds, sTM.tm_mday, uSecs) ;

		uSecs = i * 86401001000ULL ;
		Seconds = xTimeStampAsSeconds(uSecs) ;
		xTimeGMTime(Seconds, &sTM, 1) ;
		printfx("  %u / %d  ->  %!.R", Seconds, sTM.tm_mday, uSecs) ;
		printfx("\n") ;
	}
#endif

#if		(TEST_HEXDUMP == 1)
	uint8_t DumpData[] = "0123456789abcdef0123456789ABCDEF~!@#$%^&*()_+-={}[]:|;'\\<>?,./`01234" ;
	#define DUMPSIZE	(sizeof(DumpData)-1)
	PRINT("DUMP absolute lc byte\n%+b", DUMPSIZE, DumpData) ;
	PRINT("DUMP absolute lc byte\n%'+b", DUMPSIZE, DumpData) ;

	PRINT("DUMP absolute UC half\n%+H", DUMPSIZE, DumpData) ;
	PRINT("DUMP absolute UC half\n%'+H", DUMPSIZE, DumpData) ;

	PRINT("DUMP relative lc word\n%!+w", DUMPSIZE, DumpData) ;
	PRINT("DUMP relative lc word\n%!'+w", DUMPSIZE, DumpData) ;

	PRINT("DUMP relative UC dword\n%!+llW", DUMPSIZE, DumpData) ;
	PRINT("DUMP relative UC dword\n%!'+llW", DUMPSIZE, DumpData) ;
	for (int32_t idx = 0; idx < 16; idx++) {
		PRINT("\nDUMP relative lc BYTE %!'+b", idx, DumpData) ;
	}
#endif

#if		(TEST_WIDTH_PREC == 1)
	PRINT("String : Minwid=5  Precis=8  : %*.*s\n",  5,  8, "0123456789ABCDEF") ;
	PRINT("String : Minwid=30 Precis=15 : %*.*s\n", 30, 15, "0123456789ABCDEF") ;

	double	F64	= 22000.0 / 7.0 ;
	PRINT("Float  : Variables  5.8  : %*.*f\n",  5,  8, F64) ;
	PRINT("Float  : Specified  5.8  : %5.8f\n", F64) ;
	PRINT("Float  : Variables 30.14 : %*.*f\n",  30,  14, F64) ;
	PRINT("Float  : Specified 30.14 : %30.14f\n", F64) ;
#endif
}
