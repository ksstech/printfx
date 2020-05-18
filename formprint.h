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
 * formprint.h
 */

#pragma once

#include	<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ############################## formatting flags ############################

// Foreground colour flags
#define		FLAG_CB_FG_MASK					0xF0000000
#define		FLAG_CB_FG_WHITE				0xF0000000
#define		FLAG_CB_FG_CYAN					0xD0000000
#define		FLAG_CB_FG_MAGENTA				0xB0000000
#define		FLAG_CB_FG_BLUE					0x90000000
#define		FLAG_CB_FG_YELLOW				0x70000000
#define		FLAG_CB_FG_GREEN				0x50000000
#define		FLAG_CB_FG_RED					0x30000000
#define		FLAG_CB_FG_BLACK				0x10000000

// Background colour flags
#define		FLAG_CB_BG_MASK					0x0F000000
#define		FLAG_CB_BG_WHITE				0x0F000000
#define		FLAG_CB_BG_CYAN					0x0D000000
#define		FLAG_CB_BG_MAGENTA				0x0B000000
#define		FLAG_CB_BG_BLUE					0x09000000
#define		FLAG_CB_BG_YELLOW				0x07000000
#define		FLAG_CB_BG_GREEN				0x05000000
#define		FLAG_CB_BG_RED					0x03000000
#define		FLAG_CB_BG_BLACK				0x01000000

// PrePend flags
#define		FLAG_CB_PRE_CL					0x00800000
//#define		FLAG_CB_PRE_UPSECS				0x00400000
//#define		FLAG_CB_PRE_DATE				0x00200000
//#define		FLAG_CB_PRE_TIME				0x00100000
#define		FLAG_CB_PRE_SPC					0x00080000
// spare 40000-00800
#define		FLAG_CB_MID_ADDR				0x00000400			// include pointer/addres value
#define		FLAG_CB_MID_RELA				0x00000200			// pointer/address value relative offset only
#define		FLAG_CB_MID_SEP					0x00000100			// '-' or '|' or ' '
#define		FLAG_CB_MID_WID					0x000000C0			// 00=08bit, 01=16bit, 02=32bit, 03=64bit
#define		FLAG_CB_MID_WID64				0x000000C0
#define		FLAG_CB_MID_WID32				0x00000080
#define		FLAG_CB_MID_WID16				0x00000040
#define		FLAG_CB_MID_WID08				0x00000000
#define		FLAG_CB_MID_XXXXX				0x00000020			// SPARE
// Other ie Wrap Insert flags
#define		FLAG_CB_WRAP_BRAC				0x00000010
// PostPend flags
#define		FLAG_CB_POST_COMMA				0x00000008
#define		FLAG_CB_POST_SPC				0x00000004
#define		FLAG_CB_POST_TAB				0x00000002
#define		FLAG_CB_POST_CL					0x00000001

#define		CBF_CLUP_X_X					(FLAG_CB_PRE_CL | FLAG_CB_PRE_UPSECS)
#define		CBF_CLUP_X_CL					(FLAG_CB_PRE_CL | FLAG_CB_PRE_UPSECS | FLAG_CB_POST_CL)

#define		CBF_DTTISP_X_X					(FLAG_CB_PRE_DATE | FLAG_CB_PRE_TIME | FLAG_CB_PRE_SPC)
#define		CBF_DTTISP_X_CL					(FLAG_CB_PRE_DATE | FLAG_CB_PRE_TIME | FLAG_CB_PRE_SPC | FLAG_CB_POST_CL)

#define		CBF_SP_X_CL						(FLAG_CB_PRE_SPC | FLAG_CB_POST_CL)

#define		CBF_UP_X_X						(FLAG_CB_PRE_UPSECS)
#define		CBF_UP_X_COMSP					(FLAG_CB_PRE_UPSECS | FLAG_CB_POST_COMMA | FLAG_CB_POST_SPC)
#define		CBF_UP_X_CL						(FLAG_CB_PRE_UPSECS | FLAG_CB_POST_CL)
#define		CBF_UP_ADSPBR_CL				(FLAG_CB_PRE_UPSECS | FLAG_CB_MID_ADDR | FLAG_CB_MID_SEP | FLAG_CB_WRAP_BRAC | FLAG_CB_POST_CL)
#define		CBF_UPDTTI_X_CL					(FLAG_CB_PRE_UPSECS | FLAG_CB_PRE_DATE | FLAG_CB_PRE_TIME | FLAG_CB_POST_CL)
#define		CBF_UPDTTISP_X_CL				(FLAG_CB_PRE_UPSECS | FLAG_CB_PRE_DATE | FLAG_CB_PRE_TIME | FLAG_CB_PRE_SPC | FLAG_CB_POST_CL)

#define		CBF_X_ADSP_X					(FLAG_CB_MID_ADDR | FLAG_CB_MID_SEP)
#define		CBF_X_ADSP_CL					(FLAG_CB_MID_ADDR | FLAG_CB_MID_SEP | FLAG_CB_POST_CL)
#define		CBF_X_ADSPBR_CL					(FLAG_CB_MID_ADDR | FLAG_CB_MID_SEP | FLAG_CB_WRAP_BRAC | FLAG_CB_POST_CL)
#define		CBF_X_SP_X						(FLAG_CB_MID_SEP)
#define		CBF_X_BR_CL						(FLAG_CB_WRAP_BRAC | FLAG_CB_POST_CL)

#define		CBF_X_RASPBR08_CL				(FLAG_CB_WRAP_BRAC | FLAG_CB_MID_RELA | FLAG_CB_MID_SEP | FLAG_CB_MID_WID08 | FLAG_CB_POST_CL)
#define		CBF_X_RASPBR16_CL				(FLAG_CB_WRAP_BRAC | FLAG_CB_MID_RELA | FLAG_CB_MID_SEP | FLAG_CB_MID_WID16 | FLAG_CB_POST_CL)
#define		CBF_X_RASPBR32_CL				(FLAG_CB_WRAP_BRAC | FLAG_CB_MID_RELA | FLAG_CB_MID_SEP | FLAG_CB_MID_WID32 | FLAG_CB_POST_CL)
#define		CBF_X_RASPBR64_CL				(FLAG_CB_WRAP_BRAC | FLAG_CB_MID_RELA | FLAG_CB_MID_SEP | FLAG_CB_MID_WID64 | FLAG_CB_POST_CL)

#define		CBF_X_X_COMSP					(FLAG_CB_POST_COMMA | FLAG_CB_POST_SPC)
#define		CBF_X_X_CL						(FLAG_CB_POST_CL)

// ################################### global variable definitions #################################

// ############################### formatted print function prototypes #############################

void		vLogPrePend(uint32_t flag) ;
void		vLogPostPend(uint32_t flag) ;
void 		vLogPrintf(uint32_t flag, const char * format, ...) ;

int32_t		xI8ArrayPrint(const char * pHeader, uint8_t * pArray, int32_t ArraySize) ;

#ifdef __cplusplus
}
#endif
