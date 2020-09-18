/*
The MIT License (MIT)

Copyright (c) 2016 British Broadcasting Corporation.
This software is provided by Lancaster University by arrangement with the BBC.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "nrf_gpio.h"

#include "MicroBitPanic.h"
#include "MicroBitSAPanicFont.h"
#include "MicroBitMatrixMaps.h"

#ifndef P0_19
#define P0_19 19
#endif

#ifndef MICROBIT_PIN_BUTTON_RESET
#define MICROBIT_PIN_BUTTON_RESET               P0_19
#endif

static const uint8_t panicFace[5] = {0x1B, 0x1B,0x0,0x0E,0x11};


// length of message: face, E, code digit, code digit, code digit
#ifndef microbit_panic_MSGLEN
#define microbit_panic_MSGLEN         4
#endif

// position of first code digit
#ifndef microbit_panic_MSG1STDIGIT
#define microbit_panic_MSG1STDIGIT    1
#endif

// divisor for first digit
#ifndef microbit_panic_DIVMAX
#define microbit_panic_DIVMAX         100
#endif

// divisor base
#ifndef microbit_panic_DIVBASE
#define microbit_panic_DIVBASE        10
#endif


/**
  * Disables all interrupts and user processing.
  * Displays "=(" and an accompanying status code on the default display.
  * @param statusCode the appropriate status code, must be in the range 0-999.
  *
  * @code
  * microbit_standalone_panic(20);
  * @endcode
  */
void microbit_standalone_panic(int statusCode)
{
    nrf_gpio_cfg_input( MICROBIT_PIN_BUTTON_RESET, NRF_GPIO_PIN_PULLUP);

    uint32_t    row_mask = 0;
    uint32_t    col_mask = 0;
    uint32_t    row_reset = 0x01 << microbitMatrixMap.rowStart;
    uint32_t    row_data = row_reset;
    uint8_t     strobeRow = 0;

    row_mask = 0;
    for (int i = microbitMatrixMap.rowStart; i < microbitMatrixMap.rowStart + microbitMatrixMap.rows; i++)
        row_mask |= 0x01 << i;

    for (int i = microbitMatrixMap.columnStart; i < microbitMatrixMap.columnStart + microbitMatrixMap.columns; i++)
        col_mask |= 0x01 << i;

    uint32_t mask = row_mask | col_mask;
    
    for ( int pin = 0; pin < 31; pin++)
    {
        if ( mask & (1 << pin))
            nrf_gpio_cfg_output( pin);
    }

    if(statusCode < 0 || statusCode > 999)
        statusCode = 0;

    __disable_irq(); //stop ALL interrupts

    while(true)
    {
        //iterate through our chars :)
        for(int characterCount = 0; characterCount < microbit_panic_MSGLEN; characterCount++)
        {
            // find the the current character and its font bytes
            const uint8_t *fontBytes = panicFace;
            if ( characterCount >= microbit_panic_MSG1STDIGIT)
            {
                // calculate divisor for this digit: 100s, 10s or units
                int div = microbit_panic_DIVMAX;
                for ( int digit = characterCount - microbit_panic_MSG1STDIGIT; digit > 0; digit--)
                    div /= microbit_panic_DIVBASE;
                fontBytes = pendolino3_digits + MICROBIT_FONT_WIDTH * ( ( statusCode / div) % microbit_panic_DIVBASE);
            }

            int outerCount = 0;

            //display the current character
            while(outerCount < 500)
            {
                uint32_t col_data = 0;

                int i = 0;

                //if we have hit the row limit - reset both the bit mask and the row variable
                if(strobeRow == microbitMatrixMap.rows)
                {
                    strobeRow = 0;
                    row_data = row_reset;
                }

                // Calculate the bitpattern to write.
                for (i = 0; i < microbitMatrixMap.columns; i++)
                {
                    int index = (i * microbitMatrixMap.rows) + strobeRow;

                    int bitMsk = 0x10 >> microbitMatrixMap.map[index].x; //chars are right aligned but read left to right
                    int y = microbitMatrixMap.map[index].y;

                    if( fontBytes[y] & bitMsk)
                        col_data |= (1 << i);
                }

                col_data = ~col_data << microbitMatrixMap.columnStart & col_mask;

                if( outerCount < 50)
                    NRF_GPIO->OUT = NRF_GPIO->OUT & ~mask;
                else
                    NRF_GPIO->OUT = (NRF_GPIO->OUT & ~mask) | ((col_data | row_data) & mask);

                //burn cycles
                i = 3000;
                while(i>0)
                {
                    // Check if the reset button has been pressed. Interrupts are disabled, so the normal method can't be relied upon...
                    if ( !nrf_gpio_pin_read( MICROBIT_PIN_BUTTON_RESET))
                    {
                        //i--;
                        NVIC_SystemReset();
                    }

                    i--;
                }

                //update the bit mask and row count
                row_data <<= 1;
                strobeRow++;
                outerCount++;
            }
        }
    }
}


#ifdef __cplusplus
extern "C" {
#endif

/* Override WEAK void mbed_die to avoid pulling in mbed gpio functions
 */
void mbed_die(void)
{
}

#ifdef __cplusplus
}
#endif
