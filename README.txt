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


Getting Started
---------------

The SDCC source tree is organized as follows:

- CAN_XR_Controller: contains the platform-independent SDCC source
  modules

- Cross: contains the platform-dependent source modules needed to run
  SDCC in real time on an embedded target and interface it with a
  physical CAN bus.  The example module is suitable for:

  - an NXP LPC1768 microcontroller when the cpp macro
    GCC_ARM_CM3_UN_LPC1768 is defined.

  - an NXP LPC4357 microcontroller when the cpp macro
    GCC_ARM_CM4F_UN_LPC4357 is defined.

- Host: contains the platform-dependent source modules needed to run
  SDCC on a Linux or macOS host in a simulated CAN bus environment.

- Cross_Programs: contains the test programs for an embedded target.

- Host_Programs: contains the test programs for the host.

- Host_Tests: contains stimulus files and expected results used by the
  host test programs.


Prerequisites
-------------

- SDCC requires a GNU-based cross-compilation toolchain, correctly
  configured for the embedded target.  For instance, an arm-none-eabi-
  toolchain is needed to build the platform-dependent module for the
  NXP LPC1768 or the NXP LPC4357.

- In addition, the following 'make' variables must be set according to
  the toolchain's requirements, either within the Makefile or directly
  on the 'make' command line.

  - XCDEFS: Gcc code generation and optimization options
  - XSPECS: Name of the compiler driver specs file
  - XLDSCRIPT: Name of the ld linker script
  - XLDLIBS: Additional link-time libraries

- The Host_Programs require gnuplot to plot test results.


Building SDCC and running the tests
-----------------------------------

1. Download the SDCC distribution and unzip if necessary.  Ensure all
   prerequisites are met, then build SDCC and the associated test
   programs.

2. To test SDCC on the embedded target, upload the test programs
   01_can_sw_receiver.hex and 02_can_sw_transmitter.hex on two LPC1768
   or LPC4357 boards and run them.  The boards must be connected by
   means of a properly-terminated CAN bus.

3. The Makefile automatically runs test program
   Host_Programs/01_basic_pma_tests on the stimulus files found in
   Host_Tests/Inputs.  Results are available in Host_Tests/Results.
   The stimulus files provided as examples show that SDCC reacts
   correctly to edge phase errors in the input stream.

4. Have fun! ;-)


Contacts
-------
Ivan Cibrario Bertolotti <ivan.cibrario@ieiit.cnr.it>
Tingting Hu <tingting.hu@uni.lu>
