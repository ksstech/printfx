// printfx_tests.c - Copyright (c) 2021-26 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include "printfx.h"

#include "report.h"
#include "stdioX.h"
#include "hal_stdio.h"						// vStdOutBufReset()

#include <float.h>									// DBL_MIN/MAX
#include <stdatomic.h>
#include <string.h>									// memset

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ##################################### functional tests ##########################################

#define		TEST_INTEGER	0
#define		TEST_STRING		0
#define		TEST_FLOAT		0
#define		TEST_BINARY		0
#define		TEST_ADDRESS	0
#define		TEST_DATETIME	1
#define		TEST_HEXDUMP	0
#define		TEST_WIDTH_PREC	0

#define TESTP(f,...) printfx(f, ##__VA_ARGS__)

void vPrintfUnitTest(void) {
	#if	(TEST_INTEGER == 1)
	u64_t my_llong = 0x98765432;
	// Minimums and maximums
	TESTP("\nintegers\n");
	TESTP("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n");
	TESTP("Min/max i8 : %d %d\n", INT8_MIN, INT8_MAX);
	TESTP("Min/max u8 : %u %u\n", UINT8_MIN, UINT8_MAX);
	TESTP("Min/max i16 : %d %d\n", INT16_MIN, INT16_MAX);
	TESTP("Min/max u16 : %u %u\n", UINT16_MIN, UINT16_MAX);
	TESTP("Min/max i32 : %ld %ld\n", INT32_MIN, INT32_MAX);
	TESTP("Min/max u32 : %lu %lu\n", UINT32_MIN, UINT32_MAX);
	TESTP("Min/max i64 : %lld %lld\n", INT64_MIN, INT64_MAX);
	TESTP("Min/max u64 : %llu %llu\n", UINT64_MIN, UINT64_MAX);

	TESTP("0x%llx , %'lld un/signed long long\n", 9876543210ULL, -9876543210LL);
	TESTP("0x%llX , %'lld un/signed long long\n", 0x0000000076543210ULL, 0x0000000076543210ULL);
	TESTP("%'lld , %'llX , %07lld dec-hex-dec(=0 but 7 wide) long long\n", my_llong, my_llong, 0ULL);
	TESTP("long long: %lld, %llu, 0x%llX, 0x%llx\n", -831326121984LL, 831326121984LLU, 831326121984LLU, 831326121984LLU);

	// left & right padding
	TESTP(" long padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\n", 3, 3, 3);
	TESTP(" long padding (neg): zero=[%04d], left=[%-+4d], right=[%+4d]\n", -3, -3, -3);

	TESTP("multiple unsigneds: %u %u %2u %X %u\n", 15, 5, 23, 0xb38f, 65535);
	TESTP("hex %x = ff, hex 02=%02x\n", 0xff, 2);		//  hex handling
	TESTP("signed %'d = %'u U = 0x%'X\n", -3, -3, -3);	//  int formats

	TESTP("octal examples 0xFF = %o , 0x7FFF = %o 0x7FFF7FFF7FFF = %16llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL);
	TESTP("octal examples 0xFF = %04o , 0x7FFF = %08o 0x7FFF7FFF7FFF = %016llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL);
	#endif

	#if	(TEST_STRING == 1)
	char buf[192];
	char my_string[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890";
	char *	ptr = &my_string[17];
	char *	np = NULL;
	size_t	slen, count;
	TESTP("\nstrings\n");
	TESTP("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n");
	TESTP("[%d] %s\n", snprintfx(buf, 11, my_string), buf);
	TESTP("[%d] %.*s\n", 20, 20, my_string);
	TESTP("ptr=%s, %s is null pointer, char %c='a'\n", ptr, np, 'a');
	TESTP("%d %s(s) with %%\n", 0, "message");

	// test walking string builder
	slen = 0;
	slen += snprintfx(buf + slen, sizeof(buf) - slen, "padding (neg): zero=[%04d], ", -3);
	slen += snprintfx(buf + slen, sizeof(buf) - slen, "left=[%-4d], ", -3);
	slen += snprintfx(buf + slen, sizeof(buf) - slen, "right=[%4d]\n", -3);
	TESTP("[%d] %s", slen, buf);
	// left & right justification
	slen = snprintfx(buf, sizeof(buf), "justify: left=\"%-10s\", right=\"%10s\"\n", "left", "right");
	TESTP("[len=%d] %s", slen, buf);

	count = 80;
	snprintfx(buf, count, "Only %d buffered bytes should be displayed from this very long string of at least 90 characters", count);
	TESTP("%s\n", buf);
	// multiple chars
	snprintfx(buf, xpfMAXLEN_MAXVAL, "multiple chars: %c %c %c %c\n", 'a', 'b', 'c', 'd');
	TESTP("%s", buf);
	#endif

	#if	(TEST_FLOAT == 1)
	float	my_float	= 1000.0 / 9.0;
	double	my_double	= 22000.0 / 7.0;

	TESTP("\nfloat/double\n");
	TESTP("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n");
	TESTP("DBL MAX=%'.15f MIN=%'.15f\n", DBL_MAX, DBL_MIN);
	TESTP("DBL MAX=%'.15e MIN=%'.15E\n", DBL_MAX, DBL_MIN);
	TESTP("DBL MAX=%'.15g MIN=%'.15G\n", DBL_MAX, DBL_MIN);

	TESTP("float padding (pos): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", my_double, my_double, my_double);
	TESTP("float padding (neg): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", -my_double, -my_double, -my_double);
	TESTP("float padding (pos): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double, my_double, my_double);
	TESTP("float padding (neg): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double*(-1.0), my_double*(-1.0), my_double*(-1.0));

	TESTP("%'.20f = float(f)\n", my_float);
	TESTP("%'.20e = float(e)\n", my_float);
	TESTP("%'.20e = float(e)\n", my_float/100.0);

	TESTP("%'.7f = double(f)\n", my_double);
	TESTP("%'.7e = double(e)\n", my_double);
	TESTP("%'.7E = double(E)\n", my_double/1000.00);

	TESTP("%'.12g = double(g)\n", my_double/10000000.0);
	TESTP("%'.12G = double(G)\n", my_double*10000.0);

	TESTP("%.20f is a double\n", 22.0/7.0);
	TESTP("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\n", 3, -3, 3.0, -3.0, 3, 3.0);

	TESTP("multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382);
	TESTP("multiple doubles: %e %.1e %2.0e %.2e %.3e %.2e [%-8.3e]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382);

	TESTP("double special cases: %f %.f %.0f %2f %2.f %2.0f\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159);
	TESTP("double special cases: %e %.e %.0e %2e %2.e %2.0e\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159);

	TESTP("rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\n", 3.93, 3.96, 3.0988, 3.999, -4.382);
	TESTP("rounding doubles: %.1e %.1e %.3e %.2e [%-8.3e]\n", 3.93, 3.96, 3.0988, 3.999, -4.382);

	TESTP("%g  %g  %g  %g  %g  %g  %g  %g\n", 0.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001);
	TESTP("%g  %g  %g  %g  %g  %g  %g  %g\n", 1.1, 10.01, 100.001, 1000.0001, 10000.00001, 100000.000001, 1000000.0000001, 10000000.00000001);
	double dVal;
	int Width, Precis;
	for(Width=0, Precis=0, dVal=1234567.7654321; Width < 8 && Precis < 8; ++Width, ++Precis)
		TESTP("%*.*g  ", Width, Precis, dVal);
	TESTP("\n");
	#endif

	#if	(TEST_ADDRESS == 1)
	char MacAdr[6] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };
	TESTP("%I - IP Address (Default)\n", 0x01020304UL);
	TESTP("%0I - IP Address (PAD0)\n", 0x01020304UL);
	TESTP("%-I - IP Address (L-Just)\n", 0x01020304UL);
	TESTP("%#I - IP Address (Rev Default)\n", 0x01020304UL);
	TESTP("%#0I - IP Address (Rev PAD0)\n", 0x01020304UL);
	TESTP("%#-I - IP Address (Rev L-Just)\n", 0x01020304UL);
	TESTP("%M - MAC address (LC)\n", &MacAdr[0]);
	TESTP("%M - MAC address (UC)\n", &MacAdr[0]);
	TESTP("%'M - MAC address (LC+sep)\n", &MacAdr[0]);
	TESTP("%'M - MAC address (UC+sep)\n", &MacAdr[0]);
	#endif

	#if	(TEST_BINARY == 1)
	TESTP("%b - Binary 32/32 bit\n", 0xF77FA55AUL);
	TESTP("%'b - Binary 32/32 bit\n", 0xF77FA55AUL);
	TESTP("%24b - Binary 24/32 bit\n", 0xF77FA55AUL);
	TESTP("%'24b - Binary 24/32 bit\n", 0xF77FA55AUL);
	TESTP("%llb - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL);
	TESTP("%'llb - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL);
	TESTP("%40llb - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL);
	TESTP("%'40llb - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL);
	TESTP("%70llb - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL);
	TESTP("%'70llb - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL);
	#endif

	#if	(TEST_DATETIME == 1)
	#if	defined(__TIME__) && defined(__DATE__)
		TESTP("_DATE_ _TIME_   : %s %s\n", __DATE__, __TIME__);
	#endif
	#if	defined(__TIMESTAMP__)
		TESTP("_TIMESTAMP_     : %s\n", __TIMESTAMP__);
	#endif
	#if	defined(__TIMESTAMP__ISO__)
		TESTP("_TIMESTAMP_ISO_ : %s\n", __TIMESTAMP__ISO__);
	#endif
	sTSZ.usecs = (u64_t) BuildSeconds * 1000001ULL;
	TESTP("Normal (S1): %Z\n", &sTSZ);
	TESTP("Normal Alt : %#Z\n", &sTSZ);

	TESTP("Elapsed      : %!R\n", sTSZ.usecs);
	TESTP("Elapsed x3uS : %!.R\n", sTSZ.usecs);
	TESTP("Elapsed x6uS : %!.6R\n", sTSZ.usecs);

	TESTP("Relative +64b: %!.6R\n", 1000000000001LL);
	TESTP("Relative -64b: %!.6R\n", -1000000000001LL);

	TESTP("Relative +32b: %!r\n", 1000001L);
	TESTP("Relative -32b: %!r\n", -1000001L);

	seconds_t Seconds;
	u64_t uSecs;
	struct tm sTM;
	for(u64_t i = 0; i <= 1000; i += 50) {
		TESTP("#%llu", i);
		uSecs = i * 86398999000ULL;
		Seconds = xTimeStampSeconds(uSecs);
		xTimeGMTime(Seconds, &sTM, 1);
		TESTP("  %lu / %i  ->  %!.R", Seconds, sTM.tm_mday, uSecs);

		uSecs = i * 86400000000ULL;
		Seconds = xTimeStampSeconds(uSecs);
		xTimeGMTime(Seconds, &sTM, 1);
		TESTP("  %lu / %i  ->  %!.R", Seconds, sTM.tm_mday, uSecs);

		uSecs = i * 86401001000ULL;
		Seconds = xTimeStampSeconds(uSecs);
		xTimeGMTime(Seconds, &sTM, 1);
		TESTP("  %lu / %i  ->  %!.R", Seconds, sTM.tm_mday, uSecs);
		TESTP("\n");
	}
	#endif

	#if	(TEST_HEXDUMP == 1)
	u8_t DumpData[] = "0123456789abcdef0123456789ABCDEF~!@#$%^&*()_+-={}[]:|;'\\<>?,./`01234";
	#define DUMPSIZE	(sizeof(DumpData)-1)
	TESTP("DUMP absolute lc byte\n%+hhY", DUMPSIZE, DumpData);
	TESTP("DUMP absolute lc byte\n%'+hhY", DUMPSIZE, DumpData);

	TESTP("DUMP absolute UC half\n%+hY", DUMPSIZE, DumpData);
	TESTP("DUMP absolute UC half\n%'+hY", DUMPSIZE, DumpData);

	TESTP("DUMP relative lc word\n%!+lY", DUMPSIZE, DumpData);
	TESTP("DUMP relative lc word\n%!'+lY", DUMPSIZE, DumpData);

	TESTP("DUMP relative UC dword\n%!+llY", DUMPSIZE, DumpData);
	TESTP("DUMP relative UC dword\n%!'+llY", DUMPSIZE, DumpData);
	for (int idx = 0; idx < 16; ++idx) {
		TESTP("\nDUMP relative lc BYTE %!+hhY", idx, DumpData);
	}
	#endif

	#if	(TEST_WIDTH_PREC == 1)
	TESTP("String : Minwid=5  Precis=8  : %*.*s\n",  5,  8, "0123456789ABCDEF");
	TESTP("String : Minwid=30 Precis=15 : %*.*s\n", 30, 15, "0123456789ABCDEF");

	double	F64	= 22000.0 / 7.0;
	TESTP("Float  : Variables  5.8  : %*.*f\n",  5,  8, F64);
	TESTP("Float  : Specified  5.8  : %5.8f\n", F64);
	TESTP("Float  : Variables 30.14 : %*.*f\n",  30,  14, F64);
	TESTP("Float  : Specified 30.14 : %30.14f\n", F64);
	#endif
}

// ########################### Concurrency (dual core) stress test #################################

/* Reproduces the c764 field condition on the bench: two tasks, one pinned to EACH core, emitting
 * ~110 character lines at a combined 20.8 lines/sec - the rate measured on c764 (370 DS2482 resets
 * in 17.8 s). Before the vprintfx() serialisation the two cores shredded each other's output at
 * character granularity because xPrintToHandle() emits ONE character per handler call.
 *
 * Task A uses printfx(), task B uses xReport(). Those were the two families that previously used
 * DIFFERENT locks (printfx none, report its own ReportLock) so this pair is exactly what the fix
 * addresses. The syslog console path is deliberately NOT exercised here: syslog sits ABOVE printfx
 * and is still unserialised by design, and printfx must not depend on it.
 *
 * Each line is self-verifying by shape:
 *
 *   A000042 AAAA...AAAA A000042
 *   ^tag+seq  ^filler    ^tag+seq repeated
 *
 * Output is CORRECT only if, on every line, the head tag/sequence matches the tail tag/sequence and
 * the filler is a single repeated character. Any line mixing A and B, or with mismatched head/tail,
 * was interleaved.
 *
 * Excluded from production images by the same appPRODUCTION guard the rest of the tree uses.
 */
#if (appPRODUCTION == 0)

#define	prtestFILL			90						// filler chars => ~110 char line, as per ds248x
#define	prtestPERIOD_MS		96						// per task; 2 tasks => 48 ms => 20.8 lines/sec
#define	prtestSECONDS		30						// default duration if caller passes 0
#define	prtestSTACK			(configMINIMAL_STACK_SIZE + 3072)
#define	prtestPRIORITY		2

// tag(1) + seq(6) + space + filler + space + tag(1) + seq(6) + newline
#define	prtestCHARS			(1 + 6 + 1 + prtestFILL + 1 + 1 + 6 + (sizeof(strNL) - 1))

#define	prtestFORMAT		"%c%06lu %s %c%06lu" strNL

enum { prtestPRINTFX, prtestREPORT };

typedef struct {
	const char * pcName;
	u32_t Count;									// lines this task will emit
	char cTag;
	u8_t Slot;
	u8_t Family;
	BaseType_t Core;
} prtest_t;

static atomic_uint prtestActive;					// tasks still running
static u32_t prtestLines[2];						// lines emitted, per task
static u32_t prtestShort[2];						// lines where printfx did not return prtestCHARS

/**
 * @brief	emit fixed shape, self identifying lines at a fixed rate until the count is exhausted
 * @param	pvPara pointer to the prtest_t describing this task
 */
static void vPrintfStressTask(void * pvPara) {
	const prtest_t * psP = (const prtest_t *) pvPara;
	char caFill[prtestFILL + 1];
	memset(caFill, psP->cTag, prtestFILL);
	caFill[prtestFILL] = 0;
	TickType_t tWake = xTaskGetTickCount();
	for (u32_t Seq = 0; Seq < psP->Count; ++Seq) {
		int iRV = (psP->Family == prtestPRINTFX)
			? printfx(prtestFORMAT, psP->cTag, Seq, caFill, psP->cTag, Seq)
			: xReport(NULL, prtestFORMAT, psP->cTag, Seq, caFill, psP->cTag, Seq);
		++prtestLines[psP->Slot];
		if (iRV != prtestCHARS)						// short write => characters were DROPPED
			++prtestShort[psP->Slot];
		xTaskDelayUntil(&tWake, pdMS_TO_TICKS(prtestPERIOD_MS));
	}
	if (atomic_fetch_sub(&prtestActive, 1) == 1) {	// last task out reports the result
		PX(strNL "[prtest] done  A=%lu lines (%lu short)  B=%lu lines (%lu short)  expected %u chars/line" strNL,
			prtestLines[0], prtestShort[0], prtestLines[1], prtestShort[1], prtestCHARS);
		PX("[prtest] a line is CORRUPT if head and tail tag/seq differ, or the filler mixes A and B" strNL);
	}
	vTaskDelete(NULL);
}

/**
 * @brief	launch the dual core printfx concurrency stress test, returns immediately
 * @param	Seconds duration, 0 selects prtestSECONDS
 * @note	Run with the console ACTIVE. While inactive all output goes to the 8 KB RTC buffer which
 *			wraps in ~3.5 s at this rate and silently discards the oldest (O_TRUNC), so almost
 *			nothing survives to be inspected.
 */
void vPrintfStressTest(u32_t Seconds) {
	if (atomic_load(&prtestActive)) {
		PX("[prtest] already running" strNL);
		return;
	}
	static prtest_t sTask[2] = {
		{ .pcName = "prtestA", .cTag = 'A', .Slot = 0, .Family = prtestPRINTFX, .Core = 0 },
		{ .pcName = "prtestB", .cTag = 'B', .Slot = 1, .Family = prtestREPORT,  .Core = 1 },
	};
	if (Seconds == 0)
		Seconds = prtestSECONDS;
	u32_t Count = (Seconds * 1000) / prtestPERIOD_MS;
	if (bStdioConsoleGetStatus() == 0)
		PX("[prtest] WARNING console INACTIVE, output goes to the RTC buffer and will wrap" strNL);
	PX("[prtest] %lu sec, %lu lines/task, %u chars/line, %u ms/task => %lu lines/sec combined" strNL,
		Seconds, Count, prtestCHARS, prtestPERIOD_MS, (2UL * 1000UL) / prtestPERIOD_MS);
	prtestLines[0] = prtestLines[1] = prtestShort[0] = prtestShort[1] = 0;
	atomic_store(&prtestActive, 2);
	for (int i = 0; i < 2; ++i) {
		sTask[i].Count = Count;
		xTaskCreatePinnedToCore(vPrintfStressTask, sTask[i].pcName, prtestSTACK, &sTask[i],
								prtestPRIORITY, NULL, sTask[i].Core);
	}
}


// ######################### Conversion correctness / boundary values ##############################

/* Purpose: lock down xPrintValueJustified's output BEFORE optimising it (see
 * analysis/uart-console-io-flow.md §42 - a 32-bit fast path for values <= UINT32_MAX).
 * Any such change MUST be byte-identical in output, so this has to be baselined first.
 *
 * Two kinds of check, deliberately:
 *   ASSERT  - unambiguous C semantics, expected value written out. A FAIL here is a real defect.
 *   DUMP    - proprietary formats (grouping, scaling, binary, relative time) where the reference is
 *             whatever the CURRENT code produces. Capture before, diff after. */

static u32_t prtestPass, prtestFail;

#define prtestASSERT(exp, fmt, ...) do {									\
	char caBuf[128];														\
	snprintfx(caBuf, sizeof(caBuf), fmt, ##__VA_ARGS__);					\
	if (strcmp(caBuf, exp) == 0) {											\
		++prtestPass;														\
	} else {																\
		++prtestFail;														\
		PX("  FAIL  \"%s\" -> '%s'  expected '%s'" strNL, fmt, caBuf, exp);	\
	}																		\
} while (0)

#define prtestDUMP(fmt, ...) do {											\
	char caBuf[128];														\
	int iLen = snprintfx(caBuf, sizeof(caBuf), fmt, ##__VA_ARGS__);			\
	PX("  %-22s [%3d] '%s'" strNL, fmt, iLen, caBuf);						\
} while (0)

void vPrintfEdgeTest(void) {
	prtestPass = prtestFail = 0;
	PX(strNL "[edge] ASSERTED - unambiguous C semantics, a FAIL here is a real defect" strNL);

	// unsigned decimal, spanning the 32/64 bit boundary the fast path will split on
	prtestASSERT("0", "%llu", 0ULL);
	prtestASSERT("1", "%llu", 1ULL);
	prtestASSERT("9", "%llu", 9ULL);
	prtestASSERT("10", "%llu", 10ULL);
	prtestASSERT("4294967295", "%llu", (u64_t) UINT32_MAX);			// last 32-bit value
	prtestASSERT("4294967296", "%llu", (u64_t) UINT32_MAX + 1ULL);	// FIRST value needing 64 bits
	prtestASSERT("18446744073709551615", "%llu", UINT64_MAX);		// widest possible
	prtestASSERT("4294967295", "%lu", UINT32_MAX);
	prtestASSERT("65535", "%u", (unsigned) UINT16_MAX);

	// signed - the *= -1 negation at printfx_v0.c:171/598/1403 is a NO-OP for the MIN values
	prtestASSERT("-1", "%d", -1);
	prtestASSERT("2147483647", "%d", INT32_MAX);
	prtestASSERT("-2147483648", "%d", INT32_MIN);					// negation edge, 32 bit
	prtestASSERT("9223372036854775807", "%lld", INT64_MAX);
	prtestASSERT("-9223372036854775808", "%lld", INT64_MIN);		// negation edge, 64 bit

	// other bases, same boundary values
	prtestASSERT("ffffffff", "%llx", (u64_t) UINT32_MAX);
	prtestASSERT("FFFFFFFF", "%llX", (u64_t) UINT32_MAX);
	prtestASSERT("100000000", "%llx", (u64_t) UINT32_MAX + 1ULL);
	prtestASSERT("ffffffffffffffff", "%llx", UINT64_MAX);
	prtestASSERT("37777777777", "%llo", (u64_t) UINT32_MAX);
	prtestASSERT("0", "%llx", 0ULL);

	// width, precision and padding around the same values
	prtestASSERT("       42", "%9llu", 42ULL);
	prtestASSERT("000000042", "%09llu", 42ULL);
	prtestASSERT("42       ", "%-9llu", 42ULL);
	prtestASSERT("4294967296", "%5llu", (u64_t) UINT32_MAX + 1ULL);	// width < natural length

	PX("[edge] ASSERTED: %lu passed, %lu FAILED" strNL, prtestPass, prtestFail);

	PX(strNL "[edge] REFERENCE DUMP - proprietary formats, diff before/after a change" strNL);
	prtestDUMP("%'llu", 0ULL);
	prtestDUMP("%'llu", 999ULL);
	prtestDUMP("%'llu", 1000ULL);									// first grouping boundary
	prtestDUMP("%'llu", 1000000ULL);
	prtestDUMP("%'llu", (u64_t) UINT32_MAX);
	prtestDUMP("%'llu", (u64_t) UINT32_MAX + 1ULL);
	prtestDUMP("%'llu", UINT64_MAX);
	prtestDUMP("%'lld", -1000000LL);
	prtestDUMP("%#llu", 1234567890ULL);								// SI scaling, the 12-of-14 branch
	prtestDUMP("%#'llu", 1234567890ULL);
	prtestDUMP("%llb", (u64_t) 0xF0ULL);
	prtestDUMP("%'llb", (u64_t) 0xF0ULL);
	prtestDUMP("%+d", 42);
	prtestDUMP("%+d", -42);
	prtestDUMP("%.3f", 3.14159);
	prtestDUMP("%.6f", 0.0);
	prtestDUMP("%e", 1234.5678);
	prtestDUMP("%g", 0.000123456);
}

// ################################## Conversion speed benchmark ###################################

/* Times CONVERSION only - snprintfx into RAM, so no console or buffer I/O is involved. That
 * isolates xPrintValueJustified, which is what §42 changes. A second pass times the buffered
 * console path, which is what §40 (xUBufWrite memcpy) and §38A (block emit) change. */

#define	prtestBENCH_LOOPS	10000

static u32_t prtestBench(const char * pcTag, int Loops, void (*fn)(void)) {
	u64_t tNow = halTIMER_ReadRunTime();
	for (int i = 0; i < Loops; ++i)
		fn();
	u64_t tElap = halTIMER_ReadRunTime() - tNow;
	PX("  %-26s %6llu uS total   %5llu nS/call" strNL, pcTag, tElap, (tElap * 1000ULL) / Loops);
	return (u32_t) tElap;
}

static char prtestBuf[160];
static void bs_d1(void)    { snprintfx(prtestBuf, sizeof(prtestBuf), "%d", 5); }
static void bs_d7(void)    { snprintfx(prtestBuf, sizeof(prtestBuf), "%d", 3390914); }
static void bs_u32max(void){ snprintfx(prtestBuf, sizeof(prtestBuf), "%lu", UINT32_MAX); }
static void bs_u64(void)   { snprintfx(prtestBuf, sizeof(prtestBuf), "%llu", UINT64_MAX); }
static void bs_hex(void)   { snprintfx(prtestBuf, sizeof(prtestBuf), "%X", 0xDEADBEEF); }
static void bs_grp(void)   { snprintfx(prtestBuf, sizeof(prtestBuf), "%'d", 1234567); }
static void bs_str(void)   { snprintfx(prtestBuf, sizeof(prtestBuf), "%s", "ds248xReset"); }
static void bs_flt(void)   { snprintfx(prtestBuf, sizeof(prtestBuf), "%.3f", 3.14159); }
static void bs_line(void)  { snprintfx(prtestBuf, sizeof(prtestBuf),
	"%d %s ds248xReset (%d) Success after %d retries", 0, "i2c_v2", 192, 5); }

void vPrintfSpeedTest(u32_t Loops) {
	if (Loops == 0)
		Loops = prtestBENCH_LOOPS;
	PX(strNL "[speed] conversion only, snprintfx to RAM, %lu loops each" strNL, Loops);
	prtestBench("%d  single digit",     Loops, bs_d1);
	prtestBench("%d  7 digits",         Loops, bs_d7);
	prtestBench("%lu UINT32_MAX",       Loops, bs_u32max);
	prtestBench("%llu UINT64_MAX",      Loops, bs_u64);
	prtestBench("%X  hex 32bit",        Loops, bs_hex);
	prtestBench("%'d grouped",          Loops, bs_grp);
	prtestBench("%s  11 char string",   Loops, bs_str);
	prtestBench("%.3f float",           Loops, bs_flt);
	prtestBench("full log line",        Loops, bs_line);

	/* Console path -> xStdioWrite -> xUBufWrite, ie what §40 (memcpy) and §38A (block emit) change.
	 * uart_active is FORCED to 0 for the duration: with it 1 every character is a write() to the
	 * UART and the result measures 115200 baud, not the software. It also blocks the calling task
	 * for seconds and trips the task watchdog. Restored afterwards. */
	bool bSaved = bStdioConsoleGetStatus();
	u32_t Cloops = Loops / 10;
	vStdioConsoleSetStatus(0);							// force the BUFFERED path
	u64_t tNow = halTIMER_ReadRunTime();
	for (u32_t i = 0; i < Cloops; ++i)
		printfx("%d %s ds248xReset (%d) Success after %d retries" strNL, 0, "i2c_v2", 192, 5);
	u64_t tElap = halTIMER_ReadRunTime() - tNow;
	vStdioConsoleSetStatus(bSaved);						// restore before reporting
	vStdOutBufReset();									// discard what the bench just generated
	PX("[speed] console path via RTC buffer (uart_active forced 0), %lu loops" strNL, Cloops);
	PX("  %-26s %6llu uS total   %5llu nS/call" strNL, "printfx full line", tElap,
		(tElap * 1000ULL) / Cloops);
}

#endif	// appPRODUCTION == 0
