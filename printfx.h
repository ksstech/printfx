/*
 * Copyright (c) 2014-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS_Support.h"
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

// "format" used by ALL tracking macros.
extern unsigned long long RunTime ;

#define	_L_(f)						"[%s:%d] " f "", __FUNCTION__, __LINE__
#define	_T_(f)						"%!.R: " f "", RunTime
#define	_TL_(f)						"%!.R: %s:%d " f "", RunTime, __FUNCTION__, __LINE__

#define	P(f, ...)					printfx(f, ##__VA_ARGS__)
#define	PL(f, ...)					printfx(_L_(f), ##__VA_ARGS__)
#define	PT(f, ...)					printfx(_T_(f), ##__VA_ARGS__)
#define	PTL(f, ...)					printfx(_TL_(f), ##__VA_ARGS__)

#define	IF_P(T, f, ...)				if (T) P(f, ##__VA_ARGS__)
#define	IF_PL(T, f, ...)			if (T) PL(f, ##__VA_ARGS__)
#define	IF_PT(T, f, ...)			if (T) PT(f, ##__VA_ARGS__)
#define	IF_PTL(T, f, ...)			if (T) PTL(f, ##__VA_ARGS__)

#define	CP(f, ...)					printfx_nolock(f, ##__VA_ARGS__)
#define	CPL(f, ...)					printfx_nolock(_L_(f), ##__VA_ARGS__)
#define	CPT(f, ...)					printfx_nolock(_T_(f), ##__VA_ARGS__)
#define	CPTL(f, ...)				printfx_nolock(_TL_(f), ##__VA_ARGS__)

#define	IF_CP(T, f, ...)			if (T) CP(f, ##__VA_ARGS__)
#define	IF_CPL(T, f, ...)			if (T) CPL(f, ##__VA_ARGS__)
#define	IF_CPT(T, f, ...)			if (T) CPT(f, ##__VA_ARGS__)
#define	IF_CPTL(T, f, ...)			if (T) CPTL(f, ##__VA_ARGS__)

// Using ROM based esp_rom_printf
#define	RP(f, ...)					esp_rom_printf(f, ##__VA_ARGS__)
#define	RL(f, ...)					esp_rom_printf(_L_(f), ##__VA_ARGS__)

#define	IF_RP(T, f, ...)			if (T) RP(f, ##__VA_ARGS__)
#define	IF_RL(T, f, ...)			if (T) RL(f, ##__VA_ARGS__)

// ################################## public build definitions #####################################

#define	xpfMAXIMUM_DECIMALS			15
#define	xpfDEFAULT_DECIMALS			6

#define	xpfMAX_TIME_FRAC			6		// control resolution mS/uS/nS
#define	xpfDEF_TIME_FRAC			3

#ifdef ESP_PLATFORM
		/* Specifically for the ESP-IDF we1`	 * as x[v]fprintf(stdout, format, ...) with locking enabled */
	#define	xpfSUPPORT_ALIASES			1
#else
	#define	xpfSUPPORT_ALIASES		1
#endif
#define	xpfSUPPORT_FILTER_NUL		1

// ################################## x[snf]printf() related #######################################

#define	xpfMAX_LEN_TIME				sizeof("12:34:56.654321")
#define	xpfMAX_LEN_DATE				sizeof("Sun, 10 Sep 2017")

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

#define	xpfSGR(a,b,c,d)				(((uint8_t) d << 24) + ((uint8_t) c << 16) + ((uint8_t) b << 8) + (uint8_t) a)

// ####################################### enumerations ############################################

enum { srcS, srcF, srcT, srcN } ;

// #################################### Public structures ##########################################

typedef union {
	struct __attribute__((packed)) { uint8_t a, b, c, d ; } ;
	uint8_t u8[sizeof(uint32_t)];
	uint32_t u32;
} sgr_info_t ;
DUMB_STATIC_ASSERT(sizeof(sgr_info_t) == 4) ;

typedef	struct __attribute__((packed)) xpf_t {
	union {
		uint32_t	lengths ;							// maxlen & curlen ;
		struct __attribute__((packed)) {
			uint32_t	maxlen		: xpfMAXLEN_BITS ;	// max chars to output 0 = unlimited
			uint32_t	curlen		: xpfMAXLEN_BITS ;	// number of chars output so far
		} ;
	} ;
	union {
		uint32_t	limits ;
		struct __attribute__((packed)) {
			uint32_t	minwid		: xpfMINWID_BITS ;	// min field width
			uint32_t	precis		: xpfPRECIS_BITS ;	// float precision or max string length
		} ;
	} ;
	union {
		uint32_t	flags ;								// rest of flags
		struct __attribute__((packed)) {
		// byte 0
			uint8_t		group 		: 1 ;				// 0 = disable, 1 = enable
			uint8_t		alt_form	: 1 ;				// '#'
			uint8_t		ljust		: 1 ;				// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
			uint8_t		Ucase		: 1 ;				// true = 'a' or false = 'A'
			uint8_t		pad0		: 1 ;				// true = pad with leading'0'
			uint8_t		llong		: 1 ;				// long long override flag
			uint8_t		radix		: 1 ;
			uint8_t		rel_val		: 1 ;				// relative address / elapsed time
		// byte 1
			uint8_t		nbase 		: 5 ;				// 2, 8, 10 or 16
			uint8_t		size		: 2 ;				// size of value ie byte / half / word / lword
			uint8_t		negvalue	: 1 ;				// if value < 0
		// byte 2
			uint8_t		form		: 2 ;				// format specifier FLOAT, DUMP & TIME
			uint8_t		signval		: 1 ;				// true = content is signed value
			uint8_t		plus		: 1 ;				// true = force use of '+' or '-' signed
			uint8_t		arg_width	: 1 ;				// minwid specified
			uint8_t		arg_prec	: 1 ;				// precis specified
			uint8_t		dbg			: 1 ;
			uint8_t		Pspc		: 1 ;
			// byte 3
			uint8_t		spare		: 8 ;				// SPARE !!!
		} ;
	} ;
} xpf_t ;
DUMB_STATIC_ASSERT(sizeof(xpf_t) == 12) ;

typedef	struct __attribute__((packed)) xpc_t {
	int 	(*handler)(struct xpc_t * , int ) ;
	union {
		void *			pVoid ;
		char *			pStr ;							// string buffer
		FILE *			stream ;						// file stream
		struct netx_t *	psSock ;						// socket context
		struct ubuf_t *	psUBuf ;						// ubuf
		int				fd ;							// file descriptor/handle
		int 			(*DevPutc)(int ) ;				// custom device driver
		unsigned int *	pU32;							// Address of running CRC32 value
	} ;
	xpf_t	f ;
} xpc_t ;
DUMB_STATIC_ASSERT(sizeof(xpc_t) == (12 + sizeof(int *) + sizeof(void *))) ;

// ################################### Public functions ############################################

int xPrintFX(xpc_t * psXPC, const char * format, va_list vArgs);

int xPrintF(int (handler)(xpc_t *, int), void *, size_t, const char *, va_list);

/* Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string */

// ##################################### Destination = STRING ######################################

int vsnprintfx(char * , size_t , const char * , va_list );
int snprintfx(char * , size_t , const char * , ...);
int vsprintfx(char * , const char * , va_list );
int sprintfx(char * , const char * , ...);

// ##################################### Destination = STDOUT ######################################

extern SemaphoreHandle_t printfxMux;
void printfx_lock(void);
void printfx_unlock(void);

int vnprintfx(size_t, const char *, va_list);
int vprintfx(const char * , va_list);
int nprintfx(size_t, const char *, ...);
int printfx(const char *, ...);

int vnprintfx_nolock(size_t count, const char * format, va_list);
int vprintfx_nolock(const char * format, va_list);
int printfx_nolock(const char * format, ...);

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

int vcrcprintfx(unsigned int *, const char * , va_list);
int crcprintfx(unsigned int *, const char * , ...);

// ##################################### functional tests ##########################################

#ifdef __cplusplus
}
#endif
