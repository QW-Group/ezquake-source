/*
Copyright (C) 2003-2004 Agner Fog
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
See the GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
	$Id: asmlib.h,v 1.3 2007-10-01 18:31:06 disconn3ct Exp $
*/

#ifdef WITH_ASMLIB

#ifndef __ASMLIB_H__
#define __ASMLIB_H__

/* Downloaded from www.agner.org/optimize */
extern int InstructionSet (void);         // tell which instruction set is supported
extern int DetectProcessor (void);        // information about microprocessor features
extern void ProcessorName (char * text);  // ASCIIZ text describing microprocessor
extern int Round (double x);              // round to nearest or even
extern int Truncate (double x);           // truncation towards zero
extern int ReadClock (void);              // read microprocessor internal clock
extern int MinI (int a, int b);           // the smallest of two integers
extern int MaxI (int a, int b);           // the biggest  of two integers
extern double MinD (double a, double b);  // the smallest of two double precision numbers
extern double MaxD (double a, double b);  // the biggest  of two double precision numbers

/*
extern "C" int Round (double x);
--------------------------------
Converts a floating point number to the nearest integer. When two integers
are equally near, the even integer is chosen. This function does not check
for overflow. This function is much faster than the default way of converting
floating point numbers to integers in C++, which involves truncation.


extern "C" int Truncate (double x);
-----------------------------------
Converts a floating point number to an integer, with truncation towards zero.
This function does not check for overflow. In case of overflow, you may get 
an exception or an invalid result. This function is faster than the standard
C++ type-casting:  int i = (int)x;


extern "C" int MinI (int a, int b);
-----------------------------------
Returns the smallest of two signed integers. Will also work with unsigned
integers if both numbers are smaller than 2^31. This is faster than a C++
branch if the branch is unpredictable.


extern "C" int MaxI (int a, int b);
-----------------------------------
Returns the biggest of two signed integers. Will also work with unsigned
integers if both numbers are smaller than 2^31. This is faster than a C++
branch if the branch is unpredictable.


extern "C" double MinD (double a, double b);
--------------------------------------------
Returns the smallest of two double precision floating point numbers. This 
is faster than a C++ implementation.


extern "C" double MaxD (double a, double b);
--------------------------------------------
Returns the biggest of two double precision floating point numbers. This 
is faster than a C++ implementation.


extern "C" int InstructionSet (void);
-------------------------------------
This function detects which instructions are supported by the microprocessor
and the operating system. (see www.agner.org/optimize/optimizing_assembly.pdf
for a discussion of the method used for checking XMM operating system support).

Return value:
 0          = use 80386 instruction set only
 1 or above = MMX instructions can be used
 2 or above = conditional move and FCOMI can be used
 3 or above = SSE (XMM) supported by processor and enabled by Operating system
 4 or above = SSE2 supported by processor and enabled by Operating system
 5 or above = SSE3 supported by processor and enabled by Operating system


extern "C" int DetectProcessor (void);
--------------------------------------
This function detects the microprocessor type and determines which features
are supported. It gives a more detailed information than InstructionSet().

The return value is a combination of bits indicating different features.
The return value is 0 if the microprocessor has no CPUID instruction.

 bits     value       meaning
----------------------------------------------------------------------------
 0-3      0x0F        model number
 4-7      0xF0        family:  0x40 for 80486, Am486, Am5x86
                               0x50 for P1, PMMX, K6
                               0x60 for PPro, P2, P3, Athlon, Duron
                               0xF0 for P4, Athlon64, Opteron
   8      0x100       vendor is Intel
   9      0x200       vendor is AMD
  11      0x800       XMM registers (SSE) enabled by operating system
  12      0x1000      floating point instructions supported
  13      0x2000      time stamp counter supported
  14      0x4000      CMPXCHG8 instruction supported
  15      0x8000      conditional move and FCOMI supported (PPro, P2, P3, P4, Athlon, Duron, Opteron)
  23      0x800000    MMX instructions supported (PMMX, P2, P3, P4, K6, Athlon, Duron, Opteron)
  25      0x2000000   SSE instructions supported (P3, P4, Athlon64, Opteron)
  26      0x4000000   SSE2 instructions supported (P4, Athlon64, Opteron)
  27      0x8000000   SSE3 instructions supported (forthcoming "Prescott")
  28      0x10000000  hyperthreading supported (P4)
  29      0x20000000  MMX extension instructions (AMD only)
  30      0x40000000  3DNow extension instructions (AMD only)
  31      0x80000000  3DNow instructions (AMD only)
----------------------------------------------------------------------------


extern "C" void ProcessorName (char * text);
--------------------------------------------
Makes a zero-terminated text string with a description of the microprocessor.
The string is stored in the parameter "text", which must be a character array
of size at least 68.
 

extern "C" int ReadClock (void);
--------------------------------
This function returns the value of the internal clock counter in the 
microprocessor. To count how many clock cycles a piece of code takes, call
ReadClock before and after the code to measure and calculate the difference.
You may see that the count varies a lot because you may not be able to prevent
interrupts during the execution of your code. See 
www.agner.org/assem/pentopt.pdf for discussions of how to measure execution
time most accurately. The ReadClock function itself takes approximately 700
clock cycles on a Pentium 4, and approximately 225 clock cycles on Pentium II
and Pentium III. Does not work on 80386 and 80486.
*/

#endif /* !__ASMLIB_H__ */
#endif /* !WITH_ASMLIB */
