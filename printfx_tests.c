/*
 * Copyright 2021 Andre M Maree / KSS Technologies (Pty) Ltd.
 *
 * printfx_tests -  set of routines to test printfx functionality
 *
 */

#include	"printfx.h"
#include	"hal_variables.h"

#include	<string.h>
#include	<float.h>									// DBL_MIN/MAX

#define	debugFLAG					0xE001

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

void	vPrintfUnitTest(void) {
#if		(TEST_INTEGER == 1)
uint64_t	my_llong = 0x98765432 ;
// Minimums and maximums
	PRINT("\nintegers\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("Min/max i8 : %d %d\n", INT8_MIN, INT8_MAX) ;
	PRINT("Min/max i16 : %d %d\n", INT16_MIN, INT16_MAX) ;
	PRINT("Min/max i32 : %d %d\n", INT32_MIN, INT32_MAX) ;
	PRINT("Min/max i64 : %lld %lld\n", INT64_MIN, INT64_MAX) ;
	PRINT("Min/max u64 : %llu %llu\n", UINT64_MIN, UINT64_MAX) ;

	PRINT("0x%llx , %'lld un/signed long long\n", 9876543210ULL, -9876543210LL) ;
	PRINT("0x%llX , %'lld un/signed long long\n", 0x0000000076543210ULL, 0x0000000076543210ULL) ;
	PRINT("%'lld , %'llX , %07lld dec-hex-dec(=0 but 7 wide) long long\n", my_llong, my_llong, 0ULL) ;
	PRINT("long long: %lld, %llu, 0x%llX, 0x%llx\n", -831326121984LL, 831326121984LLU, 831326121984LLU, 831326121984LLU) ;

// left & right padding
	PRINT(" long padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\n", 3, 3, 3) ;
	PRINT(" long padding (neg): zero=[%04d], left=[%-+4d], right=[%+4d]\n", -3, -3, -3) ;

	PRINT("multiple unsigneds: %u %u %2u %X %u\n", 15, 5, 23, 0xb38f, 65535) ;
	PRINT("hex %x = ff, hex 02=%02x\n", 0xff, 2) ;		//  hex handling
	PRINT("signed %'d = %'u U = 0x%'X\n", -3, -3, -3) ;	//  int formats

	PRINT("octal examples 0xFF = %o , 0x7FFF = %o 0x7FFF7FFF7FFF = %16llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
	PRINT("octal examples 0xFF = %04o , 0x7FFF = %08o 0x7FFF7FFF7FFF = %016llo\n", 0xff, 0x7FFF, 0x7FFF7FFF7FFFULL) ;
#endif

#if		(TEST_STRING == 1)
	char 	buf[192] ;
	char	my_string[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ;
	char *	ptr = &my_string[17] ;
	char *	np = NULL ;
	size_t	slen, count ;
	PRINT("\nstrings\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("[%d] %s\n", snprintfx(buf, 11, my_string), buf) ;
	PRINT("[%d] %.*s\n", 20, 20, my_string) ;
	PRINT("ptr=%s, %s is null pointer, char %c='a'\n", ptr, np, 'a');
	PRINT("%d %s(s) with %%\n", 0, "message") ;

// test walking string builder
	slen = 0 ;
	slen += sprintfx(buf+slen, "padding (neg): zero=[%04d], ", -3) ;
	slen += sprintfx(buf+slen, "left=[%-4d], ", -3) ;
	slen += sprintfx(buf+slen, "right=[%4d]\n", -3) ;
	PRINT("[%d] %s", slen, buf) ;
// left & right justification
	slen = sprintfx(buf, "justify: left=\"%-10s\", right=\"%10s\"\n", "left", "right") ;
	PRINT("[len=%d] %s", slen, buf);

	count = 80 ;
	snprintfx(buf, count, "Only %d buffered bytes should be displayed from this very long string of at least 90 characters", count) ;
	PRINT("%s\n", buf) ;
// multiple chars
	sprintfx(buf, "multiple chars: %c %c %c %c\n", 'a', 'b', 'c', 'd') ;
	PRINT("%s", buf);
#endif

#if		(TEST_FLOAT == 1)
	float	my_float	= 1000.0 / 9.0 ;
	double	my_double	= 22000.0 / 7.0 ;

	PRINT("\nfloat/double\n") ;
	PRINT("0---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2\n") ;
	PRINT("DBL MAX=%'.15f MIN=%'.15f\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15e MIN=%'.15E\n", DBL_MAX, DBL_MIN) ;
	PRINT("DBL MAX=%'.15g MIN=%'.15G\n", DBL_MAX, DBL_MIN) ;

	PRINT("float padding (pos): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%020.9f], left=[%-20.9f], right=[%20.9f]\n", -my_double, -my_double, -my_double) ;
	PRINT("float padding (pos): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double, my_double, my_double) ;
	PRINT("float padding (neg): zero=[%+020.9f], left=[%-+20.9f], right=[%+20.9f]\n", my_double*(-1.0), my_double*(-1.0), my_double*(-1.0)) ;

	PRINT("%'.20f = float(f)\n", my_float) ;
	PRINT("%'.20e = float(e)\n", my_float) ;
	PRINT("%'.20e = float(e)\n", my_float/100.0) ;

	PRINT("%'.7f = double(f)\n", my_double) ;
	PRINT("%'.7e = double(e)\n", my_double) ;
	PRINT("%'.7E = double(E)\n", my_double/1000.00) ;

	PRINT("%'.12g = double(g)\n", my_double/10000000.0) ;
	PRINT("%'.12G = double(G)\n", my_double*10000.0) ;

	PRINT("%.20f is a double\n", 22.0/7.0) ;
	PRINT("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\n", 3, -3, 3.0, -3.0, 3, 3.0) ;

	PRINT("multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;
	PRINT("multiple doubles: %e %.1e %2.0e %.2e %.3e %.2e [%-8.3e]\n", 3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;

	PRINT("double special cases: %f %.f %.0f %2f %2.f %2.0f\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;
	PRINT("double special cases: %e %.e %.0e %2e %2.e %2.0e\n", 3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;

	PRINT("rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;
	PRINT("rounding doubles: %.1e %.1e %.3e %.2e [%-8.3e]\n", 3.93, 3.96, 3.0988, 3.999, -4.382) ;

	PRINT ("%g  %g  %g  %g  %g  %g  %g  %g\n", 0.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001) ;
	PRINT ("%g  %g  %g  %g  %g  %g  %g  %g\n", 1.1, 10.01, 100.001, 1000.0001, 10000.00001, 100000.000001, 1000000.0000001, 10000000.00000001) ;
	double dVal ;
	int32_t Width, Precis ;
	for(Width=0, Precis=0, dVal=1234567.7654321; Width < 8 && Precis < 8; ++Width, ++Precis)
		PRINT ("%*.*g  ", Width, Precis, dVal) ;
	PRINT("\n") ;
#endif

#if		(TEST_ADDRESS == 1)
	char MacAdr[6] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 } ;
	PRINT("%I - IP Address (Default)\n", 0x01020304UL) ;
	PRINT("%0I - IP Address (PAD0)\n", 0x01020304UL) ;
	PRINT("%-I - IP Address (L-Just)\n", 0x01020304UL) ;
	PRINT("%#I - IP Address (Rev Default)\n", 0x01020304UL) ;
	PRINT("%#0I - IP Address (Rev PAD0)\n", 0x01020304UL) ;
	PRINT("%#-I - IP Address (Rev L-Just)\n", 0x01020304UL) ;
	PRINT("%m - MAC address (LC)\n", &MacAdr[0]) ;
	PRINT("%M - MAC address (UC)\n", &MacAdr[0]) ;
	PRINT("%'m - MAC address (LC+sep)\n", &MacAdr[0]) ;
	PRINT("%'M - MAC address (UC+sep)\n", &MacAdr[0]) ;
#endif

#if		(TEST_BINARY == 1)
	PRINT("%J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%'J - Binary 32/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%'24J - Binary 24/32 bit\n", 0xF77FA55AUL) ;
	PRINT("%llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'llJ - Binary 64/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'40llJ - Binary 40/64 bit\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
	PRINT("%'70llJ - Binary 64/64 bit in 70 width\n", 0xc44c9779F77FA55AULL) ;
#endif

#if		(TEST_DATETIME == 1)
	#if		defined(__TIME__) && defined(__DATE__)
		PRINT("_DATE_ _TIME_ : %s %s\n", __DATE__, __TIME__) ;
	#endif
	#if		defined(__TIMESTAMP__)
		PRINT("_TIMESTAMP_ : %s\n", __TIMESTAMP__) ;
	#endif
	#if		defined(__TIMESTAMP__ISO__)
		PRINT("_TIMESTAMP_ISO_ : %s\n", __TIMESTAMP__ISO__) ;
	#endif
	PRINT("Normal  (S1): %Z\n", &sTSZ) ;
	PRINT("Normal  (S2): %'Z\n", &sTSZ) ;
	PRINT("Normal Alt  : %#Z\n", &sTSZ) ;

	PRINT("Elapsed S1      : %!R\n", RunTime) ;
	PRINT("Elapsed S2      : %!`R\n", RunTime) ;
	PRINT("Elapsed S1 x3uS : %!.R\n", RunTime) ;
	PRINT("Elapsed S2 x6uS : %!`.6R\n", RunTime) ;

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

#if		(TEST_HEXDUMP == 1)
	uint8_t DumpData[] = "0123456789abcdef0123456789ABCDEF~!@#$%^&*()_+-={}[]:|;'\\<>?,./`01234" ;
	#define DUMPSIZE	(sizeof(DumpData)-1)
	PRINT("DUMP absolute lc byte\n%+b", DUMPSIZE, DumpData) ;
	PRINT("DUMP absolute lc byte\n%'+b", DUMPSIZE, DumpData) ;

	PRINT("DUMP absolute UC half\n%+H", DUMPSIZE, DumpData) ;
	PRINT("DUMP absolute UC half\n%'+H", DUMPSIZE, DumpData) ;

	PRINT("DUMP relative lc word\n%!+w", DUMPSIZE, DumpData) ;
	PRINT("DUMP relative lc word\n%!'+w", DUMPSIZE, DumpData) ;

	PRINT("DUMP relative UC dword\n%!+llW", DUMPSIZE, DumpData) ;
	PRINT("DUMP relative UC dword\n%!'+llW", DUMPSIZE, DumpData) ;
	for (int32_t idx = 0; idx < 16; idx++) {
		PRINT("\nDUMP relative lc BYTE %!'+b", idx, DumpData) ;
	}
#endif

#if		(TEST_WIDTH_PREC == 1)
	PRINT("String : Minwid=5  Precis=8  : %*.*s\n",  5,  8, "0123456789ABCDEF") ;
	PRINT("String : Minwid=30 Precis=15 : %*.*s\n", 30, 15, "0123456789ABCDEF") ;

	double	F64	= 22000.0 / 7.0 ;
	PRINT("Float  : Variables  5.8  : %*.*f\n",  5,  8, F64) ;
	PRINT("Float  : Specified  5.8  : %5.8f\n", F64) ;
	PRINT("Float  : Variables 30.14 : %*.*f\n",  30,  14, F64) ;
	PRINT("Float  : Specified 30.14 : %30.14f\n", F64) ;
#endif
}
