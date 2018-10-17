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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <CAN_XR_PMA_Sim.h>
#include <CAN_XR_PCS.h>
#include <CAN_XR_MAC.h>
#include <CAN_XR_Trace.h>


/* 10 quanta per bit, sampling point between quantum #6 and #7.  This
   matches the CAN controller configuration on the LPC1768 boards, see
   CONFIG_xxx macros in CAN_XR_CAN_Driver.c.
*/
const struct CAN_XR_PCS_Bit_Time_Parameters pcs_parameters = {
    .prescaler_m = 1,
    .sync_seg = 1, /* Always this way */
    .prop_seg = 3,
    .phase_seg1 = 3,
    .phase_seg2 = 3,
    .sjw = 1 /* Full swing within phase_seg1 and _seg2 */
};

void dummy_data_ind(
    struct CAN_XR_LLC *llc, unsigned long ts, uint32_t identifier,
    enum CAN_XR_Format format, int dlc, uint8_t *data)
{
    int j;

    printf("> @%lu: id=%lu, format=%d, dlc=%d, data[] = { ",
	   ts, (unsigned long)identifier, format, dlc);
    for(j=0; j<dlc; j++) printf("0x%02x ", data[j]);
    printf("}\n");
}

void dummy_data_conf(
    struct CAN_XR_LLC *llc, unsigned long ts, uint32_t identifier,
    enum CAN_XR_MAC_Tx_Status transmission_status)
{
    printf("< @%lu: id=%lu, transmission_status=%d\n",
	   ts, (unsigned long)identifier, transmission_status);
}

int main(int argc, char *argv[])
{
    struct CAN_XR_MAC mac;
    struct CAN_XR_PCS pcs;
    struct CAN_XR_PMA pma;

    CAN_XR_PMA_Sim_Init(&pma);
    CAN_XR_PCS_Init(&pcs, &pcs_parameters, &pma);

    /* TBD: To be replaced by implementation-specific initialization
       function when there's one. */
    CAN_XR_MAC_Common_Init(&mac, &pcs);

    /* Register dummy data_ind and data_conf primitives in 'mac'. */
    CAN_XR_MAC_Set_Data_Ind(&mac, dummy_data_ind);
    CAN_XR_MAC_Set_Data_Conf(&mac, &dummy_data_conf);

    /* Example of introspection.
       Accessors would be better.
    */
    printf("# %12s %12s\n",
	   "ts", "pma.tx_bus_level");

    printf("  %12lu %16d\n",
	   pcs.state.nodeclock_ts,
	   pma.state.sim.tx_bus_level);

    /* Issue a MAC layer transmission request */
    {
	uint8_t data[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0x3E, 0x3E, 0x3E };

	CAN_XR_MAC_Data_Req(&mac,
			    0x345, CAN_XR_FORMAT_CBFF, 8, data);
    }

    /* Exit from the loop when the transmission is over.  The right
       way to do this would be to monitor the data indication though.
    */
    while(mac.state.data_req_pending)
    {
	/* Inject an idle bus (not driven by any other node) into the
	   simulated PMA and watch the show.
	*/
	CAN_XR_PMA_Sim_NodeClock_Ind(&pma, 1);


	printf("  %12lu %16d\n",
	       pcs.state.nodeclock_ts,
	       pma.state.sim.tx_bus_level);
    }

    return EXIT_SUCCESS;
}
