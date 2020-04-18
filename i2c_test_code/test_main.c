/*
 * Main code (I2C) for CMPE295 Master Project
 * By Chong Hang Cheong and Nuoya Xie
 *
 *To get lowest power consumption possible:
 * 1. Configure all GPIO as output 0
 * 2. Use VLOCLK and ACLK for low frequency and ultra low power
 * 3. When no int signal is received, the MCU stays in LPM3, which consumes ~0.8uA
 * 4. (tentative) adding external chip such as TLV2760 between sensor and ADC
 *
 * This version of code should be used for sensors read with I2C. Another version
 * is for sensors read with adc
 *
 * UART Pins: P1.1(RXD) P1.2(TXD)
 * I2C pin: 1.6: SCL 1.7: SDA
 * GPIO Trigger pin: P2.5
 * XSTAL pins: P2.6, P2.7
 */

#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

//#define POWER_SAVER_ON 1 //This line determines if we are using LPM to save power
#define DEBUG_MODE 1 //Switch between debugging mode and actual mode
#define SENSOR 0 //0 - temp/humidity, 1 = light sensor

void clock_init(void);
void port_init(void);
void I2C_init(void);

volatile int byte_counter = 1;

/**
 * main.c
 */
void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // Stop watchdog timer
    clock_init();
    port_init();
    I2C_init();
    //UART_init();
    __enable_interrupt(); //Enable interrupts

#ifdef POWER_SAVER_ON
    LPM3;
#else

    while (1)
    {
#ifdef DEBUG_MODE
        /* Initialize slave address and interrupts */
           IFG2 &= ~(UCB0TXIFG + UCB0RXIFG);       // Clear any pending interrupts
           IE2 &= ~UCB0RXIE;                       // Disable RX interrupt
           IE2 |= UCB0TXIE;                        // Enable TX interrupt

           //bit 1: UCTXSTT - transmit START condition in master mode
           //bit 4: UCTR - Transmitter/receiver. 1 = transmitter
           UCB0CTL1 |= BIT1 | BIT4; //Start transmission

           byte_counter =1;
#endif
    }
#endif
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
}

//Port 2 interrupt service routine
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{
    //If interrupt signal comes from P2.5
    if (P2IFG & BIT5)
    {
        //sensor_value = read_sensor();


        P2IFG &= ~BIT5; //Clear interrupt
    }
}

//ISRs
//I2C/UART TX interrupt service routine
#pragma vector=USCIAB0TX_VECTOR
__interrupt void I2C_buffer_ISR(void)
{

    if (IFG2 & UCB0RXIFG) // Receive Data Interrupt
    {

    }
    else if (IFG2 & UCB0TXIFG) // Transmit Data Interrupt
    {
        if (byte_counter == 1)
        {
            //send cmd byte
            UCB0TXBUF = 0xFE;
            byte_counter--;
        }
        else if (byte_counter == 0)
        {
            //send stop bit

            UCB0CTL1 |= UCTXSTP;      // Send stop condition
        }
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCIAB0RX_VECTOR))) USCIAB0RX_ISR (void)
#else
#error Compiler not supported!
#endif
{
    if (UCB0STAT & UCNACKIFG)
    {
        UCB0STAT &= ~UCNACKIFG;             // Clear NACK Flags
    }
    if (UCB0STAT & UCSTPIFG)                        //Stop or NACK Interrupt
    {
        UCB0STAT &=
            ~(UCSTTIFG + UCSTPIFG + UCNACKIFG);     //Clear START/STOP/NACK Flags
    }
    if (UCB0STAT & UCSTTIFG)
    {
        UCB0STAT &= ~(UCSTTIFG);                    //Clear START Flags
    }
}


void clock_init(void)
{
    //clock source: VLOCLK, 10.6KHz
    //clock signal: ACLK, no divider

    BCSCTL1 |= BIT7; //turn off XT2 oscillator
    BCSCTL3 |= BIT5; //LFXT1Sx - when XTS is 0, 0b10 on bit 5-4 is VLOCLK
}

//I2C initialization
void I2C_init(void)
{
    //Set UCSWRST bit
    /*UCB0CTL1 |= BIT0;

    //USCI CTL0 configuration
    /* 7-bit address, single master environment
     * BIT3 = master mode
     * BIT2 | BIT1 = I2C mode
     * BIT0 = synchronous mode

    uint8_t I2C_CTL0_value = 0x0;
    I2C_CTL0_value |= BIT3 | BIT2 | BIT1 | BIT0;
    UCB0CTL0 = I2C_CTL0_value;

    //CTL1 configuration
    /* BIT6 = ACLK
     * BIT4 = Transmitter

    uint8_t I2C_CTL1_value = 0x0;
    I2C_CTL1_value = BIT6 | BIT4;
    UCB0CTL1 = I2C_CTL1_value;

    //Frequency configuration: whatever VLO is, divide by 10
    UCB0BR0 = 10;

    //Configure ports as SCL and SDA
    P1SEL |= BIT6 | BIT7; //1.6: SCL 1.7: SDA
    P1SEL2 |= BIT6 | BIT7;

    //Clear UCSWRST
    UCB0CTL1 &= ~BIT0;

    //Configure interrupts: enable TX and RX buffer interrupts
    IE2 |= UCB0TXIE | UCB0RXIE;
    UCB0I2CIE |= UCNACKIE;*/

    UCB0CTL1 |= UCSWRST;                      // Enable SW reset
        UCB0CTL0 = UCMST + UCMODE_3 + UCSYNC;     // I2C Master, synchronous mode
        UCB0CTL1 |= BIT6 | BIT4;            // Use SMCLK, keep SW reset
        UCB0BR0 = 10;                           // fSCL = SMCLK/160 = ~100kHz
        UCB0BR1 = 0;
        UCB0I2CSA = 0x60;                   // Slave Address
        UCB0CTL1 &= ~UCSWRST;                     // Clear SW reset, resume operation
        UCB0I2CIE |= UCNACKIE;
        IE2 |= UCB0TXIE | UCB0RXIE;
}

