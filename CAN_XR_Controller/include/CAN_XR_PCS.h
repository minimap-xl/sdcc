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

/* This header contains the data structures needed by the CAN XR
   Physical Coding Sub-layer.  It is currently the same as the regular
   CAN PCS specified in ISO 11898-1:2015(E) [1], Section 11.1.
*/

#ifndef CAN_XR_PCS_H
#define CAN_XR_PCS_H

/* [1], Table 8. */
struct CAN_XR_PCS_Bit_Time_Parameters
{
    int prescaler_m; /* [ 1, 32] -- [1], Table 8. */
    int sync_seg;    /* [ 1,  1] */
    int prop_seg;    /* [ 1,  8] */
    int phase_seg1;  /* [ 1,  8] */
    int phase_seg2;  /* [ 2,  8] */
    int sjw;         /* [ 1,  4] */
};

/* PCS state information. */
struct CAN_XR_PCS_State
{
    unsigned long nodeclock_ts; /* Timestamp, nodeclock units */
    int prescaler_m_cnt; /* Prescaler m, count */
    int quantum_m_cnt; /* Quantum m counter, within a bit */
    int quanta_per_bit; /* Derived from parameters */
    int prev_bus_level; /* Previous bus level for edge detection */
    int prev_sample; /* Bus @ previous sample point for edge detection */
    int sync_inhibit; /* Sync inhibit per [1] 11.3.2.1 a) */
    int hard_sync_allowed; /* Set by MAC to allow/forbid hard sync */
    int output_unit_buf; /* Buffer for output_unit to be / being sent */
    int sending_level; /* Level being sent, resync'd @ bit boundary */
};

struct CAN_XR_PCS;
struct CAN_XR_MAC;

/* PCS function invoked upon each node clock edge with the sampled bus
   level. [1] Sections 11.2.2 and 11.2.3.
*/
typedef void (* CAN_XR_PCS_Data_Ind_t)(
    struct CAN_XR_MAC *this, unsigned long ts, int input_unit);

typedef void (* CAN_XR_PCS_Data_Req_t)(
    struct CAN_XR_PCS *this, int output_unit);

/* PCS primitives. */
struct CAN_XR_PCS_Primitives
{
    CAN_XR_PCS_Data_Ind_t data_ind;
    CAN_XR_PCS_Data_Req_t data_req;
};

struct CAN_XR_PCS
{
    struct CAN_XR_MAC *mac; /* Link to the upper protocol layer. */
    struct CAN_XR_PMA *pma; /* Link to the lower protocol layer. */

    struct CAN_XR_PCS_Bit_Time_Parameters parameters;
    struct CAN_XR_PCS_State state;
    struct CAN_XR_PCS_Primitives primitives;
};

/* Initialize 'pcs', linking it with 'pma' and also registering the
   upcall primitives.  Parameters are passed by reference but copied
   into place, so we can (and should) use const here.
*/
void CAN_XR_PCS_Init(
    struct CAN_XR_PCS *pcs,
    const struct CAN_XR_PCS_Bit_Time_Parameters *parameters,
    struct CAN_XR_PMA *pma);

/* Set the pointer to the upper layer in 'pcs'. */
void CAN_XR_PCS_Set_MAC(struct CAN_XR_PCS *pcs, struct CAN_XR_MAC *mac);

/* Register the data_ind upcall primitive in 'pcs'. */
void CAN_XR_PCS_Set_Data_Ind(
    struct CAN_XR_PCS *pcs, CAN_XR_PCS_Data_Ind_t data_ind);

/* Invoke the data_req primitive in 'pcs'. */
void CAN_XR_PCS_Data_Req(struct CAN_XR_PCS *pcs, int output_unit);

/* Set the hard_sync_allowed flag to allow/disallow hard
   synchronization.  This unconfirmed request is not specified in the
   standard but it's apparently needed by [1] 11.3.2.1 c), in which
   the choice between hard and soft synchronization depends on the MAC
   state.

   TBD: This should be in the primitives vector but it acceptable for
   the time being since we have only one PCS implementation.
*/
void CAN_XR_PCS_Hard_Sync_Allowed_Req(
    struct CAN_XR_PCS *pcs, int hard_sync_allowed);

#endif
