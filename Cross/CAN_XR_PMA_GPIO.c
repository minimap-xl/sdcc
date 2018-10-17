/*+
    SDCC - A Software-Defined CAN Controller
    Copyright (C) 2018 National Research Council of Italy and
      University of Luxembourg

    Authors: Ivan Cibrario Bertolotti and Tingting Hu

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    The use of the program may be restricted in certain countries by
    intellectual property rights owned by Bosch.  For more information, see:

    http://www.bosch-semiconductors.com/ip-modules/can-ip-modules/can-protocol/
+*/

/* Implementation of a GPIO-based CAN XR PMA on the LPC1768 and
   LPC4357 boards.

   On the LPC1768 boards:

   This PMA works on GPIO Port P0.5 (transmit) and P0.4 (receive).  On
   LPC1768 boards BRD00014A0 and BRD00058A0, these bits are connected
   to the CAN transceiver because they share the same pins as TD2 and
   RD2 (coming from the CAN2 hardware controller), respectively.

   See CAN_XR_CAN_Driver.c for details about how to set the LPC1768
   pin configuration properly.

   On the LPC4357 boards:

   This PMA works on GPIO Port P5_9 (transmit, connected to Pin P3_2)
   and GPIO Port P5_8 (receive, connected to Pin P3_1).  On the
   LPC4357 board PE2036A0-V3, these bits are connected to the CAN
   transceiver because they share the same pins as CAN0_TX and CAN0_RX
   (coming from the CAN0 hardware controller), respectively.

   Hardware-based timings and synchronization code taken from
     exp_swtx.c 1.42 (CVS Papers/supercan/Software, Super CAN)
*/

#include <stdio.h>
#include <stdlib.h>
#include "CAN_XR_PMA_GPIO.h"
#include "CAN_XR_Trace.h"


/* --------------- Architecture dependent register definition ---------------*/

#define ADDR_REG32 (volatile uint32_t *)
#define REG32(x)   (* (ADDR_REG32 (x)))

#ifdef GCC_ARM_CM3_UN_LPC1768

/* Registers */
#define PINSEL0		REG32(0x4002C000)
#define PINMODE0	REG32(0x4002C040)
#define PINMODE_OD0	REG32(0x4002C068)

/* GPIO registers */
#define FIO0DIR		REG32(0x2009C000) /* (UM 10360 p.122) */
#define FIO0PIN		REG32(0x2009C014)
#define FIO0SET		REG32(0x2009C018)
#define FIO0CLR		REG32(0x2009C01C)

#define PORT_0_4_MASK   (1 << 4)
#define PORT_0_5_MASK	(1 << 5)

/* Timer-related registers */
#define PCONP		REG32(0x400FC0C4)
#define PCONP_PCTIM0            (0x1 << 1)

#define PCLK_SEL0	REG32(0x400FC1A8)
#define PCLK_SEL0_TIMER0_MASK (~(0x3 << 2))
#define PCLK_SEL0_TIMER0_CCLK   (0x1 << 2)

#define T0TCR		REG32(0x40004004)
#define T0TCR_ENABLE             0x1
#define T0TCR_RESET              0x2

#define T0CTCR		REG32(0x40004070)
#define T0CTCR_TIMER             0x0

#define T0PR		REG32(0x4000400C)
#define T0MCR		REG32(0x40004014)

#define T0TC		REG32(0x40004008)
#define T0PC		REG32(0x40004010)

/* FreeRTOS knows better what's the CCLK frequency.  In our
   configuration, Timer 0 is driven directly by CCLK.
*/
#define T0_RES		configCPU_CLOCK_HZ

#endif

#ifdef GCC_ARM_CM4F_UN_LPC4357

/* Registers */
#define SFSP3_1		REG32(0x40086184) /* RX */
#define SFSP3_2		REG32(0x40086188) /* TX */

/* GPIO registers */
#define GPIO_DIR5	REG32(0x400F6014)
#define GPIO_PIN5	REG32(0x400F6114)
#define GPIO_SET5	REG32(0x400F6214)
#define GPIO_CLR5	REG32(0x400F6294)

#define PORT_5_8_MASK	(1 << 8) /* RX */
#define PORT_5_9_MASK	(1 << 9) /* TX */

/* Timer-related registers */
#define TIMER0_TCR	REG32(0x40084004)
#define		TCR_CEN		0x1
#define		TCR_CRST	0x2

#define TIMER0_TC	REG32(0x40084008)
#define TIMER0_PR	REG32(0x4008400C)
#define TIMER0_PC	REG32(0x40084010)
#define TIMER0_MCR	REG32(0x40084014)

#define TIMER0_CTCR	REG32(0x40084070)
#define		CTCR_TIMER	0x0

/* FreeRTOS knows better what's the CCLK frequency.  In our
   configuration, Timer 0 is driven directly by BASE_M4_CLK.
*/
#define TIMER0_RES	configCPU_CLOCK_HZ

#endif


/* -------------------------- Implementation --------------------------------*/

#ifdef GCC_ARM_CM3_UN_LPC1768

/* The Cortex-M3 port of FreeRTOS, which runs on the LPC1768 boards,
   uses the SYSTICK timer.  All general-purpose timers are free, we
   use Timer 0.
*/

/* --- Timestamping code by Tingting, exp_swtx.c 1.42 --- */

static void setup_ts(int prescaler)
{
    /* Set power/clock control bit for TIMER0 (already set @ reset) */
    PCONP |= PCONP_PCTIM0;

    /* Peripheral clock selection for TIMER0:
       CCLK (set to CCLK/4 @reset)
    */
    PCLK_SEL0 = (PCLK_SEL0 & PCLK_SEL0_TIMER0_MASK) | PCLK_SEL0_TIMER0_CCLK;

    /* Disable & reset the timer before manipulating it */
    T0TCR  = T0TCR_RESET;

    /* Set timer mode, clocked every rising PCLK edge */
    T0CTCR = T0CTCR_TIMER;

    /* Set prescaler of TIMER0 and print the frequency */
    {
	TRACE(0, ">>> With the prescaler at %d, nodeclock will be %gHz",
	      prescaler, (double)T0_RES/(double)prescaler);

	T0PR = prescaler - 1;
    }

    /* No action on match with any MR */
    T0MCR = 0;

    /* Enable the timer */
    T0TCR = T0TCR_ENABLE;
}

#define read_ts()  (T0TC)

/* --- Access to GPIO port exp_swtx.c 1.42 --- */

/* Set to HIGH --- recessive for the SN65HVD232 */
#define gpio_tx_rec()  (FIO0SET = PORT_0_5_MASK)

/* Set to LOW --- dominant for the SN65HVD232 */
#define gpio_tx_dom()  (FIO0CLR = PORT_0_5_MASK)

/* The difference between gpio_tx_pin() and gpio_rx_pin() is that:

   - gpio_tx_pin() returns the value we are driving the bus to
   - gpio_rx_pin() returns the actual bus value from the transceiver
*/

/* Read back value of tx pin --- 0: dominant, 1: recessive */
#define gpio_tx_pin()  ((FIO0PIN & PORT_0_5_MASK) ? 1 : 0)

/* Read bus value --- 0: dominant, 1: recessive */
#define gpio_rx_pin()  ((FIO0PIN & PORT_0_4_MASK) ? 1 : 0)

static void init_gpio(void)
{
    /* Set FIO0DIR<5> = 1, FIO0DIR<4> = 0,
         GPIO Port 0.5 must be an output, 0.4 an input.

       Set PINMODE0<9:8> = 10,
         GPIO Port 0.4 must have neither pull-up nor pull-down.
	 PINMODE0 is unused for outputs.

       Set PINMODE_OD0<5> = 0,
         GPIO Port 0.5 must not be open-drain.
	 PINMODE_OD0 is unused for inputs.

       Set output to recessive to not perturb the bus.

       Set PINSEL0<11:10> = 00, PINSEL0<9:8> = 00, this configures
         P0.5 and P0.4 as GPIO pins.  This is the last action to avoid
         connecting to the physical pins GPIO signals not configured
         in the right way.
    */
    FIO0DIR     = (FIO0DIR     & ~0x00000030) | 0x00000020;
    PINMODE0    = (PINMODE0    & ~0x00000300) | 0x00000200;
    PINMODE_OD0 = (PINMODE_OD0 & ~0x00000020) | 0x00000000;
    gpio_tx_rec();
    PINSEL0     = (PINSEL0     & ~0x00000F00) | 0x00000000;

    TRACE(0, ">>> gpio_tx_pin/rx_pin after init: %d/%d",
	  gpio_tx_pin(), gpio_rx_pin());
}


/* Delay before start of the nodeclock stream, in periods of TIMER0 */
#define INITIAL_NODECLOCK_DELAY 100

#define FIO2_DIR  (0x2009C040) /* (UM 10360 p.124) */
#define FIO2_SET  (0x2009C058)
#define FIO2_CLR  (0x2009C05C)

static volatile unsigned *fio2set   = (unsigned *)FIO2_SET;
static volatile unsigned *fio2clr   = (unsigned *)FIO2_CLR;

#define LED_ON(x)  *fio2clr = (1 << (x))
#define LED_OFF(x) *fio2set = (1 << (x))
#define YELLOW 12
#define GREEN  13

#endif

#ifdef GCC_ARM_CM4F_UN_LPC4357

/* --- Timer code --- */

/* The Cortex-M4F port of FreeRTOS, which runs on the LPC4357 boards,
   uses the SYSTICK timer.  All general-purpose timers are free, we
   use Timer 0.
*/

static void setup_ts(int prescaler)
{
    /* TIMER0 already receives power and clock @ reset.  The clock is
       always BASE_M4_CLK. */

    /* Disable & reset the timer before manipulating it */
    TIMER0_TCR  = TCR_CRST;

    /* Set timer mode, clocked every rising PCLK edge */
    TIMER0_CTCR = CTCR_TIMER;

    /* Set prescaler of TIMER0 and print the frequency */
    {
	TRACE(0, ">>> With the prescaler at %d, nodeclock will be %gHz",
	      prescaler, (double)TIMER0_RES/(double)prescaler);

	TIMER0_PR = prescaler - 1;
    }

    /* No action on match with any MR */
    TIMER0_MCR = 0;

    /* Enable the timer */
    TIMER0_TCR = TCR_CEN;
}

#define read_ts()  (TIMER0_TC)

/* --- Access to GPIO port --- */

/* Set to HIGH --- recessive for the SN65HVD232 */
#define gpio_tx_rec()  (GPIO_SET5 = PORT_5_9_MASK)

/* Set to LOW --- dominant for the SN65HVD232 */
#define gpio_tx_dom()  (GPIO_CLR5 = PORT_5_9_MASK)

/* The difference between gpio_tx_pin() and gpio_rx_pin() is that:

   - gpio_tx_pin() returns the value we are driving the bus to
   - gpio_rx_pin() returns the actual bus value from the transceiver
*/

/* Read back value of tx pin --- 0: dominant, 1: recessive */
#define gpio_tx_pin()  ((GPIO_PIN5 & PORT_5_9_MASK) ? 1 : 0)

/* Read bus value --- 0: dominant, 1: recessive */
#define gpio_rx_pin()  ((GPIO_PIN5 & PORT_5_8_MASK) ? 1 : 0)

static void init_gpio(void)
{
    /* Set GPIO5[8] as input and GPIO5[9] as output */
    GPIO_DIR5 &= ~PORT_5_8_MASK;
    GPIO_DIR5 |= PORT_5_9_MASK;

    /* Set TX to recessive */
    gpio_tx_rec();

    /* Set P3_1 (UM10503, Table 189) as:
       - GPIO5[8] (MODE=4)
       - Disable pull-down resistor (EPD=0)
       - Disable pull-up resistor (EPUN=1)
       - Slow slew rate (EHS=0)
       - Input buffer enabled (EZI=1)
       - Input glitch filter enabled (ZIF=0)
    */
    SFSP3_1 = 0x00000054;

    /* Set P3_2 as:
       - GPIO5[9] (MODE=4)
       - Disable pull-down resistor (EPD=0)
       - Disable pull-up resistor (EPUN=1)
       - Slow slew rate (EHS=0)
       - Input buffer disabled (EZI=0)
       - Input glitch filter enabled (ZIF=0)
    */
    SFSP3_2 = 0x00000014;

    TRACE(0, ">>> gpio_tx_pin/rx_pin after init: %d/%d",
	  gpio_tx_pin(), gpio_rx_pin());
}

/* Delay before start of the nodeclock stream, in periods of TIMER0 */
#define INITIAL_NODECLOCK_DELAY 100

/* --- LED (borrowed from the FreeRTOS port) --- */

#define GPIO_DIR7	REG32(0x400F601C) /* (UM10503, Chapter 18) */
#define GPIO_SET7	REG32(0x400F621C)
#define GPIO_CLR7	REG32(0x400F629C)

#define LED_ON(x)	GPIO_CLR7 = (1 << (x))
#define LED_OFF(x)	GPIO_SET7 = (1 << (x))
#define YELLOW 19
#define GREEN  17

#endif


static void data_req(struct CAN_XR_PMA *pma, int bus_level)
{
    TRACE(0, "CAN_XR_PMA_GPIO_Data_Req(%d)", bus_level);

    /* Set bus to bus_level.  Do it immediately because the PCS has
       already synchronized this call with the bit boundary (or, at
       least, it should).
    */
    if(bus_level)
	gpio_tx_rec();
    else
	gpio_tx_dom();
}

void CAN_XR_PMA_GPIO_Init(struct CAN_XR_PMA *pma, int prescaler)
{
    TRACE(0, "CAN_XR_PMA_GPIO_Init");

    pma->pcs = NULL;

    /* Connect GPIO pins to the CAN transceiver. */
    init_gpio();

    /* Set up nodeclock for use. */
    setup_ts(prescaler);

    pma->primitives.nodeclock_ind = NULL; /* Set by upper layer. */
    pma->primitives.data_req = data_req;

    pma->state.gpio.app_nodeclock_ind = NULL;
}

void CAN_XR_PMA_GPIO_Set_App_NodeClock_Ind(
    struct CAN_XR_PMA *pma, CAN_XR_PMA_NodeClock_Ind_t app_nodeclock_ind)
{
    pma->state.gpio.app_nodeclock_ind = app_nodeclock_ind;
}

void CAN_XR_PMA_GPIO_NodeClock_Ind(struct CAN_XR_PMA *pma)
{
    uint32_t x;

    TRACE(0, "CAN_XR_PMA_GPIO_NodeClock_Ind");

    x = read_ts() + INITIAL_NODECLOCK_DELAY;
    while(x != read_ts());

    TRACE(0, ">>> Initial delay/sync ok");

    while(1)
    {
	/* Synchronize with TIMER0, which is the source of nodeclock */
	while(x == read_ts());

	/* Sample bus level and generate a nodeclock indication for
	   the upper layer.  We assume that the whole chain of
	   indication callbacks takes less than one nodeclock period.
	*/
	if(pma->primitives.nodeclock_ind)
	    pma->primitives.nodeclock_ind(pma->pcs, gpio_rx_pin());

	/* Call GPIO-specific app_nodeclock_ind if registered */
	if(pma->state.gpio.app_nodeclock_ind)
	    pma->state.gpio.app_nodeclock_ind(pma->pcs, gpio_rx_pin());

	x++;

	/* Simple cycle overflow check.
	   Turn off the green led if we are late.
	*/
	if(x == read_ts())
	    LED_ON(GREEN);
	else
	    LED_OFF(GREEN);
    }
}
