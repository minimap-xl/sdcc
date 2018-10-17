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

/* This header is a common point to define configuration macros for
   the software that runs on the boards, like the CAN bit rate.
 */

/* #define CAN_XR_BIT_RATE 31250 and 10 quanta per bit were used for
    initial testing (INDIN and TII 2016 papers) with TRACE enabled
    (-DENABLE_TRACE in the Makefile).

   #define CAN_XR_BIT_RATE 53000 and 8 quanta per bit work reliably
    with rev. 191 of the code with TRACE disabled.

   #define CAN_XR_BIT_RATE 50000 and 8 quanta per bit is one of the
    standard CANopen bit rates and is used for the paper about Boolean
    binary functions (Mueller's protocol).  TRACE must be disabled.
*/

#define CAN_XR_BIT_RATE 50000
