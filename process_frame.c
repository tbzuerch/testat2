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

OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn, struct OSC_VIS_REGIONS *regions, uint8 Color);

void ProcessFrame(uint8 *pInputImg)
{
	int c, r;
	int nc = OSC_CAM_MAX_IMAGE_WIDTH/2;
	int siz = sizeof(data.u8TempImage[GRAYSCALE]);

	int Shift = 7;
	short Beta = 2;//the meaning is that in floating point the value of Beta is = 6/(1 << Shift) = 6/128 = 0.0469
	uint8 MaxForeground = 120;//the maximum foreground counter value (at 15 fps this corresponds to less than 10s)

	struct OSC_PICTURE Pic1, Pic2;//we require these structures to use Oscar functions
	struct OSC_VIS_REGIONS ImgRegions;//these contain the foreground objects

	if(data.ipc.state.nStepCounter == 1)
	{
		memset(data.u8TempImage[FGRCOUNTER], 0, sizeof(data.u8TempImage[FGRCOUNTER]));
	}
	else
	{
		/* this is the default case */
		for(r = 0; r < siz; r+= nc)/* we strongly rely on the fact that them images have the same size */
		{
			for(c = 0; c < nc; c++)
			{
				/* first determine the foreground estimate */
				data.u8TempImage[THRESHOLD][r+c] = (short) (data.u8TempImage[GRAYSCALE][r+c] > data.ipc.state.nThreshold ? 0 : 0xff);
				data.u8TempImage[BACKGROUND][r+c] = data.u8TempImage[THRESHOLD][r+c];
			}
		}

		/*
		{
			//for debugging purposes we log the background values to console out
			//we chose the center pixel of the image (adaption to other pixel is straight forward)
			int offs = nc*(OSC_CAM_MAX_IMAGE_HEIGHT/2)/2+nc/2;

			OscLog(INFO, "%d %d %d %d %d\n", (int) data.u8TempImage[GRAYSCALE][offs], (int) data.u8TempImage[BACKGROUND][offs], (int) data.u8TempImage[BACKGROUND][offs]-data.ipc.state.nThreshold,
											 (int) data.u8TempImage[BACKGROUND][offs]+data.ipc.state.nThreshold, (int) data.u8TempImage[FGRCOUNTER][offs]);
		}
		*/
/*
		for(r = nc; r < siz-nc; r+= nc)
		{
			for(c = 1; c < nc-1; c++)
			{
				unsigned char* p = &data.u8TempImage[THRESHOLD][r+c];
				data.u8TempImage[EROSION][r+c] = *(p-nc-1) & *(p-nc) & *(p-nc+1) &
												 *(p-1)    & *p      & *(p+1)    &
												 *(p+nc-1) & *(p+nc) & *(p+nc+1);
			}
		}

		for(r = nc; r < siz-nc; r+= nc)
		{
			for(c = 1; c < nc-1; c++)
			{
				unsigned char* p = &data.u8TempImage[EROSION][r+c];
				data.u8TempImage[DILATION][r+c] = *(p-nc-1) | *(p-nc) | *(p-nc+1) |
												  *(p-1)    | *p      | *(p+1)    |
												  *(p+nc-1) | *(p+nc) | *(p+nc+1);
			}
		}
*/
		//wrap image DILATION in picture struct
		Pic1.data = data.u8TempImage[BACKGROUND];
		Pic1.width = nc;
		Pic1.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic1.type = OSC_PICTURE_GREYSCALE;
		//as well as EROSION (will be used as output)
		Pic2.data = data.u8TempImage[THRESHOLD];
		Pic2.width = nc;
		Pic2.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic2.type = OSC_PICTURE_BINARY;//probably has no consequences
		//have to convert to OSC_PICTURE_BINARY which has values 0x01 (and not 0xff)
		OscVisGrey2BW(&Pic1, &Pic2, 0x80, false);

		//now do region labeling and feature extraction
		OscVisLabelBinary( &Pic2, &ImgRegions);
		OscVisGetRegionProperties( &ImgRegions);

		//OscLog(INFO, "number of objects %d\n", ImgRegions.noOfObjects);
		//plot bounding boxes both in gray and dilation image
		Pic2.data = data.u8TempImage[GRAYSCALE];
		OscVisDrawBoundingBoxBW( &Pic2, &ImgRegions, 255);
		OscVisDrawBoundingBoxBW( &Pic1, &ImgRegions, 128);
	}
}


/* Drawing Function for Bounding Boxes; own implementation because Oscar only allows colored boxes; here in Gray value "Color"  */
/* should only be used for debugging purposes because we should not drawn into a gray scale image */
OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn, struct OSC_VIS_REGIONS *regions, uint8 Color)
{
	 uint16 i, o;
	 uint8 *pImg = (uint8*)picIn->data;
	 const uint16 width = picIn->width;
	 for(o = 0; o < regions->noOfObjects; o++)//loop over regions
	 {
		 /* Draw the horizontal lines. */
		 for (i = regions->objects[o].bboxLeft; i < regions->objects[o].bboxRight; i += 1)
		 {
				 pImg[width * regions->objects[o].bboxTop + i] = Color;
				 pImg[width * (regions->objects[o].bboxBottom - 1) + i] = Color;
		 }

		 /* Draw the vertical lines. */
		 for (i = regions->objects[o].bboxTop; i < regions->objects[o].bboxBottom-1; i += 1)
		 {
				 pImg[width * i + regions->objects[o].bboxLeft] = Color;
				 pImg[width * i + regions->objects[o].bboxRight] = Color;
		 }
	 }
	 return SUCCESS;
}


