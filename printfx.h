// printfx.h

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
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

#define	P(f, ...)					printfx(f, ##__VA_ARGS__)
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
#define	_RL_(f)						"[%s:%d]" f "", __FUNCTION__, __LINE__
#define	_RT_(f)						"[%u.%03u]" f "", u32TS_Seconds(RunTime), u32TS_FracMillis(RunTime)
#define	_RTL_(f)					"[%u.%03u:%s:%d]" f "", u32TS_Seconds(RunTime), u32TS_FracMillis(RunTime), __FUNCTION__, __LINE__

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

_Static_assert(sizeof (void*) == sizeof (uintptr_t), "TBD code needed to determine pointer size");

// C99 or later
#if UINTPTR_MAX == 0xFFFF
	#define xpfSIZE_POINTER			2
#elif UINTPTR_MAX == 0xFFFFFFFF
	#define xpfSIZE_POINTER			4
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
	#define xpfSIZE_POINTER			8
#else
	#error "TBD pointer size!!!"
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
#define	xpfMAXLEN_MAXVAL			((u32_t) ((1 << xpfMAXLEN_BITS) - 1))

#define	xpfMINWID_BITS				16			// Number of bits in field(s)
#define	xpfMINWID_MAXVAL			((u32_t) ((1 << xpfMINWID_BITS) - 1))

#define	xpfPRECIS_BITS				16			// Number of bits in field(s)
#define	xpfPRECIS_MAXVAL			((u32_t) ((1 << xpfPRECIS_BITS) - 1))

/* https://en.wikipedia.org/wiki/ANSI_escape_code#Escape_sequences
 * http://www.termsys.demon.co.uk/vtansi.htm#colors
 */

#define	xpfSGR(a,b,c,d)				(((u8_t) d << 24) + ((u8_t) c << 16) + ((u8_t) b << 8) + (u8_t) a)

#define	xpfBITS_REPORT				8

// ####################################### enumerations ############################################


// #################################### Public structures ##########################################

typedef union {
	struct __attribute__((packed)) { u8_t a, b, c, d; };
	u8_t u8[sizeof(u32_t)];
	u32_t u32;
} sgr_info_t;
DUMB_STATIC_ASSERT(sizeof(sgr_info_t) == 4);

/**
 * structures used with xprintfx() and wvprintfx() functions used to facilitate 
 * the transparent transfer of selected flags from the report_t structure into 
 * the flags member of the xpf_t structure.
*/
typedef	union {
	struct __attribute__((packed)) {// 8:24 Generic
		union {
			struct __attribute__((packed)) { u32_t z00:1, z01:1, z02:1, z03:1, zv04:1, z05:1, z06:1, z07:1, z08:1, z09:1, z10:1, z11:1, z12:1, z13:1, z14:1, z15:1, z16:1, z17:1, z18:1, z19:1, z20:1, z21:1, z22:1, z23:1; };
			struct __attribute__((packed)) { u32_t y00:2, y01:1, y02:1, y03:1, yv04:1, y05:1, y06:1, y07:1, y08:1, y09:1, y10:1, y11:1, y12:1, y13:1, y14:1, y15:1, y16:1, y17:1, y18:1, y19:1, y20:1, y21:1, y22:1; };
			struct __attribute__((packed)) { u32_t x00:3, x01:1, x02:1, x03:1, xv04:1, x05:1, x06:1, x07:1, x08:1, x09:1, x10:1, x11:1, x12:1, x13:1, x14:1, x15:1, x16:1, x17:1, x18:1, x19:1, x20:1, x21:1; };
			struct __attribute__((packed)) { u32_t w00:4, w01:1, w02:1, w03:1, wv04:1, w05:1, w06:1, w07:1, w08:1, w09:1, w10:1, w11:1, w12:1, w13:1, w14:1, w15:1, w16:1, w17:1, w18:1, w19:1, w20:1; };
			struct __attribute__((packed)) { u32_t v00:5, v01:1, v02:1, v03:1, vv04:1, v05:1, v06:1, v07:1, v08:1, v09:1, v10:1, v11:1, v12:1, v13:1, v14:1, v15:1, v16:1, v17:1, v18:1, v19:1; };
			struct __attribute__((packed)) { u32_t u00:6, u01:1, u02:1, u03:1, uv04:1, u05:1, u06:1, u07:1, u08:1, u09:1, u10:1, u11:1, u12:1, u13:1, u14:1, u15:1, u16:1, u17:1, u18:1; };
			struct __attribute__((packed)) { u32_t t00:7, t01:1, t02:1, t03:1, tv04:1, t05:1, t06:1, t07:1, t08:1, t09:1, t10:1, t11:1, t12:1, t13:1, t14:1, t15:1, t16:1, t17:1; };
			struct __attribute__((packed)) { u32_t s00:8, s01:1, s02:1, s03:1, sv04:1, s05:1, s06:1, s07:1, s08:1, s09:1, s10:1, s11:1, s12:1, s13:1, s14:1, s15:1, s16:1; };
			struct __attribute__((packed)) { u32_t r00:9, r01:1, r02:1, r03:1, rv04:1, r05:1, r06:1, r07:1, r08:1, r09:1, r10:1, r11:1, r12:1, r13:1, r14:1, r15:1; };
			struct __attribute__((packed)) { u32_t q00:10, q01:1, q02:1, q03:1, qv04:1, q05:1, q06:1, q07:1, q08:1, q09:1, q10:1, q11:1, q12:1, q13:1, q14:1; };
			struct __attribute__((packed)) { u32_t p00:11, p01:1, p02:1, p03:1, pv04:1, p05:1, p06:1, p07:1, p08:1, p09:1, p10:1, p11:1, p12:1, p13:1; };
			struct __attribute__((packed)) { u32_t o00:12, o01:1, o02:1, o03:1, ov04:1, o05:1, o06:1, o07:1, o08:1, o09:1, o10:1, o11:1, o12:1; };
			struct __attribute__((packed)) { u32_t n00:13, n01:1, n02:1, n03:1, nv04:1, n05:1, n06:1, n07:1, n08:1, n09:1, n10:1, n11:1; };
			struct __attribute__((packed)) { u32_t m00:14, m01:1, m02:1, m03:1, mv04:1, m05:1, m06:1, m07:1, m08:1, m09:1, m10:1; };
			struct __attribute__((packed)) { u32_t l00:15, l01:1, l02:1, l03:1, lv04:1, l05:1, l06:1, l07:1, l08:1, l09:1; };
			struct __attribute__((packed)) { u32_t k00:16, k01:1, k02:1, k03:1, kv04:1, k05:1, k06:1, k07:1, k08:1; };
			struct __attribute__((packed)) { u32_t h00:17, h01:1, h02:1, h03:1, hv04:1, h05:1, h06:1, h07:1; };
			struct __attribute__((packed)) { u32_t g00:18, g01:1, g02:1, g03:1, gv04:1, g05:1, g06:1; };
			struct __attribute__((packed)) { u32_t f00:19, f01:1, f02:1, f03:1, fv04:1, f05:1; };
			struct __attribute__((packed)) { u32_t e00:20, e01:1, e02:1, e03:1, ev04:1; };
			struct __attribute__((packed)) { u32_t d00:21, d01:1, d02:1, d03:1; };
			struct __attribute__((packed)) { u32_t c00:22, c01:1, c02:1; };
			struct __attribute__((packed)) { u32_t b00:23, b01:1; };
			struct __attribute__((packed)) { u32_t a00:24; };
		};
		u32_t	h:1, g:1, f:1, e:1, d:1, c:1, b:1, a:1;
	};
	struct __attribute__((packed)) {// 8:24 General printing
		u32_t	aCount:24;			// Counter
		u32_t	aColor:1;			// Use colour
		u32_t	aPrioX:1;			// Priorities
		u32_t	aState:1;			// Task state RBPS
		u32_t	aStack:1;			// Low Stack value
		u32_t	aCore:1;			// MCU 01X
		u32_t	aXtras:1;
		u32_t	aNL:1;
		u32_t	aRT:1;
	};
	struct __attribute__((packed)) {// 8:24 (fs) filesystem
		u32_t	fsCount:24;			// Counter
		u32_t	fsColor:1;			// Use colour
		u32_t	fsContent:1;		// File content
		u32_t	fsDetail:1;			// File details 
		u32_t	fsS2:1;				// 
		u32_t	fsS1:1;				// 
		u32_t	fsS0:1;				//
		u32_t	fsNL:1;
		u32_t	fsRT:1;
	};
	struct __attribute__((packed)) {// 9:23 Printing tasks
		u32_t	uCount:23;			// Task # mask
		u32_t	bTskNum:1;			// Task #
		u32_t	bColor:1;
		u32_t	bPrioX:1;			// Priorities
		u32_t	bState:1;			// Task state RBPS
		u32_t	bStack:1;			// Low Stack value
		u32_t	bCore:1;			// MCU 01X
		u32_t	bXtras:1;
		u32_t	bNL:1;				// NewLine
		u32_t	bRT:1;				// RunTime
	};
	struct __attribute__((packed)) {// 9:23 Memory reporting
		u32_t	rmCAPS:23;
		u32_t	rmDetail:1;			// individual block details
		u32_t	rmColor:1;
		u32_t	rmCompact:1;
		u32_t	rwF:1;
		u32_t	rmE:1;
		u32_t	rmHdr2:1;
		u32_t 	rmHdr1:1;
		u32_t	rmNL:1;
		u32_t	rmRT:1;
	};
	struct __attribute__((packed)) {// 10:22 Sensors reporting
		u32_t	senFree:22;
		u32_t	senTlog:1;
		u32_t	senTsen:1;
		u32_t	senColor:1;
		u32_t 	senDev:1;
		u32_t 	senAvg:1;
		u32_t 	senSum:1;
		u32_t 	senMMP:1;
		u32_t	senUnit:1;
		u32_t	senNL:1;
		u32_t	senURI:1;
	};
	u32_t	u32Val;
} fm_t;
DUMB_STATIC_ASSERT(sizeof(fm_t) == sizeof(u32_t));

typedef struct report_t {
	char * pcBuf;
	union {
		u32_t Size;
		struct __attribute__((packed)) {
			u32_t size : (32 - xpfBITS_REPORT);
			union __attribute__((packed)) {
				u32_t flags : xpfBITS_REPORT;
				struct __attribute__((packed)) {
					u32_t sgr : 2;
					u32_t spare : 3;
					u32_t fEcho : 1;		// enable command character(s) echo
					u32_t fFlags : 1;		// Force checking of flag changes
					u32_t fForce : 1;		// Force display of flags
				};
			};
		};
	};
	fm_t sFM;
} report_t;
DUMB_STATIC_ASSERT(sizeof(report_t) == (sizeof(char *) + 8));

typedef	struct xpf_t {
	union {
		u32_t lengths;									// maxlen & curlen;
		struct __attribute__((packed)) {
			u32_t	maxlen		: xpfMAXLEN_BITS;	// max chars to output 0 = unlimited
			u32_t	curlen		: xpfMAXLEN_BITS;	// number of chars output so far
		};
	};
	union {
		u32_t limits;
		struct __attribute__((packed)) {
			u32_t	minwid		: xpfMINWID_BITS;	// min field width
			u32_t	precis		: xpfPRECIS_BITS;	// float precision or max string length
		};
	};
	union {
		u32_t flags;									// rest of flags
		struct __attribute__((packed)) {
			u32_t	flg1 : (32-xpfBITS_REPORT);			// flags to be reset
			u32_t	flg2 : xpfBITS_REPORT;				// flags to be retained
		};
		struct __attribute__((packed)) {
/*byte 0*/	u8_t	group 		: 1;				// 0 = disable, 1 = enable
			u8_t	alt_form	: 1;				// '#'
			u8_t	ljust		: 1;				// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
			u8_t	Ucase		: 1;				// true = 'a' or false = 'A'
			u8_t	pad0		: 1;				// true = pad with leading'0'
			u8_t	radix		: 1;
			u8_t	signval		: 1;				// true = content is signed value
			u8_t	rel_val		: 1;				// relative address / elapsed time
/*byte 1*/	u32_t	nbase 		: 5;				// 2, 8, 10 or 16
			u8_t	form		: 2;				// format specifier FLOAT, DUMP & TIME
			u8_t	negvalue	: 1;				// if value < 0
/*byte 2*/	u8_t	llong		: 4;				// va_arg override
			u8_t	arg_width	: 1;				// minwid specified
			u8_t	arg_prec	: 1;				// precis specified
			u8_t	plus		: 1;				// true = force use of '+' or '-' signed
			u8_t	Pspc		: 1;
/*byte 3*/	u8_t	sgr			: 2;				// check to align with report_t size struct
			u8_t	spare		: 6;				// SPARE !!!
		};
	};
} xpf_t;
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
} xpc_t;
DUMB_STATIC_ASSERT(sizeof(xpc_t) == (sizeof(int *) + sizeof(void *) + sizeof(xpf_t) + sizeof(va_list)));

// ################################### Public functions ############################################

int xPrintFX(xpc_t * psXPC, const char * format);
int xPrintF(int (handler)(xpc_t *, int), void *, size_t, const char *, va_list);

/* Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string */

// ##################################### Destination = STDOUT ######################################

extern SemaphoreHandle_t printfxMux;
void printfx_lock(report_t * psR);
void printfx_unlock(report_t * psR);

int vnprintfx_nolock(size_t count, const char * format, va_list);
int vprintfx_nolock(const char * format, va_list);
int printfx_nolock(const char * format, ...);

int vnprintfx(size_t, const char *, va_list) _ATTRIBUTE ((__format__ (__printf__, 2, 0)));
int vprintfx(const char *, va_list)	_ATTRIBUTE ((__format__ (__printf__, 1, 0)));
int nprintfx(size_t, const char *, ...) _ATTRIBUTE ((__format__ (__printf__, 2, 3)));
//int printfx(const char *, ...) _ATTRIBUTE ((__format__ (__printf__, 1, 2)));
int printfx(const char *, ...);

// ##################################### Destination = STRING ######################################

int vsnprintfx(char *, size_t, const char *, va_list) _ATTRIBUTE ((__format__ (__printf__, 3, 0)));
int vsprintfx(char *, const char *, va_list) _ATTRIBUTE ((__format__ (__printf__, 2, 0)));
//int snprintfx(char *, size_t, const char *, ...) _ATTRIBUTE ((__format__ (__printf__, 3, 4)));
int snprintfx(char *, size_t, const char *, ...);
int sprintfx(char *, const char *, ...) _ATTRIBUTE ((__format__ (__printf__, 2, 3)));

// ############################## Destination = STDOUT -or- STRING #################################

int	wvprintfx(report_t * psRprt, const char * pcFormat, va_list vaList);
int wprintfx(report_t * psRprt, const char * pcFormat, ...);

// ############################## LOW LEVEL DIRECT formatted output ################################

int vcprintfx(const char *, va_list);
int cprintfx(const char *, ...);

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
