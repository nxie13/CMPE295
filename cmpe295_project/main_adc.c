/*
 * Main code (adc) for CMPE295 Master Project
 * By Chong Hang Cheong and Nuoya Xie
 *
 *To get lowest power consumption possible:
 * 1. Configure all GPIO as output 0
 * 2. Use VLOCLK and ACLK for low frequency and ultra low power
 * 3. When no int signal is received, the MCU stays in LPM3, which consumes ~0.8uA
 * 4. (tentative) adding external chip such as TLV2760 between sensor and ADC
 *
 * This version of code should be used for sensors read with ADC. Another version
 * is for sensors read with I2C
 *
 * UART Pins: P1.1(RXD) P1.2(TXD)
 * ADC pin: P1.4 (A4)
 * GPIO Trigger pin: P2.5
 * XSTAL pins: P2.6, P2.7
 */

#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define POWER_SAVER_ON 1 //This line determines if we are using LPM to save power
//#define DEBUG_MODE 1 //Switch between debugging mode and actual mode

void wait(unsigned long val);
void clock_init(void);
void adc_init(void);
void trigger_adc(void);
void port_init(void);
void UART_init(void);

static bool stop_transmission = true;
static uint16_t adc_value = 0;
/**
 * main.c
 */
void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // Stop watchdog timer
    clock_init();
    port_init();
    adc_init();
    UART_init();
    __enable_interrupt(); //Enable interrupts

#ifdef POWER_SAVER_ON
    LPM3;
#else

    while (1)
    {
#ifdef DEBUG_MODE
        trigger_adc();
        wait(100);
#endif
    }
#endif
}

void wait(unsigned long val)
{
    int i = 0;
    for (i; i < val * 1000; i++)
    {
        //nothing here
    }
}

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
    P2IE |= BIT5; //Enable Interrupt
    P2IES &= ~BIT5; //rising edge

    //debug: P1.0 is ACLK
    P1DIR |= BIT0;
    P1SEL |= BIT0;
    P1SEL2 &= ~BIT0;
}

void adc_init(void)
{
    //disable ADC for configuration
    ADC10CTL0 &= ~ENC; //reset ENC bit (bit 1)

    //CTL0 configuration
    uint16_t adc_CTL0_value = 0x0;
    adc_CTL0_value |= BITC; //SHTx: sample and hold time. bit 11: 8x ADC10CLKs
    adc_CTL0_value |= BIT3; //IE: interrupt enable
    ADC10CTL0 = adc_CTL0_value;

    //CTL1 configuration
    uint16_t adc_CTL1_value = 0x0;
    adc_CTL1_value |= BITE; //input pin is P1.4 corresponding ADC Input: A4
    adc_CTL1_value |= BIT3; //SSEL: clock source select: bit3 = ACLK
    ADC10CTL1 = adc_CTL1_value;

    //ADC10AE0 Analog input enable
    ADC10AE0 |= BIT4; //Enable analog input on P1.4
}

void trigger_adc(void)
{
    ADC10CTL0 |= ADC10ON | ENC; //ADC on and ADC enable conversion
    ADC10CTL0 |= ADC10SC; //ADC start conversion
}

//ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
    adc_value = ADC10MEM; //obtain ADC value from MEM buffer

#ifdef DEBUG_MODE
    //The code segment below is just for debugging purpose
    //No float conversion will be done in MSP430
    float adc_voltage = ((float) adc_value) * 3.3 / 1024.0;
    if (adc_voltage > 2)
    P1OUT |= BIT6;//Turn on on-board led
    else
    P1OUT &= ~BIT6;//Turn off on-board led
#endif

    //send data (10-bits) to xbee module
    //UART transmission starts by writing upper 2 bits of data into TXBUF
    UCA0TXBUF = adc_value >> 8;
    stop_transmission = false;
}

//Port 2 interrupt service routine
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{
    //If interrupt signal comes from P2.5
    if (P2IFG & BIT5)
    {
        //trigger ADC
        ADC10CTL0 |= ADC10ON | ENC; //ADC on and ADC enable conversion
        ADC10CTL0 |= ADC10SC; //ADC start conversion

        P2IFG &= ~BIT5; //Clear interrupt
    }
}

//UCA0 transmit interrupt service routine
#pragma vector=USCIAB0TX_VECTOR
__interrupt void UART_TX_ISR(void)
{
    if (!stop_transmission)
    {
        //transmit the LSB 8 bits
        UCA0TXBUF = adc_value & 0xff;
        stop_transmission = true;
    }
    else
    {
        //clear TX interrupt
        IFG2 &= ~BIT1;
    }
}

void clock_init(void)
{
    //clock source(for the future): LFXT1CLK, 32.768kHz
    //clock source: VLOCLK, 10.57KHz
    //clock signal: ACLK, no divider

    //PIN config: P2.6, P2.7 xstal input/output
    /*P2DIR &= ~BIT6; //Xin
    P2DIR |= BIT7; //Xout
    P2SEL |= BIT6 | BIT7;
    P2SEL2 &= ~(BIT6 | BIT7);*/

    BCSCTL1 |= BIT7; //turn off XT2 oscillator
    //BCSCTL3 |= BIT2; //XCAPx - crystal cap selection. when XTS is 0, 0b01 on bit 3-2 is ~6pF
    BCSCTL3 |= BIT5; //LFXT1Sx - when XTS is 0, 0b10 on bit 5-4 is VLOCLK
}

void UART_init(void)
{
    //Configuration for xbee: 8-bit, no parity bit, 1 stop bit, lsb first
    UCA0CTL1 |= BIT0; //Set UCSWRST bit

    //CTL0 configuration
    UCA0CTL0 = 0x0;

    //CTL1 configuration
    uint16_t uart_CTL1_value = 0x0;
    uart_CTL1_value = BIT6 | BIT0; //bit6 = ACLK
    UCA0CTL1 = uart_CTL1_value;

    //baud rate configuration (check with xbee ->1200bps)
    /* N = f(BRCLK)/Baud rate, f(BRCLK) = 10.6KHz, baud rate is 1200bps. N = 8.833
     * low freq. mode, UCBRx = INT(N) = 8, UCBRSx = 7F(0.875), UCOS16 = 0
     */
    UCA0BR0 = 8;
    UCA0MCTL= 0x7F << 1;

    //Port Configuration TX and RX
    P1SEL |= BIT1 | BIT2; //TX: P1.2. RX: P1.1
    P1SEL2 |= BIT1 | BIT2;

    //clear UCSWRST bit
    UCA0CTL1 &= ~BIT0; //Reset UCSWRST bit

    //Enable Interrupt - TXIFG
    IE2 |= UCA0TXIE;
}

