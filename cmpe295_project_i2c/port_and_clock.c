/*
 * port_and_clock.c
 *
 *  Created on: Apr 3, 2020
 *      Author: Matt
 */

#include "port_and_clock.h"

void port_init(void)
{
    //All pins will be configured to output driving low to avoid unnecessary power draw
    P1DIR = 0xFF;
    P1OUT = 0x0;
    P2DIR = 0xFF;
    P2OUT = 0x0;
    P3DIR = 0xFF;
    P3OUT = 0x0;

    //P2.5 is used as the interrupt pin to wakeup MSP430
    P2SEL &= ~BIT5;
    P2SEL2 &= ~BIT5; //GPIO function
    P2DIR &= ~BIT5; //input
    //P2IE |= BIT5; //Enable Interrupt
    P2IES &= ~BIT5; //rising edge

    P1DIR |= BIT0 + BIT1 + BIT2 + BIT3 + BIT4;
    P1OUT &= ~(BIT0 + BIT1 + BIT2 + BIT3 + BIT4);

    //Configure ports as SCL and SDA
    P1SEL |= BIT6 | BIT7; //1.6: SCL 1.7: SDA
    P1SEL2 |= BIT6 | BIT7;
}

void clock_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // Stop watchdog timer

    //clock source: DCOCLK, 1MHz
    //clock signal: SMCLK, no divider

    //load DCO calibration data from TLV
    if (CALBC1_16MHZ == 0xFF)                  // If calibration constant erased
    {
        while (1)
            ;                               // do not load, trap CPU!!
    }
    DCOCTL = 0;                          // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                    // Set DCO calibration
    DCOCTL = CALDCO_1MHZ;
}




