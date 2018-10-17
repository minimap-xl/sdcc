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

/* This header defines the TRACE macro on the host. */

#ifndef CAN_XR_TRACE_H
#define CAN_XR_TRACE_H

#include <stdio.h> /* For fputc, fprintf. */

/* Yes, if you are wondering, this is a variadic macro, and is
   perfectly standard.  Note the implicit literal concatenation and
   the use of ## to suppress the preceding comma when varargs are
   empty (this is admittedly a GNU extension).  Just to have some fun
   with cpp.  :)
*/

#define TRACE(level, format, ...)			\
    do {						\
	int i;						\
	for(i=0; i<level*4; i++) fputc(' ', stderr);	\
	fprintf(stderr, format "\n", ##__VA_ARGS__);	\
    } while(0)

#define TRACE_FUNCTION(level, function, ...)		\
    do {						\
	function(__VA_ARGS__);				\
    } while(0)

#endif
