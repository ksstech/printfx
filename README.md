# printf
  New enhanced replacement for variants of printf() - optimized for embedded system

# Key features:
  Does not do ANY dynamic memory allocation.
  All functions are fully re-entrant
  Minimal stack usage, only the absolute minimum size (based on specific format being processed) is allocated.
  Size can be scaled down by excluding support for various formats, both standard and extensions, at compile time.

# Extensions:
  Numeric output scaling
  	#	Alternate format will scale value downwards and adding SI indicator based on thresholds being
  		10^3=K  10^6=M  10^9=G  10^12=T
  		
  Date and/or time output in POSIX or other formats.
    !	modifier to change treatment absolute->relative time
    #	modifier to select ALTernative (HTTP header style) format output
    +	modifier to enable TZ information being output
    
  Hexdump (debug style) in byte, short, word or double word formats
  	b/B	Byte (8 bit) width display
  	h/H	Half word (16 bit) width display
  	w/W	Word (32 bit) width display
    ?/?	Selectable upper/lower case output (bhw vs BHW).
    !	modifier to change absolute -> relative address preceding each line of hexdump output.
    -	modifier to remove the default address preceding each line of output
    '	modifier to enable seperator characters between groups of byte/short/word/llong
    	output using '|: -' for 32/16/8/4 bit boundaries
    +	modifier to enable addition of ASCII character display at end of each line.
    
  IP address output:
    # 	modifier for big/little endian inversion.
    -	modifier for left alignment
  	  	Leading padding, none, space or zero through field width specifiers
    
  MAC address output with optional separator characters
    j/J Selectable upper/lower case output.

  BINARY format output
    '	modifier to enable seperator character using '|: -' for 32/16/8/4 bit boundaries
		Support 32 and 64 bit values
  
  ANSI Set Graphics Rendition (SGR) support

  URL encoding format
  	U	format specified
  	  
# Valid formatting characters:
  !#'*+-%0.0-9ABC D EFGHI J KL MNO PQRS T U VWXYZ
  |||||||||\_/ab|c|defgh|i|jk|lmn|opqr|s|t|uvwxy|z
  ||||||||| | ||||||||||||||||||||||||||||||||||||
  ||||||||| | |||||||||||||||||||||||||||||||||||*----> UNUSED (z)
  ||||||||| | ||||||||||||||||||||||||||||||||||*-----> DT(Z)
  ||||||||| | |||||||||||||||||||||||||||||||||*------> UNUSED (Yy)
  ||||||||| | ||||||||||||||||||||||||||||||||*---> HEX UC/lc value
  ||||||||| | |||||||||||||||||||||||||||||||*--------> HEXDUMP, word (u32) sized values UC/lc
  ||||||||| | ||||||||||||||||||||||||||||||*---------> UNUSED (Vv)
  ||||||||| | |||||||||||||||||||||||||||||*------> (u)NSIGNED number (uint32_t)
  ||||||||| | ||||||||||||||||||||||||||||*-----------> UNUSED (U)
  ||||||||| | |||||||||||||||||||||||||||*--------> Not implemented (t)
  ||||||||| | ||||||||||||||||||||||||||*-------------> (T)IME
  ||||||||| | |||||||||||||||||||||||||*----------> STRING null terminated ascii
  ||||||||| | ||||||||||||||||||||||||*-----------> Not implemented (S)
  ||||||||| | |||||||||||||||||||||||*----------------> UNUSED (Rr)
  ||||||||| | ||||||||||||||||||||||*-----------------> UNUSED (Qq)
  ||||||||| | |||||||||||||||||||||*--------------> POINTER U32 address with (0x/0X) prefix
  ||||||||| | ||||||||||||||||||||*---------------> (o)CTAL value
  ||||||||| | |||||||||||||||||||*--------------------> UNUSED (O)
  ||||||||| | ||||||||||||||||||*-----------------> Not implemented (Nn)
  ||||||||| | |||||||||||||||||*----------------------> MAC address UC/lc
  ||||||||| | ||||||||||||||||*-------------------> LLONG modifier
  ||||||||| | |||||||||||||||*--------------------> Not implemented (L)
  ||||||||| | ||||||||||||||*-------------------------> UNUSED (Kk)
  ||||||||| | |||||||||||||*----------------------> Not implemented (j)
  ||||||||| | ||||||||||||*---------------------------> (J) BINARY U32 string
  ||||||||| | |||||||||||*------------------------> (i)NTEGER same as 'd
  ||||||||| | ||||||||||*-----------------------------> (I)P address
  ||||||||| | |||||||||*------------------------------> HEXDUMP, halfword (u16) sized values UC/lc
  ||||||||| | ||||||||*---------------------------> FLOAT generic format
  ||||||||| | |||||||*----------------------------> FLOAT fixed format
  ||||||||| | ||||||*-----------------------------> FLOAT exponential format
  ||||||||| | |||||*------------------------------> (d)ECIMAL signed formatted long
  ||||||||| | ||||*-----------------------------------> (D)ATE formatted
  ||||||||| | |||*--------------------------------> (c)HAR single U8
  ||||||||| | ||*---------------------------------> (C) ANSI SGR Color support
  ||||||||| | |*--------------------------------------> (Bb) HEXDUMP, byte (u8) sized values UC/lc
  ||||||||| | *-----------------------------------> (Aa) Not implemented!!
  ||||||||| *--> field width specifiers
  ||||||||*----> FLOAT fractional field size separator
  |||||||*-----> PAD0 enable flag
  ||||||*------> format specifier flag
  |||||*-------> LEFT justification, HEXDUMP remove address info
  ||||*--------> SIGN leading '+' or '-',	DTZ=Add full TZ info, HEXDUMP add ASCII info
  |||*---------> Minwid or precision variable provided
  ||*----------> Xxx=3 digits group, DTZ "::." -> "hms"	DUMP use '|-+'
  |*-----------> Xxx=Scaling, DTZ=alt form (Sun, 10 Sep 2017 20:50:37 GMT) IP=ntohl() Hex=Reverse order
  *------------> DTZ=elapsed time,	DUMP=relative addr,

# Examples:
   "%'03llJ"			- print binary representation, optional separator, llong & field width modifiers
	"%['!#+ll]{BbHhWw}"	- hexdump of memory area, USE 2 PARAMETERS FOR START AND LENGTH !!!!
 						MUST NOT specify "*" or "." or "*." or .*", this will screw up the parameter sequence
	"%[-0]I"			- print IP address, justified left or right (pad 0 or ' ')
	"%[']{Mm}"			- prints MAC address, optional ':' separator, upper/lower case
	"%[!']D"			- POSIX [relative/altform] date (1 parameter, pointer to TSZ_t
	"%[!']T"			- POSIX [relative/altform] time
	"%[!']Z"			- POSIX [relative/altform] date, time & zone

# Todo list:
   Add modifier (and OPTIONAL value specifier) to indicate that pointer to ARRAY of values provided
   If the modifier provided, but no counter, then the pointer to be used to meet the content requirements of the number of specifiers
   If the value specifier provided, then use/increment the pointer, using the SAME FORMAT specifier, for the number of times specified.

# What is NOT supported:
  locale, radix fixed to '.'
  *n*th argument in the form of %n$
  'n' conversion character
  'h' flag as applied to 'diouxX' to specify source value being short [un]signed int
  'l' flag as applied to 'diouxX' to specify source value being long [un]signed int
  'L' flag as applied to 'eEfgG' to specify source value being long double
  ' ' flag

# What is PARTIALLY supported:
  '#' flag, not in 'oxXeEfgG' or '.' radix conversions
