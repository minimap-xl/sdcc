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
   CAN XR Physical Medium Attachment.  It is currently the same as the
   regular CAN PMA.
*/

#ifndef CAN_XR_PMA_H
#define CAN_XR_PMA_H

struct CAN_XR_PMA;
struct CAN_XR_PCS;

/* PMA primitive invoked upon each node clock edge.  Arguments are the
   target PCS data structure and the sampled bus level at the edge.
*/
typedef void (* CAN_XR_PMA_NodeClock_Ind_t)(
    struct CAN_XR_PCS *pcs, int bus_level);

/* PMA primitive invoked to drive the transceiver.  Arguments are the
   target PMA data structure and the requested bus level.
*/
typedef void (* CAN_XR_PMA_Data_Req_t)(
    struct CAN_XR_PMA *pma, int bus_level);

/* PMA state, it depends on the PMA implementation.
*/
struct CAN_XR_PMA_Sim_State
{
    int rx_bus_level; /* Bus level from simulated transceiver. */
    int tx_bus_level; /* Bus level from Data_Req to simulated
			 transceiver. */
};

struct CAN_XR_PMA_GPIO_State
{
    /* App-layer nodeclock indication.  TBD: This is a bit forceful
       because this data structure should contain state information
       rather than primitives.
    */
    CAN_XR_PMA_NodeClock_Ind_t app_nodeclock_ind;
};

union CAN_XR_PMA_State
{
    struct CAN_XR_PMA_Sim_State sim;
    struct CAN_XR_PMA_GPIO_State gpio;
};

/* PMA primitives. */
struct CAN_XR_PMA_Primitives
{
    CAN_XR_PMA_NodeClock_Ind_t nodeclock_ind;
    CAN_XR_PMA_Data_Req_t data_req;
};

struct CAN_XR_PMA
{
    struct CAN_XR_PCS *pcs; /* Link to the upper protocol layer. */
    /* No link to the lower protocol layer. */

    union  CAN_XR_PMA_State state;
    struct CAN_XR_PMA_Primitives primitives;
};

/* Set the pointer to the upper layer in 'pma' */
void CAN_XR_PMA_Set_PCS(struct CAN_XR_PMA *pma, struct CAN_XR_PCS *pcs);

/* Register the nodeclock_ind upcall primitive in 'pma' */
void CAN_XR_PMA_Set_NodeClock_Ind(
    struct CAN_XR_PMA *pma, CAN_XR_PMA_NodeClock_Ind_t nodeclock_ind);

/* Invoke the data_req primitive in 'pma' */
void CAN_XR_PMA_Data_Req(struct CAN_XR_PMA *pma, int bus_level);

#endif
