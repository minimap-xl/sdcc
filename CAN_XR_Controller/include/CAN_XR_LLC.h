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
   CAN XR Link Layer Control.  Specified in ISO 11898-1:2015(E) [1],
   Section 8.

   TBD: Incomplete.  Currently it only contains data type definitions
   shared between LLC and MAC.
*/


/* LLC frame format, [1] Table 4, also used by MAC.  Generally, data
   types are defined in the header of the highest layer that uses them
   and, when shared, they don't have a layer identification in their
   name.
*/
enum CAN_XR_Format {
    CAN_XR_FORMAT_CBFF, /* Classical Base (11b) */
    CAN_XR_FORMAT_CEFF, /* Classical Extended (29b), unsupported */
    CAN_XR_FORMAT_FBFF, /* FD Base (11b), unsupported */
    CAN_XR_FORMAT_FEFF  /* FD Extended (29b), unsupported */
};
