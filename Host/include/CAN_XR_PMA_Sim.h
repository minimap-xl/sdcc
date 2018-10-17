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
   simulated CAN XR PMA that runs on the host.
*/

#ifndef CAN_XR_PMA_SIM_H
#define CAN_XR_PMA_SIM_H

#include <CAN_XR_PMA.h>

/* Initialize pma with an instance of CAN_XR_PMA_Sim. */
void CAN_XR_PMA_Sim_Init(struct CAN_XR_PMA *pma);

/* Trigger a NodeClock indication in CAN_XR_PMA_Sim. */
void CAN_XR_PMA_Sim_NodeClock_Ind(struct CAN_XR_PMA *pma, int rx_bus_level);

#endif
