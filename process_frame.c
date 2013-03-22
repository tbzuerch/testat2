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

/* min and max not defined in stdlib ? */
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


void ProcessFrame(uint8 *pInputImg)
{
	int c, r;
	int nc = OSC_CAM_MAX_IMAGE_WIDTH/2;/* we work only on half of the camera image width */
	int siz = sizeof(data.u8TempImage[GRAYSCALE]);

	/* initialize image to zero (can be skipped if performance is an issue */
	/* we use 'THRESHOLD' for gradient in x-direction */ 
	memset(data.u8TempImage[THRESHOLD], 0, siz);
	for(r = nc; r < siz-nc; r+= nc)/* we skip the first and last line */
	{
		for(c = 1; c < nc-1; c++)
		{	/* do pointer arithmetics with respect to center pixel location */
			unsigned char* p = &data.u8TempImage[GRAYSCALE][r+c];
			/* implement Sobel filter (shift by 128 to 'center' grey values */
			short v = 128   -(short) *(p-nc-1) + (short) *(p-nc+1) 
					-2* (short) *(p-1) + 2* (short) *(p+1) 
					-(short) *(p+nc-1) + (short) *(p+nc+1);

			/* apply min()/max() to avoid wrap around of values below 0 and above 255 */
			data.u8TempImage[THRESHOLD][r+c] = (unsigned char) max(0, min(255, v));
		}
	}

	/* initialize image to zero (can be skipped if performance is an issue */
	/* we use 'BACKGROUND' for gradient in y-direction */ 	
	memset(data.u8TempImage[BACKGROUND], 0, siz);
	for(r = nc; r < siz-nc; r+= nc)/* we skip the first and last line */
	{
		for(c = 1; c < nc-1; c++)
		{	/* do pointer arithmetics with respect to center pixel location */
			unsigned char* p = &data.u8TempImage[GRAYSCALE][r+c];
			/* implement Sobel filter (shift by 128 to 'center' grey values */			
			short v = 128   - (short) *(p-nc-1) - 2* (short) *(p-nc) - (short) *(p-nc+1)
					+ (short) *(p+nc-1) + 2* (short) *(p+nc) + (short) *(p+nc+1);

			/* apply min()/max() to avoid wrap around of values below 0 and above 255 */
			data.u8TempImage[BACKGROUND][r+c] = (unsigned char) max(0, min(255, v));
		}
	}
}

