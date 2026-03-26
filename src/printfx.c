// printfx.c

#include "printfx.h"

#if defined(printfxVER0)
	#include "printfx_v0.c"
#elif defined(printfxVER1)
	#include "printfx_v1.c"
#else
	#error"Invalid printf version specified"
#endif
