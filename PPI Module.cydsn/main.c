/*

    PSoc5 (CY8CKIT-059 development board) PPI module on the cheap

    Based on blog post by dirktheeng

    http://www.buildlog.net/blog/2011/12/getting-more-power-and-cutting-accuracy-out-of-your-home-built-laser-system/

    NOTE: the solution is NOT tested in a machine, it is just a part of learning PSoC coding for me 
          An I2C or SPI interface for parameter setting should be added in a practical implementation,
          allowing control from gcode.
    
*/

/*

Copyright (c) 2017, Terje Io
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

· Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

· Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

· Neither the name of the copyright holder nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#include "project.h"

//#define PPIDEBUG // uncomment to output info on a OLED screen (requires OLED lib - TBR)

#ifdef PPIDEBUG
#include "fonts/font.h"
#include "fonts/freepixel_9x17.h"
#include "fonts/font_23x16.h"
#include "OLED/OLED.h"
#include "OLED/FontPack.h"
#endif

#define MIDPOINT (2^31)
#define XSTEPSPERINCH (250.0f*25.4f)
#define YSTEPSPERINCH (250.0f*25.4f)

typedef struct {
	int32_t x;
	int32_t y;
} position;

float ppiDist = 1.0f / 30.0f;
uint16_t pulseuS = 4000, ppi = 30;

position current = {0, 0}, previous = {0, 0};

inline void laserOn()
{
    LaserControl_Write(2);
    PWTimer_Enable();
    LaserControl_Write(0);
}

inline bool laserIsOn (void)
{
	return LaserIn_Read() != 0;
}

void process (void) 
{
	float x, y;
	static float distance = 0.0f;
  
    if(ppi != 0) { // PPI mode:

//		CyGlobalIntDisable; // Disable interrupts

//      current.x = X_Axis_ReadCounter() - MIDPOINT;	// read X position
//		current.y = Y_Axis_ReadCounter() - MIDPOINT;	// read y position

        PositionLatch_Write(1);
        CyDelayUs(1);
		current.x = X_Axis_ReadCapture() - MIDPOINT;	// read X position
		current.y = Y_Axis_ReadCapture() - MIDPOINT;	// read y position
        PositionLatch_Write(0);

//		CyGlobalIntEnable; // Reenable interrupts

	 // if laser enabled and not already on then calculate movement (if any) and pulse laser if larger or equal to PPI distance

		if (!LaserOut_Read() && laserIsOn() && ((current.x != previous.x) || (current.y != previous.y))) {

			x = (previous.x - current.x) / XSTEPSPERINCH;
			y = (previous.y - current.y) / YSTEPSPERINCH;
			previous.x = current.x;
			previous.y = current.y;
            
            distance += sqrtf(x * x + y * y);

			if(distance >= ppiDist) {
				laserOn();
                distance = 0.0f;
			}

		} else
			CyDelayUs(100);
    }
}

char *itoa (int32_t val)
{
	static char s[16];

    sprintf(s, "%0+6d", val);
    
	return s;
}

int main(void)
{
    
    uint8_t c;

    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */

    UART_Start();
    PWTimer_Start();
    X_Axis_Start();
    Y_Axis_Start();
    
    X_Axis_WriteCounter(MIDPOINT);                    
    Y_Axis_WriteCounter(MIDPOINT);
    PWTimer_WritePeriod(pulseuS);

#ifdef PPIDEBUG    
    OLED_Init(); 
    OLEDF_DrawRectangle(4, 0, 123, 23);
    OLEDF_DrawString(font_freepixel_9x17, 10, 19, "Io Engineering", false);
    OLEDF_DrawString((Font *)font_23x16, 5, 43, "PSoC5 OLED", false);

    CyDelay(2000);
    
    OLED_ClearScreen();
    OLEDF_DrawRectangle(0, 0, 127, 63);

    OLED_DrawCircle(64, 32, 30);
    
    CyDelay(2000);
    
    OLED_ClearScreen();
    OLED_DrawString(FONT6x8, 0, 1, "Laser PPI Module");
    OLED_DrawString(FONT6x8, 3, 3, "X:");  
    OLED_DrawString(FONT6x8, 4, 3, "Y:");  
    OLED_DrawString(FONT6x8, 5, 3, "PPI:");  
    OLED_DrawString(FONT6x8, 6, 3, "PW:");
    OLED_DrawString(FONT6x8, 5, 30, itoa(ppi));
    OLED_DrawString(FONT6x8, 6, 30, itoa(pulseuS));
#endif

    UART_PutString("PPI Module\r\n");
     
    ppiDist = 1.0f / ppi;

    while(true)
    {
        if((c = UART_GetChar())) {
            
            UART_PutChar(c);
            
            switch(c) {
                
                case 'r': // reset step counters
                    X_Axis_WriteCounter(MIDPOINT);                    
                    Y_Axis_WriteCounter(MIDPOINT);
                    break;
                
                case 'o': // set pass-through mode
                    ppi = 0;
                    LaserControl_Write(1);
                    break;

                case 'p': // set PPI mode
                    ppi = (uint16_t)(1.0f / ppiDist);
                    LaserControl_Write(0);
                    break;

                case 'd': // cycle PPI value
                    if(ppi) {
                        ppi *= 2;
                        if(ppi > 1200)
                            ppi = 30;
                        ppiDist = 1.0f / ppi;
                      #ifdef PPIDEBUG
                        OLED_DrawString(FONT6x8, 5, 30, itoa(ppi));
                      #endif
                    }
                    break;

                case 'w': // cycle pulse witdth value
                    pulseuS += 500;
                    if(pulseuS > 6000)
                        pulseuS = 1500;
                    PWTimer_WritePeriod(pulseuS);
                  #ifdef PPIDEBUG
                    OLED_DrawString(FONT6x8, 6, 30, itoa(pulseuS));
                  #endif
                    break;

            }
        }
        
        process();

      #ifdef PPIDEBUG                  
        OLED_DrawString(FONT6x8, 3, 30, itoa(current.x));                  
        OLED_DrawString(FONT6x8, 4, 30, itoa(current.y));            
      #endif
    
    }
}

/* [] END OF FILE */
