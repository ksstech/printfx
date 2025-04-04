// report.h

#pragma once

#include "printfx.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################### Macros ##############################################

#define	makeMASK08_3x8(A,B,C,D,E,F,G,H,I,J,K)		\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|(I&0xFF)<<16|(J&0xFF)<<8|(K&0xFF)))
#define	makeMASK08x24(A,B,C,D,E,F,G,H,I)			\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|(I&0x00FFFFFF)))
#define	makeMASK09x23(A,B,C,D,E,F,G,H,I,J)			\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|I<<23|(J&0x007FFFFF)))
#define	makeMASK10x22(A,B,C,D,E,F,G,H,I,J,K) 		\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|I<<23|J<<22|(K&0x003FFFFF)))
#define	makeMASK11x21(A,B,C,D,E,F,G,H,I,J,K,L)		\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|I<<23|J<<22|K<<21|(L&0x001FFFFF)))
#define	makeMASK12x20(A,B,C,D,E,F,G,H,I,J,K,L,M)	\
	((u32_t) (A<<31|B<<30|C<<29|D<<28|E<<27|F<<26|G<<25|H<<24|I<<23|J<<22|K<<21|L<<20|(M&0x000FFFFF)))

#define repBIT_SGR				30						// width = 2
#define repBIT_DEBUG			29						// width = 1
#define repBIT_FORCE			28
#define repBIT_FLAGS			27
#define repBIT_ECHO				26
#define repBIT_NOLOCK			25

#define repSIZE_SET(s0,s1,echo,nolock,sgr,debug,size) (u32_t) ( \
	((sgr & 3) << repBIT_SGR) 		|	\
	((debug & 1) << repBIT_DEBUG)	|	\
	((s0 & 1) << repBIT_FORCE)		|	\
	((s1 & 1) << repBIT_FLAGS)		|	\
	((echo & 1) << repBIT_ECHO)		|	\
	((nolock & 1) << repBIT_NOLOCK) |	\
	(size & 0xFFFF))

#define repSIZE_SETX(s0,s1,echo,nolock,sgr,debug,Size)	(u32_t) { 	\
	.s0=s0, .s1=s1, .fEcho=echo, .fNoLock=nolock, .uSGR=sgr,		\
	.dDebug=debug, .size=Size, .col1 = 0, .col2 = 0, .spare = 0, }

#define fmSAVE()					fm_t sFM = { .u32Val = psR->sFM.u32Val };
#define fmREST()					psR->sFM.u32Val = sFM.u32Val;
#define fmBACK(Mem)					psR->sFM.Mem = sFM.Mem;

#define fmSET(Mem,Val)				{ if (psR) psR->sFM.Mem = Val; }
#define fmTST(Mem)					(psR && psR->sFM.Mem)

#define REP_LVGL(a,b,c,d,e,f,g,h,i) (fm_t){ .lvPart=i, .lv07=h, .lv06=g, .lv05=f, .lvStyle=e, .lvScroll=d, .lvClick=c, .lvContent=b, .lvNL=a }

// ####################################### enumerations ############################################

// #################################### Public structures ##########################################

/**
 * structures used with wprintfx() and wvprintfx() functions used to facilitate 
 * the transparent transfer of selected flags from the report_t structure into 
 * the flags member of the xpc_t structure.
*/
typedef	union  __attribute__((packed)) fm_t {
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
	struct __attribute__((packed)) {// 8:3x8 JSON printing
		u8_t jsCount;
		u8_t jsDepth;
		u8_t jsIndent;
		u8_t js7:1, js6:1, js5:1, js4:1, js3:1, js2:1, jsNL:1, js0:1;
	};
	struct __attribute__((packed)) {// 8:24 General printing
		u32_t	aSpare8:24;
		u32_t	aSpare7:1;
		u32_t	aSpare6:1;
		u32_t	aLev4:1;
		u32_t	aLev3:1;
		u32_t	aLev2:1;
		u32_t	aLev1:1;
		u32_t	aRT:1;
		u32_t	aNL:1;
	};
	struct __attribute__((packed)) {// 8:24 (fs) filesystem
		u32_t	fsCount:24;			// Counter
		u32_t	fsS1:1;
		u32_t	fss0:1;
		u32_t	fsLev4:1;			// File content
		u32_t	fsLev3:1;			// File extended info
		u32_t	fsLev2:1;			// Directory & file info
		u32_t	fsLev1:1;			// system info
		u32_t	fsRT:1;
		u32_t	fsNL:1;
	};
	struct __attribute__((packed)) {// 9:23 Printing tasks
		u32_t	uCount:23;			// Task # mask
		u32_t	bTskNum:1;			// Task #
		u32_t	bSpare:1;
		u32_t	bPrioX:1;			// Priorities
		u32_t	bState:1;			// Task state RBPS
		u32_t	bStack:1;			// Low Stack value
		u32_t	bCore:1;			// MCU 01X
		u32_t	bXtras:1;
		u32_t	bRT:1;				// RunTime
		u32_t	bNL:1;				// NewLine
	};
	struct __attribute__((packed)) {// 9:23 Sensors reporting
		u32_t	senFree:23;
		u32_t	senTlog:1;
		u32_t	senTsen:1;
		u32_t 	senDev:1;
		u32_t 	senAvg:1;
		u32_t 	senSum:1;
		u32_t 	senMMP:1;
		u32_t	senUnit:1;
		u32_t	senURI:1;
		u32_t	senNL:1;			// 0= no CRLF, 1= single CRLF
	};
	struct __attribute__((packed)) {// 12:20 Memory reporting
		u32_t	rmCAPS:20;
		u32_t	rmSect:1;			// Summary Section
		u32_t	rmDetail:1;			// individual block details
		u32_t	rmSmall:1;			// Select compressed heading & data formats
		u32_t	rmMinFre:1;			// Field #6 far right
		u32_t	rmUsed:1;			// Field #5
		u32_t	rmLFBlock:1;		// Field #4
		u32_t	rmFreBlk:1;			// Field #3
		u32_t	rmUseBlk:1;			// Field #2
		u32_t	rmTotBlk:1;			// Field #1 far left
		u32_t	rmHdr2:1;			// additional header for CAPS
		u32_t 	rmHdr1:1;			// 1 header for whole report
		u32_t	rmNL:1;
	};
	struct __attribute__((packed)) {// 08:24 LVGL reporting
		u32_t	lvPart : 24;		// LV_PART??? number
		u32_t	lv07 : 1;
		u32_t	lv06 : 1;
		u32_t	lv05 : 1;
		u32_t	lvStyle : 1;
		u32_t	lvScroll : 1;
		u32_t	lvClick : 1;
		u32_t	lvContent : 1;
		u32_t	lvNL : 1;
	};
	u32_t u32Val;
} fm_t;
DUMB_STATIC_ASSERT(sizeof(fm_t) == sizeof(u32_t));

typedef struct __attribute__((packed)) report_t {
	void * pvAlloc;
	int (* putc)(struct xp_t *, int);					/* alternative character output handler */
	union {
		void * pvArg;
		char * pcBuf;
	};
	union __attribute__((packed)) {						/* Size as value and/or structure */
		u32_t Size;
		struct __attribute__((packed)) {
			u32_t size : xpfMAXLEN_BITS;
			/* flags NOT passed onto xPrintF() only used in in higher level formatting */
			u8_t col1 : 4;
			u8_t col2 : 4;
			u8_t sNoLock : 1;		/* saved fNoLock */
			u8_t fNoLock : 1;		/* Do not lock/unlock */
			u8_t fEcho : 1;			// enable command character(s) echo
			u8_t bHdlr : 1;			// indicate handler specified
			u8_t bStrOut : 1;		// String buffer output
			// flags passed on to xPrintF()
			u8_t bDebug : 1;		// Enable debug output where placed
			u8_t uSGR : 2;
		};
	};
	fm_t sFM;
} report_t;
DUMB_STATIC_ASSERT(sizeof(report_t) == ((3 * sizeof(void *)) + 8));

// ################################### Public functions ############################################

BaseType_t xPrintFxSaveLock(report_t * psR);
BaseType_t xPrintFxRestoreUnLock(report_t * psR);

int	vreport(report_t * psRprt, const char * pcFormat, va_list vaList);
int report(report_t * psRprt, const char * pcFormat, ...);

#ifdef __cplusplus
}
#endif
