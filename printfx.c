/*
 * Copyright 2014-21 Andre M. Maree / KSS Technologies (Pty) Ltd.
 * <snf>printfx -  set of routines to replace equivalent printf functionality
 */

#include	<string.h>
#include	<stdarg.h>
#include	<math.h>									// isnan()
#include	<float.h>									// DBL_MIN/MAX

#include	"printfx.h"

#include	"hal_config.h"
#include	"hal_usart.h"

#include	"FreeRTOS_Support.h"

#include	"socketsX.h"
#include	"x_ubuf.h"
#include	"x_string_general.h"						// xinstring function
#include	"x_errors_events.h"
#include	"x_terminal.h"
#include	"x_utilities.h"
#include	"struct_union.h"

#ifdef ESP_PLATFORM
	#include	"esp_log.h"
	#include	"esp32/rom/crc.h"					// ESP32 ROM routine
#else
	#include	"crc-barr.h"						// Barr group CRC
#endif

#define	debugFLAG					0xE001

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Enumerations ############################################

//FLOAT "Gg"	"Ff"	"Ee"	None
//Dump	None	':'		'-'		Complex
//Other			'!'				'`'
enum { form0G, form1F, form2E, form3X } ;

// ######################## Character and value translation & rounding tables ######################

const char vPrintStr1[] = {						// table of characters where lc/UC is applicable
		'X',									// hex formatted 'x' or 'X' values, always there
#if		(xpfSUPPORT_MAC_ADDR == 1)
		'M',									// MAC address UC/LC
#endif
#if		(xpfSUPPORT_IEEE754 == 1)
		'A', 'E', 'G',							// float hex/exponential/general
#endif
#if		(xpfSUPPORT_POINTER == 1)
		'P',									// Pointer lc=0x or UC=0X
#endif
		'\0' } ;								// terminated

static	const char hexchars[] = "0123456789ABCDEF" ;

static	const double round_nums[xpfMAXIMUM_DECIMALS+1] = {
	0.5, 			0.05, 			0.005, 			0.0005,			0.00005, 			0.000005, 			0.0000005, 			0.00000005,
	0.000000005,	0.0000000005, 	0.00000000005, 	0.000000000005,	0.0000000000005, 	0.00000000000005, 	0.000000000000005, 	0.0000000000000005
} ;

// ############################## private function variables #######################################

// ############################# Foundation character and string output ############################

/**
 * @brief	output a single character using/via the preselected function
 * 			before output verify against specified width
 * 			adjust character count (if required) & remaining width
 * @param	psXPC - pointer to control structure to be referenced/updated
 * 			c - char to be output
 * @return	1 or 0 based on whether char was '\000'
 */
static int vPrintChar(xpc_t * psXPC, char cChr) {
	int iRV = cChr;
	#if (xpfSUPPORT_FILTER_NUL == 1)
	if (cChr == 0)
		return iRV;
	#endif
	if ((psXPC->f.maxlen == 0) || (psXPC->f.curlen < psXPC->f.maxlen)) {
		iRV = psXPC->handler(psXPC, cChr);
		if (iRV == cChr) {
			psXPC->f.curlen++ ;							// adjust count
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
static int xPrintChars (xpc_t * psXPC, char * pStr) {
	int len ;
	for (len = 0; *pStr; ++len, ++pStr) {
		int iRV = vPrintChar(psXPC, *pStr);
		if (iRV != *pStr)
			return iRV;
	}
	return len ;
}

/**
 * @brief	perform formatted output of a string to the preselected device/string/buffer/file
 * @param	psXPC - pointer to structure controlling the operation
 * 			string - pointer to the string to be output
 * @return	number of ACTUAL characters output.
 */
void vPrintString (xpc_t * psXPC, char * pStr) {
	// determine natural or limited length of string
	int	Len ;
	if (psXPC->f.arg_prec && psXPC->f.arg_width && psXPC->f.precis < psXPC->f.minwid) {
		Len = xstrnlen(pStr, psXPC->f.precis) ;
	} else {
		Len	 = psXPC->f.precis ? psXPC->f.precis : xstrlen(pStr) ;
	}
	int	Tpad = psXPC->f.minwid > Len ? psXPC->f.minwid - Len : 0 ;
	int	Lpad = 0, Rpad = 0 ;
	uint8_t	Cpad = psXPC->f.pad0 ? '0' : ' ' ;
	if (Tpad) {
		if (psXPC->f.alt_form) {
			Rpad = Tpad >> 1 ; Lpad = Tpad - Rpad ;
		} else if (psXPC->f.ljust) {
			Rpad = Tpad ;
		} else {
			Lpad = Tpad ;
		}
	}
	for (;Lpad--; vPrintChar(psXPC, Cpad)) ;
	for (;Len-- && *pStr; vPrintChar(psXPC, *pStr++)) ;
	for (;Rpad--; vPrintChar(psXPC, Cpad)) ;
}

/**
 * @brief		returns a pointer to the hexadecimal character representing the value of the low order nibble
 * 				Based on the Flag, the pointer will be adjusted for UpperCase (if required)
 * @param[in]	Val = value in low order nibble, to be converted
 * @return		pointer to the correct character
 */
char cPrintNibbleToChar(xpc_t * psXPC, uint8_t Value) {
	char cChr = hexchars[Value] ;
	if (psXPC->f.Ucase == 0 && Value > 9)
		cChr += 0x20 ;									// convert to lower case
	return cChr ;
}

/**
 * xPrintXxx() convert uint64_t value to a formatted string (right to left, L <- R)
 * @param	psXPC - pointer to control structure
 * 			ullVal - uint64_t value to convert & output
 * 			pBuffer - pointer to buffer for converted string storage
 * 			BufSize - available (remaining) space in buffer
 * @return	number of actual characters output (incl leading '-' and/or ' ' and/or '0' as possibly added)
 * @comment		Honour & interpret the following modifier flags
 * 				'`'		Group digits in 3 digits groups to left of decimal '.'
 * 				'-'		Left align the individual numbers between the '.'
 * 				'+'		Force a '+' or '-' sign on the left
 * 				'0'		Zero pad to the left of the value to fill the field
 * Protection against buffer overflow based on correct sized buffer being allocated in calling function
 */
int	xPrintXxx(xpc_t * psXPC, uint64_t ullVal, char * pBuffer, int BufSize) {
	int	Len = 0, Count = 0, iTemp = 0 ;
	char * pTemp = pBuffer + BufSize - 1 ;				// Point to last space in buffer
	if (ullVal) {
		#if	(xpfSUPPORT_SCALING == 1)
		uint8_t	ScaleChr = 0 ;
		if (psXPC->f.alt_form) {
			if (ullVal > 10000000000000000000ULL) {
				ullVal		/= 1000000000000000000ULL ;
				ScaleChr	= 's' ;
			} else if (ullVal > 10000000000000000000ULL) {
				ullVal		/= 1000000000000000000ULL ;
				ScaleChr	= 'Q' ;
			} else if (ullVal > 10000000000000000ULL) {
				ullVal		/= 1000000000000000ULL ;
				ScaleChr	= 'q' ;
			} else if (ullVal > 10000000000000ULL) {
				ullVal		/= 1000000000000ULL ;
				ScaleChr	= 'T' ;
			} else if (ullVal > 10000000000ULL) {
				ullVal		/= 1000000000ULL ;
				ScaleChr	= 'B' ;
			} else if (ullVal > 10000000ULL) {
				ullVal		/= 1000000ULL ;
				ScaleChr	= 'M' ;
			} else if (ullVal > 10000ULL) {
				ullVal		/= 1000ULL ;
				ScaleChr	= 'K' ;
			}
			if (ScaleChr != 0) {
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
					*pTemp-- = ',' ;
					++Len ;
					Count = 0 ;
				}
			}
		}
	} else {
		*pTemp-- = '0' ;
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
			*pTemp-- = psXPC->f.negvalue ? '-' : '+' ;			// start by prepend of '+' or '-'
			--Count ;
			++Len ;
		}
		if (Count > 0) {								// If any space left to pad
			iTemp = psXPC->f.pad0 ? '0' : ' ' ;			// define applicable padding char
			Len += Count ;								// Now do the actual padding
			while (Count--) *pTemp-- = iTemp ;
		}
		// If we are padding with '0' AND a sign is required (-value or +requested), do that first
		if (psXPC->f.pad0 && (psXPC->f.negvalue || psXPC->f.plus)) {	// If +/- sign is required
			if (pTemp[1] == ' ' || pTemp[1] == '0') ++pTemp;	// set overwrite last with '+' or '-'
			else ++Len;											// set to add extra '+' or '-'
			*pTemp = psXPC->f.negvalue ? '-' : '+';
		}
	} else {
		if (psXPC->f.negvalue || psXPC->f.plus) {		// If a sign is required
			*pTemp = psXPC->f.negvalue ? '-' : '+' ;	// just prepend '+' or '-'
			++Len ;
		}
	}
	return Len ;
}

void vPrintX64(xpc_t * psXPC, uint64_t Value) {
	char 	Buffer[xpfMAX_LEN_X64] ;
	Buffer[xpfMAX_LEN_X64 - 1] = 0 ;				// terminate the buffer, single value built R to L
	int Len = xPrintXxx(psXPC, Value, Buffer, xpfMAX_LEN_X64 - 1) ;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_X64 - 1 - Len)) ;
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
	if (isnan(F64)) {
		vPrintString(psXPC, psXPC->f.Ucase ? "NAN" : "nan") ;
		return ;
	} else if (isinf(F64)) {
		vPrintString(psXPC, psXPC->f.Ucase ? "INF" : "inf") ;
		return ;
	}
	psXPC->f.negvalue	= F64 < 0.0 ? 1 : 0 ;			// set negvalue if < 0.0
	F64	*= psXPC->f.negvalue ? -1.0 : 1.0 ;				// convert to positive number
	xpf_t xpf ;
	xpf.limits	= psXPC->f.limits ;						// save original flags
	xpf.flags	= psXPC->f.flags ;
	x64_t X64 	= { 0 } ;

	int	Exp = 0 ;										// if exponential format requested, calculate the exponent
	if (F64 != 0.0) {									// if not 0 and...
		if (psXPC->f.form != form1F) {					// any of "eEgG" specified ?
			X64.f64	= F64 ;
			while (X64.f64 > 10.0) {					// if number is greater that 10.0
				X64.f64 /= 10.0 ;						// re-adjust and
				Exp += 1 ;								// update exponent
			}
			while (X64.f64 < 1.0) {						// similarly, if number smaller than 1.0
				X64.f64 *= 10.0 ;						// re-adjust upwards and
				Exp	-= 1 ;								// update exponent
			}
		}
	}
	uint8_t	AdjForm ;
	if (psXPC->f.form == form0G) {						// 'gG' req, check exponent & select mode.
		AdjForm = Exp < -4 || Exp >= psXPC->f.precis ? form2E : form1F ;
	} else {
		AdjForm = psXPC->f.form ;
	}
	if (AdjForm == form2E) F64 = X64.f64 ;				// change to exponent adjusted value
	if (F64 < (DBL_MAX - round_nums[psXPC->f.precis])) {// if addition of rounding value will NOT cause overflow.
		F64 += round_nums[psXPC->f.precis] ;			// round by adding .5LSB to the value
	}
	char	Buffer[xpfMAX_LEN_F64] ;
	Buffer[xpfMAX_LEN_F64 - 1] = 0 ;					// building R to L, ensure buffer NULL-term

	int Len = 0 ;
	if (AdjForm == form2E) {							// If required, handle the exponent
		psXPC->f.minwid		= 2 ;
		psXPC->f.pad0		= 1 ;						// MUST left pad with '0'
		psXPC->f.signval	= 1 ;
		psXPC->f.ljust		= 0 ;
		psXPC->f.negvalue	= (Exp < 0) ? 1 : 0 ;
		Exp *= psXPC->f.negvalue ? -1LL : 1LL ;
		Len += xPrintXxx(psXPC, Exp, Buffer, xpfMAX_LEN_F64 - 1) ;
		Buffer[xpfMAX_LEN_F64-2-Len] = psXPC->f.Ucase ? 'E' : 'e' ;
		++Len ;
	}

	X64.f64	= F64 - (uint64_t) F64 ;					// isolate fraction as double
	X64.f64	= X64.f64 * (double) u64pow(10, psXPC->f.precis) ;	// fraction to integer
	X64.u64	= (uint64_t) X64.f64 ;						// extract integer portion

	if (psXPC->f.arg_prec) {							// explicit minwid specified ?
		psXPC->f.minwid	= psXPC->f.precis ; 			// yes, stick to it.
	} else if (X64.u64 == 0) {							// process 0 value
		psXPC->f.minwid	= psXPC->f.radix ? 1 : 0;
	} else {											// process non 0 value
		if (psXPC->f.alt_form && psXPC->f.form == form0G) {
			psXPC->f.minwid	= psXPC->f.precis ;			// keep trailing 0's
		} else {
			uint32_t u = u64Trailing0(X64.u64) ;
			X64.u64 /= u64pow(10, u) ;
			psXPC->f.minwid	= psXPC->f.precis - u ;		// remove trailing 0's
		}
	}
	if (psXPC->f.minwid > 0) {
		psXPC->f.pad0		= 1 ;						// MUST left pad with '0'
		psXPC->f.group		= 0 ;						// cannot group in fractional
		psXPC->f.signval	= 0 ;						// always unsigned value
		psXPC->f.negvalue	= 0 ;						// and never negative
		psXPC->f.plus		= 0 ;						// no leading +/- before fractional part
		Len += xPrintXxx(psXPC, X64.u64, Buffer, xpfMAX_LEN_F64-1-Len) ;
	}
	// process the radix = '.'
	if (psXPC->f.minwid || psXPC->f.radix) {
		Buffer[xpfMAX_LEN_F64 - 2 - Len] = '.' ;
		++Len ;
	}

	X64.u64	= F64 ;										// extract and convert the whole number portions
	psXPC->f.limits	= xpf.limits ;						// restore original limits & flags
	psXPC->f.flags	= xpf.flags ;

	// adjust minwid to do padding (if required) based on string length after adding whole number
	psXPC->f.minwid	= psXPC->f.minwid > Len ? psXPC->f.minwid - Len : 0 ;
	Len += xPrintXxx(psXPC, X64.u64, Buffer, xpfMAX_LEN_F64 - 1 - Len) ;

	psXPC->f.arg_prec	= 1 ;
	psXPC->f.precis		= Len ;
	psXPC->f.arg_width	= xpf.arg_width ;
	psXPC->f.minwid		= xpf.minwid ;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_F64 - 1 - Len)) ;
}

/**
 * @brief	write char value as 2 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU8(xpc_t * psXPC, uint8_t Value) {
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value >> 4)) ;
	vPrintChar(psXPC, cPrintNibbleToChar(psXPC, Value & 0x0F)) ;
}

/**
 * @brief	write uint16_t value as 4 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU16(xpc_t * psXPC, uint16_t Value) {
	vPrintHexU8(psXPC, (Value >> 8) & 0x000000FF) ;
	vPrintHexU8(psXPC, Value & 0x000000FF) ;
}

/**
 * @brief	write uint32_t value as 8 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU32(xpc_t * psXPC, uint32_t Value) {
	vPrintHexU16(psXPC, (Value >> 16) & 0x0000FFFF) ;
	vPrintHexU16(psXPC, Value & 0x0000FFFF) ;
}

/**
 * @brief	write uint64_t value as 16 hex chars to the buffer, always NULL terminated
 * @param	psXPC - pointer to control structure
 * @param	Value - unsigned byte to convert
 * return	none
 */
void vPrintHexU64(xpc_t * psXPC, uint64_t Value) {
	vPrintHexU32(psXPC, (Value >> 32) & 0xFFFFFFFFULL) ;
	vPrintHexU32(psXPC, Value & 0xFFFFFFFFULL) ;
}

/**
 * @brief	write series of char values as hex chars to the buffer, always NULL terminated
 * @param 	psXPC
 * @param 	Num		number of bytes to print
 * @param 	pStr	pointer to bytes to print
 * @comment			Use the following modifier flags
 *					'`'	select grouping separators ':' (byte) '-' (short) ' ' (word) '|' (dword)
 *					'#' via alt_form select reverse order (little vs big endian) interpretation
 */
void vPrintHexValues(xpc_t * psXPC, int Num, char * pStr) {
	int32_t	Size = 1 << psXPC->f.size ;
	if (psXPC->f.alt_form)								// '#' specified to invert order ?
		pStr += Num - Size ;							// working backwards so point to last

	x64_t	x64Val ;
	int32_t	Idx	= 0 ;
	while (Idx < Num) {
		switch (psXPC->f.size) {
		case 0:
			x64Val.x8[0].u8	  = *((uint8_t *) pStr) ;
			vPrintHexU8(psXPC, x64Val.x8[0].u8) ;
			break ;
		case 1:
			x64Val.x16[0].u16 = *((uint16_t *) pStr) ;
			vPrintHexU16(psXPC, x64Val.x16[0].u16) ;
			break ;
		case 2:
			x64Val.x32[0].u32 = *((uint32_t *) pStr) ;
			vPrintHexU32(psXPC, x64Val.x32[0].u32) ;
			break ;
		case 3:
			x64Val.u64	  = *((uint64_t *) pStr) ;
			vPrintHexU64(psXPC, x64Val.u64) ;
			break ;
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
		if (psXPC->f.form == form0G)				// no separator required?
			continue ;
		vPrintChar(psXPC, psXPC->f.form == form1F ? ':' :
						  psXPC->f.form == form2E ? '-' :
						  psXPC->f.form == form3X ? (
							(Idx % 8) == 0 ? ' ' :
							(Idx % 4) == 0 ? '|' :
							(Idx % 2) == 0 ? '-' : ':' ) : '?') ;
	}
}

// ############################### Proprietary extensions to printf() ##############################

/**
 * @brief	display a 32 bit hexadecimal number as address
 * @brief	Currently 64 bit addresses not supported ...
 * @param	psXPC - pointer to print control structure
 * @param	uint32_t - address to be printed
 * @param	psXPC - updated based on characters displayed
 * @return	none
 */
void vPrintPointer(xpc_t * psXPC, void * pVoid) {
	xPrintChars(psXPC, (char *) "0x") ;
	vPrintHexU32(psXPC, (uint32_t) pVoid) ;
}

seconds_t xPrintCalcSeconds(xpc_t * psXPC, tsz_t * psTSZ, struct tm * psTM) {
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

int	xPrintTimeCalcSize(xpc_t * psXPC, int i32Val) {
	if (!psXPC->f.rel_val || psXPC->f.pad0 || i32Val > 9) return psXPC->f.minwid = 2 ;
	psXPC->f.minwid = 1 ;
	return xDigitsInI32(i32Val, psXPC->f.group) ;
}

int	xPrintDate_Year(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	psXPC->f.minwid	= 0 ;
	int Len = xPrintXxx(psXPC, (uint64_t) (psTM->tm_year + YEAR_BASE_MIN), pBuffer, 4) ;
	if (psXPC->f.alt_form == 0)	pBuffer[Len++] = (psXPC->f.form == form3X) ? '/' : '-' ;
	return Len ;
}

int	xPrintDate_Month(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXPC, (uint64_t) (psTM->tm_mon + 1), pBuffer, psXPC->f.minwid = 2) ;
	pBuffer[Len++] = psXPC->f.alt_form ? ' ' : (psXPC->f.form == form3X) ? '/' : '-' ;
	return Len ;
}

int	xPrintDate_Day(xpc_t * psXPC, struct tm * psTM, char * pBuffer) {
	int Len = xPrintXxx(psXPC, (uint64_t) psTM->tm_mday, pBuffer, xPrintTimeCalcSize(psXPC, psTM->tm_mday)) ;
	pBuffer[Len++] = psXPC->f.alt_form ? ' ' :
					(psXPC->f.form == form3X && psXPC->f.rel_val) ? 'd' :
					(psXPC->f.form == form3X) ? ' ' : 'T' ;
	psXPC->f.pad0	= 1 ;
	return Len ;
}

void vPrintDate(xpc_t * psXPC, struct tm * psTM) {
	int	Len = 0 ;
	char Buffer[xpfMAX_LEN_DATE] ;
	psXPC->f.form	= psXPC->f.group ? form3X : form0G ;
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
		if (psTM->tm_mday || psXPC->f.pad0) Len += xPrintDate_Day(psXPC, psTM, Buffer + Len) ;
	}
	Buffer[Len] = 0 ;									// converted L to R, so terminate
	psXPC->f.limits	= 0 ;								// enable full string
	vPrintString(psXPC, Buffer) ;
}

void vPrintTime(xpc_t * psXPC, struct tm * psTM, uint32_t uSecs) {
	char Buffer[xpfMAX_LEN_TIME] ;
	int	Len ;
	psXPC->f.form	= psXPC->f.group ? form3X : form0G ;
	// Part 1: hours
	if (psTM->tm_hour || psXPC->f.pad0) {
		Len = xPrintXxx(psXPC, (uint64_t) psTM->tm_hour, Buffer, xPrintTimeCalcSize(psXPC, psTM->tm_hour)) ;
		Buffer[Len++]	= psXPC->f.form == form3X ? 'h' :  ':' ;
		psXPC->f.pad0	= 1 ;
	} else
		Len = 0 ;

	// Part 2: minutes
	if (psTM->tm_min || psXPC->f.pad0) {
		Len += xPrintXxx(psXPC, (uint64_t) psTM->tm_min, Buffer+Len, xPrintTimeCalcSize(psXPC, psTM->tm_min)) ;
		Buffer[Len++]	= psXPC->f.form == form3X ? 'm' :  ':' ;
		psXPC->f.pad0	= 1 ;
	}

	// Part 3: seconds
	Len += xPrintXxx(psXPC, (uint64_t) psTM->tm_sec, Buffer+Len, xPrintTimeCalcSize(psXPC, psTM->tm_sec)) ;
	// Part 4: [.xxxxxx]
	if (psXPC->f.radix && psXPC->f.alt_form == 0) {
		Buffer[Len++]	= psXPC->f.form == form3X ? 's' :  '.' ;
		psXPC->f.precis	= (psXPC->f.precis == 0) ? xpfDEF_TIME_FRAC :
						  (psXPC->f.precis > xpfMAX_TIME_FRAC) ? xpfMAX_TIME_FRAC :
						  psXPC->f.precis ;
		if (psXPC->f.precis < xpfMAX_TIME_FRAC) uSecs /= u32pow(10, xpfMAX_TIME_FRAC - psXPC->f.precis);
		psXPC->f.pad0	= 1 ;						// need leading '0's
		psXPC->f.signval= 0 ;
		psXPC->f.ljust	= 0 ;						// force R-just
		Len += xPrintXxx(psXPC, uSecs, Buffer+Len, psXPC->f.minwid	= psXPC->f.precis) ;
	} else if (psXPC->f.form == form3X) {
		Buffer[Len++]	= 's' ;
	}

	Buffer[Len] = 0 ;
	psXPC->f.limits	= 0 ;							// enable full string
	vPrintString(psXPC, Buffer) ;
}

void vPrintZone(xpc_t * psXPC, tsz_t * psTSZ) {
	int	Len = 0;
	char Buffer[configTIME_MAX_LEN_TZINFO];
	if (psTSZ->pTZ == 0) {								// If no TZ info supplied
		Buffer[Len++]	= 'Z' ;							// add 'Z' for Zulu/zero time zone

	} else if (psXPC->f.alt_form) {						// TZ info available but '#' format specified
		Len = xstrncpy(Buffer, (char *) " GMT", 4) ;	// show as GMT (ie UTC)

	} else {											// TZ info available & '+x:xx(???)' format requested
		#if	(timexTZTYPE_SELECTED == timexTZTYPE_RFC5424)
		psXPC->f.signval = 1;							// TZ hours offset is a signed value
		psXPC->f.plus = 1;								// force display of sign
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer+Len, psXPC->f.minwid = 3);
		Buffer[Len++] = psXPC->f.form ? 'h' : ':';
		psXPC->f.signval = 0;							// TZ offset minutes unsigned
		psXPC->f.plus  = 0;
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer+Len, psXPC->f.minwid = 2);

		#elif (timexTZTYPE_SELECTED == timexTZTYPE_POINTER)
		psXPC->f.signval	= 1 ;						// TZ hours offset is a signed value
		psXPC->f.plus		= 1 ;						// force display of sign
		Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3) ;
		Buffer[Len++]	= psXPC->f.form ? 'h' : ':' ;
		psXPC->f.signval	= 0 ;						// TZ offset minutes unsigned
		psXPC->f.plus		= 0 ;
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXPC->f.minwid = 2) ;
		if (psTSZ->pTZ->pcTZName) {						// handle TZ name (as string pointer) if there
			Buffer[Len++]	= '(' ;
			psXPC->f.minwid = 0 ;						// then complete with the TZ name
			while (psXPC->f.minwid < configTIME_MAX_LEN_TZNAME &&
					psTSZ->pTZ->pcTZName[psXPC->f.minwid] != 0) {
				Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->pcTZName[psXPC->f.minwid] ;
				psXPC->f.minwid++ ;
			}
			Len += psXPC->f.minwid ;
			Buffer[Len++]	= ')' ;
		}

		#elif (timexTZTYPE_SELECTED == timexTZTYPE_FOURCHARS)
		psXPC->f.signval	= 1 ;						// TZ hours offset is a signed value
		psXPC->f.plus		= 1 ;						// force display of sign
		Len = xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone / SECONDS_IN_HOUR, Buffer, psXPC->f.minwid = 3) ;
		Buffer[Len++]	= psXPC->f.form ? 'h' : ':' ;
		psXPC->f.signval	= 0 ;						// TZ offset minutes unsigned
		psXPC->f.plus		= 0 ;
		Len += xPrintXxx(psXPC, (int64_t) psTSZ->pTZ->timezone % SECONDS_IN_MINUTE, Buffer + Len, psXPC->f.minwid = 2) ;
		// Now handle the TZ name if there, check to ensure max 4 chars all UCase
		if (xstrverify(&psTSZ->pTZ->tzname[0], 'A', 'Z', configTIME_MAX_LEN_TZNAME) == erSUCCESS) {
			Buffer[Len++]	= '(' ;
			// then complete with the TZ name
			psXPC->f.minwid = 0 ;
			while ((psXPC->f.minwid < configTIME_MAX_LEN_TZNAME) &&
					(psTSZ->pTZ->tzname[psXPC->f.minwid] != 0) &&
					(psTSZ->pTZ->tzname[psXPC->f.minwid] != ' ')) {
				Buffer[Len + psXPC->f.minwid] = psTSZ->pTZ->tzname[psXPC->f.minwid] ;
				psXPC->f.minwid++ ;
			}
			Len += psXPC->f.minwid ;
			Buffer[Len++]	= ')' ;
		}
		#endif
	}
	Buffer[Len]		= 0 ;								// converted L to R, so add terminating NUL
	psXPC->f.limits	= 0 ;								// enable full string
	vPrintString(psXPC, Buffer) ;
}

void vPrintLocalDTZ(xpc_t * psXPC, tsz_t * psTSZ, uint8_t cFmt) {
	struct tm sTM;
	psXPC->f.pad0 = 1;
	uint32_t flags = psXPC->f.flags;
	seconds_t tSec = xTimeStampAsSeconds(psTSZ->usecs) + psTSZ->pTZ->timezone + (int) psTSZ->pTZ->daylight;
	xTimeGMTime(tSec, &sTM, psXPC->f.rel_val);
	if (cFmt == CHR_D || cFmt == CHR_Z)
		vPrintDate(psXPC, &sTM);
	if (cFmt == CHR_T || cFmt == CHR_Z)
		vPrintTime(psXPC, &sTM, (uint32_t)(psTSZ->usecs % MICROS_IN_SECOND));
	if (cFmt == CHR_Z) {
		psXPC->f.flags = flags;
		vPrintZone(psXPC, psTSZ);
	}
}

/**
 * vPrintURL() -
 * @param psXPC
 * @param pString
 */
void vPrintURL(xpc_t * psXPC, char * pStr) {
	if (halCONFIG_inMEM(pStr)) {
		char cIn ;
		while ((cIn = *pStr++) != 0) {
			if (INRANGE('A', cIn, 'Z', char)
			||	INRANGE('a', cIn, 'z', char)
			||	INRANGE('0', cIn, '9', char)
			||	cIn == '-' || cIn == '.' || cIn == '_' || cIn == '~') {
				vPrintChar(psXPC, cIn) ;
			} else {
				vPrintChar(psXPC, '%') ;
				vPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn >> 4)) ;
				vPrintChar(psXPC, cPrintNibbleToChar(psXPC, cIn & 0x0F)) ;
			}
		}
	} else {
		vPrintString(psXPC, pStr) ;						// "null" or "pOOR"
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
 *				'`'		Grouping of values using ' ' '-' or '|' as separators
 * 				'!'		Use relative address format
 * 				'#'		Use absolute address format
 *						Relative/absolute address prefixed using format '0x12345678:'
 * 				'+'		Add the ASCII char equivalents to the right of the hex output
 */
void vPrintHexDump(xpc_t * psXPC, int xLen, char * pStr) {
	for (int Now = 0; Now < xLen; Now += xpfHEXDUMP_WIDTH) {
		#if	(xpfSUPPORT_POINTER == 1)
		if (psXPC->f.ljust == 0) {						// display absolute or relative address
			vPrintPointer(psXPC, psXPC->f.rel_val ? (void *) Now : (void *)(pStr + Now)) ;
			xPrintChars(psXPC, (char *) ": ") ;
		}
		#endif
		// then the actual series of values in 8-32 bit groups
		int Width = (xLen - Now) > xpfHEXDUMP_WIDTH ? xpfHEXDUMP_WIDTH : xLen - Now ;
		vPrintHexValues(psXPC, Width, pStr + Now) ;
		if (psXPC->f.plus == 1) {						// handle values dumped as ASCII chars
		// handle space padding for ASCII dump to line up
			uint32_t	Count ;
			int	Size = 1 << psXPC->f.size ;
			Count = (xLen <= xpfHEXDUMP_WIDTH) ? 1 :
					((xpfHEXDUMP_WIDTH - Width) / Size) * (Size*2 + (psXPC->f.form ? 1 : 0)) + 1;
			while (Count--)	vPrintChar(psXPC, ' ') ;
			// handle same values dumped as ASCII characters
			for (Count = 0; Count < Width; ++Count) {
				uint32_t cChr = *(pStr + Now + Count) ;
			#if 0										// Device supports characters in range 0x80 to 0xFE
				vPrintChar(psXPC, (cChr < ' ' || cChr == 0x7F || cChr == 0xFF) ? '.' : cChr) ;
			#else										// Device does NOT support ANY characters > 0x7E
				vPrintChar(psXPC, (cChr < ' ' || cChr >= 0x7F) ? '.' : cChr) ;
			#endif
			}
			vPrintChar(psXPC, ' ') ;
		}
		if ((Now < xLen) && (xLen > xpfHEXDUMP_WIDTH))
			xPrintChars(psXPC, (char *) "\n");
	}
}

/**
 * vPrintBinary()
 * @brief		convert unsigned 32/64 bit value to 1/0 ASCI string
 * 				field width specifier is applied as mask starting from LSB to MSB
 * @param[in]	psXPC - pointer to print control structure
 * @param[in]	ullVal - 64bit value to convert to binary string & display
 * @param[out]	none
 * @return		none
 * @comment		Honour & interpret the following modifier flags
 * 				'`'		Group digits using '|' (bytes) and '-' (nibbles)
 */
void vPrintBinary(xpc_t * psXPC, uint64_t ullVal) {
	int	len ;
	if (psXPC->f.minwid == 0)		len = (psXPC->f.llong == 1) ? 64 : 32 ;
	else if (psXPC->f.llong == 1)	len = (psXPC->f.minwid > 64) ? 64 : psXPC->f.minwid ;
	else len = (psXPC->f.minwid > 32) ? 32 : psXPC->f.minwid ;
	uint64_t mask	= 1ULL << (len - 1) ;
	psXPC->f.form	= psXPC->f.group ? form3X : form0G ;
	while (mask) {
		vPrintChar(psXPC, (ullVal & mask) ? '1' : '0') ;
		mask >>= 1;
	// handle the complex grouping separator(s) boundary 8 use '|' or 4 use '-'
		if (--len && psXPC->f.form) {
			vPrintChar(psXPC, len % 32 == 0 ? '|' :		// word boundary
							  len % 16 == 0 ? ':' :		// short boundary
							  len % 8 == 0 ? ' ' :		// byte boundary
							  len % 4 == 0 ? '-' : '?') ;	// nibble
		}
	}
}

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
void vPrintIpAddress(xpc_t * psXPC, uint32_t Val) {
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
	Buffer[xpfMAX_LEN_IP - 1] = 0 ;

	// then convert the IP address, LSB first
	int	Idx, Len ;
	for (Idx = 0, Len = 0; Idx < sizeof(Val); ++Idx) {
		uint64_t u64Val = (uint8_t) pChr[Idx] ;
		Len += xPrintXxx(psXPC, u64Val, Buffer + xpfMAX_LEN_IP - 5 - Len, 4) ;
		if (Idx < 3) Buffer[xpfMAX_LEN_IP - 1 - ++Len] = '.';
	}
	// then send the formatted output to the correct stream
	psXPC->f.limits	= 0 ;
	vPrintString(psXPC, Buffer + (xpfMAX_LEN_IP - 1 - Len)) ;
}

/**
 * vPrintSetGraphicRendition() - set starting and ending fore/background colors
 * @param psXPC
 * @param Val	U32 value treated as 4x U8 being SGR color/attribute codes
 */
void vPrintSetGraphicRendition(xpc_t * psXPC, uint32_t Val) {
	char Buffer[xpfMAX_LEN_SGR] ;
	sgr_info_t sSGR ;
	sSGR.u32 = Val ;
	if (pcANSIlocate(Buffer, sSGR.c, sSGR.d) != Buffer)
		xPrintChars(psXPC, Buffer) ;
	if (pcANSIattrib(Buffer, sSGR.a, sSGR.b) != Buffer)
		xPrintChars(psXPC, Buffer) ;
}

/* ################################# The HEART of the PRINTFX matter ###############################
 * PrintFX - common routine for formatted print functionality
 * @brief	parse the format string and interpret the conversions, flags and modifiers
 * 			extract the parameters variables in correct type format
 * 			call the correct routine(s) to perform conversion, formatting & output
 * @param	psXPC - pointer to structure containing formatting and output destination info
 * 			format - pointer to the formatting string
 * 			vaList - variable number of arguments
 * @return	void (other than updated info in the original structure passed by reference
 */

int	xpcprintfx(xpc_t * psXPC, const char * fmt, va_list vaList) {
	if (fmt == NULL)
		return 0;
	for (; *fmt != 0; ++fmt) {
	// start by expecting format indicator
		if (*fmt == '%') {
			psXPC->f.flags 	= 0 ;						// set ALL flags to default 0
			psXPC->f.limits	= 0 ;						// reset field specific limits
			psXPC->f.nbase	= BASE10 ;					// default number base
			++fmt ;
			if (*fmt == 0)
				break;
			/* In order for the optional modifiers to work correctly, especially in cases such as HEXDUMP
			 * the modifiers MUST be in correct sequence of interpretation being [ ! # ' * + - % 0 ] */
			int	cFmt ;
			x32_t X32 = { 0 } ;
			while ((cFmt = xinstring("!#'*+- 0%", *fmt)) != erFAILURE) {
				switch (cFmt) {
				case 0:									// '!' HEXDUMP absolute->relative address
					++fmt ;								// DTZ absolute->relative time
					psXPC->f.rel_val = 1 ;				// MAC use ':' separator
					break ;
				case 1:									// '#' DTZ alternative form
					++fmt ;								// HEXDUMP / IP swop endian
					psXPC->f.alt_form = 1 ;				// STRING middle justify
					break ;
				case 2:									// "'" decimal= ',' separated 3 digit grouping
					++fmt ;								// DTZ=alternate format
					psXPC->f.group = 1 ;
					break ;
				case 3:									// '*' indicate argument will supply field width
					X32.i32	= va_arg(vaList, int) ;
					IF_myASSERT(debugTRACK, psXPC->f.arg_width == 0 && X32.i32 <= xpfMINWID_MAXVAL) ;
					++fmt ;
					psXPC->f.minwid = X32.i32 ;
					psXPC->f.arg_width = 1 ;
					break ;
				case 4:									// '+' force leading +/- signed
					++fmt ;								// or HEXDUMP add ASCII char dump
					psXPC->f.plus = 1 ;					// or TIME add TZ info
					break ;
				case 5:									// '-' Left justify modifier
					++fmt ;								// or HEXDUMP remove address pointer
					psXPC->f.ljust = 1 ;
					break ;
				case 6:									// ' ' instead of '+'
					++fmt ;
					psXPC->f.Pspc = 1 ;
					break ;
				case 7:									// '0 force leading '0's
					++fmt ;
					psXPC->f.pad0 = 1 ;
					break ;
				default:								// '%' literal to display
					goto out_lbl ;
				}
			}
			// handle pre and post decimal field width/precision indicators
			if (*fmt == '.' || INRANGE('0', *fmt, '9', char)) {
				X32.i32 = 0;
				while (1) {
					if (INRANGE('0', *fmt, '9', char)) {
						X32.i32 *= 10;
						X32.i32 += *fmt - '0';
						++fmt;
					} else if (*fmt == '.') {
						IF_myASSERT(debugTRACK, psXPC->f.radix == 0);	// 2x radix not allowed
						++fmt;
						psXPC->f.radix = 1;
						if (X32.i32 > 0) {
							IF_myASSERT(debugTRACK, psXPC->f.arg_width == 0 && X32.i32 <= xpfMINWID_MAXVAL);
							psXPC->f.minwid = X32.i32;
							psXPC->f.arg_width = 1;
							X32.i32 = 0;
						}
					} else if (*fmt == '*') {
						IF_myASSERT(debugTRACK, psXPC->f.radix == 1 && X32.i32 == 0);
						++fmt;
						X32.i32	= va_arg(vaList, int);
						IF_myASSERT(debugTRACK, X32.i32 <= xpfPRECIS_MAXVAL);
						psXPC->f.precis = X32.i32;
						psXPC->f.arg_prec = 1;
						X32.i32 = 0;
					} else {
						break;
					}
				}
				// Save possible parsed value in cFmt
				if (X32.i32 > 0) {
					if (psXPC->f.arg_width == 0 && psXPC->f.radix == 0) {
						IF_myASSERT(debugTRACK, X32.i32 <= xpfMINWID_MAXVAL);
						psXPC->f.minwid	= X32.i32;
						psXPC->f.arg_width = 1;
					} else if (psXPC->f.arg_prec == 0 && psXPC->f.radix == 1) {
						IF_myASSERT(debugTRACK, X32.i32 <= xpfPRECIS_MAXVAL) ;
						psXPC->f.precis	= X32.i32;
						psXPC->f.arg_prec = 1;
					} else {
						IF_myASSERT(debugTRACK, 0) ;
					}
				}
			}
			// handle 2x successive 'l' characters, lower case ONLY, form long long value treatment
			if (*fmt == 'l' && *(fmt+1) == 'l') {
				fmt += 2 ;
				psXPC->f.llong = 1 ;
			}
			// Check if format character where UC/lc same character control the case of the output
			cFmt = *fmt ;
			if (xinstring(vPrintStr1, cFmt) != erFAILURE) {
				cFmt |= 0x20 ;							// convert to lower case, but ...
				psXPC->f.Ucase = 1 ;					// indicate as UPPER case requested
			}

			x64_t	x64Val ;							// default x64 variable
			px_t	px ;
			#if	(xpfSUPPORT_DATETIME == 1)
			tsz_t * psTSZ ;
			struct tm sTM;
			#endif
			switch (cFmt) {
			#if	(xpfSUPPORT_SGR == 1)
			/* XXX: Since we are using the same command handler to process (UART, HTTP & TNET)
			 * requests, and we are embedding colour ESC sequences into the formatted output,
			 * and the colour ESC sequences (handled by UART & TNET) are not understood by
			 * the HTTP protocol, we must try to filter out the possible output produced by
			 * the ESC sequences if the output is going to a socket, one way or another.
			 */
			case CHR_C:
				vPrintSetGraphicRendition(psXPC, va_arg(vaList, uint32_t)) ;
				break ;
			#endif

			#if	(xpfSUPPORT_IP_ADDR == 1)						// IP address
			case CHR_I:
				vPrintIpAddress(psXPC, va_arg(vaList, uint32_t)) ;
				break ;
			#endif

			#if	(xpfSUPPORT_BINARY == 1)
			case CHR_J:
				x64Val.u64 = psXPC->f.llong ? va_arg(vaList, uint64_t) : (uint64_t) va_arg(vaList, uint32_t) ;
				vPrintBinary(psXPC, x64Val.u64) ;
				break ;
			#endif

			#if	(xpfSUPPORT_DATETIME == 1)
			/* Prints date and/or time in POSIX format
			 * Use the following modifier flags
			 *	'`'		select between 2 different separator sets being
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
				psTSZ = va_arg(vaList, tsz_t *) ;
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(psTSZ)) ;
				psXPC->f.pad0 = 1;
				X32.u32 = xTimeStampAsSeconds(psTSZ->usecs) + psTSZ->pTZ->timezone + (int) psTSZ->pTZ->daylight;
				xTimeGMTime(X32.u32, &sTM, psXPC->f.rel_val);
				if (cFmt == CHR_D || cFmt == CHR_Z)
					vPrintDate(psXPC, &sTM);
				if (cFmt == CHR_T || cFmt == CHR_Z)
					vPrintTime(psXPC, &sTM, (uint32_t)(psTSZ->usecs % MICROS_IN_SECOND));
				if (cFmt == CHR_Z) {
					vPrintZone(psXPC, psTSZ);
				}
				break ;

			case CHR_R:				// U64 epoch (yr+mth+day) OR relative (days) + TIME
				IF_myASSERT(debugTRACK, !psXPC->f.plus && !psXPC->f.pad0 && !psXPC->f.group);
				x64Val.u64 = va_arg(vaList, uint64_t);
				if (psXPC->f.alt_form) {
					psXPC->f.group = 1 ;
					psXPC->f.alt_form = 0 ;
				}
				X32.u32 = xTimeStampAsSeconds(x64Val.u64) ;
				xTimeGMTime(X32.u32, &sTM, psXPC->f.rel_val) ;
				if (psXPC->f.rel_val == 0)
					psXPC->f.pad0 = 1;
				if (psXPC->f.rel_val == 0 || sTM.tm_mday) {
					uint32_t limits = psXPC->f.limits ;
					vPrintDate(psXPC, &sTM) ;
					psXPC->f.limits = limits ;
				}
				vPrintTime(psXPC, &sTM, (uint32_t) (x64Val.u64 % MICROS_IN_SECOND)) ;
				if (psXPC->f.rel_val == 0)
					vPrintChar(psXPC, 'Z');
				break ;
			#endif

			#if	(xpfSUPPORT_URL == 1)							// para = pointer to string to be encoded
			case CHR_U:
				px.pc8	= va_arg(vaList, char *) ;
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(px.pc8)) ;
				vPrintURL(psXPC, px.pc8) ;
				break ;
			#endif

			#if	(xpfSUPPORT_HEXDUMP == 1)
			case CHR_B:									// HEXDUMP 8bit sized
			case CHR_H:									// HEXDUMP 16bit sized
			case CHR_W:									// HEXDUMP 32bit sized
				IF_myASSERT(debugTRACK, !psXPC->f.arg_width && !psXPC->f.arg_prec) ;
				/* In order for formatting to work  the "*" or "." radix specifiers
				 * should not be used. The requirement for a second parameter is implied and assumed */
				psXPC->f.form	= psXPC->f.group ? form3X : form0G ;
				psXPC->f.size = cFmt == 'B' ? xpfSIZING_BYTE : cFmt == 'H' ? xpfSIZING_SHORT : xpfSIZING_WORD ;
				psXPC->f.size	+= psXPC->f.llong ? 1 : 0 ; 	// apply ll modifier to size
				X32.i32	= va_arg(vaList, int) ;
				px.pc8	= va_arg(vaList, char *) ;
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(px.pc8)) ;
				vPrintHexDump(psXPC, X32.i32, px.pc8) ;
				break ;
			#endif

			#if	(xpfSUPPORT_MAC_ADDR == 1)
			/* Formats 6 byte string (0x00 is valid) as a series of hex characters.
			 * default format uses no separators eg. '0123456789AB'
			 * Support the following modifier flags:
			 *  '!'	select ':' separator between digits
			 */
			case CHR_m:									// MAC address UC/LC format ??:??:??:??:??:??
				IF_myASSERT(debugTRACK, !psXPC->f.arg_width && !psXPC->f.arg_prec) ;
				psXPC->f.size	= 0 ;
				psXPC->f.llong	= 0 ;					// force interpretation as sequence of U8 values
				psXPC->f.form	= psXPC->f.group ? form1F : form0G ;
				px.pc8	= va_arg(vaList, char *) ;
				IF_myASSERT(debugTRACK, halCONFIG_inMEM(px.pc8)) ;
				vPrintHexValues(psXPC, lenMAC_ADDRESS, px.pc8) ;
				break ;
			#endif

			case CHR_c:
				vPrintChar(psXPC, va_arg(vaList, int)) ;
				break ;

			case CHR_d:									// signed decimal "[-]ddddd"
			case CHR_i:									// signed integer (same as decimal ?)
				psXPC->f.signval = 1 ;
				x64Val.i64	= psXPC->f.llong ? va_arg(vaList, int64_t) : va_arg(vaList, int32_t) ;
				if (x64Val.i64 < 0LL)	{
					psXPC->f.negvalue = 1;
					x64Val.i64 *= -1; 					// convert the value to unsigned
				}
				vPrintX64(psXPC, x64Val.i64) ;
				break ;

			case CHR_o:									// unsigned octal "ddddd"
			case CHR_x:									// hex as in "789abcd" UC/LC
				psXPC->f.group = 0 ;					// disable grouping
				/* FALLTHRU */ /* no break */
			case CHR_u:									// unsigned decimal "ddddd"
				x64Val.u64	= psXPC->f.llong ? va_arg(vaList, uint64_t) : va_arg(vaList, uint32_t) ;
				psXPC->f.nbase = cFmt == 'x' ? BASE16 : cFmt == 'o' ? BASE08 : BASE10 ;
				vPrintX64(psXPC, x64Val.u64) ;
				break ;

			#if	(xpfSUPPORT_IEEE754 == 1)
			case CHR_e:									// form = 2
				psXPC->f.form++ ;
				/* FALLTHRU */ /* no break */
			case CHR_f:									// form = 1
				psXPC->f.form++ ;
				/* FALLTHRU */ /* no break */
			case CHR_a:									// stopgap, HEX format no supported
			case CHR_g:									// form = 0
				psXPC->f.signval = 1 ;					// float always signed value.
				/* https://en.cppreference.com/w/c/io/fprintf
				 * https://pubs.opengroup.org/onlinepubs/007908799/xsh/fprintf.html
				 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-160
				 */
				if (psXPC->f.arg_prec) {				// explicit precision specified ?
					psXPC->f.precis = psXPC->f.precis > xpfMAXIMUM_DECIMALS ? xpfMAXIMUM_DECIMALS : psXPC->f.precis ;
				} else {
					psXPC->f.precis	= xpfDEFAULT_DECIMALS ;
				}
				vPrintF64(psXPC, va_arg(vaList, double));
				break ;
			#endif

			#if	(xpfSUPPORT_POINTER == 1)						// pointer value UC/lc
			case CHR_p:
				// Does cause crash if pointer not currently mapped
				vPrintPointer(psXPC, va_arg(vaList, void *)) ;
				break ;
			#endif

			case CHR_s:
				px.pv = va_arg(vaList, char *) ;
				// Required to avoid crash when wifi message is intercepted and a string pointer parameter
				// is evaluated as out of valid memory address (0xFFFFFFE6). Replace string with "pOOR"
				px.pc8 = halCONFIG_inMEM(px.pc8) ? px.pc8 : px.pc8 == NULL ? STRING_NULL : STRING_OOR ;
				vPrintString(psXPC, px.pc8) ;
				break;

			case CHR_b:							// Unsupported types to be filtered.
			case CHR_h:
			case CHR_w:
				myASSERT(0) ;
				break ;

			default:
				/* At this stage we have handled the '%' as assumed, but the next character found is invalid.
				 * Show the '%' we swallowed and then the extra, invalid, character as well */
				vPrintChar(psXPC, '%') ;
				vPrintChar(psXPC, *fmt) ;
				break ;
			}
			psXPC->f.plus				= 0 ;		// reset this form after printing one value
		} else {
out_lbl:
			vPrintChar(psXPC, *fmt) ;
		}
	}
	return psXPC->f.curlen ;
}

int	xprintfx(int (Hdlr)(xpc_t *, int), void * pVoid, size_t szBuf, const char * fmt, va_list vaList) {
	xpc_t	sXPC ;
	sXPC.handler	= Hdlr ;
	sXPC.pVoid		= pVoid ;
	sXPC.f.maxlen	= (szBuf > xpfMAXLEN_MAXVAL) ? xpfMAXLEN_MAXVAL : szBuf ;
	sXPC.f.curlen	= 0 ;
	return xpcprintfx(&sXPC, fmt, vaList) ;
}

// ##################################### Destination = STRING ######################################

int	xPrintToString(xpc_t * psXPC, int cChr) {
	if (psXPC->pStr)
		*psXPC->pStr++ = cChr;
	return cChr ;
}

int vsnprintfx(char * pBuf, size_t szBuf, const char * format, va_list vaList) {
	if (szBuf == 1) {									// any "real" space ?
		if (pBuf)										// no, buffer supplied?
			*pBuf = 0 ;									// yes, terminate
		return 0 ; 										// & return
	}
	int iRV = xprintfx(xPrintToString, pBuf, szBuf, format, vaList) ;
	if (pBuf) {											// buffer specified ?
		if (iRV == szBuf) 								// yes, but full?
			--iRV ;										// make space for terminator
		pBuf[iRV] = 0 ;									// terminate
	}
	return iRV ;
}

int snprintfx(char * pBuf, size_t szBuf, const char * format, ...) {
	va_list vaList ;
	va_start(vaList, format) ;
	int iRV = vsnprintfx(pBuf, szBuf, format, vaList) ;
	va_end(vaList) ;
	return iRV ;
}
#if (buildNEW_CODE == 1)

#define vsprintfx(pBuf,format,vaList)	vsnprintfx(pBuf,xpfMAXLEN_MAXVAL,format,vaList)
#define sprintfx(pBuf,format,...)		snprintfx(pBuf,xpfMAXLEN_MAXVAL,format,##__VA_ARGS__)

#else
int vsprintfx(char * pBuf, const char * format, va_list vaList) {
	return vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vaList) ;
}

int sprintfx(char * pBuf, const char * format, ...) {
	va_list	vaList ;
	va_start(vaList, format) ;
	int iRV = vsnprintfx(pBuf, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
	return iRV ;
}
#endif
// ################################### Destination = STDOUT ########################################

static SemaphoreHandle_t printfxMux = NULL ;

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
	return putchar(cChr) ;
#endif
}

int vnprintfx(size_t szLen, const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xprintfx(xPrintStdOut, NULL, szLen, format, vaList);
	printfx_unlock();
	return iRV ;
}

}

int nprintfx(size_t szLen, const char * format, ...) {
	va_list vaList ;
	va_start(vaList, format) ;
	int iRV = vnprintfx(szLen, format, vaList) ;
	va_end(vaList) ;
	return iRV ;
int vprintfx(const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xprintfx(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	return iRV;
}

int printfx(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	printfx_lock();
	int iRV = xprintfx(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	va_end(vaList);
	return iRV;
}

#endif

/*
 * [v[n]]printfx_nolock() - print to stdout without any semaphore locking.
 * 					securing the channel must be done manually
 */
int vnprintfx_nolock(size_t szLen, const char * format, va_list vaList) {
	return xprintfx(xPrintStdOut, NULL, szLen, format, vaList);
}

int vprintfx_nolock(const char * format, va_list vaList) {
	return xprintfx(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList);
}

int printfx_nolock(const char * format, ...) {
	va_list vaList;
	va_start(vaList, format);
	int iRV = xprintfx(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
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

int	wsnprintfx(char ** ppcBuf, size_t * pSize, const char * pcFormat, ...) {
	va_list vaList ;
	va_start(vaList, pcFormat) ;
	int iRV ;
	if (*ppcBuf && (*pSize > 1)) {
		iRV = vsnprintfx(*ppcBuf, *pSize, pcFormat, vaList) ;
		if (iRV > 0) {
			*ppcBuf	+= iRV ;
			*pSize	-= iRV ;
		}
	} else {
		iRV = xprintfx(xPrintStdOut, NULL, xpfMAXLEN_MAXVAL, pcFormat, vaList) ;
	}
	va_end(vaList) ;
	return iRV ;
}

// ################################### Destination = FILE PTR ######################################

int	xPrintToFile(xpc_t * psXPC, int cChr) { return fputc(cChr, psXPC->stream) ; }

int vfprintfx(FILE * stream, const char * format, va_list vaList) {
	return xprintfx(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vaList);
}

int fprintfx(FILE * stream, const char * format, ...) {
	va_list vaList ;
	va_start(vaList, format) ;
	int count = xprintfx(xPrintToFile, stream, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
	return count ;
}

// ################################### Destination = HANDLE ########################################

int	xPrintToHandle(xpc_t * psXPC, int cChr) {
	char cChar = cChr ;
	int size = write(psXPC->fd, &cChar, sizeof(cChar)) ;
	return size == 1 ? cChr : size ;
}

int	vdprintfx(int fd, const char * format, va_list vaList) {
	return xprintfx(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vaList) ;
}

int	dprintfx(int fd, const char * format, ...) {
	va_list	vaList ;
	va_start(vaList, format) ;
	int count = xprintfx(xPrintToHandle, (void *) fd, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
	return count ;
}

#if	defined(ESP_PLATFORM)
/* ################################## Destination = UART/TELNET ####################################
 * Output directly to the [possibly redirected] stdout/UART channel
 */

int	xPrintToConsole(xpc_t * psXPC, int cChr) { return putcharRT(cChr) ; }

int vcprintfx(const char * format, va_list vaList) {
	printfx_lock();
	int iRV = xprintfx(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList);
	printfx_unlock();
	return iRV;
}

int cprintfx(const char * format, ...) {
	va_list vaList ;
	va_start(vaList, format) ;
	printfx_lock();
	int iRV = xprintfx(xPrintToConsole, NULL, xpfMAXLEN_MAXVAL, format, vaList) ;
	printfx_unlock();
	va_end(vaList) ;
	return iRV ;
}

// ################################### Destination = DEVICE ########################################

int	xPrintToDevice(xpc_t * psXPC, int cChr) { return psXPC->DevPutc(cChr) ; }

int vdevprintfx(int (* Hdlr)(int ), const char * format, va_list vaList) {
	return xprintfx(xPrintToDevice, Hdlr, xpfMAXLEN_MAXVAL, format, vaList) ;
}

int devprintfx(int (* Hdlr)(int ), const char * format, ...) {
	va_list vaList ;
	va_start(vaList, format) ;
	int iRV = xprintfx(xPrintToDevice, Hdlr, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
	return iRV ;
}

/* #################################### Destination : SOCKET #######################################
 * SOCKET directed formatted print support. Problem here is that MSG_MORE is primarily supported on
 * TCP sockets, UDP support officially in LwIP 2.6 but has not been included into ESP-IDF yet. */

int	xPrintToSocket(xpc_t * psXPC, int cChr) {
	char cBuf = cChr ;
	int iRV = xNetWrite(psXPC->psSock, &cBuf, sizeof(cBuf));
	if (iRV != sizeof(cBuf))
		return iRV;
	return cChr ;
}

int vsocprintfx(netx_t * psSock, const char * format, va_list vaList) {
	int	Fsav = psSock->flags;
	psSock->flags |= MSG_MORE ;
	int iRV = xprintfx(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vaList) ;
	psSock->flags = Fsav ;
	return (psSock->error == 0) ? iRV : erFAILURE ;
}

int socprintfx(netx_t * psSock, const char * format, ...) {
	int	Fsav = psSock->flags;
	psSock->flags |= MSG_MORE ;
	va_list vaList ;
	va_start(vaList, format) ;
	int iRV = xprintfx(xPrintToSocket, psSock, xpfMAXLEN_MAXVAL, format, vaList) ;
	va_end(vaList) ;
	psSock->flags = Fsav ;
	return (psSock->error == 0) ? iRV : erFAILURE ;
}

// #################################### Destination : UBUF #########################################

int	xPrintToUBuf(xpc_t * psXPC, int cChr) { return xUBufPutC(psXPC->psUBuf, cChr) ; }

int	vuprintfx(ubuf_t * psUBuf, const char * format, va_list vaList) {
	return xprintfx(xPrintToUBuf, psUBuf, xUBufSpace(psUBuf), format, vaList) ;
}

int	uprintfx(ubuf_t * psUBuf, const char * format, ...) {
	va_list	vaList ;
	va_start(vaList, format) ;
	int count = vuprintfx(psUBuf, format, vaList) ;
	va_end(vaList) ;
	return count ;
}
#endif		// ESP_PLATFORM

// #################################### Destination : CRC32 ########################################

static int xPrintToCRC32(xpc_t * psXPC, int cChr) {
	#if defined(ESP_PLATFORM)							// use ROM based CRC lookup table
	uint8_t cBuf = cChr;
	*psXPC->pU32 = crc32_le(*psXPC->pU32, &cBuf, sizeof(cBuf));
	#else												// use fastest of external libraries
	uint32_t MsgCRC = crcSlow((uint8_t *) pBuf, iRV);
	#endif
	return cChr;
}

int	vcrcprintfx(unsigned int * pU32, const char * format, va_list vaList) {
	return xprintfx(xPrintToCRC32, pU32, xpfMAXLEN_MAXVAL, format, vaList);
}

int	crcprintfx(unsigned int * pU32, const char * format, ...) {
	va_list	vaList;
	va_start(vaList, format);
	int count = xprintfx(xPrintToCRC32, pU32, xpfMAXLEN_MAXVAL, format, vaList);
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
	int	printf(const char * format, ...) __attribute__((alias("printfx"), unused)) ;
	int	printf_r(const char * format, ...) __attribute__((alias("printfx"), unused)) ;

	int	vprintf(const char * format, va_list vaList) __attribute__((alias("vprintfx"), unused)) ;
	int	vprintf_r(const char * format, va_list vaList) __attribute__((alias("vprintfx"), unused)) ;

	int	sprintf(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused)) ;
	int	sprintf_r(char * pBuf, const char * format, ...) __attribute__((alias("sprintfx"), unused)) ;

	int	vsprintf(char * pBuf, const char * format, va_list vaList) __attribute__((alias("vsprintfx"), unused)) ;
	int	vsprintf_r(char * pBuf, const char * format, va_list vaList) __attribute__((alias("vsprintfx"), unused)) ;

	int snprintf(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused)) ;
	int snprintf_r(char * pBuf, size_t BufSize, const char * format, ...) __attribute__((alias("snprintfx"), unused)) ;

	int vsnprintf(char * pBuf, size_t BufSize, const char * format, va_list vaList) __attribute__((alias("vsnprintfx"), unused)) ;
	int vsnprintf_r(char * pBuf, size_t BufSize, const char * format, va_list vaList) __attribute__((alias("vsnprintfx"), unused)) ;

	int fprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;
	int fprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;

	int vfprintf(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused)) ;
	int vfprintf_r(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused)) ;

	int dprintf(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused)) ;
	int dprintf_r(int fd, const char * format, ...) __attribute__((alias("dprintfx"), unused)) ;

	int vdprintf(int fd, const char * format, va_list vaList) __attribute__((alias("vdprintfx"), unused)) ;
	int vdprintf_r(int fd, const char * format, va_list vaList) __attribute__((alias("vdprintfx"), unused)) ;

	int fiprintf(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;
	int fiprintf_r(FILE * stream, const char * format, ...) __attribute__((alias("fprintfx"), unused)) ;

	int vfiprintf(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused)) ;
	int vfiprintf_r(FILE * stream, const char * format, va_list vaList) __attribute__((alias("vfprintfx"), unused)) ;
#endif
