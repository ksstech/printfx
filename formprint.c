/*
 * Copyright 2014-21 AM Maree/KSS Technologies (Pty) Ltd.
 *
 * formprint.c
 *
 */

#include 	"formprint.h"
#include	"printfx.h"
#include	"x_terminal.h"

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
    	PRINT("%c", CHR_CR) ;
    	PRINT("%c", CHR_LF) ;
    }
    if (flag & FLAG_CB_PRE_SPC) {
    	PRINT("%c", CHR_SPACE) ;
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
    	PRINT("%c", CHR_COMMA) ;
    }
    if (flag & FLAG_CB_POST_SPC) {
    	PRINT("%c", CHR_SPACE) ;
    }
    if (flag & FLAG_CB_POST_TAB) {
    	PRINT("%c", CHR_TAB) ;
    }
    if (flag & FLAG_CB_POST_CL) {
    	PRINT("%c", CHR_CR) ;
    	PRINT("%c", CHR_LF) ;
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
	if (flag) {
		vLogPrePend(flag) ;								// PREpend handling
	}

    va_start(args, format) ;
    vprintfx(format, args) ;							// output the actual formatted message
    va_end(args) ;

    if (flag) {
    	vLogPostPend(flag) ;						    // POSTpend handling
    }
}

int32_t	xI8ArrayPrint(const char * pHeader, uint8_t * pArray, int32_t ArraySize) {
	int32_t iRV = 0 ;
	if (pHeader) {
		iRV += PRINT(pHeader) ;
	}
	for (int32_t Idx = 0; Idx < ArraySize; ++Idx) {
		iRV += PRINT("  #%d=%d", Idx, pArray[Idx]) ;
	}
	iRV += PRINT("\n") ;
	return iRV ;
}
