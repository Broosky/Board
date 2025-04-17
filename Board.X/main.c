/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Name: Board (C)                                                                                                 //
// Author: Jeffrey Bednar                                                                                                  //
// Copyright (c) Illusion Interactive, 2011 - 2025.                                                                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <xc.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration bits: Internal Oscillator, Watchdog Timer Off, Power-up Timer On, Code Protection Off
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma config FOSC = XT    // Oscillator Selection bits (XT oscillator)
#pragma config WDTE = OFF   // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON   // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOREN = ON   // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF    // Low-Voltage (single-supply) In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function, low-voltage programming disabled)
#pragma config CPD = OFF    // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF    // Flash Program Memory Write Enable bits (Write protection off)
#pragma config CP = OFF     // Flash Program Memory Code Protection bit (Code protection off)
#define _XTAL_FREQ 4000000  // 4 MHz
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void main(void) {
    TRISBbits.TRISB0 = 0;   // Set RB0 as output
    while (1) {
        PORTBbits.RB0 = 1;  // Cycle PIC Activity LED.
        __delay_ms(500);
        PORTBbits.RB0 = 0;
        __delay_ms(500);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
