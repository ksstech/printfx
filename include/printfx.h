// printfx.h

#pragma once

#if __has_include("hal_platform.h")
	#include "hal_platform.h"
#else
	#ifdef __cplusplus
		extern "C" {
			#define debugFLAG_GLOBAL	0xFFFFFF
		}
	#endif
#endif

#if __has_include("common-vars.h")
	#include "common-vars.h"
#endif

#if __has_include("x_ubuf.h")
	#include "x_ubuf.h"
#endif

#if __has_include("hal_timer.h")
	#include "hal_timer.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// common-vars module not available, need these definitions

#ifndef u8_t
	typedef unsigned char u8_t;
#endif

#ifndef u32_t
	typedef uint32_t u32_t;
#endif

/* ############################### CHOOSING AN OUTPUT ROUTE #######################################
 *
 * FOUR routes, and they are NOT interchangeable. Picking by habit is how debug relics end up on
 * paths that cannot tolerate them. Measured properties, see analysis/uart-console-io-flow.md S81:
 *
 *  route            mechanism                    blocks?              on pressure / notes
 *  ---------------  ---------------------------  -------------------  --------------------------
 *  CP*  cprintfx    uart_tx_chars()              NEVER                DROPS chars silently.
 *                                                                     UART only - not telnet, not
 *                                                                     the crash-survivable buffer.
 *  PX*  printfx     staged -> RTC ubuf or UART   up to ~2 s           evicts old buffer content.
 *                                                (stage + uart locks,  telnet-visible. THE DEFAULT.
 *                                                 both WPFX_TIMEOUT)
 *  RP* / IRP*       esp_rom_printf               ROM, direct          IRP* only in ISR context.
 *  SL_* vSyslog     console AND/OR host          UNBOUNDED            shSLvars is portMAX_DELAY,
 *                                                                     then a TCP send, then an LFS
 *                                                                     write if the host is down.
 *
 * RULES
 *  1. Default to PX*. It is buffered, locked, telnet-visible and survives a reboot in the RTC buffer.
 *  2. Use SL_* for messages that belong in the operational log - it adds severity, dedup and the
 *     host route. NEVER on a path where an unbounded stall matters: it can block on the network.
 *  3. Use CP* where blocking is HARMFUL and losing characters is acceptable - actuator edge timing,
 *     network receive/request handlers, anything holding a lock others need. Current CP* users are
 *     x_http_server (request handlers), socketsX xNetReConnect (receive path, and it keeps syslog
 *     out of a syslog->socket->syslog cycle) and pca9555Flush (actuator output path). These are
 *     DELIBERATE - do not "tidy" them to PX*.
 *  4. Use IRP* / IF_IRP in ISR or cache-disabled context, never plain RP*. See the IRP block below.
 *  5. Use plain RP* when INSTRUMENTING THE OUTPUT PATH ITSELF - anything called from inside
 *     xPrintFX, xvReport or the ubuf/stdio layers. PX* there would re-enter the code under test
 *     (PX -> printfx -> xPrintFX -> ...), so RP* is not a preference, it is the only route that
 *     works. Existing users: vReportDebug (report.c), xpDebugMem / xpDebugFlags / xpDebugSpin
 *     (printfx_v0.c). Development helpers with no permanent call sites - that is normal for this
 *     category, do NOT read "unused" as "delete".
 *
 * Three axes decide a site: CONTEXT (can it run cache-disabled?), GATING (compile-time dead, guard
 * dead, or reachable?) and LATENCY (would a multi-second stall break something?).
 */

// #################################################################################################

unsigned long long halTIMER_ReadRunTime(void);
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

/* ISR / cache-disabled context ONLY. Use these, not RP*, anywhere the code can run while the flash
 * cache is disabled - an ESP_INTR_FLAG_IRAM handler, a FreeRTOS tick hook, or anything reachable
 * during an NVS/littlefs/FOTA write.
 *
 * RP() alone is NOT sufficient there. esp_rom_printf is ROM-resident so the CODE is safe, but a
 * plain string literal lives in .flash.rodata and faults exactly the same way. IRP() bakes in
 * DRAM_STR so the format cannot be got wrong.
 *
 * There is deliberately no IRPL/IRPT/IRPTL. Those prefixes cannot be made safe:
 *   _RL_  passes __FUNCTION__, a compiler-generated FLASH string, to a %s
 *   _RT_  calls halTIMER_ReadRunSeconds()/Millis(), which live in FLASH (0x400dac90)
 * Providing them would rebuild the trap this macro exists to close. If an ISR trace needs a
 * timestamp or location, put a literal marker in the format string instead.
 *
 * Note the cost: every IRP format string moves from flash into DRAM, which is the scarcer
 * resource. Use it where correctness demands it, not by default. */
#define	IRP(f, ...)					esp_rom_printf(DRAM_STR(f), ##__VA_ARGS__)
#define	IF_IRP(T, f, ...)			if (T) IRP(f, ##__VA_ARGS__)


// ################################## public build definitions #####################################

#define printfxVER0

/* Build the printfx self-test / benchmark suite (vPrintfUnitTest, vPrintfStressTest,
 * vPrintfEdgeTest, vPrintfSpeedTest) and the 'E'/'G'/'Q' console commands that launch them.
 *
 * DEFAULT OFF. Enable ONLY while actively changing printfx/report/x_ubuf and needing to
 * re-baseline. It was previously gated on (appPRODUCTION == 0), which is every build this product
 * has ever shipped, so the suite was permanently in the field image for no benefit. */
#define	printfxTESTS				0

#define	xpfMAXIMUM_DECIMALS			15
#define	xpfDEFAULT_DECIMALS			6

#define	xpfMAX_TIME_FRAC			6		// control resolution mS/uS/nS
#define	xpfDEF_TIME_FRAC			3

#define xpfMAXWIDTH_HEXDUMP			32

// ################################## C11 Pointer size determination ###############################

static_assert(sizeof (void*) == sizeof (uintptr_t), "TBD code needed to determine pointer size");

// C99 or later
#if (UINTPTR_MAX == 0xFFFF)
	#define xpfSIZE_POINTER			2
#elif (UINTPTR_MAX == 0xFFFFFFFF)
	#define xpfSIZE_POINTER			4
#elif (UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu)
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
#define	xpfMAXLEN_MAXVAL			((unsigned long) ((1 << xpfMAXLEN_BITS) - 1))

#define	xpfMINWID_BITS				16			// Number of bits in field(s)
#define	xpfMINWID_MAXVAL			((unsigned long) ((1 << xpfMINWID_BITS) - 1))

#define	xpfPRECIS_BITS				16			// Number of bits in field(s)
#define	xpfPRECIS_MAXVAL			((unsigned long) ((1 << xpfPRECIS_BITS) - 1))

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

//typedef union sgr_info_t {
typedef union {
	struct __attribute__((packed)) { unsigned char a2, a1, c, r; };
	struct __attribute__((packed)) { unsigned short attrib, rowcol; };
	unsigned long u32;
} sgr_info_t;
static_assert(sizeof(sgr_info_t) == 4, "Invalid structure size");

typedef struct __attribute__((packed)) xpc_flg_t {
	unsigned short MinWid : xpfMINWID_BITS;		// min field width
	unsigned short Precis : xpfPRECIS_BITS;		// float precision or max string length
/*b4*/											// start flg1
	unsigned char bAltF : 1;					// # alternative form
	unsigned char bLeft : 1;					// if "%-[0][1-9]{diouxX}" then justify LEFT ie pad on right
	unsigned char bCase : 1;					// true = 'a' or false = 'A'
	unsigned char bPlus : 1;					// true = force use of '+' or '-' signed
	unsigned char bSigned : 1;					// true = content is signed value
	unsigned char bNegVal : 1;					// if value < 0
	unsigned char bRelVal : 1;					// relative address / elapsed time
	unsigned char bRadix : 1;					// '.' specified
/*b5*/
	unsigned long uBase : 5;							// 2, 8, 10 or 16
	unsigned char uForm : 2;					// format specifier FLOAT, MAC & HEXDUMP
	unsigned char bGroup : 1;					// ' SI group digits or select separator
/*b6*/
	unsigned char uSize : 4;					// size override
	unsigned char bMinWid : 1;					// MinWid specified
	unsigned char bPrecis : 1;					// Precis specified
	unsigned char bPad0 : 1;					// 0 = ' ' 1 = '0'
	unsigned char bArray : 1;					// array pointer as parameter
/*b7*/
	unsigned char bFloat : 1;					// array printing FLOAT values
	unsigned char bGT:1;						// convert to LC
	unsigned char bLT:1;						// convert to UC 
	unsigned char uSpare : 2;
	// start flg2, sum of bit widths below = XPC_BITS_XFER
	unsigned char bDebug : 1;					// debug flag from xvReport
	unsigned char uSGR : 2;						// check to align with report_t size struct
} xpc_flg_t;

typedef struct __attribute__((packed)) xpc_val_t {
	unsigned long limits;						// Combined MinWid & Precis
	unsigned long flg1 : (32-XPC_BITS_XFER);	// flags to be reset
	unsigned long flg2 : XPC_BITS_XFER;			// flags to be retained
} xpc_val_t;

typedef	union __attribute__((packed)) xpc_t {
	xpc_flg_t flg;
	xpc_val_t val;
	unsigned long long u64XPC;					// used by XPC_SAVE & XPC_REST
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
	unsigned long MaxLen : xpfMAXLEN_BITS;					// max chars to output 0 = unlimited
	unsigned long CurLen : xpfMAXLEN_BITS;					// number of chars output so far
	union __attribute__((packed)) {
		xpc_flg_t flg;
		xpc_val_t val;
		unsigned long long u64XPC;								// used by XPC_SAVE & XPC_REST
	};
	va_list vaList;
} xp_t;
static_assert(sizeof(xp_t) == (2 * sizeof(void *)) + sizeof(unsigned long) + sizeof(unsigned long long) + sizeof(va_list), "Invalid structure size");

// ################################### Public variables ############################################

// ################################### Public functions ############################################

#if defined(printfxVER0)
	int xPrintFX(int (Hdlr)(xp_t *, int), void * pvPara, size_t Size, const char * pcFmt, va_list vaList);
#elif defined(printfxVER1)
	int xPrintFX(int (Hdlr)(xp_t *, const char *, size_t), void * pvPara, size_t Size, const char * pcFmt, va_list vaList);
#endif


/* Public function prototypes for extended functionality version of stdio supplied functions
 * These names MUST be used if any of the extended functionality is used in a format string */

// ######################################## Unit / stress tests ####################################

#if (printfxTESTS > 0)

void vPrintfUnitTest(void);

/**
 * @brief	launch the dual core printfx concurrency stress test, returns immediately
 * @param	Seconds duration, 0 selects the default. Run with the console ACTIVE
 */
void vPrintfStressTest(u32_t Seconds);

/**
 * @brief	value conversion boundary tests - asserted cases plus a reference dump
 * @note	BASELINE THIS before changing xPrintValueJustified, output must stay byte identical
 */
void vPrintfEdgeTest(void);

/**
 * @brief	conversion and console path speed benchmark
 * @param	Loops iterations per case, 0 selects the default
 */
void vPrintfSpeedTest(u32_t Loops);

#endif	// printfxTESTS

 // #################################### Destination handlers #######################################

#if defined(printfxVER0)
	int xPrintToString(xp_t *, int);
	int xPrintToHandle(xp_t *, int);
#elif defined(printfxVER1)
	int xPrintToString(xp_t *, const char *, size_t);
	int xPrintToHandle(xp_t *, const char *, size_t);
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

#if defined(ESP_PLATFORM)								// only available on ESP32
	int vcprintfx(const char *, va_list);
	int cprintfx(const char *, ...);
#endif

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

#if __has_include("socketsX.h")
	struct netx_t;
	int vsocprintfx(struct netx_t *, const char *, va_list);
	int socprintfx(struct netx_t *, const char *, ...);
#endif

// #################################### Destination : UBUF #########################################

#if __has_include("x_ubuf.h")
	int vuprintfx(struct ubuf_t *, const char *, va_list);
	int uprintfx(struct ubuf_t *, const char *, ...);
#endif

// #################################### Destination : CRC32 ########################################

int vcrcprintfx(unsigned long *, const char *, va_list);
int crcprintfx(unsigned long *, const char *, ...);

#ifdef __cplusplus
}
#endif
