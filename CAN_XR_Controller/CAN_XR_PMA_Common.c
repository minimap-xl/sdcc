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

/* This file implements the functions common to all PMAs, mainly
   setters and primitive invocation functions.
*/

#include <stdlib.h>
#include "CAN_XR_PMA.h"

/* Link the PCS pointer of 'pma' to point to 'pcs'. */
void CAN_XR_PMA_Set_PCS(struct CAN_XR_PMA *pma, struct CAN_XR_PCS *pcs)
{
    pma->pcs = pcs;
}

/* Set the nodeclock_ind callback of 'pma' to 'nodeclock_ind'. */
void CAN_XR_PMA_Set_NodeClock_Ind(
    struct CAN_XR_PMA *pma, CAN_XR_PMA_NodeClock_Ind_t nodeclock_ind)
{
    pma->primitives.nodeclock_ind = nodeclock_ind;
}

/* Invoke the data_req primitive of 'pma' to drive the bus to 'bus_level' */
void CAN_XR_PMA_Data_Req(struct CAN_XR_PMA *pma, int bus_level)
{
    if(pma->primitives.data_req)
	pma->primitives.data_req(pma, bus_level);
}
