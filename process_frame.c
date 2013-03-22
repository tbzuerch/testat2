/* Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty. This file is offered as-is,
 * without any warranty.
 */

/*! @file process_frame.c
 * @brief Contains the actual algorithm and calculations.
 */

/* Definitions specific to this application. Also includes the Oscar main header file. */
#include "template.h"

void ProcessFrame(uint8 *pInputImg)
{
	int c, r;
	
	for(r = 0; r < sizeof(data.u8ResultImage); r+= OSC_CAM_MAX_IMAGE_WIDTH/2)
	{
		for(c = 0; c < OSC_CAM_MAX_IMAGE_WIDTH/2; c++)
		{
			data.u8ResultImage[r+c] = 255-pInputImg[r+c];

			/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
			/* |                                                                 */
			/* |                    Add your code here                           */
			/* |                                                                 */
			/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		}
	}

}
