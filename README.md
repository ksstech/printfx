# printf
New enhanced replacement for variants of printf() - very suitable to embedded system

Key features:
  Does not do any dynamic memory allocation.
  All functions are fully re-entrant
  Minimal stack usage, only the absolute minimum size (depending on the specifc format being processed) is allocated.
  Size can be scaled down by excluding support for various formats, both standard and extensions, at compile time.
  
Extensions:
  Date and/or time output in POSIX or other formats.
    ! modifier to change treatment absolute->relative time
    # modifier to select ALTernative format output
    + modifier to enable TZ information being output
  Hexdump (debug style) in byte, short, word or double word formats
    - modifier to remove the default address preceding each line of output
    ! modifier to select absolute or relative address preceding each line of hexdump output.
    + modifier to enable addition of ASCII character display at end of each line.
    Optional upper / lower case conversion of hexdump output.
    ' modifier to enable seperator characters between groups of byte/short/word/longlong output 
  IP address output with leading none, space or zero padding
    Optional big/little endian inversion.
  MAC address output with optional separator characters
    Optional upper / lower case conversion of MAC address output.
  BINARY format output 
  
What is NOT supported:
  locale, radix fixed to '.'
  *n*th argument in the form of %n$
  'n' conversion character
  'h' flag as applied to 'diouxX' to specify source value being short [un]signed int
  'l' flag as applied to 'diouxX' to specify source value being long [un]signed int
  'L' flag as applied to 'eEfgG' to specify source value being long double
  ' ' flag
  
What is PARTIALLY supported:
  '#' flag, not in 'oxXeEfgG' or '.' radix conversions
  