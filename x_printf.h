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
 *	x_printf.h
 */

#pragma once

#include	"x_definitions.h"
#include	"x_sockets.h"
#include	"x_ubuf.h"

#if		(ESP32_PLATFORM == 1)
	#include	<regex.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ###################### control functionality included in xprintf.c ##############################

#define	xpfSUPPORT_BINARY				1
#define	xpfSUPPORT_MAC_ADDR				1
#define	xpfSUPPORT_IP_ADDR				1
#define	xpfSUPPORT_HEXDUMP				1
#define	xpfSUPPORT_POINTER				1
#define	xpfSUPPORT_DATETIME				1
#define	xpfSUPPORT_IEEE754				1		// float point support in x_printf.c functions
#define	xpfSUPPORT_SCALING				1		// scale number down by 10^3/10^6/10^9/10^12
#define	xpfSUPPORT_SGR					1		// Set Graphics Rendition FG & BG colors only
#define	xpfSUPPORT_URL					1		// URL encoding

#define	xpfMAXIMUM_DECIMALS				15
#define	xpfDEFAULT_DECIMALS				6

/* Specifically for the ESP-IDF we force output to x[v]printf(format, ...) to be treated same
 * as x[v]fprintf(stdout, format, ...) with locking enabled */
#define	xpfSUPPORT_ALIASES				1
#define	xpfSUPPORT_FILTER_NUL			1

// ################################## x[snf]printf() related #######################################

#define	xpfMAX_DIGITS_FRAC_SEC			3

#if		(xpfMAX_DIGITS_FRAC_SEC == 0)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(1000000)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 1)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(100000)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 2)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(10000)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 3)
	#define	xpfTIME_FRAC_SEC_DIVISOR	(1000)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 4)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(100)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 5)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(10)
#elif	(xpfMAX_DIGITS_FRAC_SEC == 6)
	#define	xpfTIME_FRAC_SEC_DIVISOR		(1)
#else
	#error "Invalid values for xpfMAX_DIGITS_FRAC_SEC (must be 0 -> 6)"
#endif


#define	xpfMAX_LEN_FRAC_SEC				(xpfMAX_DIGITS_FRAC_SEC + 3)
#define	xpfMAX_LEN_TIME					(sizeof("12:34:56.") + xpfMAX_LEN_FRAC_SEC)

#define	xpfMAX_LEN_DATE					sizeof("Sun, 10 Sep 2017")	// was "2015-04-01T"

#define	xpfMAX_LEN_DTZ					(xpfMAX_LEN_DATE + xpfMAX_LEN_TIME + configTIME_MAX_LEN_TZINFO)

#define xpfMAX_LEN_X32					sizeof("+4,294,967,295")
#define xpfMAX_LEN_X64					sizeof("+18,446,744,073,709,551,615")

#define xpfMAX_LEN_F32					(sizeof("+4,294,967,295.") + xpfMAXIMUM_DECIMALS)
#define xpfMAX_LEN_F64					(sizeof("+18,446,744,073,709,551,615.") + xpfMAXIMUM_DECIMALS)

#define	xpfMAX_LEN_B32					sizeof("1010-1010|1010-1010 1010-1010|1010-1010")
#define	xpfMAX_LEN_B64					(xpfMAX_LEN_B32 * 2)

#define	xpfMAX_LEN_IP					sizeof("123.456.789.012")
#define	xpfMAX_LEN_MAC					sizeof("01:23:45:67:89:ab")

#define	xpfMAX_LEN_SGR					sizeof("\033[?;??;?;??;?;??;?;??m\000")

#define	xpfNULL_STRING					"'null'"

#define	xpfFLAGS_NONE					((xpf_t) 0ULL)

// Used in hexdump to determine size of each value conversion
#define	xpfSIZING_BYTE					0			// 8 bit byte
#define	xpfSIZING_SHORT					1			// 16 bit short
#define	xpfSIZING_WORD					2			// 32 bit word
#define	xpfSIZING_DWORD					3			// 64 bit long long word

#define	xpfFORMAT_0_G					0			// FLOAT 'G' or 'g' else NONE
#define	xpfFORMAT_1_F					1			// FLOAT 'F' or 'f' else COLON
#define	xpfFORMAT_2_E					2			// FLOAT 'E' or 'e'
#define	xpfFORMAT_3						3			// COMPLEX

/* For HEXDUMP functionality the size is as follows
 * ( 0x12345678 {32 x 3} [ 32 x Char]) = 142 plus some safety = 160 characters max.
 * When done in stages max size about 96 */
#define	xpfHEXDUMP_WIDTH				32			// number of bytes (as bytes/short/word/llong) in a single row

/* Maximum size is determined by bit width of maxlen and curlen fields below */
#define	xpfMAXLEN_BITS					16			// Number of bits in field(s)
#define	xpfMAXLEN_MAXVAL				((1 << xpfMAXLEN_BITS) - 1)

#define	xpfMINWID_BITS					16			// Number of bits in field(s)
#define	xpfMINWID_MAXVAL				((1 << xpfMINWID_BITS) - 1)
#define	xpfMINWID_MINVAL				0

#define	xpfPRECIS_BITS					16			// Number of bits in field(s)
#define	xpfPRECIS_MAXVAL				((1 << xpfPRECIS_BITS) - 1)
#define	xpfPRECIS_MINVAL				0

#define	xpfSGR(a,b,c,d)					(((uint8_t) a << 24) + ((uint8_t) b << 16) + ((uint8_t) c << 8) + (uint8_t) d)

/* https://en.wikipedia.org/wiki/ANSI_escape_code#Escape_sequences
 * http://www.termsys.demon.co.uk/vtansi.htm#colors
 */
typedef union {
	struct {
		uint8_t		d ;
		uint8_t		c ;
		uint8_t		b ;
		uint8_t		a ;
	} ;
	uint8_t		u8[sizeof(uint32_t)] ;
	uint32_t	u32 ;
} sgr_info_t ;

typedef	union xpf_u {
	struct {
		uint32_t	lengths ;							// maxlen & curlen ;
		uint32_t	limits ;							// minwid & precision
		uint32_t	flags ;								// rest of flags
	} ;
	struct {
	/* Be careful about changing the size of the next 4 fields since the 16bits each
	 * align directly with the uint32_t fields "limits and "flags" above. The "flags" field is
	 * used to reset the flags in the man vPrint routine */
		uint32_t	maxlen		: xpfMAXLEN_BITS ;		// max chars to output 0 = unlimited
		uint32_t	curlen		: xpfMAXLEN_BITS ;		// number of chars output so far
		uint32_t	minwid		: xpfMINWID_BITS ;		// minimum field width
		uint32_t	precision	: xpfPRECIS_BITS ;		// number of decimal digits or width of string
	// byte 0
		uint8_t		group 		: 1 ;					// 0 = disable, 1 = enable
		uint8_t		alt_form	: 1 ;					// '#'
		uint8_t		ljust		: 1 ;					// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
		uint8_t		Ucase		: 1 ;					// true = 'a' or false = 'A'
		uint8_t		pad0		: 1 ;					// true = pad with leading'0'
		uint8_t		llong		: 1 ;					// long long override flag
		uint8_t		radix		: 1 ;					// radix found flag
		uint8_t		abs_rel		: 1 ;					// relative address / elapsed time
	// byte 1
		uint8_t		nbase 		: 5 ;					// 2, 8, 10 or 16
		uint8_t		size		: 2 ;					// size of value ie byte / half / word / lword
		uint8_t		negvalue	: 1 ;					// if value < 0
	// byte 2
		uint8_t		form		: 2 ;					// format specifier in FLOAT & DUMP
		uint8_t		signval		: 1 ;					// true = content is signed value
		uint8_t		plus		: 1 ;					// true = force use of '+' or '-' signed
		uint8_t		arg_width	: 1 ;					// read arg for width
		uint8_t		arg_prec	: 1 ;					// read arg for precision
		uint8_t		yday_ok		: 1 ;					// day of year
		uint8_t		year_ok		: 1 ;					// year
	// byte 3
		uint8_t		mon_ok		: 1 ;					// month
		uint8_t		dow_ok		: 1 ;					// day of week
		uint8_t		mday_ok		: 1 ;					// day of month
		uint8_t		hour_ok		: 1 ;					// hours
		uint8_t		min_ok		: 1 ;					// minutes
		uint8_t		sec_ok		: 1 ;					// seconds
	// unused
		uint8_t		no_zone		: 1 ;					// 1=delay doing 'Z'
		uint8_t		spare		: 1 ;
	} ;
} xpf_t ;

typedef	struct xpc_s {
	int 	(*handler)(struct xpc_s * , int ) ;
	union {
		void *		pVoid ;
		char *		pStr ;
		FILE *		stream ;
		sock_ctx_t * psSock ;						// socket context pointer
		ubuf_t *	psUBuf ;
		int			fd ;							// file descriptor/handle
		int 		(*DevPutc)(int ) ;
	} ;
	xpf_t	f ;
} xpc_t ;

/*
 * Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string
 */
int		xPrint(int (handler)(xpc_t *, int), void *, size_t, const char *, va_list) ;
// ##################################### Destination = STRING ######################################
int 	xvsnprintf(char * , size_t , const char * , va_list ) ;
int 	xsnprintf(char * , size_t , const char * , ...) ;
int 	xvsprintf(char * , const char * , va_list ) ;
int		xsprintf(char * , const char * , ...) ;
// ################################### Destination = FILE PTR ######################################
int		xvfprintf(FILE * , const char * , va_list ) ;
int		xfprintf(FILE * , const char * , ...) ;
// ################################### Destination = STDOUT ########################################
int 	xvnprintf(size_t, const char *, va_list) ;
int 	xvprintf(const char * , va_list) ;
int 	xnprintf(size_t, const char *, ...) ;
int		xprintf(const char *, ...) ;
// ################################### Destination = HANDLE ########################################
int		xvdprintf(int , const char * , va_list ) ;
int		xdprintf(int , const char * , ...) ;
// ################################### Destination = DEVICE ########################################
int 	vdevprintf(int (* handler)(int ), const char *, va_list) ;
int 	devprintf(int (* handler)(int), const char *, ...) ;
// #################################### Destination : SOCKET #######################################
int 	vsocprintf(sock_ctx_t *, const char *, va_list) ;
int 	socprintf(sock_ctx_t *, const char *, ...) ;
// #################################### Destination : UBUF #########################################
int     vuprintf(ubuf_t *, const char * , va_list) ;
int     uprintf(ubuf_t *, const char * , ...) ;
// ############################## LOW LEVEL DIRECT formatted output ################################
//void	cprintf_lock(void) ;
//void	cprintf_unlock(void) ;
int 	vcprintf(const char *, va_list) ;
int 	cprintf(const char *, ...) ;
int		cprintf_noblock(const char *, ...) ;
// ##################################### functional tests ##########################################

#ifdef __cplusplus
}
#endif
