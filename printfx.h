/*
 * Copyright (c) 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#pragma once

#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS_Support.h"
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

// "format" used by ALL tracking macros.
extern u64_t RunTime;

#define	_L_(f)						" [%s:%d] " f "", __FUNCTION__, __LINE__
#define	_T_(f)						" [%!.R] " f "", RunTime
#define	_TL_(f)						" [%!.R:%s:%d] " f "", RunTime, __FUNCTION__, __LINE__

#define	P(f, ...)					printf(f, ##__VA_ARGS__)
#define	PX(f, ...)					printfx(f, ##__VA_ARGS__)
#define	PL(f, ...)					printfx(_L_(f), ##__VA_ARGS__)
#define	PT(f, ...)					printfx(_T_(f), ##__VA_ARGS__)
#define	PTL(f, ...)					printfx(_TL_(f), ##__VA_ARGS__)

#define	IF_P(T, f, ...)				if (T) P(f, ##__VA_ARGS__)
#define	IF_PX(T, f, ...)			if (T) PX(f, ##__VA_ARGS__)
#define	IF_PL(T, f, ...)			if (T) PL(f, ##__VA_ARGS__)
#define	IF_PT(T, f, ...)			if (T) PT(f, ##__VA_ARGS__)
#define	IF_PTL(T, f, ...)			if (T) PTL(f, ##__VA_ARGS__)

#define	CP(f, ...)					cprintfx(f, ##__VA_ARGS__)
#define	CPL(f, ...)					cprintfx(_L_(f), ##__VA_ARGS__)
#define	CPT(f, ...)					cprintfx(_T_(f), ##__VA_ARGS__)
#define	CPTL(f, ...)				cprintfx(_TL_(f), ##__VA_ARGS__)

#define	IF_CP(T, f, ...)			if (T) CP(f, ##__VA_ARGS__)
#define	IF_CPL(T, f, ...)			if (T) CPL(f, ##__VA_ARGS__)
#define	IF_CPT(T, f, ...)			if (T) CPT(f, ##__VA_ARGS__)
#define	IF_CPTL(T, f, ...)			if (T) CPTL(f, ##__VA_ARGS__)

// Using ROM based esp_rom_printf (no 64bit support so 32bit timestamps)
#define	_RL_(f)						"[%s:%d " f "]", __FUNCTION__, __LINE__
#define	_RT_(f)						"[%u.%03u " f "]", u32TS_Seconds(RunTime), u32TS_FracMillis(RunTime)
#define	_RTL_(f)					"[%u.%03u:%s:%d " f "]", u32TS_Seconds(RunTime), u32TS_FracMillis(RunTime), __FUNCTION__, __LINE__

#define	RP(f, ...)					esp_rom_printf(f, ##__VA_ARGS__)
#define	RPL(f, ...)					esp_rom_printf(_RL_(f), ##__VA_ARGS__)
#define	RPT(f, ...)					esp_rom_printf(_RT_(f), ##__VA_ARGS__)
#define	RPTL(f, ...)				esp_rom_printf(_RTL_(f), ##__VA_ARGS__)

#define	IF_RP(T, f, ...)			if (T) RP(f, ##__VA_ARGS__)
#define	IF_RPL(T, f, ...)			if (T) RPL(f, ##__VA_ARGS__)
#define	IF_RPT(T, f, ...)			if (T) RPT(f, ##__VA_ARGS__)

// ################################## public build definitions #####################################

#define	xpfMAXIMUM_DECIMALS			15
#define	xpfDEFAULT_DECIMALS			6

#define	xpfMAX_TIME_FRAC			6		// control resolution mS/uS/nS
#define	xpfDEF_TIME_FRAC			3

// ################################## C11 Pointer size determination ###############################

#include <stdint.h>

_Static_assert(sizeof (void*) == sizeof (uintptr_t), "TBD code needed to determine pointer size");

// C99 or later
#if UINTPTR_MAX == 0xFFFF
	#define xpfSIZE_POINTER			2
#elif UINTPTR_MAX == 0xFFFFFFFF
	#define xpfSIZE_POINTER			4
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
	#define xpfSIZE_POINTER			8
#else
	#error TBD pointer size
#endif

/* Number of bits in inttype_MAX, or in any (1<<k)-1 where 0 <= k < 2040 */
#define IMAX_BITS(m) ((m)/((m)%255+1) / 255%255*8 + 7-86/((m)%255+12))
#define UINTPTR_MAX_BITWIDTH IMAX_BITS(UINTPTR_MAX)

// ################################## x[snf]printf() related #######################################

#define xpfMAX_LEN_PNTR				((xpfSIZE_POINTER * 2) + sizeof("0x"))

#define	xpfMAX_LEN_TIME				sizeof("12:34:56.654321")
#define	xpfMAX_LEN_DATE				sizeof("Sun, 10 Sep 2017   ")
#define	xpfMAX_LEN_DTZ				(xpfMAX_LEN_DATE + xpfMAX_LEN_TIME + configTIME_MAX_LEN_TZINFO)

#define xpfMAX_LEN_X32				sizeof("+4,294,967,295")
#define xpfMAX_LEN_X64				sizeof("+18,446,744,073,709,551,615")

#define xpfMAX_LEN_F32				(sizeof("+4,294,967,295.") + xpfMAXIMUM_DECIMALS)
#define xpfMAX_LEN_F64				(sizeof("+18,446,744,073,709,551,615.") + xpfMAXIMUM_DECIMALS)

#define	xpfMAX_LEN_B32				sizeof("1010-1010|1010-1010 1010-1010|1010-1010")
#define	xpfMAX_LEN_B64				(xpfMAX_LEN_B32 * 2)

#define	xpfMAX_LEN_IP				sizeof("123.456.789.012")
#define	xpfMAX_LEN_MAC				sizeof("01:23:45:67:89:ab")

#define	xpfMAX_LEN_SGR				sizeof("\033[???;???;???;???m\000")

#define	xpfNULL_STRING				"'null'"

#define	xpfFLAGS_NONE				((xpf_t) 0ULL)

// Used in hexdump to determine size of each value conversion
#define	xpfSIZING_BYTE				0			// 8 bit byte
#define	xpfSIZING_SHORT				1			// 16 bit short
#define	xpfSIZING_WORD				2			// 32 bit word
#define	xpfSIZING_DWORD				3			// 64 bit long long word

/* For HEXDUMP functionality the size is as follows
 * ( 0x12345678 {32 x 3} [ 32 x Char]) = 142 plus some safety = 160 characters max.
 * When done in stages max size about 96 */
#define	xpfHEXDUMP_WIDTH			32			// number of bytes (as bytes/short/word/llong) in a single row

/* Maximum size is determined by bit width of maxlen and curlen fields below */
#define	xpfMAXLEN_BITS				16			// Number of bits in field(s)
#define	xpfMAXLEN_MAXVAL			((1 << xpfMAXLEN_BITS) - 1)

#define	xpfMINWID_BITS				16			// Number of bits in field(s)
#define	xpfMINWID_MAXVAL			((1 << xpfMINWID_BITS) - 1)

#define	xpfPRECIS_BITS				16			// Number of bits in field(s)
#define	xpfPRECIS_MAXVAL			((1 << xpfPRECIS_BITS) - 1)

/* https://en.wikipedia.org/wiki/ANSI_escape_code#Escape_sequences
 * http://www.termsys.demon.co.uk/vtansi.htm#colors
 */

#define	xpfSGR(a,b,c,d)				(((u8_t) d << 24) + ((u8_t) c << 16) + ((u8_t) b << 8) + (u8_t) a)

// ####################################### enumerations ############################################


// #################################### Public structures ##########################################

typedef union {
	struct __attribute__((packed)) { u8_t a, b, c, d ; } ;
	u8_t u8[sizeof(u32_t)];
	u32_t u32;
} sgr_info_t ;
DUMB_STATIC_ASSERT(sizeof(sgr_info_t) == 4) ;

typedef	struct xpf_t {
	union {
		u32_t	lengths ;							// maxlen & curlen ;
		struct __attribute__((packed)) {
			u32_t	maxlen		: xpfMAXLEN_BITS ;	// max chars to output 0 = unlimited
			u32_t	curlen		: xpfMAXLEN_BITS ;	// number of chars output so far
		} ;
	} ;
	union {
		u32_t	limits ;
		struct __attribute__((packed)) {
			u32_t	minwid		: xpfMINWID_BITS ;	// min field width
			u32_t	precis		: xpfPRECIS_BITS ;	// float precision or max string length
		} ;
	} ;
	union {
		u32_t	flags ;								// rest of flags
		struct __attribute__((packed)) {
/*byte 0*/	u8_t	group 		: 1 ;				// 0 = disable, 1 = enable
			u8_t	alt_form	: 1 ;				// '#'
			u8_t	ljust		: 1 ;				// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
			u8_t	Ucase		: 1 ;				// true = 'a' or false = 'A'
			u8_t	pad0		: 1 ;				// true = pad with leading'0'
			u8_t	radix		: 1 ;
			u8_t	signval		: 1 ;				// true = content is signed value
			u8_t	rel_val		: 1 ;				// relative address / elapsed time
/*byte 1*/	u32_t	nbase 		: 5 ;				// 2, 8, 10 or 16
			u8_t	form		: 2 ;				// format specifier FLOAT, DUMP & TIME
			u8_t	negvalue	: 1 ;				// if value < 0
/*byte 2*/	u8_t	llong		: 4 ;				// va_arg override
			u8_t	arg_width	: 1 ;				// minwid specified
			u8_t	arg_prec	: 1 ;				// precis specified
			u8_t	plus		: 1 ;				// true = force use of '+' or '-' signed
			u8_t	dbg			: 1 ;
/*byte 3*/	u8_t	Pspc		: 1 ;
			u8_t	spare		: 7 ;				// SPARE !!!
		};
	};
} xpf_t ;
DUMB_STATIC_ASSERT(sizeof(xpf_t) == 12);

typedef	struct xpc_t {
	int (*handler)(struct xpc_t *, int);
	union {
		void * pVoid;
		char * pStr;				// string buffer
		FILE * stream;				// file stream
		struct netx_t *	psSock;		// socket context
		struct ubuf_t *	psUBuf;		// ubuf
		int fd;						// file descriptor/handle
		int (*DevPutc)(int);		// custom device driver
		unsigned int * pU32;		// Address of running CRC32 value
	};
	xpf_t f;
	va_list vaList;
} xpc_t ;
DUMB_STATIC_ASSERT(sizeof(xpc_t) == (sizeof(int *) + sizeof(void *) + sizeof(xpf_t) + sizeof(va_list)));

// ################################### Public functions ############################################

int xPrintFX(xpc_t * psXPC, const char * format);
int xPrintF(int (handler)(xpc_t *, int), void *, size_t, const char *, va_list);

/* Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string */

// ##################################### Destination = STRING ######################################

int vsnprintfx(char *, size_t, const char *, va_list)	_ATTRIBUTE ((__format__ (__printf__, 3, 0)));
//int vsnprintfx(char *, size_t, const char *, va_list);

int vsprintfx(char *, const char *, va_list)			_ATTRIBUTE ((__format__ (__printf__, 2, 0)));
//int vsprintfx(char *, const char *, va_list);

//int snprintfx(char *, size_t, const char *, ...)		_ATTRIBUTE ((__format__ (__printf__, 3, 4)));
int snprintfx(char *, size_t, const char *, ...);

int sprintfx(char *, const char *, ...)					_ATTRIBUTE ((__format__ (__printf__, 2, 3)));
//int sprintfx(char *, const char *, ...);

// ##################################### Destination = STDOUT ######################################

extern SemaphoreHandle_t printfxMux;
void printfx_lock(void);
void printfx_unlock(void);

int vnprintfx_nolock(size_t count, const char * format, va_list);
int vprintfx_nolock(const char * format, va_list);
int printfx_nolock(const char * format, ...);

int vnprintfx(size_t, const char *, va_list)	_ATTRIBUTE ((__format__ (__printf__, 2, 0)));
//int vnprintfx(size_t, const char *, va_list);

int vprintfx(const char * , va_list)			_ATTRIBUTE ((__format__ (__printf__, 1, 0)));
//int vprintfx(const char * , va_list);

int nprintfx(size_t, const char *, ...) 		_ATTRIBUTE ((__format__ (__printf__, 2, 3)));
//int nprintfx(size_t, const char *, ...);

//int printfx(const char *, ...)					_ATTRIBUTE ((__format__ (__printf__, 1, 2)));
int printfx(const char *, ...);

// ############################## LOW LEVEL DIRECT formatted output ################################

int vcprintfx(const char *, va_list);
int cprintfx(const char *, ...);

int wsnprintfx(char ** ppcBuf, size_t * pSize, const char * pcFormat, ...);

// ################################### Destination = FILE PTR ######################################

int vfprintfx(FILE * , const char * , va_list );
int fprintfx(FILE * , const char * , ...);

// ################################### Destination = HANDLE ########################################

int vdprintfx(int , const char *, va_list );
int dprintfx(int , const char *, ...);

// ################################### Destination = DEVICE ########################################

int vdevprintfx(int (* handler)(int ), const char *, va_list);
int devprintfx(int (* handler)(int), const char *, ...);

// #################################### Destination : SOCKET #######################################

int vsocprintfx(struct netx_t *, const char *, va_list);
int socprintfx(struct netx_t *, const char *, ...);

// #################################### Destination : UBUF #########################################

int vuprintfx(struct ubuf_t *, const char * , va_list);
int uprintfx(struct ubuf_t *, const char * , ...);

// #################################### Destination : CRC32 ########################################

int vcrcprintfx(u32_t *, const char * , va_list);
int crcprintfx(u32_t *, const char * , ...);

// ##################################### functional tests ##########################################

#ifdef __cplusplus
}
#endif
