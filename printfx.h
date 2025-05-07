// printfx.h

#pragma once

#include "hal_stdio.h"
#include "hal_timer.h"
#include "x_ubuf.h"

#include "common-vars.h"

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	_L_(f)						"[%s:%d] " f, __FUNCTION__, __LINE__
#define	_T_(f)						"%!.3R " f, halTIMER_ReadRunTime()
#define	_TL_(f)						"%!.3R [%s:%d] " f, halTIMER_ReadRunTime() , __FUNCTION__, __LINE__

#define	CP(f, ...)					cprintfx(f, ##__VA_ARGS__)
#define	CPL(f, ...)					cprintfx(_L_(f), ##__VA_ARGS__)
#define	CPT(f, ...)					cprintfx(_T_(f), ##__VA_ARGS__)
#define	CPTL(f, ...)				cprintfx(_TL_(f), ##__VA_ARGS__)

#define	PX(f, ...)					printfx(f, ##__VA_ARGS__)
#define	PXL(f, ...)					printfx(_L_(f), ##__VA_ARGS__)
#define	PXT(f, ...)					printfx(_T_(f), ##__VA_ARGS__)
#define	PXTL(f, ...)				printfx(_TL_(f), ##__VA_ARGS__)

#define	IF_CP(T, f, ...)			if (T) CP(f, ##__VA_ARGS__)
#define	IF_CPL(T, f, ...)			if (T) CPL(f, ##__VA_ARGS__)
#define	IF_CPT(T, f, ...)			if (T) CPT(f, ##__VA_ARGS__)
#define	IF_CPTL(T, f, ...)			if (T) CPTL(f, ##__VA_ARGS__)

#define	IF_PX(T, f, ...)			if (T) PX(f, ##__VA_ARGS__)
#define	IF_PXL(T, f, ...)			if (T) PXL(f, ##__VA_ARGS__)
#define	IF_PXT(T, f, ...)			if (T) PXT(f, ##__VA_ARGS__)
#define	IF_PXTL(T, f, ...)			if (T) PXTL(f, ##__VA_ARGS__)

// Using ROM based esp_rom_printf (no 64bit support so 32bit timestamps)
#define	_RL_(f)						"[%s:%d] " f, __FUNCTION__, __LINE__
#define	_RT_(f)						"%u.%03u " f, halTIMER_ReadRunSeconds(), halTIMER_ReadRunMillis()
#define	_RTL_(f)					"%u.%03u [%s:%d] " f, halTIMER_ReadRunSeconds(), halTIMER_ReadRunMillis(), __FUNCTION__, __LINE__

#define	RP(f, ...)					esp_rom_printf(f, ##__VA_ARGS__)
#define	RPL(f, ...)					esp_rom_printf(_RL_(f), ##__VA_ARGS__)
#define	RPT(f, ...)					esp_rom_printf(_RT_(f), ##__VA_ARGS__)
#define	RPTL(f, ...)				esp_rom_printf(_RTL_(f), ##__VA_ARGS__)

#define	IF_RP(T, f, ...)			if (T) RP(f, ##__VA_ARGS__)
#define	IF_RPL(T, f, ...)			if (T) RPL(f, ##__VA_ARGS__)
#define	IF_RPT(T, f, ...)			if (T) RPT(f, ##__VA_ARGS__)
#define	IF_RPTL(T, f, ...)			if (T) RPTL(f, ##__VA_ARGS__)


// ################################## public build definitions #####################################

#define printfxVER0

#define	xpfMAXIMUM_DECIMALS			15
#define	xpfDEFAULT_DECIMALS			6

#define	xpfMAX_TIME_FRAC			6		// control resolution mS/uS/nS
#define	xpfDEF_TIME_FRAC			3

#define xpfMAXWIDTH_HEXDUMP			32

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

#define	xpfMAX_LEN_TIME				sizeof("-12:34:56.654321")
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

#define	xpfLEN_SGR_ANSI				"\e[xxx;xxx?\000"
#define	xpfLEN_SGR_LVGL_COL			"#?????? "
#define	xpfMAX_LEN_SGR				(sizeof(xpfLEN_SGR_ANSI) + 1)

#define	xpfNULL_STRING				"'null'"

#define	xpfFLAGS_NONE				((xpc_t) 0ULL)

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
#define _xpfSGR(p,v)				((v & 0xFF) << (p * 8))
#define	xpfLOC(r,c)					(_xpfSGR(3,r) | _xpfSGR(2,c) | _xpfSGR(1,0) | _xpfSGR(0,0))
#define	xpfCOL(a1,a2)				(_xpfSGR(3,0) | _xpfSGR(2,0) | _xpfSGR(1,a1) | _xpfSGR(0,a2))
#define	xpfSGR(r,c,a1,a2)			(_xpfSGR(3,r) | _xpfSGR(2,c) | _xpfSGR(1,a1) | _xpfSGR(0,a2))

// Used to convert LVGL 16bit colour code to 2x 8bit values
#define xpfSGR_LVGL_Ha(val)			(val >> 8)
#define xpfSGR_LVGL_Lb(val)			(val &0xFF)

#define	XPC_BITS_XFER				3					// bDebug:1, sgr:2

#define WPFX_TIMEOUT				pdMS_TO_TICKS(1000)

// ####################################### enumerations ############################################

enum { sgrNONE, sgrANSI, sgrAGFX, sgrLVGL };

// #################################### Public structures ##########################################

typedef union sgr_info_t {
	struct __attribute__((packed)) { u8_t a2, a1, c, r; };
	struct __attribute__((packed)) { u16_t attrib, rowcol; };
	u32_t u32;
} sgr_info_t;
DUMB_STATIC_ASSERT(sizeof(sgr_info_t) == 4);

typedef struct __attribute__((packed)) xpc_flg_t {
	u16_t MinWid : xpfMINWID_BITS;		// min field width
	u16_t Precis : xpfPRECIS_BITS;		// float precision or max string length
/*b4*/							// start flg1
	u8_t bAltF : 1;				// # alternative form
	u8_t bLeft : 1;				// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
	u8_t bCase : 1;				// true = 'a' or false = 'A'
	u8_t bPlus : 1;				// true = force use of '+' or '-' signed
	u8_t bSigned : 1;			// true = content is signed value
	u8_t bNegVal : 1;			// if value < 0
	u8_t bRelVal : 1;			// relative address / elapsed time
	u8_t bRadix : 1;			// '.' specified
/*b5*/
	u32_t uBase : 5;			// 2, 8, 10 or 16
	u8_t uForm : 2;				// format specifier FLOAT, MAC & HEXDUMP
	u8_t bGroup : 1;			// ' SI group digits or select separator
/*b6*/
	u8_t uSize : 4;				// size override
	u8_t bMinWid : 1;			// MinWid specified
	u8_t bPrecis : 1;			// Precis specified
	u8_t bPad0 : 1;				// 0 = ' ' 1 = '0'
	u8_t bArray : 1;			// array pointer as parameter
/*b7*/
	u8_t bFloat : 1;			// array printing FLOAT values
	u8_t bGT:1;					// convert to LC
	u8_t bLT:1;					// convert to UC 
	u8_t uSpare : 2;
	// start flg2, sum of bit widths below = XPC_BITS_XFER
	u8_t bDebug : 1;			// debug flag from xvReport
	u8_t uSGR : 2;				// check to align with report_t size struct
} xpc_flg_t;

typedef struct __attribute__((packed)) xpc_val_t {
	u32_t limits;						// Combined MinWid & Precis
	u32_t flg1 : (32-XPC_BITS_XFER);	// flags to be reset
	u32_t flg2 : XPC_BITS_XFER;			// flags to be retained
} xpc_val_t;

typedef	union __attribute__((packed)) xpc_t {
	xpc_flg_t flg;
	xpc_val_t val;
	u64_t u64XPC;					// used by XPC_SAVE & XPC_REST
} xpc_t;

/* Example of bDebug flag usage
 * IF_PX(psXP->ctl.bDebug, "[%.*s", Len, Buffer);
 * IF_PX(psXP->ctl.bDebug, " %.*s", Len, Buffer);
 * IF_PX(psXP->ctl.bDebug, " %.*s]", Len, Buffer);
 * To set the bDebug flag use reportSIZE(a,b,c,d,e,) 
*/
typedef	struct xp_t {
#if defined(printfxVER0)
	int (*hdlr)(struct xp_t *, int);
#elif defined(printfxVER1)
	int (*hdlr)(struct xp_t *, const char *, size_t);
#endif
	void * pvPara;									// buffer/stream/socket/ubuf/handle/driver/pCRC 
	u32_t MaxLen : xpfMAXLEN_BITS;					// max chars to output 0 = unlimited
	u32_t CurLen : xpfMAXLEN_BITS;					// number of chars output so far
	union __attribute__((packed)) {
		xpc_flg_t flg;
		xpc_val_t val;
		u64_t u64XPC;								// used by XPC_SAVE & XPC_REST
	};
	va_list vaList;
} xp_t;
DUMB_STATIC_ASSERT(sizeof(xp_t) == (2 * sizeof(void *)) + sizeof(u32_t) + sizeof(u64_t) + sizeof(va_list));

// ################################### Public variables ############################################

// ################################### Public functions ############################################

#if defined(printfxVER0)
	int xPrintFX(int (Hdlr)(xp_t *, int), void * pvPara, size_t Size, const char * pcFmt, va_list vaList);
#elif defined(printfxVER1)
	int xPrintFX(int (Hdlr)(xp_t *, const char *, size_t), void * pvPara, size_t Size, const char * pcFmt, va_list vaList);
#endif


/* Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string */

 // #################################### Destination handlers #######################################

#if defined(printfxVER0)
	int xPrintToString(xp_t *, int);
	int xPrintToStdOut(xp_t *, int);
#elif defined(printfxVER1)
	int xPrintToString(xp_t *, const char *, size_t);
	int xPrintToStdOut(xp_t *, const char *, size_t);
#endif

 // ##################################### Destination = STDOUT ######################################

int vprintfx(const char *, va_list)		_ATTRIBUTE ((__format__ (__printf__, 1, 0)));
int printfx(const char *, ...);			//_ATTRIBUTE ((__format__ (__printf__, 1, 2)));

// ##################################### Destination = STRING ######################################

int vsnprintfx(char *, size_t, const char *, va_list)	_ATTRIBUTE ((__format__ (__printf__, 3, 0)));
int vsprintfx(char *, const char *, va_list)			_ATTRIBUTE ((__format__ (__printf__, 2, 0)));
int snprintfx(char *, size_t, const char *, ...);		//_ATTRIBUTE ((__format__ (__printf__, 3, 2)));
int sprintfx(char *, const char *, ...);				//_ATTRIBUTE ((__format__ (__printf__, 3, 2)));

// ############################## LOW LEVEL DIRECT formatted output ################################

int vcprintfx(const char *, va_list);
int cprintfx(const char *, ...);

// ################################### Destination = FILE PTR ######################################

int vfprintfx(FILE *, const char *, va_list );
int fprintfx(FILE *, const char *, ...);

// ################################### Destination = HANDLE ########################################

int vdprintfx(int, const char *, va_list );
int dprintfx(int, const char *, ...);

// ################################### Destination = DEVICE ########################################

int vdevprintfx(int (*)(int ), const char *, va_list);
int devprintfx(int (*)(int), const char *, ...);

// #################################### Destination : SOCKET #######################################

struct netx_t;
int vsocprintfx(struct netx_t *, const char *, va_list);
int socprintfx(struct netx_t *, const char *, ...);

// #################################### Destination : UBUF #########################################

int vuprintfx(struct ubuf_t *, const char *, va_list);
int uprintfx(struct ubuf_t *, const char *, ...);

// #################################### Destination : CRC32 ########################################

int vcrcprintfx(u32_t *, const char *, va_list);
int crcprintfx(u32_t *, const char *, ...);

#ifdef __cplusplus
}
#endif
