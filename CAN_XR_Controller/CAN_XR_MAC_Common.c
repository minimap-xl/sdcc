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

/* This file implements the functions common to all MACs, for
   instance, setters and primitive invocation functions.

   Full TRACE at level 2.
   Errors are emitted at TRACE level 9.
*/

#include <stdlib.h>
#include <string.h>
#include "CAN_XR_PCS.h"
#include "CAN_XR_MAC.h"
#include "CAN_XR_Trace.h"


#define shift_in(v, b) (((v) << 1) | ((b) & 0x1))

/* Prepare v, which is n_bits wide (<= 32) for MSb-first shifting.
   Return the prepared value as uint32_t.
*/
#define shift_prepare(v, n_bits) ((uint32_t)(v) << (32 - (n_bits)))

/* Store into b the bit of v to be transmitted.  Shift v to put the
   next bit into position.  v is assumed to be uint32_t.
*/
#define shift_out(b, v)				\
    do {					\
	b = ((v) & 0x80000000) ? 1 : 0;		\
	v <<= 1;				\
    } while(0)


/* MAC_Data.Request primitive invoked by upper later (typically LLC) to
   request the transmission of a frame.
*/
static void mac_data_req(
    struct CAN_XR_MAC *mac,
    uint32_t identifier, enum CAN_XR_Format format, int dlc, uint8_t *data)
{
    TRACE(2, "MAC Common::mac_data_req(%lu, ...)", (unsigned long)identifier);

    /* Check if there is a pending transmission request already, of
       any kind.  This would be an LLC handshake error.
    */
    if(mac->state.data_req_pending)
    {
	if(mac->primitives.data_conf)
	    mac->primitives.data_conf(
		mac->llc, 0, identifier, CAN_XR_MAC_TX_STATUS_NO_SUCCESS);
    }

    else
    {
	/* Check format, complain immediately if it is unsupported */
	switch(format)
	{
	case CAN_XR_FORMAT_CBFF:
	    /* Save arguments in MAC state for later use. */
	    mac->state.tx_identifier = identifier;
	    mac->state.tx_format = format; /* TBD: We'll support more */
	    mac->state.tx_dlc = dlc;
	    /* Clear tx_data completely, then fill the right amount */
	    memset(mac->state.tx_data, 0, sizeof(mac->state.tx_data));
	    memcpy(mac->state.tx_data, data, dlc);
	    mac->state.data_req_pending = 1;
	    break;

	default:
	    /* Unsupported format.  Confirm with
	       CAN_XR_MAC_TX_STATUS_NO_SUCCESS.
	       TBD: ts not in scope.  Use 0 for the time being.
	    */
	    if(mac->primitives.data_conf)
		mac->primitives.data_conf(
		    mac->llc, 0, identifier, CAN_XR_MAC_TX_STATUS_NO_SUCCESS);
	    break;
	}
    }
}

#define CRC_POLYNOMIAL 0x4599 /* It's monic, MSb omitted */

/* From [1], 10.4.2.6.  Update crc considering the LSb of nxtbit.  It
   is meant to be correct, not fast.
*/
static uint16_t crc_nxtbit(uint16_t crc, uint16_t nxtbit)
{
    int crcnxt = ((crc & 0x4000) >> 14) ^ nxtbit;
    crc = (crc << 1) & 0x7FFF; /* Shift in 0 */
    if(crcnxt)  crc ^= CRC_POLYNOMIAL;
    return crc;
}

/* Static primitive invoked on all de-stuffed bits after SOF while the
   MAC is receiving.  It performs CRC calculation using crc_nextibt
   and deserialization and recompiling of the frame structure, [1]
   10.3.3.

   TBD:

   - We currently support only CBFF
   - We don't implement OF
   - We don't handle intermission
   - We allow hard synchronization even in the first bit of intermission
*/
static void de_stuffed_data_ind(
    struct CAN_XR_MAC *mac, unsigned long ts, int input_unit)
{
    TRACE(2, "MAC @%lu Common::de_stuffed_data_ind(%d)", ts, input_unit);

    switch(mac->state.rx_fsm_state)
    {
    case CAN_XR_MAC_RX_FSM_IDLE:
	/* SOF received.  Process it, then start receiving the
	   identifier.  FSM state transitions related to framing (so,
	   not error-related transitions) are taken here and not in
	   pcs_data_ind (even though it makes use of the state)
	   because they must consider the de-stuffed data stream.
	*/
	TRACE(2, "MAC @%lu SOF received (%d)", ts, input_unit);

	/* Disable hard synchronization per [1] 11.3.2.1 c) */
	CAN_XR_PCS_Hard_Sync_Allowed_Req(mac->pcs, 0);

	/* Initialize CRC and start receiving the identifier field */
	mac->state.crc = crc_nxtbit(0x0000, input_unit);
	mac->state.field_bits = 10;
	mac->state.rx_identifier = 0;
	mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_IDENTIFIER;
	break;

    case CAN_XR_MAC_RX_FSM_RX_IDENTIFIER:
	/* .field_bits indicates the current bit number within the
	   current field, the LSb is zero.  Together with .rx_fsm_state
	   it determines where we believe to be within a frame.
	*/
	TRACE(2, "MAC @%lu identifier bit #%d (%d)",
	      ts, mac->state.field_bits, input_unit);

	/* Within a field, the MSB shall be transmitted first.  Order
	   of bit transmission, [1] 10.8. */
	mac->state.rx_identifier =
	    shift_in(mac->state.rx_identifier, input_unit);

	/* Update CRC and switch to the control field if needed. */
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);
	if(mac->state.field_bits-- == 0)
	{
	    TRACE(2, "MAC @%lu rx_identifier=%lu", ts,
		  (unsigned long)mac->state.rx_identifier);

	    mac->state.field_bits = 1;
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_RTR;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_RTR:
	TRACE(2, "MAC @%lu RTR bit (%d)", ts, input_unit);

	/* TBD: RTR bit unchecked, shall be dominant because we do not
	   support RTR frames at this time.
	*/
	mac->state.rx_rtr = input_unit;
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);
	mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_IDE;
	break;

    case CAN_XR_MAC_RX_FSM_RX_IDE:
	TRACE(2, "MAC @%lu IDE bit (%d)", ts, input_unit);
	mac->state.rx_ide = input_unit;
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);

	/* TBD: We currently support only CBFF, it must be IDE=0. */
	if(mac->state.rx_ide != 0)
	{
	    TRACE(2, "MAC @%lu xEFF formats unsupported", ts);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_FDF;

	break;

    case CAN_XR_MAC_RX_FSM_RX_FDF:
	TRACE(2, "MAC @%lu FDF bit (%d)", ts, input_unit);
	mac->state.rx_fdf = input_unit;
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);

	/* TBD: We currently support only CBFF, it must be FDF=0. */
	if(mac->state.rx_ide != 0)
	{
	    TRACE(2, "MAC @%lu FBFF format unsupported", ts);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else
	{
	    mac->state.field_bits= 3;
	    mac->state.rx_dlc = 0;
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_DLC;
	}

	break;

    case CAN_XR_MAC_RX_FSM_RX_DLC:
	TRACE(2, "MAC @%lu DLC bit #%d (%d)",
	      ts, mac->state.field_bits, input_unit);

	mac->state.rx_dlc = shift_in(mac->state.rx_dlc, input_unit);
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);
	if(mac->state.field_bits-- == 0)
	{
	    TRACE(2, "MAC @%lu rx_dlc=%d", ts, mac->state.rx_dlc);

	    /* Calculate how many bits the data field has.  It may be
	       empty, skip directly to the CRC in that case.
	    */
	    mac->state.field_bits =
		8 * ((mac->state.rx_dlc > 8) ? 8 : mac->state.rx_dlc) - 1;

	    if(mac->state.field_bits > 0)
	    {
		/* Clear the whole .rx_data[] buffer, initialize byte
		   buffer .rx_byte and byte index .rx_byte_index
		   within rx_data[]
		*/
		memset(mac->state.rx_data, 0, sizeof(mac->state.rx_data));
		mac->state.rx_byte = 0;
		mac->state.rx_byte_index = 0;
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_DATA;

		/* If an ext_tx_data_ind primitive has been registered
		   to extend the base MAC, switch the transmit
		   automaton to the appropriate state.

		   To help the ext_tx_data_ind primitive, in case it
		   has to start transmitting immediately, we also set
		   tx_byte_index and prepare tx_data[0] for
		   transmission in tx_shift_reg.
		*/
		if(mac->primitives.ext_tx_data_ind)
		{
		    mac->state.tx_byte_index = 0;
		    mac->state.tx_shift_reg = shift_prepare(mac->state.tx_data[0], 8);
		    mac->state.tx_bit_count = mac->state.field_bits;
		    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_EXT_DATA;
		}
	    }

	    else
	    {
		mac->state.field_bits = 14;
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_CRC;
	    }
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_DATA:
	TRACE(2, "MAC @%lu data bit #%d (%d)",
	      ts, mac->state.field_bits, input_unit);

	mac->state.rx_byte = shift_in(mac->state.rx_byte, input_unit);
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);
	if(mac->state.field_bits % 8 == 0)
	{
	    /* Byte boundary, move reassembled byte from .rx_byte into
	       .rx_data[] at the right position.  Even though bits
	       within a byte are transmitted big-endian, bytes within
	       the data field are transmitted little-endian.  See [1],
	       Figures 12-17.Interesting.

	       TBD: Are we sure we don't read rx_data[8] in this way?
	    */
	    mac->state.rx_data[mac->state.rx_byte_index++] = mac->state.rx_byte;
	    mac->state.rx_byte = 0;
	}

	if(mac->state.field_bits-- == 0)
	{
	    mac->state.field_bits = 14;
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_CRC;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_CRC:
	TRACE(2, "MAC @%lu CRC bit #%d (%d)",
	      ts, mac->state.field_bits, input_unit);

	/* No need to store the CRC being received anywhere, just keep
	   going with the CRC calculation.  Due to a well-known
	   property, if the received CRC was ok, the calculated CRC
	   must be 0 at the end.  Wow, magic! :)
	*/
	mac->state.crc = crc_nxtbit(mac->state.crc, input_unit);
	if(mac->state.field_bits-- == 0)
	{
	    if(mac->state.crc != 0)
	    {
		TRACE(9, ">>> MAC @%lu CRC error id=%lu dlc=%d", ts,
		      (unsigned long)mac->state.rx_identifier,
		      mac->state.rx_dlc);
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	    }

	    else
		/* CRC Ok */
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_CDEL;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_CDEL:
	TRACE(2, "MAC @%lu CDEL bit (%d)", ts, input_unit);

	if(input_unit != 1)
	{
	    TRACE(9, ">>> MAC @%lu CDEL form error", ts);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else
	{
	    /* Acknowledge the frame by starting the transmission of a
	       dominant ACK bit at the next bit boundary.  PCS takes
	       care of bit boundary synchronization.

	       TBD: If we are transmitting, we should not acknowledge
	       our own frame.  At this time we data data_req_pending,
	       but we don't have data_req_active, we probably need it.

	       TBD: This is also a (strong) hint at how to implement
	       SRR mode.
	    */
	    CAN_XR_PCS_Data_Req(mac->pcs, 0);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_ACK;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_ACK:
	TRACE(2, "MAC @%lu ACK bit (%d)", ts, input_unit);

	if(input_unit != 0)
	{
	    TRACE(9, ">>> MAC @%lu ACK bit error", ts);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else
	{
	    /* Stop transmitting the dominant ACK bit */
	    CAN_XR_PCS_Data_Req(mac->pcs, 1);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_ADEL;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_ADEL:
	TRACE(2, "MAC @%lu ADEL bit (%d)", ts, input_unit);

	if(input_unit != 1)
	{
	    TRACE(9, ">>> MAC @%lu ADEL form error", ts);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else
	{
	    mac->state.field_bits = 6;
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_RX_EOF;
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_EOF:
	TRACE(2, "MAC @%lu EOF bit #%d (%d)",
	      ts, mac->state.field_bits, input_unit);

	/* The EOF field consists of 7 recessive bits ([1] 10.4.2.8).
	   However, "the value of the last bit of EOF shall not
	   inhibit frame validation and a dominant value shall not
	   lead to a form error.  A receiver that detects a dominant
	   bit at the last bit of EOF shall respond with an OF" ([1]
	   10.7).

	   TBD: We don't implement OF, so we simply ignore the 7th bit
	   of EOF for the time being.
	*/
	if(input_unit != 1 && mac->state.field_bits != 0)
	{
	    TRACE(9, ">>> MAC @%lu EOF bit #%d form error", ts,
		  mac->state.field_bits);

	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	}

	else if(mac->state.field_bits-- == 0)
	{
	    TRACE(2, "MAC @%lu Frame OK id=%lu dlc=%d", ts,
		  (unsigned long)mac->state.rx_identifier,
		  mac->state.rx_dlc);

	    /* We got a frame, eventually.  Generate Data_Ind for LLC. */
	    if(mac->primitives.data_ind)
		mac->primitives.data_ind(
		    mac->llc, ts, mac->state.rx_identifier,
		    CAN_XR_FORMAT_CBFF, mac->state.rx_dlc, mac->state.rx_data);

	    /* TBD: We don't handle intermission properly.  Moreover,
	       we shouldn't allow hard synchronization in the first
	       bit of intermission 11.3.2.1 c)
	    */
	    CAN_XR_PCS_Hard_Sync_Allowed_Req(mac->pcs, 1);
	    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_IDLE;
	}
	break;

    default:
	TRACE(9,
	      ">>> MAC @%lu Common::de_stuffed_data_ind invalid rx_fsm_state %d",
	      ts, mac->state.rx_fsm_state);
	break;
    }
}

static void tx_processing_ind(
    struct CAN_XR_MAC *mac, unsigned long ts, int input_unit)
{
    int bit;

    TRACE(2, "MAC @%lu Common::tx_processing_ind(%d)", ts, input_unit);

    switch(mac->state.tx_fsm_state)
    {
    case CAN_XR_MAC_TX_FSM_IDLE:
	/* Transmit SOF.  At the next sample point, this will also
	   cause the rx automaton to exit from the idle state.
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 0);

	/* Prepare for transmitting the identifier */
	mac->state.tx_shift_reg = shift_prepare(mac->state.tx_identifier, 11);
	mac->state.tx_bit_count = 10;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_IDENTIFIER;
	break;

    case CAN_XR_MAC_TX_FSM_TX_IDENTIFIER:
	shift_out(bit, mac->state.tx_shift_reg);
	CAN_XR_PCS_Data_Req(mac->pcs, bit);

	if(mac->state.tx_bit_count-- == 0)
	    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_RTR;
	break;

    case CAN_XR_MAC_TX_FSM_TX_RTR:
	/* TBD: RTR is dominant in CBFF frames, other frame formats
	   are unsupported for now.
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 0);
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_IDE;
	break;

    case CAN_XR_MAC_TX_FSM_TX_IDE:
	/* TBD: IDE is dominant in CBFF frames, other frame formats
	   are unsupported for now.
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 0);
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_FDF;
	break;

    case CAN_XR_MAC_TX_FSM_TX_FDF:
	/* TBD: FDF is dominant in CBFF frames, other frame formats
	   are unsupported for now.
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 0);
	mac->state.tx_shift_reg = shift_prepare(mac->state.tx_dlc, 4);
	mac->state.tx_bit_count = 3;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_DLC;
	break;

    case CAN_XR_MAC_TX_FSM_TX_DLC:
	shift_out(bit, mac->state.tx_shift_reg);
	CAN_XR_PCS_Data_Req(mac->pcs, bit);

	if(mac->state.tx_bit_count-- == 0)
	{
	    if(mac->state.tx_dlc > 0)
	    {
		mac->state.tx_byte_index = 0;
		mac->state.tx_shift_reg = shift_prepare(mac->state.tx_data[0], 8);
		mac->state.tx_bit_count =
		    8 * ((mac->state.tx_dlc > 8) ? 8 : mac->state.tx_dlc) - 1;
		mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_DATA;
	    }

	    else
		mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_CRC_LATCH;
	}
	break;

    case CAN_XR_MAC_TX_FSM_TX_DATA:
	shift_out(bit, mac->state.tx_shift_reg);
	CAN_XR_PCS_Data_Req(mac->pcs, bit);

	if(mac->state.tx_bit_count == 0)
	    /* Done with data bits */
	    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_CRC_LATCH;

	else if(mac->state.tx_bit_count-- % 8 == 0)
	    /* Byte boundary, switch to the next */
	    mac->state.tx_shift_reg =
		shift_prepare(mac->state.tx_data[++mac->state.tx_byte_index], 8);
	break;

    case CAN_XR_MAC_TX_FSM_TX_CRC_LATCH:
	/* At this sampling point, the receiver has kindly performed
	   the CRC calculation up to the last bit of the data field.
	   The result is stored in mac->state.crc, so we can latch it
	   into tx_shift_reg and start transmitting it.
	*/
	mac->state.tx_shift_reg = shift_prepare(mac->state.crc, 15);
	mac->state.tx_bit_count = 14;

	/* The first bit of the CRC must be transmitted at the next
	   bit boundary, so issue the Data_Req here, without state
	   transitions in between.  tx_bit_count is adjusted
	   accordingly.
	*/
	shift_out(bit, mac->state.tx_shift_reg);
	CAN_XR_PCS_Data_Req(mac->pcs, bit);
	mac->state.tx_bit_count--;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_CRC;
	break;

    case CAN_XR_MAC_TX_FSM_TX_CRC:
	shift_out(bit, mac->state.tx_shift_reg);
	CAN_XR_PCS_Data_Req(mac->pcs, bit);

	if(mac->state.tx_bit_count-- == 0)
	    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_CDEL;
	break;

    case CAN_XR_MAC_TX_FSM_TX_CDEL:
	TRACE(2, ">>> MAC @%lu Sending CDEL", ts);
	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_ACK;
	break;

    case CAN_XR_MAC_TX_FSM_TX_ACK:
	/* The ACK bit is sent recessive and shall be sampled
	   dominant, otherwise an ACK error occurs [1] 10.4.2.7.

	   Here we issue
	   CAN_XR_PCS_Data_Req(mac->pcs, 1);

	   because we don't want to self-acknowledge the frame, but
	   the receive automaton just asked to transmit a dominant ACK
	   at the next bit boundary if it received the frame
	   successfully.
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_ADEL;
	break;

    case CAN_XR_MAC_TX_FSM_TX_ADEL:
	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	mac->state.tx_bit_count = 6;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_EOF;
	break;

    case CAN_XR_MAC_TX_FSM_TX_EOF:
	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	if(mac->state.tx_bit_count-- == 0)
	    /* Delay the return to TX_FSM_IDLE by one bit, to have
	       time to sample the last bit of EOF.
	    */
	    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_EOF_TAIL;
	break;

    case CAN_XR_MAC_TX_FSM_TX_EOF_TAIL:
	/* At this sampling point, the last bit of EOF has been
	   sampled.

	   TBD: No intermission, the transmitter returns to the IDLE
	   state immediately after sampling the last bit of EOF.
	*/
	TRACE(2, ">>> MAC @%lu back to TX_FSM_IDLE", ts);

	mac->state.data_req_pending = 0;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_IDLE;

	if(mac->primitives.data_conf)
	    mac->primitives.data_conf(
		mac->llc, ts, mac->state.tx_identifier,
		CAN_XR_MAC_TX_STATUS_SUCCESS);
	break;

    case CAN_XR_MAC_TX_FSM_TX_EXT_DATA:
	/* While in this state, the transmit automaton invokes the
	   primitive at every sampling point suitable for transmitting
	   a bit in the data field.  Then, it switches back to idle.

	   The Data_Req below can be overridden by the ext_tx_data_ind
	   and makes sure that we drive the bus back to recessive
	   after inserting a stuff bit at zero in pcs_data_ind, even
	   though the ext_tx_data_ind does not issue any Data_Req by
	   itself (which is probably incorrect behavior anyway, but
	   better safe than sorry).
	*/
	CAN_XR_PCS_Data_Req(mac->pcs, 1);

	if(mac->primitives.ext_tx_data_ind)
	    mac->primitives.ext_tx_data_ind(mac, ts, input_unit);

	if(mac->state.tx_bit_count-- == 0)
	    /* Done with ext_tx_data_ind, switch to the transient
	       state CAN_XR_MAC_TX_FSM_TX_EXT_TAIL to drive the bus
	       back to recessive regardless of what the last
	       ext_tx_data_ind transmitted, and

	       TBD, currently not implemented: possibly insert a
	       (dominant) stuff bit that must come immediately after
	       the payload.
	    */
	    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_TX_EXT_TAIL;
	break;

    default:
	TRACE(9,
	      ">>> MAC @%lu Common::tx_processing_ind invalid tx_fsm_state %d",
	      ts, mac->state.tx_fsm_state);
	break;
    }
}


/* PCS_Data.Indicate primitive invoked by PCS the arrival of a bit.
   This is the starting point for MAC-layer processing.

   Bus off condition unchecked / recovery unsupported.
   FD tolerant / FD enabled MAC unsupported.
*/
static void pcs_data_ind(
    struct CAN_XR_MAC *mac, unsigned long ts, int input_unit)
{
    TRACE(2, "MAC @%lu Common::pcs_data_ind(%d)", ts, input_unit);

    /* Handle rx FSM first */
    switch(mac->state.rx_fsm_state)
    {
    case CAN_XR_MAC_RX_FSM_BUS_INTEGRATION:
	TRACE(2, ">>> MAC @%lu bus integration", ts);

	if(input_unit == 0)
	{
	    /* Bus dominant @ sample point.  Stay in bus integration
	       and reset integration counter. */
	    mac->state.bus_integration_counter = 0;
	}

	else
	{
	    /* Bus recessive @ sample point.  Keep counting and
	       transition to idle after 11 recessive bits.  This way
	       of transitioning implies a one-bit delay between
	       declaring the idle state and doing anytyhing else in
	       the MAC.  TBD: Do we need a bypass?
	    */
	    if(++mac->state.bus_integration_counter == 11)
	    {
		TRACE(2, ">>> MAC @%lu declaring bus idle", ts);

		mac->state.bus_integration_counter = 0;
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_IDLE;
	    }
	}
	break;

    case CAN_XR_MAC_RX_FSM_IDLE:
	if(input_unit == 0)
	{
	    /* SOF received, [1] 10.4.2.2 and 10.4.6.3.  Initialize
	       bit de-stuffing state, received 1 bit @ 0. */
	    mac->state.nc_bits = 1;
	    mac->state.nc_pol = input_unit;
	    mac->state.bus_bits = 1;
	    mac->state.de_stuffed_bits = 1;

	    de_stuffed_data_ind(mac, ts, input_unit);
	}
	break;

    case CAN_XR_MAC_RX_FSM_RX_IDENTIFIER:
    case CAN_XR_MAC_RX_FSM_RX_RTR:
    case CAN_XR_MAC_RX_FSM_RX_IDE:
    case CAN_XR_MAC_RX_FSM_RX_FDF:
    case CAN_XR_MAC_RX_FSM_RX_DLC:
    case CAN_XR_MAC_RX_FSM_RX_DATA:
    case CAN_XR_MAC_RX_FSM_RX_CRC:
    case CAN_XR_MAC_RX_FSM_RX_CDEL:
	/* Common entry point for all states in which the MAC is
	   receiving and bit de-stuffing is needed.  Do it, then call
	   de_stuffed_data_ind to continue processing.

	   RX_CDEL is in the list because there may be a stuff bit
	   after the last bit of CRC, as it must not be considered as
	   CDEL by itself.

	   TBD: This is probably also a good place to implement bit
	   monitoring and detect arbitration loss.  Neither of those
	   are implemented for now.
	*/
	mac->state.bus_bits++;

	if(mac->state.nc_bits == 5)
	{
	    /* Expecting a stuff bit, must be the opposite of nc_pol. */
	    if(input_unit == mac->state.nc_pol)
	    {
		TRACE(9, ">>> MAC @%lu stuff error", ts);
		TRACE_FUNCTION(9, CAN_XR_MAC_Dump, "[after stuff error]", mac);
		mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_ERROR;
	    }
	    else
	    {
		TRACE(2, ">>> MAC @%lu discarding stuff bit @%d", ts, input_unit);
		mac->state.nc_bits = 1;
		mac->state.nc_pol = input_unit;
	    }
	}

	else
	{
	    if(input_unit != mac->state.nc_pol)
	    {
		/* Reset bit counter on polarity change. */
		mac->state.nc_bits = 1;
		mac->state.nc_pol = input_unit;
	    }

	    else
		/* Same polarity, keep counting */
		mac->state.nc_bits++;

	    mac->state.de_stuffed_bits++;
	    de_stuffed_data_ind(mac, ts, input_unit);
	}

	break;

    case CAN_XR_MAC_RX_FSM_RX_ACK:
	/* TBD: This is probably a good place to detect ACK errors.
	   The transmitter transmits a recessive bit, we should sample
	   dominant here.
	*/
    case CAN_XR_MAC_RX_FSM_RX_ADEL:
    case CAN_XR_MAC_RX_FSM_RX_EOF:
	/* Bypass bit de-stuffing in the frame trailer [1] 10.5 last
	   sentence.  See above for the special tratment of the
	   CAN_XR_MAC_RX_FSM_RX_CDEL state.
	*/
	de_stuffed_data_ind(mac, ts, input_unit);
	break;

    default:
	/* This currently catches CAN_XR_MAC_RX_FSM_ERROR, too.

	   TBD: Very simple error recovery, transmit recessive at next
	   bit boundary, enable hard synchronization, bring the tx
	   automaton to idle and the rx automaton to the bus
	   integration state.
	*/
	TRACE(9, ">>> MAC @%lu Common::pcs_data_ind invalid rx_fsm_state %d",
	      ts, mac->state.rx_fsm_state);

	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	CAN_XR_PCS_Hard_Sync_Allowed_Req(mac->pcs, 1);
	mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_BUS_INTEGRATION;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_IDLE;
	break;
    }

    /* Handle tx FSM next */
    switch(mac->state.tx_fsm_state)
    {
    case CAN_XR_MAC_TX_FSM_IDLE:
	/* Honor MAC_Data.Req if the receiver is idle and transition
	   to the transmitting state.

	   Both the rx and tx automata run at the sampling point.  If
	   we issue a transmit request to PCS now, it will be
	   synchronized and honored by PCS at the next bit boundary.
	   The result of bit monitoring will be available for use upon
	   the next run of the automata, at the next sampling point.

	   In this way, the rx automaton can keep track of stuff bit
	   insertion, receive messages being transmitted by the tx
	   automaton and perform bit monitoring (TBD: although bit
	   monitoring is not implemented at this time).  It also
	   calculates the CRC to transmit for us.

	   TBD: According to [1], 10.4.2.2 we should bypass sending
	   SOF if we sample a SOF at the third bit of intermission and
	   several other conditions are met.  At this time we do not
	   implement intermission.  So, we stay with the stricter
	   constraint that we honor a pending transmission request
	   only if the bus was sampled idle.  We transmit the SOF at
	   the next bit boundary.

	   The transmission-related processing is implemented in
	   tx_processing_ind().
	*/
	if(mac->state.data_req_pending &&
	   mac->state.rx_fsm_state == CAN_XR_MAC_RX_FSM_IDLE)
	    tx_processing_ind(mac, ts, input_unit);
	break;

    case CAN_XR_MAC_TX_FSM_TX_IDENTIFIER:
    case CAN_XR_MAC_TX_FSM_TX_RTR:
    case CAN_XR_MAC_TX_FSM_TX_IDE:
    case CAN_XR_MAC_TX_FSM_TX_FDF:
    case CAN_XR_MAC_TX_FSM_TX_DLC:
    case CAN_XR_MAC_TX_FSM_TX_DATA:
    case CAN_XR_MAC_TX_FSM_TX_CRC_LATCH:
    case CAN_XR_MAC_TX_FSM_TX_CRC:
    case CAN_XR_MAC_TX_FSM_TX_CDEL:

    case CAN_XR_MAC_TX_FSM_TX_EXT_DATA:
	/* Common entry point for all states in which the MAC is
	   transmitting and bit stuffing is needed.  Do it, then call
	   tx_processing_ind to continue processing.

	   Bit stuff state information is kindly kept by the rx
	   automaton and we thankfully use it.

	   TX_CDEL is in the list because a stuff bit may be required
	   after the last bit of CRC, at it must be transmitted before
	   tx_processing_ind() transmits CDEL.
	*/
	if(mac->state.nc_bits == 5)
	{
	    TRACE(2, ">>> MAC %lu inserting stuff bit @%d", ts,
		  1 - mac->state.nc_pol);

	    CAN_XR_PCS_Data_Req(mac->pcs, 1 - mac->state.nc_pol);
	}

	else
	    tx_processing_ind(mac, ts, input_unit);

	break;

    case CAN_XR_MAC_TX_FSM_TX_EXT_TAIL:
	/* This is a transient state entered after transmitting the
	   last payload bit by means of ext_tx_data_ind.  It makes
	   sure that we drive the bus back to recessive regardless of
	   what the last call to ext_data_ind transmitted, and
	   switches back the transmitter automaton to idle.

	   TBD: In the current implementation, the responder cuts
	   short and does *not* transmit a (dominant) stuff bit that
	   must come immediately after the end of the payload.

	   The initiator does, so the frame on the bus is correct in
	   any case.

	   Within the payload, the responder does, too, and this is an
	   asymmetry we may want to correct.
	*/
	TRACE(2, ">>> MAC @%lu in CAN_XR_MAC_TX_FSM_TX_EXT_TAIL", ts);

	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_IDLE;
	break;

    case CAN_XR_MAC_TX_FSM_TX_ACK:
    case CAN_XR_MAC_TX_FSM_TX_ADEL:
    case CAN_XR_MAC_TX_FSM_TX_EOF:
    case CAN_XR_MAC_TX_FSM_TX_EOF_TAIL:
	/* Bypass bit stuffing in the frame trailer [1] 10.5 last
	   sentence.  See above for the special treatment of the
	   CAN_XR_MAC_TX_FSM_TX_CDEL state.
	*/
	tx_processing_ind(mac, ts, input_unit);
	break;

    default:
	/* This currently catches CAN_XR_MAC_TX_FSM_ERROR, too.

	   TBD: Very simple error recovery: clear data_req_pending,
	   notify LLC, ask the PCS to transmit recessive at next bit
	   boundary, enable hard synchronization, bring the tx
	   automaton to idle and the rx automaton to the bus
	   integration state.
	*/
	TRACE(9, ">>> MAC @%lu Common::pcs_data_ind invalid tx_fsm_state %d",
	      ts, mac->state.tx_fsm_state);

	mac->state.data_req_pending = 0;
	if(mac->primitives.data_conf)
	    mac->primitives.data_conf(
		mac->llc, ts, mac->state.tx_identifier,
		CAN_XR_MAC_TX_STATUS_NO_SUCCESS);
	CAN_XR_PCS_Data_Req(mac->pcs, 1);
	CAN_XR_PCS_Hard_Sync_Allowed_Req(mac->pcs, 1);
	mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_BUS_INTEGRATION;
	mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_IDLE;
	break;
    }
}


void CAN_XR_MAC_Common_Init(
    struct CAN_XR_MAC *mac,
    struct CAN_XR_PCS *pcs)
{
    TRACE(2, "CAN_XR_MAC_Common_Init");

    /* No LLC for now, link to PCS */
    mac->llc = NULL;
    mac->pcs = pcs;

    /* MAC state initialization (implementation-independent part.
       The MAC FSM starts in bus integration state, [1] 10.9.4.

    */
    mac->state.rx_fsm_state = CAN_XR_MAC_RX_FSM_BUS_INTEGRATION;
    mac->state.bus_integration_counter = 0;

    mac->state.tx_fsm_state = CAN_XR_MAC_TX_FSM_IDLE;
    mac->state.data_req_pending = 0;

    /* No data_ind, data_conf for now.  Link the common, static
       data_req, may be overridden by implementation-specific
       initialization function at a later time.
    */
    mac->primitives.data_ind = NULL;
    mac->primitives.data_conf = NULL;
    mac->primitives.data_req = mac_data_req;
    mac->primitives.ext_tx_data_ind = NULL;

    /* Link PCS to MAC, register the common, static data_ind */
    CAN_XR_PCS_Set_MAC(pcs, mac);
    CAN_XR_PCS_Set_Data_Ind(pcs, pcs_data_ind);
}

void CAN_XR_MAC_Set_LLC(struct CAN_XR_MAC *mac, struct CAN_XR_LLC *llc)
{
    mac->llc = llc;
}

void CAN_XR_MAC_Set_Data_Ind(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Data_Ind_t data_ind)
{
    mac->primitives.data_ind = data_ind;
}

void CAN_XR_MAC_Set_Data_Conf(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Data_Conf_t data_conf)
{
    mac->primitives.data_conf = data_conf;
}

void CAN_XR_MAC_Set_Ext_Tx_Data_Ind(
    struct CAN_XR_MAC *mac, CAN_XR_MAC_Ext_Tx_Data_Ind_t ext_tx_data_ind)
{
    mac->primitives.ext_tx_data_ind = ext_tx_data_ind;
}



void CAN_XR_MAC_Data_Req(
    struct CAN_XR_MAC *mac,
    uint32_t identifier, enum CAN_XR_Format format, int dlc, uint8_t *data)
{
    if(mac->primitives.data_req)
	mac->primitives.data_req(mac, identifier, format, dlc, data);
}
