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

/* This header contains the declarations and definitions needed by the
   GPIO-based CAN XR PMA for the LPC1768 and LPC4357 boards.
*/

#ifndef CAN_XR_PMA_GPIO_H
#define CAN_XR_PMA_GPIO_H

#include <stdint.h>
#include <CAN_XR_PMA.h>

/* Initialize pma with an instance of CAN_XR_PMA_GPIO.
   The nodeclock is set to CCLK / prescaler.
*/
void CAN_XR_PMA_GPIO_Init(struct CAN_XR_PMA *pma, int prescaler);

/* Register the app_nodeclock_ind upcall primitive in 'pma'.  It is
   invoked on every nodeclock cycle.
*/
void CAN_XR_PMA_GPIO_Set_App_NodeClock_Ind(
    struct CAN_XR_PMA *pma, CAN_XR_PMA_NodeClock_Ind_t app_nodeclock_ind);

/* Trigger an infinite stream of NodeClock indications in
   CAN_XR_PMA_GPIO.  This function does not return.  Unlike the Sim
   PMA it does not take a (simulated) bus level as input, because it
   samples the CAN bus for real.
*/
void CAN_XR_PMA_GPIO_NodeClock_Ind(struct CAN_XR_PMA *pma);

#endif
