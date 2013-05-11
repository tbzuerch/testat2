/*	Oscar, a hardware abstraction framework for the LeanXcam and IndXcam.
	Copyright (C) 2008 Supercomputing Systems AG
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*! @file
 * @brief Support module implementation for host
 */

#include <time.h>
#include <sys/time.h>
#include "sup.h"

OSC_ERR OscSupCreate();

// FIXME: Why do we have this instance on the host?
/*! @brief The module singelton instance. */
struct OSC_SUP sup;

struct OscModule OscModule_sup = {
	.name = "sup",
	.create = OscSupCreate,
	.dependencies = {
		&OscModule_log,
		NULL // To end the flexible array.
	}
};

OSC_ERR OscSupCreate()
{
	sup = (struct OSC_SUP) { };
		
	return SUCCESS;
}

/*=========================== Watchdog =================================*/

OSC_ERR OscSupWdtInit()
{
	/* There is no watchdog on the host => do nothing. */

	return SUCCESS;
}

OSC_ERR OscSupWdtClose()
{
	/* There is no watchdog on the host => do nothing. */

	return SUCCESS;
}

inline void OscSupWdtKeepAlive()
{
	/* There is no watchdog on the host => do nothing. */
}

/*============================= Cycles =================================*/
inline uint32 OscSupCycGet()
{
	/* The host implementation uses the ANSI C function clock()
	 * with much lower precision than on the blackfin.
	 * The actual precision is dependent on the platform. */

	/* in microseconds */
	struct timeval tim;
	gettimeofday(&tim, NULL);
	return ((uint32)tim.tv_sec*1000000+tim.tv_usec);
}

inline long long OscSupCycGet64()
{
	/* The host implementation uses the ANSI C function clock()
	 * with much lower precision than on the blackfin.
	 * The actual precision is dependent on the platform. */

	/* in microseconds */
	struct timeval tim;
	gettimeofday(&tim, NULL);
	return ((long long)tim.tv_sec*1000000+(long long)tim.tv_usec);
}

inline uint32 OscSupCycToMicroSecs(uint32 cycles)
{
	return (cycles);
}

inline uint32 OscSupCycToMilliSecs(uint32 cycles)
{
	return (cycles/1000);
}

inline uint32 OscSupCycToSecs(uint32 cycles)
{
	return (cycles/1000000);
}

inline long long OscSupCycToMicroSecs64(long long cycles)
{
	return (cycles);
}

inline long long OscSupCycToMilliSecs64(long long cycles)
{
	return (cycles/1000);
}

inline long long OscSupCycToSecs64(long long cycles)
{
	return (cycles/1000000);
}


/*============================== SRAM =================================*/
void* OscSupSramAllocL1DataA(unsigned int size)
{
	if(size > SRAM_L1A_LENGTH)
		return NULL;
		
	/* Just allocate normal memory on host. */
	return malloc(size);
}

void* OscSupSramAllocL1DataB(unsigned int size)
{
	if(size > SRAM_L1B_LENGTH)
		return NULL;
		
	/* Just allocate normal memory on host. */
	return malloc(size);
}

void* OscSupSramAllocL1Data(unsigned int size)
{
	if((size > SRAM_L1A_LENGTH) && (size > SRAM_L1B_LENGTH))
		return NULL;
		
	/* Just allocate normal memory on host. */
	return malloc(size);
}

void* OscSupSramAllocL1Instr(unsigned int size)
{
	if(size > SRAM_INSTR_LENGTH)
		return NULL;
		
	/* Just allocate normal memory on host. */
	return malloc(size);
}

void* OscSupSramAllocScratch(unsigned int size)
{
	OscLog(ERROR, "%s: Allocating scratchpad not supported since "
					"not supported by uClinux yet!\n",
					__func__);
	return NULL;
}

OSC_ERR OscSupSramFree(void *pAddr)
{
	free(pAddr);
	return SUCCESS;
}


