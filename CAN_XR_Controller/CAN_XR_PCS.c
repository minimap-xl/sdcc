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

/* This module implements the CAN XR Physical Coding Sub-layer.  It is
   currently the same as the regular CAN PCS specified in ISO
   11898-1:2015(E) [1], Section 11.1.

   This module is also responsible of keeping a timestamp counter
   based on nodeclock.  This is not specified in the standard.
*/

#include <stdlib.h>
#include "CAN_XR_PMA.h"
#include "CAN_XR_PCS.h"
#include "CAN_XR_Trace.h"

/* Initialize PCS state. */
static void init_state(struct CAN_XR_PCS *pcs)
{
    /* State information derived from parameters */
    pcs->state.quanta_per_bit =
	pcs->parameters.sync_seg + pcs->parameters.prop_seg
	+ pcs->parameters.phase_seg1 + pcs->parameters.phase_seg2;

    /* Fixed state information */
    pcs->state.nodeclock_ts = (unsigned long)0;
    pcs->state.prescaler_m_cnt = 0;
    pcs->state.quantum_m_cnt = 0;

    /* Previous bus_level and bus stat at previous sample point for
       edge detector.
    */
    pcs->state.prev_bus_level = 1;
    pcs->state.prev_sample = 1;

    /* Synchronization state information */
    pcs->state.sync_inhibit = 0;
    pcs->state.hard_sync_allowed = 1;

    /* Output unit buffer set by data_req */
    pcs->state.output_unit_buf = 1;

    /* Bus level being sent, resynchronized at bit boundary according
       to the quantum counter.
    */
    pcs->state.sending_level = 1;
}

/* Implementation of data_req primitive.

   This primitive can be invoked at any time by the MAC layer.
   However, the PCS actually synchronizes bit transmission with its
   notion of bit boundaries.

   The PMA_Data_Req primitive is invoked after the last quantum of a
   bit, that is, when quantum_n_cnt == quanta_per_bit - 1.

   TBD: A borderline case happens when synchronization reduces the
   length of phase_seg2 to zero, and hence, the sync_seg of the next
   bit is declared to start immediately after the sampling point of
   the current bit.

   In the current implementation, PMA is notified of the bit level to
   be transmitted at the end of the repositioned sync_seg instead of
   the beginning, one quantum later than the usual case.  This case
   can be spotted because quantum_n_cnt temporarily becomes ==
   quanta_per_bit.  Not sure this interpretation of the standard is
   correct.
*/
static void data_req(struct CAN_XR_PCS *pcs, int output_unit)
{
    pcs->state.output_unit_buf = output_unit;
}

/* This internal primitive is invoked on the edges of the m quantum
   clock (nominal bit time).

   It implements
   - quantum m counting: [1] Section 11.3.1.1.
   - edge detection: [1] Section 11.3.2.1.
   - synchronization: [1] Section 11.3.2.
   - sampling: [1] Section 11.3.1.1.
   - PCS_Data.Indicate (upcall to data_ind): [1] Section 11.2.3.
   - bit transmission, synchronized to bit boundaries.
*/
static void quantumclock_m_ind(
    struct CAN_XR_PCS *pcs, unsigned long ts, int bus_level)
{
    int edge;
    int phase_error;
    int sync_amount;

    TRACE(1, "PCS @%lu quantumclock_m_ind(%d)", ts, bus_level);

    /* Bit synchronization, [1] Section 11.3.2. */

    /* Edge detection, [1] Section 11.3.2.1. */
    edge = pcs->state.prev_bus_level ^ bus_level;

    if(edge
       && !pcs->state.sync_inhibit      /* [1] 11.3.2.1 a) */
       && pcs->state.prev_sample == 1)  /* [1] 11.3.2.1 b) 1) */
    {
	/* Here quantum_m_cnt holds the quantum number of the quantum
	   that *ends* at this quantum clock edge.

	   In other words, we assume that the first quantumclock_m_ind
	   is raised after 1 quantum after the beginning of the epoch.

	   So the sync_seg has quantum_m_cnt == 0 and sampling must
	   take place when quantum_m_cnt == sync_seg + prop_seg +
	   phase_seg1 - 1.  The position of the sampling point is also
	   used to determine the sign of the phase error e, see later.
	*/

	/* Phase error e, computed according to [1] 11.3.2.2 */
	phase_error =
	    /* Case 1, edge in quantum 0 */
	    (pcs->state.quantum_m_cnt == 0)
	    ? 0
	    : ((pcs->state.quantum_m_cnt
		<= (pcs->parameters.sync_seg + pcs->parameters.prop_seg
		    + pcs->parameters.phase_seg1 - 1)
		/* Case 2, positive phase error (edge before s.p.) */
		? pcs->state.quantum_m_cnt
		/* Case 3, negative phase error (edge after s.p.) */
		: (pcs->state.quantum_m_cnt - pcs->state.quanta_per_bit)));

	/* [1] 11.3.2.1 b) 2), part 1) of the same clause was handled
	   before computing the phase error.
	*/
	if(phase_error < 0
	   || (phase_error > 0 && pcs->state.sending_level == 1))
	{
	    /* Edge good for synchronization.  [1] 11.3.2.1 d) is
	       irrelevant for legacy CAN.

	       Choose between hard and soft synchronization according to
	       what the MAC tells us.  [1] 11.3.2.1 c) depends on MAC
	       state items: first bit of intermission or bus integration.
	    */
	    if(pcs->state.hard_sync_allowed)
	    {
		/* [1] 11.3.2.3, sjw is not considered upon hard
		   synchronization.  Simply restart the quantum
		   counter as suggested in the standard.

		   When quantum_m_cnt == 0 we are just past the
		   sync_seg.
		*/
		pcs->state.quantum_m_cnt = 0;

		TRACE(1, ">>> Hard sync");
	    }

	    else
	    {
		/* [1] 11.3.2.4, lenghtening phase_seg1 (positive
		   phase_error) means -decreasing- the quantum
		   counter, shortening phase_seg2 (negative phase
		   error) means -increasing- *the quantum counter.

		   Clip phase_error with sjw on both ends.
		*/
		sync_amount =
		    (phase_error > pcs->parameters.sjw)
		    ? pcs->parameters.sjw
		    : ((phase_error < - pcs->parameters.sjw)
		       ? - pcs->parameters.sjw
		       : phase_error);

		/* Before the sampling point the phase error is always
		   positive, whereas after the sampling point it is
		   always negative.  This implies that the following
		   statement never causes the same bit to be sampled
		   twice.

		   On the other hand, a negative phase error unclipped
		   by sjw makes quantum_m_cnt == quanta_per_bit
		   because it declares the quantum just elapsed as
		   sync_seg.  This value is outside the normal range
		   of quantum_m_cnt but it is still valid because it
		   is 0 modulus quanta_per_bit and it will be brought
		   back to normal range by the update that follows.
		*/
		pcs->state.quantum_m_cnt -= sync_amount;

		TRACE(1,
		      ">>> Soft sync, phase_error %d, clipped to %d,"
		      " quantum_m_cnt now %d",
		      phase_error, sync_amount, pcs->state.quantum_m_cnt);
	    }
	}

	else if(phase_error == 0)
	{
	    /* This is only for tracing.  The compiler should be able
	       to optimize it away when TRACE() is empty.
	    */
	    TRACE(1, ">>> Edge ignored "
		  "due to phase_error=%d",
		  phase_error);
	}

	else
	{
	    /* This is only for tracing. */
	    TRACE(1, ">>> Edge ignored "
		  "due to sending_level=%d",
		  pcs->state.sending_level);
	}
    }

    else if(edge)
    {
	/* This is only for tracing. */
	TRACE(1, ">>> Edge ignored "
	      "due to sync_inhibit=%d or prev_sample=%d",
	      pcs->state.sync_inhibit, pcs->state.prev_sample);
    }

    /* [1] 11.3.2.1 a) Inhibit further synchronizations after an edge
       is detected, even though the edge is not actually used for
       synchronization.
    */
    if(edge)  pcs->state.sync_inhibit = 1;

    /* Sample at sampling point and invoke PCS_Data.Indicate,
       [1] 11.3.1.1 and 11.2.3.

       This may lead the MAC to call PCS back through
       PCS_Data.Request.
    */
    if(pcs->state.quantum_m_cnt ==
       (pcs->parameters.sync_seg + pcs->parameters.prop_seg
	+ pcs->parameters.phase_seg1 - 1))
    {
	if(pcs->primitives.data_ind)
	    pcs->primitives.data_ind(pcs->mac, ts, bus_level);

	/* Per [1] 11.3.2.1 a) reset sync_inhibit when the bus state
	   detected at the sample point is recessive.
	*/
	if(bus_level == 1)  pcs->state.sync_inhibit = 0;

	/* Save bus state at (soon previous) sampling point. */
	pcs->state.prev_sample = bus_level;
    }

    /* Handle transmission requests buffered in output_unit_buf by
       data_ind.  The transmission normally starts at bit boundary,
       but may also start after the sync_seg when synchronization
       reduced phase_seg2 to zero length.  This is why there's >=
       instead of == below.  See the comments on data_req and
       above.
    */
    if(pcs->state.quantum_m_cnt >= pcs->state.quanta_per_bit - 1)
    {
	TRACE(1, ">>> Synchronized PMA_Data_Req%s",
	      pcs->state.quantum_m_cnt == pcs->state.quanta_per_bit - 1
	      ? "" : " (after repositioned sync_seg)");

	CAN_XR_PMA_Data_Req(pcs->pma, pcs->state.output_unit_buf);

	/* Update our internal notion of what is being sent on the
	   bus, for synchronization.
	*/
	pcs->state.sending_level = pcs->state.output_unit_buf;
    }

    /* Update quantum_m_cnt, wrap around at end of bit */
    pcs->state.quantum_m_cnt =
	(pcs->state.quantum_m_cnt + 1) % pcs->state.quanta_per_bit;

    /* Update prev_bus_level for the edge detector */
    pcs->state.prev_bus_level = bus_level;
}

/* Implementation of nodeclock_ind, invoked from PMA. */
static void nodeclock_ind(struct CAN_XR_PCS *pcs, int bus_level)
{
    TRACE(1, "PCS nodeclock_ind(%d)", bus_level);

    /* Update timestamp counter.  We assume that the first
       nodeclock_ind is raised after 1 nodeclock tick after the
       beginning of the epoch, so this is incremented beforehand.
    */
    pcs->state.nodeclock_ts++;

    /* Prescaler, [1] Section 11.3.1.1. */
    pcs->state.prescaler_m_cnt =
	(pcs->state.prescaler_m_cnt + 1) % pcs->parameters.prescaler_m;

    if(pcs->state.prescaler_m_cnt == 0)
    {
	/* At m quantum edge */
	quantumclock_m_ind(pcs, pcs->state.nodeclock_ts, bus_level);
    }

}

void CAN_XR_PCS_Init(
    struct CAN_XR_PCS *pcs,
    const struct CAN_XR_PCS_Bit_Time_Parameters *parameters,
    struct CAN_XR_PMA *pma)
{
    TRACE(1, "CAN_XR_PCS_Init");

    /* No MAC for now, link to PMA. */
    pcs->mac = NULL;
    pcs->pma = pma;

    pcs->parameters = *parameters; /* Copy, just in case. */

    init_state(pcs); /* May use parameters */

    /* No data_ind for now, link static data_req */
    pcs->primitives.data_ind = NULL;
    pcs->primitives.data_req = data_req;

    /* Link PMA to PCS, register nodeclock_ind */
    CAN_XR_PMA_Set_PCS(pma, pcs);
    CAN_XR_PMA_Set_NodeClock_Ind(pma, nodeclock_ind);
}

void CAN_XR_PCS_Set_MAC(struct CAN_XR_PCS *pcs, struct CAN_XR_MAC *mac)
{
    pcs->mac = mac;
}

void CAN_XR_PCS_Set_Data_Ind(
    struct CAN_XR_PCS *pcs, CAN_XR_PCS_Data_Ind_t data_ind)
{
    pcs->primitives.data_ind = data_ind;
}

void CAN_XR_PCS_Data_Req(struct CAN_XR_PCS *pcs, int output_unit)
{
    if(pcs->primitives.data_req)
	pcs->primitives.data_req(pcs, output_unit);
}

void CAN_XR_PCS_Hard_Sync_Allowed_Req(
    struct CAN_XR_PCS *pcs, int hard_sync_allowed)
{
    pcs->state.hard_sync_allowed = hard_sync_allowed;
}
