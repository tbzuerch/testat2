/* Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty. This file is offered as-is,
 * without any warranty.
 */

/*! @file process_frame.c
 * @brief Contains the actual algorithm and calculations.
 */

/* Definitions specific to this application. Also includes the Oscar main header file. */
#include "template.h"
#include <string.h>
#include <stdlib.h>


void ProcessFrame(uint8 *pInputImg)
{
	int c, r;
	int Shift = 7;
	short Beta = 2;//the meaning is that in floating point the value of Beta is = 6/(1 << Shift) = 6/128 = 0.0469

	if(data.ipc.state.nStepCounter == 1)
	{
		/* this is the first time we call this function */
		/* first time we call this; index 1 always has the background image */
		memcpy(data.u8TempImage[BACKGROUND], data.u8TempImage[GRAYSCALE], sizeof(data.u8TempImage[GRAYSCALE]));
	}
	else
	{
		/* this is the default case */
		for(r = 0; r < sizeof(data.u8TempImage[GRAYSCALE]); r+= OSC_CAM_MAX_IMAGE_WIDTH/2)/* we strongly rely on the fact that them images have the same size */
		{
			for(c = 0; c < OSC_CAM_MAX_IMAGE_WIDTH/2; c++)
			{
				/* first determine the forground estimate */
				data.u8TempImage[THRESHOLD][r+c] = abs(data.u8TempImage[GRAYSCALE][r+c]-data.u8TempImage[BACKGROUND][r+c]) < data.ipc.state.nThreshold ? 0 : 255;

				/* now update the background image; the value of background should be corrected by the following difference (* 1/128) */
				short Diff = Beta*((short) data.u8TempImage[GRAYSCALE][r+c] - (short) data.u8TempImage[BACKGROUND][r+c]);

				if(abs(Diff) >= 128) //we will have a correction - apply it (this also avoids the "bug" that -1 >> 1 = -1)
					data.u8TempImage[BACKGROUND][r+c] += (uint8) (Diff >> Shift);
				else //due to the division by 128 the correction would be zero -> thus add/subtract at least unity
				{
					if(Diff > 0 && data.u8TempImage[BACKGROUND][r+c] < 255)
							data.u8TempImage[BACKGROUND][r+c] += 1;
					else if(Diff < 0 && data.u8TempImage[BACKGROUND][r+c] > 1)
							data.u8TempImage[BACKGROUND][r+c] -= 1;
				}
			}
		}
	}
}

