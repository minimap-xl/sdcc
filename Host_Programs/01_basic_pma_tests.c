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


/* 8 quanta per bit, sampling point between quantum #5 and #6 */
const struct CAN_XR_PCS_Bit_Time_Parameters pcs_parameters = {
    .prescaler_m = 1,
    .sync_seg = 1, /* Always this way */
    .prop_seg = 3,
    .phase_seg1 = 2,
    .phase_seg2 = 2,
    .sjw = 2 /* Full swing within phase_seg1 and _seg2 */
};

int main(int argc, char *argv[])
{
    struct CAN_XR_MAC mac;
    struct CAN_XR_PCS pcs;
    struct CAN_XR_PMA pma;
    int rx_level;

    CAN_XR_PMA_Sim_Init(&pma);
    CAN_XR_PCS_Init(&pcs, &pcs_parameters, &pma);

    /* TBD: To be replaced by implementation-specific initialization
       function when there's one. */
    CAN_XR_MAC_Common_Init(&mac, &pcs);

    /* Inhibit hard synchronization for testing. */
    CAN_XR_PCS_Hard_Sync_Allowed_Req(&pcs, 0);

    /* Example of introspection.
       Accessors would be better.
    */
    printf("# %12s %12s %12s %12s\n",
	   "ts", "rx_level", "quantum_m_cnt", "sync_inhibit");

    printf("  %12lu %12d %12d %12d\n",
	   pcs.state.nodeclock_ts,
	   1, /* Initial rx_level (assumed) */
	   pcs.state.quantum_m_cnt,
	   pcs.state.sync_inhibit);

    while(1)
    {
	int c, q;

	if((c = getchar()) == EOF) break;

	else if(isspace(c))
	    continue;

	else if(c == '=')
	{
	    if(scanf("%d", &rx_level) <=0) break;
	    for(q=0; q<pcs.state.quanta_per_bit; q++)
		CAN_XR_PMA_Sim_NodeClock_Ind(&pma, rx_level);
	}

	else
	{
	    ungetc(c, stdin);
	    if(scanf("%d", &rx_level) <=0) break;

	    CAN_XR_PMA_Sim_NodeClock_Ind(&pma, rx_level);
	}

	printf("  %12lu %12d %12d %12d\n",
	       pcs.state.nodeclock_ts,
	       rx_level,
	       pcs.state.quantum_m_cnt,
	       pcs.state.sync_inhibit);
    }

    return EXIT_SUCCESS;
}
