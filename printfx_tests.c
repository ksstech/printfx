/*
 * printfx_tests.c -  set of routines to test printfx functionality
 * Copyright (c) 2021-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#include <float.h>									// DBL_MIN/MAX

#include "hal_variables.h"
#include "printfx.h"

#define	debugFLAG					0xE000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ##################################### functional tests ##########################################

#define		TEST_INTEGER	1
#define		TEST_STRING		1
#define		TEST_FLOAT		1
#define		TEST_BINARY		1
#define		TEST_ADDRESS	1
#define		TEST_DATETIME	1
#define		TEST_HEXDUMP	1
#define		TEST_WIDTH_PREC	1

uint64_t	my_llong = 0x98765432 ;
void vPrintfUnitTest(void) {
	#if	(TEST_INTEGER == 1)
	// Minimums and maximums
	printfx("\nintegers\n") ;
	printfx("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	printfx("Min/max i8 : %d %d\n", INT8_MIN, INT8_MAX) ;
	printfx("Min/max i16 : %d %d\n", INT16_MIN, INT16_MAX) ;
	printfx("Min/max i32 : %d %d\n", INT32_MIN, INT32_MAX) ;
	printfx("Min/max i64 : %lld %lld\n", INT64_MIN, INT64_MAX) ;
	printfx("Min/max u64 : %llu %llu\n", UINT64_MIN, UINT64_MAX) ;

	printfx("0x%llx , %'lld un/signed long long\n", 9876543210ULL, -9876543210LL) ;
	printfx("0x%llX , %'lld un/signed long long\n", 0x0000000076543210ULL, 0x0000000076543210ULL) ;
	printfx("%'lld , %'llX , %07lld dec-hex-dec(=0 but 7 wide) long long\n", my_llong, my_llong, 0ULL) ;
	printfx("long long: %lld, %llu, 0x%llX, 0x%llx\n", -831326121984LL, 831326121984LLU, 831326121984LLU, 831326121984LLU) ;

	// left & right padding
	printfx(" long padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\n", 3, 3, 3) ;
	printfx(" long padding (neg): zero=[%04d], left=[%-+4d], right=[%+4d]\n", -3, -3, -3) ;

	printfx("multiple unsigneds: %u %u %2u %X %u\n", 15, 5, 23, 0xb38f, 65535) ;
	printfx("hex %x = ff, hex 02=%02x\n", 0xff, 2) ;		//  hex handling
	printfx("signed %'d = %'u U = 0x%'X\n", -3, -3, -3) ;	//  int formats

	printfx("octal examples 0xFF = %o , 0x7FFF = %o 0x7FFF7FFF7FFF = %16llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
	printfx("octal examples 0xFF = %04o , 0x7FFF = %08o 0x7FFF7FFF7FFF = %016llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
	#endif

	#if	(TEST_STRING == 1)
	char buf[192] ;
	char my_string[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ;
	char *	ptr = &my_string[17] ;
	char *	np = NULL ;
	size_t	slen, count ;
	printfx("\nstrings\n") ;
	printfx("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	printfx("[%d] %s\n", snprintfx(buf, 11, my_string), buf) ;
	printfx("[%d] %.*s\n", 20, 20, my_string) ;
	printfx("ptr=%s, %s is null pointer, char %c='a'\n", ptr, np, 'a');
	printfx("%d %s(s) with %%\n", 0, "message") ;

	// test walking string builder
	slen = 0 ;
	slen += sprintfx(buf+slen, "padding (neg): zero=[%04d], ", -3) ;
	slen += sprintfx(buf+slen, "left=[%-4d], ", -3) ;
	slen += sprintfx(buf+slen, "right=[%4d]\n", -3) ;
	printfx("[%d] %s", slen, buf) ;
	// left & right justification
	slen = sprintfx(buf, "justify: left=\"%-10s\", right=\"%10s\"\n", "left", "right") ;
	printfx("[len=%d] %s", slen, buf);

	count = 80 ;
	snprintfx(buf, count, "Only %d buffered bytes should be displayed from this very long string of at least 90 characters", count) ;
	printfx("%s\n", buf) ;
	// multiple chars
	sprintfx(buf, "multiple chars: %c %c %c %c\n", 'a', 'b', 'c', 'd') ;
	printfx("%s", buf);
	#endif

	#if	(TEST_FLOAT == 1)
	float	my_float	= 1000.0 / 9.0 ;
	double	my_double	= 22000.0 / 7.0 ;

	printfx("\nfloat/double\n") ;
	printfx("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	printfx("DBL MAX=%'.15f MIN=%'.15f\n", DBL_MAX, DBL_MIN) ;
	printfx("DBL MAX=%'.15e MIN=%'.15E\n", DBL_MAX, DBL_MIN) ;
	printfx("DBL MAX=%'.15g MIN=%'.15G\n", DBL_MAX, DBL_MIN) ;

	printfx("float padding (pos): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", my_double, my_double, my_double) ;
	printfx("float padding (neg): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", -my_double, -my_double, -my_double) ;
	printfx("float padding (pos): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double, my_double, my_double) ;
	printfx("float padding (neg): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double*(-1.0), my_double*(-1.0), my_double*(-1.0)) ;

	printfx("%'.20f = float(f)\n", my_float) ;
	printfx("%'.20e = float(e)\n", my_float) ;
	printfx("%'.20e = float(e)\n", my_float/100.0) ;

	printfx("%'.7f = double(f)\n", my_double) ;
	printfx("%'.7e = double(e)\n", my_double) ;
	printfx("%'.7E = double(E)\n", my_double/1000.00) ;

	printfx("%'.12g = double(g)\n", my_double/10000000.0) ;
	printfx("%'.12G = double(G)\n", my_double*10000.0) ;

	printfx("%.20f is a double\n", 22.0/7.0) ;
	printfx("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\n", 3, -3, 3.0, -3.0, 3, 3.0) ;

	printfx("multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;
	printfx("multiple doubles: %e %.1e %2.0e %.2e %.3e %.2e [%-8.3e]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;

	printfx("double special cases: %f %.f %.0f %2f %2.f %2.0f\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;
	printfx("double special cases: %e %.e %.0e %2e %2.e %2.0e\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;

	printfx("rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;
	printfx("rounding doubles: %.1e %.1e %.3e %.2e [%-8.3e]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;

	printfx ("%g  %g  %g  %g  %g  %g  %g  %g\n", 0.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001) ;
	printfx ("%g  %g  %g  %g  %g  %g  %g  %g\n", 1.1, 10.01, 100.001, 1000.0001, 10000.00001, 100000.000001, 1000000.0000001, 10000000.00000001) ;
	double dVal ;
	int32_t Width, Precis ;
	for(Width=0, Precis=0, dVal=1234567.7654321; Width < 8 && Precis < 8; ++Width, ++Precis)
		printfx ("%*.*g  ", Width, Precis, dVal) ;
	printfx("\n") ;
	#endif

	#if	(TEST_ADDRESS == 1)
	char MacAdr[6] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 } ;
	printfx("%I - IP Address (Default)\n", 0x01020304UL) ;
	printfx("%0I - IP Address (PAD0)\n", 0x01020304UL) ;
	printfx("%-I - IP Address (L-Just)\n", 0x01020304UL) ;
	printfx("%#I - IP Address (Rev Default)\n", 0x01020304UL) ;
	printfx("%#0I - IP Address (Rev PAD0)\n", 0x01020304UL) ;
	printfx("%#-I - IP Address (Rev L-Just)\n", 0x01020304UL) ;
	printfx("%m - MAC address (LC)\n", &MacAdr[0]) ;
	printfx("%M - MAC address (UC)\n", &MacAdr[0]) ;
	printfx("%'m - MAC address (LC+sep)\n", &MacAdr[0]) ;
	printfx("%'M - MAC address (UC+sep)\n", &MacAdr[0]) ;
	#endif

	#if	(TEST_BINARY == 1)
	printfx("%J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	printfx("%'J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	printfx("%24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	printfx("%'24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	printfx("%llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	printfx("%'llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	printfx("%40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	printfx("%'40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	printfx("%70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
	printfx("%'70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
	#endif

	#if	(TEST_DATETIME == 1)
	#if	defined(__TIME__) && defined(__DATE__)
		printfx("_DATE_ _TIME_ : %s %s\n", __DATE__, __TIME__) ;
	#endif
	#if	defined(__TIMESTAMP__)
		printfx("_TIMESTAMP_ : %s\n", __TIMESTAMP__) ;
	#endif
	#if	defined(__TIMESTAMP__ISO__)
		printfx("_TIMESTAMP_ISO_ : %s\n", __TIMESTAMP__ISO__) ;
	#endif
	printfx("Normal  (S1): %Z\n", &sTSZ) ;
	printfx("Normal  (S2): %'Z\n", &sTSZ) ;
	printfx("Normal Alt  : %#Z\n", &sTSZ) ;

	printfx("Elapsed S1      : %!R\n", RunTime) ;
	printfx("Elapsed S2      : %!`R\n", RunTime) ;
	printfx("Elapsed S1 x3uS : %!.R\n", RunTime) ;
	printfx("Elapsed S2 x6uS : %!`.6R\n", RunTime) ;

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

	uint8_t DumpData[] = "0123456789abcdef0123456789ABCDEF~!@#$%^&*()_+-={}[]:|;'\\<>?,./`01234" ;
	#if	(TEST_HEXDUMP == 1)
	#define DUMPSIZE	(sizeof(DumpData)-1)
	printfx("DUMP absolute lc byte\n%+B", DUMPSIZE, DumpData) ;
	printfx("DUMP absolute lc byte\n%'+B", DUMPSIZE, DumpData) ;

	printfx("DUMP absolute UC half\n%+H", DUMPSIZE, DumpData) ;
	printfx("DUMP absolute UC half\n%'+H", DUMPSIZE, DumpData) ;

	printfx("DUMP relative lc word\n%!+W", DUMPSIZE, DumpData) ;
	printfx("DUMP relative lc word\n%!'+W", DUMPSIZE, DumpData) ;

	printfx("DUMP relative UC dword\n%!+llW", DUMPSIZE, DumpData) ;
	printfx("DUMP relative UC dword\n%!'+llW", DUMPSIZE, DumpData) ;
	for (int32_t idx = 0; idx < 16; idx++) {
		printfx("\nDUMP relative lc BYTE %!'+B", idx, DumpData) ;
	}
	#endif

	#if	(TEST_WIDTH_PREC == 1)
	printfx("String : Minwid=5  Precis=8  : %*.*s\n",  5,  8, "0123456789ABCDEF") ;
	printfx("String : Minwid=30 Precis=15 : %*.*s\n", 30, 15, "0123456789ABCDEF") ;

	double	F64	= 22000.0 / 7.0 ;
	printfx("Float  : Variables  5.8  : %*.*f\n",  5,  8, F64) ;
	printfx("Float  : Specified  5.8  : %5.8f\n", F64) ;
	printfx("Float  : Variables 30.14 : %*.*f\n",  30,  14, F64) ;
	printfx("Float  : Specified 30.14 : %30.14f\n", F64) ;
	#endif
}
