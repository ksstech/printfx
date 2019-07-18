/*
 * Copyright 2014-18 AM Maree/KSS Technologies (Pty) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * x_formprint.c
 *
 * Date			Ver		Author	Comments
 * 2015/01/17	1.00	AMM		Original version split from x_circbuf
  */

#include 	"x_formprint.h"

#include	"x_config.h"
#include	"x_debug.h"
#include	"x_printf.h"
#include	"x_terminal.h"

#include	"hal_timer.h"

/*
 *	vLogPrePend - process all options to prepend info
 *	\brief	prepend all the characters as per the flag in the specific sequence
 *	\param	pCB - pointer to the circular buffer control structure
 * 			flag	- control buffer formatting flag
 *	\return	none
 */
void	vLogPrePend(uint32_t flag) {
// Handle foreground colour
	if (flag & FLAG_CB_FG_MASK) {
		vTermSetForeground((flag & FLAG_CB_FG_MASK) >> 29) ;
	}
// Handle background colour
	if (flag & FLAG_CB_BG_MASK) {
		vTermSetBackground((flag & FLAG_CB_BG_MASK) >> 25) ;
	}
// Insert the PREpend options
    if (flag & FLAG_CB_PRE_CL) {
    	printfx("%c", CHR_CR) ;
    	printfx("%c", CHR_LF) ;
    }
	if (flag & FLAG_CB_PRE_UPSECS) {
		printfx("%'T: ", RunTime) ;
	}
	if (flag & FLAG_CB_PRE_DATE) {
		printfx("%D ", &sTSZ) ;
	}
	if (flag & FLAG_CB_PRE_TIME) {
		printfx("%T ", &sTSZ) ;
	}
    if (flag & FLAG_CB_PRE_SPC) {
    	printfx("%c", CHR_SPACE) ;
    }
}

/*
 *	vLogPostPend - process all options to postpend info
 *	\brief	append all the character as per the flag in the specific sequence/priority
 *	\param	pCB - pointer to the circular buffer control structure
 * 			flag	- control buffer formatting flag
 *	\return	none
 */
void	vLogPostPend(uint32_t flag) {
// Reset fore/background colours
	if (flag & FLAG_CB_FG_MASK) {
		vTermSetForeground(FLAG_CB_FG_WHITE >> 29) ;
	}
	if (flag & FLAG_CB_BG_MASK) {
		vTermSetBackground(FLAG_CB_BG_BLACK >> 25) ;
	}
// Insert the POSTpend options
    if (flag & FLAG_CB_POST_COMMA) {
    	printfx("%c", CHR_COMMA) ;
    }
    if (flag & FLAG_CB_POST_SPC) {
    	printfx("%c", CHR_SPACE) ;
    }
    if (flag & FLAG_CB_POST_TAB) {
    	printfx("%c", CHR_TAB) ;
    }
    if (flag & FLAG_CB_POST_CL) {
    	printfx("%c", CHR_CR) ;
    	printfx("%c", CHR_LF) ;
    }
}

/*
 * vLogPrintf - write printf format output to the circular buffer
 * \brief	buffer and control structure MUST have been created prior
 * \param	pCB - pointer to circular buffer control structure
 * 			flag	- control buffer formatting flag
 * 			format is pointer to the array to be written
 * 			[...] variable number of parameters
 * \return	None
 */
void 	vLogPrintf(uint32_t flag, const char * format, ...) {
va_list args ;
// PREpend handling
	if (flag) {
		vLogPrePend(flag) ;
	}
// output the actual formatted message
    va_start(args, format) ;
    vprintfx(format, args) ;
    va_end(args) ;
// POSTpend handling
    if (flag) {
    	vLogPostPend(flag) ;
    }
}

int32_t	xI8ArrayPrint(const char * pHeader, uint8_t * pArray, int32_t ArraySize) {
	int32_t iRV = 0 ;
	if (pHeader) {
		iRV += printfx(pHeader) ;
	}
	for (int32_t Idx = 0; Idx < ArraySize; ++Idx) {
		iRV += printfx("  #%d=%d", Idx, pArray[Idx]) ;
	}
	iRV += printfx("\n") ;
	return iRV ;
}
