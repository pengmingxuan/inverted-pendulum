#include "SysTickInts.h"
#include "PLL.h"
#include "tm4c123gh6pm.h"

#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_uart.h"
#include "inc/hw_gpio.h"
#include "inc/hw_pwm.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/timer.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "driverlib/pwm.h"
#include "driverlib/ssi.h"
#include "driverlib/systick.h"
#include "driverlib/adc.h"
#include "utils/uartstdio.h"
#include "utils/uartstdio.c"
#include <string.h>


/* -----------------------      Function Prototypes     --------------------- */

//Inits
void PortF_Init(void);
void PortC_Init(void);
void PWM_Init(void);
void ADC_Init(void);
void InitConsole(void);

//Interrupts, ISRs
void disable_interrupts(void);
void enable_interrupts(void);
void wait_for_interrupts(void);
void PortF_Handler(void);

//Other
void MSDelay(unsigned int itime);
void read_ADC(void);
/* -----------------------      Global Variables        --------------------- */

volatile signed long ComparatorValue = 5600;
uint32_t pui32ADC0Value[1]; //data from ADC0

/* -----------------------          Main Program        --------------------- */
int main(void){
    //Inits
    PLL_Init(); //set CPU clock to 80MHz
    PortF_Init();
    //PortC_Init();
    PWM_Init();
    ADC_Init();
    InitConsole();
    // Display the setup on the terminal, View > Terminal
    UARTprintf("ADC0 ->\n");
    UARTprintf("  Type: Potentiometer\n");
    UARTprintf("  Samples: One\n");
    UARTprintf("  Update Rate: 100ms\n");


    // Master interrupt enable func for all interrupts
    IntMasterEnable();
    enable_interrupts();
    while(1){
        read_ADC();
        MSDelay(100);

    }
}


/* Initialize PortF GPIOs */
void PortF_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
    SYSCTL_RCGC2_R |= 0x00000020;           // activate clock for PortF
    while ((SYSCTL_PRGPIO_R & 0x00000020) == 0)
    {};                          // wait until PortF is ready
    GPIO_PORTF_LOCK_R = 0x4C4F434B;         // unlock GPIO PortF
    GPIO_PORTF_CR_R |= 0x1F;                 // allow changes to PF4-0
    GPIO_PORTF_AMSEL_R &= 0x00;              // disable analog on PortF
    GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
    GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
    GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
    GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
    GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4

    GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
    GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
    GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
    GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
    GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
    NVIC_PRI7_R = (NVIC_PRI7_R & 0xFF00FFFF)|0x00A00000; // (g) priority 5
    NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC for PF4 Handler
}

void PortF_Handler(void){
    /*MSDelay (100);
    GPIO_PORTF_ICR_R = 0x10;
    ComparatorValue -= 1000;
        if (ComparatorValue < 0){
            ComparatorValue = 10000; // reload to 10000 if it's less than 0
            PWM1_1_CMPA_R = abs(ComparatorValue - 1); // update comparatorA value
            PWM1_1_CMPB_R = abs(ComparatorValue - 1); // update comparatorB value
        }*/
    MSDelay (100);
    GPIO_PORTF_ICR_R = 0x10;
    PWM1_1_CMPA_R -= 0xC35;
    PWM1_1_CMPB_R -= 0xC35;
    if (PWM1_1_CMPA_R <= 0) {
       PWM1_1_CMPA_R = 0x7A12;
       PWM1_1_CMPB_R = 0x7A12;
    }
}

/* Initialize PortC GPIOs */
void PortC_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000004; // (a) activate clock for port C
    SYSCTL_RCGC2_R |= 0x00000004;           // activate clock for PortC
    while ((SYSCTL_PRGPIO_R & 0x00000004) == 0)
    {};                          // wait until PortC is ready
    GPIO_PORTC_LOCK_R = 0x4C4F434B;         // unlock GPIO PortC
    GPIO_PORTC_CR_R |= 0x0C;                 // allow changes to PC2,3
    GPIO_PORTC_AMSEL_R &= 0x00;              // disable analog on PortC
    GPIO_PORTC_PCTL_R &= ~0x0000FF00;         // use PC2,3 as GPIO
    GPIO_PORTC_DIR_R &= ~0x0C;                // PC2,3 are inputs
    GPIO_PORTC_AFSEL_R &= ~0x0C;              // disable alt function
    GPIO_PORTC_PUR_R |= 0x0C;                // enable pull-up on PC2,3
    GPIO_PORTC_DEN_R |= 0x0C;                // enable digital I/O on PC2,3

    //Interrupts
    /*
    GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
    GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
    GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
    GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
    GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
    NVIC_PRI7_R = (NVIC_PRI7_R & 0xFF00FFFF)|0x00A00000; // (g) priority 5
    NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC for PF4 Handler
    */
}


void PWM_Init(void) {
    SYSCTL_RCGCPWM_R |= 0x02;     // 1) enable PWM1 clock
    SYSCTL_RCGC2_R |= 0x10;          // 2.1) activate clock for PortE
    while ((SYSCTL_PRGPIO_R & 0x10) == 0) {}; // 2.2) wait until PortE is ready
    GPIO_PORTE_AFSEL_R |= 0x30;       // 3) enable alt function on PE5-4
    GPIO_PORTE_PCTL_R &= 0x00FF0000; // 4.1) clear PE5-4 GPIOPCTL PMCx fields
    GPIO_PORTE_PCTL_R |= 0x00550000; // 4.2) configure PE5-4 as PWM Module 1
    GPIO_PORTE_AMSEL_R &= ~0x30;     // 4.3) disable analog functionality on PE5-4
    GPIO_PORTE_DEN_R |= 0x30;        // 4.4) enable digital I/O on PE5-4
    SYSCTL_RCC_R |= 0x00100000;      // 5.1) configure PWM clock divider as the source for PWM clock
    SYSCTL_RCC_R &= ~0x000E0000;     // 5.2) clear PWMDIV field
    SYSCTL_RCC_R |= 0x000060000;     // 5.3) set divisor to 16 so PWM clock source is 1 MHz
    PWM1_1_CTL_R = 0;                // 6.1) disable PWM while initializing; also configure Count-Down mode
    PWM1_1_GENA_R = 0x08C;           // 6.2) drives pwmA HIGH when counter matches value in PWM1LOAD
                                          // drive pwmA LOW when counter matches comparator A
    PWM1_1_GENB_R = 0x80C;           // 6.3) drives pwmB HIGH when counter matches value in PWM1LOAD
                                          // drive pwmB LOW when counter matches comparator B
    //PWM1_1_LOAD_R = 10001 -1;        // 7) since target period is 100Hz, there are 10,000 clock ticks per period
    PWM1_1_LOAD_R = 0x7A12;
    //PWM1_1_CMPA_R = 10000 -1;        // 8) set 0% duty cycle to PE4
    //PWM1_1_CMPB_R = 10000 -1;        // 9) set 0% duty cycle to PE5
    PWM1_1_CMPA_R = 0x7A12;
    PWM1_1_CMPB_R  = 0x7A12;
    PWM1_1_CTL_R |= 0x01;            // 10) start the timers in PWM generator 1 by enabling the PWM clock
    PWM1_ENABLE_R |= 0x0C;           // 11) Enable M1PWM2 and M1PWM3
}

//Initialize console to display information while debugging
void InitConsole(void)
{
    // Enable GPIO port A which is used for UART0 pins.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Configure the pin muxing for UART0 functions on port A0 and A1.
    // This step is not necessary if your part does not support pin muxing.
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);


    // Enable UART0 so that we can configure the clock.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    // Use the internal 16MHz oscillator as the UART clock source.
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    // Select the alternate (UART) function for these pins.
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);


    // Initialize the UART for console I/O, baud rate of 115,200
    UARTStdioConfig(0, 115200, 16000000);
}


void MSDelay(unsigned int itime){
    unsigned int i;
    unsigned int j;
    for (i=0;i<itime; i++){
        for(j=0;j<331;j++){}
    }
}


void ADC_Init(void){
        SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); //enable ADC0
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB); //enable GPIO B
        GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_4); //PB4 as analog input

        // Enable sample sequence 3 with a processor signal trigger.  Sequence 3
        // will do a single sample when the processor sends a signal to start the
        // conversion.  Each ADC module has 4 programmable sequences, sequence 0
        // to sequence 3.  This example is arbitrarily using sequence 3.
        ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);

        // Configure step 0 on sequence 3.  Sample channel 0 (ADC_CTL_CH0) in
        // single-ended mode (default) and configure the interrupt flag
        // (ADC_CTL_IE) to be set when the sample is done.  Tell the ADC logic
        // that this is the last conversion on sequence 3 (ADC_CTL_END).  Sequence
        // 3 has only one programmable step.  Sequence 1 and 2 have 4 steps, and
        // sequence 0 has 8 programmable steps.  Since we are only doing a single
        // conversion using sequence 3 we will only configure step 0.  For more
        // information on the ADC sequences and steps, reference the datasheet.
        ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH10 | ADC_CTL_IE |
                                 ADC_CTL_END);

        // Since sample sequence 3 is now configured, it must be enabled.
        ADCSequenceEnable(ADC0_BASE, 3);

        // Clear the interrupt status flag.  This is done to make sure the
        // interrupt flag is cleared before we sample.
        ADCIntClear(ADC0_BASE, 3);

}

void read_ADC(void){
    //Trigger the conversion
    ADCProcessorTrigger(ADC0_BASE, 3);

    // Wait for conversion to be completed.
    while(!ADCIntStatus(ADC0_BASE, 3, false)){}

    // Clear the ADC interrupt flag.
    ADCIntClear(ADC0_BASE, 3);

    // Read ADC Value.
    ADCSequenceDataGet(ADC0_BASE, 3, pui32ADC0Value);

    // Display the AIN10 (PB4) digital value on the console.
    UARTprintf("PB4 = %4d\r", pui32ADC0Value[0],"\n");
}


/* Disable interrupts by setting the I bit in the PRIMASK system register */
void disable_interrupts(void) {
    __asm("    CPSID  I\n"
          "    BX     LR");
}


/* Enable interrupts by clearing the I bit in the PRIMASK system register */
void enable_interrupts(void) {
    __asm("    CPSIE  I\n"
          "    BX     LR");
}


/* Enter low-power mode while waiting for interrupts */
void wait_for_interrupts(void) {
    __asm("    WFI\n"
          "    BX     LR");
}
