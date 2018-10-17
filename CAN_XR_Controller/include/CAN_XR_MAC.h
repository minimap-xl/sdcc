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
   CAN XR Medium Access Control.  Specified in ISO 11898-1:2015(E)
   [1], Section 10.
*/

#ifndef CAN_XR_MAC_H
#define CAN_XR_MAC_H

#include <stdint.h>
#include "CAN_XR_LLC.h" /* For enum CAN_XR_Format */

/* Implementation-dependent part of the MAC state.  Currently we have
   only CAN_XR_MAC_Bare_Bones_State.

   TBD: Empty at this time.
*/
struct CAN_XR_MAC_Bare_Bones_State
{
    int tbd;
};

union CAN_XR_MAC_ID_State
{
    struct CAN_XR_MAC_Bare_Bones_State bare_bones;
};

enum CAN_XR_MAC_RX_FSM_State
{
    CAN_XR_MAC_RX_FSM_BUS_INTEGRATION, /* [1], 10.9.4 */
    CAN_XR_MAC_RX_FSM_IDLE,
    CAN_XR_MAC_RX_FSM_RX_IDENTIFIER,   /* [1], Figure 12 */
    CAN_XR_MAC_RX_FSM_RX_RTR,
    CAN_XR_MAC_RX_FSM_RX_IDE,
    CAN_XR_MAC_RX_FSM_RX_FDF,
    CAN_XR_MAC_RX_FSM_RX_DLC,
    CAN_XR_MAC_RX_FSM_RX_DATA,
    CAN_XR_MAC_RX_FSM_RX_CRC,
    CAN_XR_MAC_RX_FSM_RX_CDEL,
    CAN_XR_MAC_RX_FSM_RX_ACK,
    CAN_XR_MAC_RX_FSM_RX_ADEL,
    CAN_XR_MAC_RX_FSM_RX_EOF,
    CAN_XR_MAC_RX_FSM_ERROR
};

enum CAN_XR_MAC_TX_FSM_State
{
    CAN_XR_MAC_TX_FSM_IDLE,
    CAN_XR_MAC_TX_FSM_TX_IDENTIFIER,    /* Mimic the RX_FSM ones */
    CAN_XR_MAC_TX_FSM_TX_RTR,
    CAN_XR_MAC_TX_FSM_TX_IDE,
    CAN_XR_MAC_TX_FSM_TX_FDF,
    CAN_XR_MAC_TX_FSM_TX_DLC,
    CAN_XR_MAC_TX_FSM_TX_DATA,
    CAN_XR_MAC_TX_FSM_TX_CRC_LATCH,
    CAN_XR_MAC_TX_FSM_TX_CRC,
    CAN_XR_MAC_TX_FSM_TX_CDEL,
    CAN_XR_MAC_TX_FSM_TX_ACK,
    CAN_XR_MAC_TX_FSM_TX_ADEL,
    CAN_XR_MAC_TX_FSM_TX_EOF,
    CAN_XR_MAC_TX_FSM_TX_EOF_TAIL,
    CAN_XR_MAC_TX_FSM_TX_EXT_DATA,      /* For ext_tx_data_ind */
    CAN_XR_MAC_TX_FSM_TX_EXT_TAIL,      /* After last ext_tx_data_ind */
    CAN_XR_MAC_TX_FSM_ERROR
};

/* Overall MAC state.  Made up of an implementation-independent part
   (defined directly in this structure) and an
   implementation-dependent part (members of the id union).
*/
struct CAN_XR_MAC_State
{
    enum CAN_XR_MAC_RX_FSM_State rx_fsm_state;

    int bus_integration_counter; /* Bus integration */

    int nc_bits; /* De-stuffing and CRC calculation */
    int nc_pol;
    uint16_t crc;
    int field_bits;
    int bus_bits;
    int de_stuffed_bits;

    uint32_t rx_identifier; /* Buffers for reassembled frame */
    int rx_rtr;
    int rx_ide;
    int rx_fdf;
    int rx_dlc;
    uint8_t rx_byte;
    int rx_byte_index;
    uint8_t rx_data[8];

    enum CAN_XR_MAC_TX_FSM_State tx_fsm_state;

    int data_req_pending;
    uint32_t tx_identifier;
    enum CAN_XR_Format tx_format;
    int tx_dlc;
    uint8_t tx_data[8];
    int tx_byte_index;
    int tx_bit_count;
    uint32_t tx_shift_reg;

    union CAN_XR_MAC_ID_State id;
};

struct CAN_XR_MAC;
struct CAN_XR_LLC;

enum CAN_XR_MAC_Tx_Status {
    CAN_XR_MAC_TX_STATUS_SUCCESS = 0,
    CAN_XR_MAC_TX_STATUS_NO_SUCCESS
};

/* MAC primitives for acknowledged data transfer.  Arguments as
   specified in the standard, plus this and ts (ts only for
   indications and confirmations).  Types inferred from the standard,
   ISO C9899 stdint.h.

   enum CAN_XR_Format defined in CAN_XR_LLC.h.
*/
typedef void (* CAN_XR_MAC_Data_Req_t)(
    struct CAN_XR_MAC *this,
    uint32_t identifier,
    enum CAN_XR_Format format, int dlc, uint8_t *data);

typedef void (* CAN_XR_MAC_Data_Ind_t)(
    struct CAN_XR_LLC *this, unsigned long ts,
    uint32_t identifier,
    enum CAN_XR_Format format, int dlc, uint8_t *data);

typedef void (* CAN_XR_MAC_Data_Conf_t)(
    struct CAN_XR_LLC *this, unsigned long ts,
    uint32_t identifier,
    enum CAN_XR_MAC_Tx_Status transmission_status);

/* MAC Remote_Req, Remote_Ind, Remote_Conf unsupported */
/* MAC OVLD_Req, OVLD_Ind, OVLD_Conf unsupported */

/* Additional MAC primitive to extend the functionality of the base
   CAN controller.  Per Tingting's suggestion, when this primitive is
   set, the base CAN controller invokes it on every sampling point
   within the data field through the transmit automaton.  The exact
   position within the data field is make available to the primitive
   through the receive and transmit automaton state within 'this'.
   'input_unit' is the sampled bit at the current position in the data
   field.
*/
typedef void (* CAN_XR_MAC_Ext_Tx_Data_Ind_t)(
    struct CAN_XR_MAC *this, unsigned long ts, int input_unit);

/* MAC primitives. */
struct CAN_XR_MAC_Primitives
{
    /* ISO 11898 primitives */
    CAN_XR_MAC_Data_Req_t data_req;
    CAN_XR_MAC_Data_Ind_t data_ind;
    CAN_XR_MAC_Data_Conf_t data_conf;

    /* Additional primitives for internal use */
    CAN_XR_MAC_Ext_Tx_Data_Ind_t ext_tx_data_ind;
};

struct CAN_XR_MAC
{
    struct CAN_XR_LLC *llc; /* Link to the upper protocol layer. */
    struct CAN_XR_PCS *pcs; /* Link to the lower protocol layer. */

    struct CAN_XR_MAC_State state;
    struct CAN_XR_MAC_Primitives primitives;
};

/* Initialize the part common to all implementations of 'mac', linking
   it with 'pcs' and also registering default upcall primitives.
   Invoked implicitly by the implementation-specific initialization
   functions below.
*/
void CAN_XR_MAC_Common_Init(
    struct CAN_XR_MAC *mac,
    struct CAN_XR_PCS *pcs);

/* Implementation-specific MAC initialization functions. */
void CAN_XR_MAC_Bare_Bones_Init(
    struct CAN_XR_MAC *mac,
    struct CAN_XR_PCS *pcs);

/* Set the pointer to the upper layer in 'mac' */
void CAN_XR_MAC_Set_LLC(struct CAN_XR_MAC *mac, struct CAN_XR_LLC *llc);

/* Register the data_ind and data_conf upcall primitives in 'mac'. */
void CAN_XR_MAC_Set_Data_Ind(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Data_Ind_t data_ind);

void CAN_XR_MAC_Set_Data_Conf(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Data_Conf_t data_conf);

/* Register the ext_tx_data_ind primitive in 'mac'. */
void CAN_XR_MAC_Set_Ext_Tx_Data_Ind(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Ext_Tx_Data_Ind_t ext_tx_data_ind);

/* Invoke the data_req primitive in 'mac'. */
void CAN_XR_MAC_Data_Req(
    struct CAN_XR_MAC *mac,
    uint32_t identifier, enum CAN_XR_Format format, int dlc, uint8_t *data);

/* Dump the MAC state on stderr. */
void CAN_XR_MAC_Dump(
    const char *desc, const struct CAN_XR_MAC *mac);

#endif
