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

/* Real CAN controller polling device driver for the LPC1768
   boards.
*/

#ifndef CAN_XR_MAC_H
#define CAN_XR_MAC_H

#include <stdint.h>

enum CAN_XR_CAN_Status {
    CAN_XR_CAN_STATUS_OK=0,
    CAN_XR_CAN_STATUS_UNAVAILABLE=0, /* Not an error */
    CAN_XR_CAN_STATUS_AVAILABLE,     /* Not an error */
    CAN_XR_CAN_STATUS_INV_RATE,
    CAN_XR_CAN_STATUS_INV_FRAME,
    CAN_XR_CAN_STATUS_TIMEOUT        /* Polling timeout */
};

#define CAN_XR_CAN_TEST_MODE_NORMAL	0
#define CAN_XR_CAN_TEST_MODE_LOM	(1<<1)
#define CAN_XR_CAN_TEST_MODE_STM	(1<<2)

/* Initialize the CAN controller to work at 'rate' b/s.  Returns a
   non-zero status code upon failure.  A reason for failure is that
   'rate' cannot be obtained from CCLK.

   Argument 'test_mode' optionally selects a test mode.  See UM10360,
   Table 317.  As suggested by Tingting, this is very useful in
   different ways:

   - In listen-only mode, while the software transmitter is
     transmitting, the CAN controller receiver hardware is able to
     receive back.

   - In self-test mode, no acknowledge from other nodes is needed, and
     the controller receives its own messages.
*/
enum CAN_XR_CAN_Status CAN_XR_CAN_Init(
    int rate, uint32_t test_mode);

/* Returns a non-zero value if the CAN controller's transmit buffer is
   currently available for use.  If argument 'wait' is non-zero, waits
   until this becomes true.  Both this function and
   Receive_Data_Available have an internal timeout that avoids
   trapping the caller forever if the controller stops responding.
*/
enum CAN_XR_CAN_Status CAN_XR_CAN_Transmit_Buf_Available(int wait);

/* Requests the transmission of a data CBFF frame, with the given
   'identifier', 'dlc', and 'data'.  It automatically calls
   CAN_Transmit_Buf_Available(1) before doing anything else.
*/
enum CAN_XR_CAN_Status CAN_XR_CAN_Transmit_Req(
    uint32_t identifier, int dlc, uint8_t *data);

/* Returns a non-zero value if the CAN controller received a frame
   available for retrieval.  It argument 'wait' is non-zero, waits
   until this becomes true.
*/
enum CAN_XR_CAN_Status CAN_XR_CAN_Receive_Data_Available(int wait);

/* Retrieves a frame from the CAN controller.  Returns a non-zero
   status code upon failure.  A reason for failure is that the
   received frame was not CBFF, or not a data frame.  In this case,
   retry the operation.
*/
enum CAN_XR_CAN_Status CAN_XR_CAN_Receive(
    uint32_t *identifier, int *dlc, uint8_t *data);

#endif
