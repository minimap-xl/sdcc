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

/* This file contains CAN_XR_MAC_Dump() alone, to avoid pulling it
   from the library when it is not needed.
*/

#include <stdio.h>
#include <stdlib.h>
#include "CAN_XR_MAC.h"

static void dump_array(FILE *f, const char *desc, const uint8_t *d, int len)
{
    int i;

    fprintf(f, "%s0x { ", desc);
    for(i=0; i<len; i++)
	fprintf(f, "%02x%s", d[i], i<len-1 ? ", " : " ");
    fprintf(f, "},\n");
}

void CAN_XR_MAC_Dump(
    const char *desc, const struct CAN_XR_MAC *mac)
{
    const struct CAN_XR_MAC_State *state = &(mac->state);

    fprintf(stderr,
	    "struct CAN_XR_MAC %s = {\n"
	    "  rx_fsm_state=%d,\n"
	    "  bus_integration_counter=%d,\n"
	    "  nc_bits=%d, nc_pol=%d,\n"
	    "  crc=0x%04x,\n"
	    "  field_bits=%d, bus_bits=%d, de_stuffed_bits=%d,\n"
	    "  rx_identifier=%u, rx_rtr=%d, rx_ide=%d, rx_fdf=%d, rx_dlc=%d,\n"
	    "  rx_byte=0x%02x, rx_byte_index=%d,\n",
	    desc,
	    state->rx_fsm_state,
	    state->bus_integration_counter,
	    state->nc_bits, state->nc_pol,
	    state->crc,
	    state->field_bits, state->bus_bits, state->de_stuffed_bits,
	    (unsigned int)state->rx_identifier, state->rx_rtr,
	    state->rx_ide, state->rx_fdf, state->rx_dlc,
	    state->rx_byte, state->rx_byte_index
	);
    dump_array(stderr, "  rx_data[]= ", state->rx_data,
	       state->rx_dlc <= 8 ? state->rx_dlc : 8);

    fprintf(stderr,
	    "\n"
	    "  tx_fsm_state=%d,\n"
	    "  data_req_pending=%d,\n"
	    "  tx_identifier=%u, tx_format=%d, tx_dlc=%d,\n",
	    state->tx_fsm_state,
	    state->data_req_pending,
	    (unsigned int)state->tx_identifier, state->tx_format, state->tx_dlc
	);
    dump_array(stderr, "  tx_data[]= ", state->tx_data,
	       state->tx_dlc <= 8 ? state->tx_dlc : 8);
    fprintf(stderr,
	    "  tx_byte_index=%d, tx_bit_count=%d, tx_shift_reg=0x%02x\n"
	    "}\n",
	    state->tx_byte_index, state->tx_bit_count,
	    (unsigned int)state->tx_shift_reg);
}
