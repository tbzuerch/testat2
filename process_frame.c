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
#include <math.h>

uint32 Hist[256];
uint8 getOtsuK(void);


OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn, struct OSC_VIS_REGIONS *regions, uint8 Color);

void ProcessFrame(uint8 *pInputImg, uint8 isManual)
{
	int c, r;
	int nc = OSC_CAM_MAX_IMAGE_WIDTH/2;
	int siz = sizeof(data.u8TempImage[GRAYSCALE]);
	uint8 k;


	struct OSC_PICTURE Pic1,Pic2;//we require these structures to use Oscar functions
	struct OSC_VIS_REGIONS ImgRegions;//these contain the foreground objects

	if(data.ipc.state.nStepCounter == 1)
	{
		 //memcpy(data.u8TempImage[MANUAL], data.u8TempImage[GRAYSCALE], sizeof(data.u8TempImage[GRAYSCALE]));
	}
	else
	{
		if(isManual){
			k=data.ipc.state.nThreshold;
		}
		else{
			k=getOtsuK();
		}
		/* this is the default case */
		for(r = 0; r < siz; r+= nc)/* we strongly rely on the fact that them images have the same size */
		{
			for(c = 0; c < nc; c++)
			{
				data.u8TempImage[MANUAL_THRESHOLD][r+c] = (uint8) (data.u8TempImage[GRAYSCALE][r+c] > k ? 0 : 0xff);
			}
		}



		Pic1.data = data.u8TempImage[MANUAL_THRESHOLD];
		Pic1.width = nc;
		Pic1.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic1.type = OSC_PICTURE_GREYSCALE;

		Pic2.data = data.u8TempImage[BW];
		Pic2.width = nc;
		Pic2.height = OSC_CAM_MAX_IMAGE_HEIGHT/2;
		Pic2.type = OSC_PICTURE_GREYSCALE;

		OscVisGrey2BW(&Pic1, &Pic2, 0x80, false);

		//now do region labeling and feature extraction
		OscVisLabelBinary( &Pic2, &ImgRegions);
		OscVisGetRegionProperties( &ImgRegions);

		//OscLog(INFO, "number of objects %d\n", ImgRegions.noOfObjects);
		//plot bounding boxes both in gray and dilation image

		Pic1.data = data.u8TempImage[MANUAL_THRESHOLD];
		OscVisDrawBoundingBoxBW( &Pic1, &ImgRegions, 255);
		Pic1.data = data.u8TempImage[GRAYSCALE];
		OscVisDrawBoundingBoxBW( &Pic1, &ImgRegions, 255);
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

uint8 getOtsuK(void){

		int i,k;
		int siz = sizeof(data.u8TempImage[GRAYSCALE]);
		uint32 Hist[256], w0, w1, m0, m1;
		double sb_max,sb;
		int k_max;
		uint8* p= data.u8TempImage[GRAYSCALE];
		sb_max=0;
		k_max=0;

		memset(Hist, 0, sizeof(Hist));


		for(i = 0; i < siz; i++){
			Hist[p[i]]++;
		}

		for(k=0;k<256;k++){
			w0=w1=m0=m1=0;

			for(i=0;i<k;i++){
				m0 += Hist[i]*i;
				w0 += Hist[i];
			}


			for(i=k;i<256;i++){
				m1 += Hist[i]*i;
				w1 += Hist[i];
			}
			//sb=(uint32)((double)w0*w1)*pow(((double)m0/w0)-((double)m1/w1),2);
			  sb=w0*w1*(m0/(float)w0 -m1/(float)w1)*(m0/(float)w0 -m1/(float)w1);
			if(sb>sb_max){
				sb_max=sb;
				k_max=k;
			}
		}
		sb_max=sb_max/(((uint32)376*240)*(376*240));
		//OscLog(INFO, "k %d\n", (k_max));
		return k_max;

}

