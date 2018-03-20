/*
  TP 3 GLO-2001 Systèmes d'exploitation.
  Interface disque <-> steganographie.
*/

#include "lodepng.h"
#include "UFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define NUM_BITS_PER_PIXEL 1
#define numPixelsPerBlock (8*BLOCK_SIZE/NUM_BITS_PER_PIXEL)

typedef struct {
	unsigned char *image;
	unsigned width, height;
} ImageStruct;


// Je vais hardcoder le nom de l'image, pour simplifier.
static char imageFilename[] = "google-go.png";

char getLeastSignificantBitAndShiftToSpecifiedPositionFromChar(const unsigned char val, const unsigned int bitPosition) {
	if (bitPosition > 7) {
		printf("getBitAtSpecifiedPositionFromChar cannot check for position %d greater than 7!\n",bitPosition);
		exit(-1);
	}
	return (val & 0x01) << bitPosition;
} 


ImageStruct openSteganoFile() {
	// On vérifie si on a les droits de lecture du fichier passé en argument
	if (access(imageFilename,R_OK) == -1) {
		// Le fichier n'existe pas ou n'a pas les bons droits
  		printf("Le fichier %s n'existe pas ou n'a pas les bons droits!\n",imageFilename);
  		exit(-1);
	}

	ImageStruct ImageData;
	unsigned error = lodepng_decode32_file(&ImageData.image, &ImageData.width, &ImageData.height, imageFilename);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		exit(-1);
	}
	if ( (ImageData.width*ImageData.height*3) < (numPixelsPerBlock*N_BLOCK_ON_DISK) ) {
		printf("Image size %dx%d is too small to store filesystem!\n",ImageData.width,ImageData.height);
		exit(-1);
 	}
	return ImageData;
}


int closeSteganoFile(ImageStruct imageData) {
	int RetCode = 1;

	// On sauvegarde le fichier
	unsigned error = lodepng_encode32_file(imageFilename, imageData.image, imageData.width, imageData.height);

	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		RetCode = -1;
	}

	free(imageData.image);
	return RetCode;
}
  

int ReadBlock(UINT16 BlockNum, char *pBuffer) {
	if ((BlockNum > N_BLOCK_ON_DISK) || (BlockNum < 0) ) {
		printf("ReadBlock ne peut lire le bloc %d\n",BlockNum);
		return 0;
	}
	
	ImageStruct imageData = openSteganoFile();
	
	if (imageData.image == NULL) {
		printf("ReadBlock ne peut ouvrir le fichier %s\n",imageFilename);
		return -1;
	}
	
	unsigned long int iPixel,startPixel=BlockNum*numPixelsPerBlock;
	for (iPixel=startPixel; iPixel < (startPixel + numPixelsPerBlock); iPixel+=8) {
		unsigned iBit;
		(*pBuffer) = 0;
		for (iBit=0; iBit<8; iBit++) {
			(*pBuffer) = (*pBuffer) | getLeastSignificantBitAndShiftToSpecifiedPositionFromChar(imageData.image[iPixel+iBit],iBit);
		}	
		pBuffer++;
	}
	return BLOCK_SIZE;
}	

int WriteBlock(UINT16 BlockNum, const char *pBuffer) {
	if ((BlockNum > N_BLOCK_ON_DISK) || (BlockNum < 0) )  {
		printf("WriteBlock ne peut ecrire le bloc %d\n",BlockNum);
		return 0;
	}
	
	ImageStruct imageData = openSteganoFile();
	
	if (imageData.image == NULL) {
		printf("ReadBlock ne peut ouvrir le fichier %s\n",imageFilename);
		return -1;
	}
	
	unsigned long int iPixel,startPixel=BlockNum*numPixelsPerBlock;
	for (iPixel=startPixel; iPixel < (startPixel + numPixelsPerBlock); iPixel+=8) {
		unsigned iBit;
		for (iBit=0; iBit<8; iBit++) {
			unsigned char bitMask = (0x01 << iBit);
			unsigned char bitToCopy = (*pBuffer) & bitMask;
			if (bitToCopy) {
				imageData.image[iPixel+iBit] |= 0x01;
			}
			else {
				imageData.image[iPixel+iBit] &= 0xFE;
			}
		}
		pBuffer++;
	}
	
	closeSteganoFile(imageData);
	
	return BLOCK_SIZE;
}


