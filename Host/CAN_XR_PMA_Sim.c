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

/* Implementation of a simulated CAN XR PMA on the host.  This PMA
   only simulates the transceiver/bus combination.  When a
   nodeclock_ind is triggered, it combines with a wired AND the
   rx_bus_level passed as argument to nodeclock_ind itself with the
   stored tx_bus_level from the last call to data_req.  The result is
   propagated to the upper layer as a nodeclock_ind.
*/

#include <stdio.h>
#include <stdlib.h>
#include "CAN_XR_PMA_Sim.h"
#include "CAN_XR_Trace.h"

static void data_req(struct CAN_XR_PMA *pma, int bus_level)
{
    TRACE(0, "CAN_XR_PMA_Sim_Data_Req(%d)", bus_level);
    pma->state.sim.tx_bus_level = bus_level;
}

void CAN_XR_PMA_Sim_Init(struct CAN_XR_PMA *pma)
{
    TRACE(0, "CAN_XR_PMA_Sim_Init");

    pma->pcs = NULL;

    /* Bus is initially recessive on both rx/tx sides. */
    pma->state.sim.rx_bus_level = 1;
    pma->state.sim.tx_bus_level = 1;

    pma->primitives.nodeclock_ind = NULL; /* To be set by upper
					     layer. */
    pma->primitives.data_req = data_req;
}

void CAN_XR_PMA_Sim_NodeClock_Ind(struct CAN_XR_PMA *pma, int rx_bus_level)
{
    TRACE(0, "CAN_XR_PMA_Sim_NodeClock_Ind(%d)", rx_bus_level);

    pma->state.sim.rx_bus_level = rx_bus_level;

    /* Propagate indication to the upper layer. */
    if(pma->primitives.nodeclock_ind)
	pma->primitives.nodeclock_ind(
	    pma->pcs,
	    pma->state.sim.rx_bus_level & pma->state.sim.tx_bus_level);
}
