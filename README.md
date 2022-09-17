# printfx
	Replacement for variants of printf() - optimized for embedded system

# Key features:
	Does not do ANY dynamic memory allocation.
	All functions are fully re-entrant.
	Minimal stack usage, only the absolute minimum size (based on specific format being processed) is allocated.
	Size can be scaled down by excluding support for various formats, both standard and extensions, at compile time."

# Extensions:
	Numeric output scaling
	#	Alt format scale value in SI steps with SI indicator based on thresholds
	`	If used with # selects 12?34 as opposed to 12.34? scaling format
		10^3=K  10^6=M  10^9=G  10^12=T  10^15=q  10^18=Q

	Text center justify
	#	Place string in center of field

	Date and/or time output in POSIX or other formats.
	D	date using pointer to tsz_t structure
	T	time   "      "     "   "       "
	Z	d+t+z  "      "     "   "       "
	R	d+t  using U64 (uSec) value, support "%.?R" to enable uSec display, 3 digits (mSec) default.
    !	modifier to change interpretation of uSec value as absolute->relative time
    #	modifier to select ALTernative (HTTP header style) format output
    +	modifier to enable TZ information being output

	Hexdump (debug style) in byte, short, word or double word formats
    !	modifier to change absolute -> relative address preceding each line of hexdump output.
    -	modifier to remove the default address preceding each line of output
    `	modifier to enable seperators between values using '|: -' on 32/16/8/4 bit boundaries
    +	modifier to enable addition of ASCII character display at end of each line.
	B	Byte (8 bit) width display
	H	Half word (16 bit) width display
	W	Word (32 bit) width display
    	PLEASE NOTE: Requires 2 parameters being LENGTH and POINTER

	IP address output:
	I	format specifier
    # 	alt format modifier for big/little endian inversion.
    -	modifier for left alignment
    0	modifier for leading zero output
		Leading padding, none, space or zero through field width specifiers

	MAC address output with optional separator characters
    m/M format specifier selectable upper/lower case output.
    # 	alt format modifier for sequence inversion.
    `	modifier to enable ':' seperator characters between bytes

	BINARY format output
	J	format specifier taking U32 or U64 value as input
    `	modifier to enable seperator character using '|: -' for 32/16/8/4 bit boundaries

	ANSI Set Graphics Rendition (SGR) support
	C	format specifier, takes U32 as 4x U8 values

	URL encoding format
	U	format specifier
  	  
# Valid formatting characters:
	!#`*+-%0.0-9AB C D EFGH I J KL MNO PQR S T U VW XYZ
	|||||||||\_/a|b|c|defg h|i|jk|lmn|opq|r|s|t|uv|wxy|z
	||||||||| | ||||||||||||||||||||||||||||||||||||||||
	||||||||| | |||||||||||||||||||||||||||||||||||||||*----> (z) [s]size_t[*]
	||||||||| | ||||||||||||||||||||||||||||||||||||||*---------> (Z) DTZone
	||||||||| | |||||||||||||||||||||||||||||||||||||*------> (Yy) UNUSED
	||||||||| | ||||||||||||||||||||||||||||||||||||*-------> HEX UC/lc value
	||||||||| | |||||||||||||||||||||||||||||||||||*--------> (w) UNUSED
	||||||||| | ||||||||||||||||||||||||||||||||||*-------------> (W) HEXDUMP, word (u32) sized values
	||||||||| | |||||||||||||||||||||||||||||||||*----------> (Vv) UNUSED
	||||||||| | ||||||||||||||||||||||||||||||||*-----------> (u)NSIGNED number (uint32_t)
	||||||||| | |||||||||||||||||||||||||||||||*------------> UNUSED (U)
	||||||||| | ||||||||||||||||||||||||||||||*-------------> (t) ptrdiff[*]
	||||||||| | |||||||||||||||||||||||||||||*------------------> (T)IME uSec based
	||||||||| | ||||||||||||||||||||||||||||*---------------> STRING null terminated ascii
	||||||||| | |||||||||||||||||||||||||||*----------------> (S) Not implemented
	||||||||| | ||||||||||||||||||||||||||*-----------------> (r) UNUSED
	||||||||| | |||||||||||||||||||||||||*----------------------> (R) DateTime U64 uSec based
	||||||||| | ||||||||||||||||||||||||*-------------------> (Qq) UNUSED
	||||||||| | |||||||||||||||||||||||*--------------------> POINTER U32 address with (0x/0X) prefix
	||||||||| | ||||||||||||||||||||||*---------------------> (o)CTAL value
	||||||||| | |||||||||||||||||||||*----------------------> (O) UNUSED
	||||||||| | ||||||||||||||||||||*-----------------------> (Nn) Not implemented
	||||||||| | |||||||||||||||||||*----------------------------> (M)AC address UC/lc
	||||||||| | ||||||||||||||||||*-------------------------> LLONG modifier
	||||||||| | |||||||||||||||||*--------------------------> (L) long double float (F128)
	||||||||| | ||||||||||||||||*---------------------------> (Kk) UNUSED
	||||||||| | |||||||||||||||*----------------------------> (j) [u]intmax_t[*]
	||||||||| | ||||||||||||||*---------------------------------> (J) BINARY U32 string
	||||||||| | |||||||||||||*------------------------------> (i)NTEGER same as 'd'
	||||||||| | ||||||||||||*-----------------------------------> (I)P address
	||||||||| | |||||||||||*--------------------------------> (h) Not implemented !!!
	||||||||| | ||||||||||*-------------------------------------> HEXDUMP, halfword (u16) sized values
	||||||||| | |||||||||*----------------------------------> FLOAT generic format
	||||||||| | ||||||||*-----------------------------------> FLOAT fixed format
	||||||||| | |||||||*------------------------------------> FLOAT exponential format
	||||||||| | ||||||*-------------------------------------> (d)ECIMAL signed formatted long
	||||||||| | |||||*------------------------------------------> (D)ATE formatted
	||||||||| | ||||*---------------------------------------> (c) signed c8
	||||||||| | |||*--------------------------------------------> (C) ANSI SGR Color support
	||||||||| | ||*-----------------------------------------> (b) UNUSED
	||||||||| | |*----------------------------------------------> (B) HEXDUMP, byte (u8) sized values
	||||||||| | *-------------------------------------------> (Aa) Not implemented !!!
	||||||||| *--> field width specifiers
	||||||||*----> FLOAT fractional field size separator
	|||||||*-----> PAD0 enable flag
	||||||*------> format specifier flag
	|||||*-------> LEFT justification, HEXDUMP remove address info
	||||*--------> SIGN leading '+' or '-',	DTZ=Add full TZ info, HEXDUMP add ASCII info
	|||*---------> Minwid or precision variable provided
	||*----------> Xxx=3 digits group, DTZ "::." -> "hms"	MAC/DUMP use '|-+'
	|*-----------> AltForm: Values, DTZ, IP, STRing, DUMP,
	*------------> Abs_Rel: DTZ=relative time, DUMP=relative addr
	
	AltForm usage:
		Values		Scaling down in SI units to fit.
		DateTime		Sun, 10 Sep 2017 20:50:37 GMT
		IP addr		order for network <> host correction
		MAC addr		Use ':' seperator
		STRing		Center justified
		HexDump		Reverse Start <> End addresses

# Examples:
	%`03llJ			- print binary representation, optional separator, llong & field width modifiers
	%[`!#+ll]{BbHhWw}	- hexdump of memory area, USE 2 PARAMETERS FOR START and LENGTH
					  MUST NOT specify "*", ".", "*." or .*", this will screw up the parameter sequence
	%[-0]I			- print IP address, justified left or right, pad 0 or ' '
	%[`]{Mm}			- prints MAC address, optional ':' separator, upper/lower case
	%[!`]D			- POSIX [relative/altform] date (1 parameter, pointer to tsz_t
	%[!`]T			- POSIX [relative/altform] time
	%[!`]Z			- POSIX [relative/altform] date, time & zone

# Todo list:
	Add modifier (and OPTIONAL value specifier) to indicate that pointer to ARRAY of values provided
	If the modifier provided, but no counter, then the pointer to be used to meet the content requirements of the number of specifiers
	If the value specifier provided, then use/increment the pointer, using the SAME FORMAT specifier, for the number of times specified.

# What is NOT supported:
	Specifiers:
	'a' or 'A'	treated as 'g' or 'G' DECIMAL not HEX format
	'n'
	Sub-specifiers:
	'b' conversion specifier
	'h' or 'hh' as applied to any specifiers
	'l' as applied to 'cns' specifiers
	'll' as applied to 'n' specifier
	'L' as applied to 'eEfgG' to specify long double
	'j' 't' or 'z' as applied to any specifiers	 
	Modifier
	' '
	locale, radix fixed to '.'
	*n*th argument in the form of %n$

# What is PARTIALLY supported:
	'#' flag, not in 'oxXeEfgG' or '.' radix conversions
