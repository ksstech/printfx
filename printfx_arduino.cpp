// printfx_arduino.cpp - Copyright (c) 2024 Andre M. Maree / KSS Technologies (Pty) Ltd.

#if (buildGUI == 2)					// LCDGFX
	#include "gui_lcdgfx.hpp"
#elif (buildGUI == 3)				// LGFX
	#include "gui_lovyan.hpp"
#endif
#include "printfx.h"

// ######################################## Build macros ###########################################

#define colGFX(r,g,b)	((r & 0x001F) << 11 | (g & 0x003F) << 5 | (b & 0x001F))

// ##################################### Private constants #########################################

const u16_t Ansi2GFX[] = {
	colGFX(0,0,0),
	colGFX(31,0,0), 
	colGFX(0,63,0), 
	colGFX(0,63,31),
    colGFX(0,0,31), 
	colGFX(31,0,31), 
	colGFX(0,63,31), 
	colGFX(31,63,31),
};

// ##################################### Private functions #########################################

//		PX("%d->%d->x%hX  ", u8Val, u8Val - colourFG_BLACK, Ansi2GFX[u8Val - colourFG_BLACK]);
//		PX("%d->%d->x%hX  ", u8Val, u8Val - colourBG_BLACK, Ansi2GFX[u8Val - colourBG_BLACK]);

void vPrintArduinoProcess(u8_t u8Val) {
	if (INRANGE(colourFG_BLACK, u8Val, colourFG_WHITE)) {					// foreground
		#if (guiOPTION == 0)
			display.setColor(Ansi2GFX[u8Val - colourFG_BLACK]);
		#elif (guiOPTION == 1)
			canvasX.setColor(Ansi2GFX[u8Val - colourFG_BLACK]);
		#endif
	} else if (INRANGE(colourBG_BLACK, u8Val, colourBG_WHITE)) {			// background
		#if (guiOPTION == 0)
			display.setBackground(Ansi2GFX[u8Val - colourBG_BLACK]);
		#elif (guiOPTION == 1)
			canvasX.setBackground(Ansi2GFX[u8Val - colourBG_BLACK]);
		#endif
	}
}

extern "C" void vPrintArduino(u32_t Val) {
   	sgr_info_t sSGR = { .u32 = Val };
	if (sSGR.r && sSGR.c) {
		PX("r=%d  c=%d\r\n", sSGR.r, sSGR.c);
	    display.setTextCursor(sSGR.c * halLCD_FONT_PX, sSGR.r * halLCD_FONT_PY);
	}
	if (sSGR.a1 == 0 && sSGR.a2 == 0) {
		vPrintArduinoProcess(colourFG_WHITE);
		vPrintArduinoProcess(colourBG_BLACK);
	} else {
		vPrintArduinoProcess(sSGR.a1);
		vPrintArduinoProcess(sSGR.a2);
	}
}
