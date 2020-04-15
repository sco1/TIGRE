#define _CRT_SECURE_NO_WARNINGS

#include "io64.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
//**** C data types are defined in tmwtypes.h
#include <tmwtypes.h>
#include "mex.h"
#include <math.h>
#include "matrix.h"
#include "XimPara.hpp"


int cReadXim(char *XimFullFile, XimPara *XimStr, int *XimImg);

void mexFunction(
        int nlhs , mxArray *plhs[],
        int nrhs, mxArray const *prhs[])
{
    //check input variable
    if (mxIsChar(prhs[0]) != 1)
    mexErrMsgIdAndTxt( "MATLAB:revord:inputNotString",
          "Input must be a string.");
    
    // .xim filename
    char *filename;
    filename = mxArrayToString(prhs[0]);
    mexPrintf("%s\n", filename);

    // file open
    FILE *fid = fopen(filename, "rb");    
    if(fid == NULL)
    {
        mexPrintf("%s open failed.\n", filename);
        getchar();
        exit(1);
    }

    // Parameter structure
    XimPara *para = new XimPara[1];
    
    // file pointer position
    fpos_t position = 0;
    
	// Skip useless information
    // 8 * sizeof(char) + sizeof(int32_t);
	position = 8*sizeof(char) + sizeof(int32_T); 
	setFilePos(fid, (fpos_t*) &position);
	// Read ImgWidth & ImgHeight (int32)
	fread(&(para->ImgWidth), sizeof(int32_T), 1, fid);
	fread(&(para->ImgHeight), sizeof(int32_T), 1, fid);
    fclose(fid);

    para->PixelNO = para->ImgWidth * para->ImgHeight;

    int *frame;    
	plhs[0] = mxCreateNumericMatrix(para->ImgWidth, para->ImgHeight, mxINT32_CLASS, mxREAL);
    frame = (int*)mxGetPr(plhs[0]);

	// empty file return
	if (para->PixelNO == 0)
    {
    	plhs[1] = mxCreateDoubleScalar(10000);
    	mexPrintf("%s is an empty file\n", filename);
        return;
    }
        
	/******* Kernel Function *********/
    cReadXim(filename, para, frame);

	/**** GantryRtn is the only parameter-of-interest to return ****/
	double GantryRtn = para->GantryRtn;
	plhs[1] = mxCreateDoubleScalar(para->GantryRtn);

}

/************* Kernel Funtion to read .xim ***************/
// Kernel function
int cReadXim(char *XimFullFile,
	XimPara *XimStr,
	int *XimImg)
{
	// Read the .xim file name

//	char *ptr = strrchr(XimFullFile, '\\');
//	sprintf(XimStr->FileName, "%s", ptr + 1);

	// ****** Open .xim File Pointer ***********//
	FILE *fid = fopen(XimFullFile, "rb");
    
	// Syntax Parsing
	if (fid == NULL)
	{
		printf("Error: file %s doesn't exist, at all\n", XimFullFile);
        getchar();
		exit(1);
	}

    // ******* Stage 1: Portal Image Data ****//
	// Skip useless information
	fseek(fid, 8 * sizeof(char) + sizeof(int32_T), SEEK_CUR);

	// Read ImgWidth & ImgHeight
	fread(&(XimStr->ImgWidth), sizeof(int32_T), 1, fid);
	fread(&(XimStr->ImgHeight), sizeof(int32_T), 1, fid);
	XimStr->PixelNO = (XimStr->ImgWidth)*(XimStr->ImgHeight);

	// Skip the useless information: bits_per_pixel
	fseek(fid, sizeof(int32_T), SEEK_CUR);

	// Load .xim file compression  parameters
	fread(&(XimStr->BytesPerPixel), sizeof(int32_T), 1, fid);
	fread(&(XimStr->Compression_Indicator), sizeof(int32_T), 1, fid);

	// Load .xim Pixel Data
	if (1 == XimStr->Compression_Indicator)
	{
		int LookUpTableSize = 0;
		fread(&LookUpTableSize, sizeof(int), 1, fid);

		int *LookUpTable = new int[XimStr->ImgHeight * XimStr->ImgWidth];
		memset(LookUpTable, 0, XimStr->ImgHeight * XimStr->ImgWidth * sizeof(int));

		// Load the LookUpTable data
		for (int ii = 0; ii < LookUpTableSize; ii++)
		{
			// Load in the 8-bit date
			//unsigned __int8 tmp = 0;
			//fread(&tmp, sizeof(unsigned __int8), 1, fid);
            int tmp = 0;
			fread(&tmp, sizeof(uint8_T), 1, fid);
			
			int Bit2[4] = { 0 };

			// extract the lookup_table data
			for (int jj = 0; jj < 8; jj = jj +2)
			{
				Bit2[jj/2] = ((tmp & 1 << jj) != 0);
				// It's 4, because 1 unsigned __int8 in tmp is represented by 4 ints in LookUpTable.
				LookUpTable[ii * 4 + jj / 2] = Bit2[jj / 2];
				
				//printf("Index = %d, LookUpTable = %d\n", ii * 4 + jj / 2, LookUpTable[ii * 4 + jj / 2]);
			}
		}

		// Skip compressed_pixel_buffer_size: passed
		fseek(fid, sizeof(int32_T), SEEK_CUR);

		// Allocate memory for XimImg
		fread(XimImg, sizeof(int32_T), (XimStr->ImgWidth) + 1, fid);

		// load the compressed pixel data
		int delta = 0;
		int LUT_Pos = 0;

		// You must be very careful with all data types!!!
		int8_T tmp8 = 0;
		int16_T tmp16 = 0;

		for (int ImgTag = XimStr->ImgWidth + 1;
			ImgTag < (XimStr->ImgHeight) * (XimStr->ImgWidth);
			ImgTag++)
		{
			if (0 == LookUpTable[LUT_Pos])
			{
				fread(&tmp8, sizeof(int8_T), 1, fid);
				delta = int(tmp8);
			}
			else if (1 == LookUpTable[LUT_Pos])
			{
				fread(&tmp16, sizeof(int16_T), 1, fid);
				delta = int(tmp16);
			}
			else
			{
				fread(&delta, sizeof(int32_T), 1, fid);
			}
			
			XimImg[ImgTag] = delta + XimImg[ImgTag - 1]
				+ XimImg[ImgTag - XimStr->ImgWidth]
				- XimImg[ImgTag - XimStr->ImgWidth - 1];

			LUT_Pos = LUT_Pos + 1;
		}

		// Skip uncompressed_pixel_buffer_size
		fseek(fid, sizeof(int32_T), SEEK_CUR);

	}
	else
	{
		// Be careful: the code block for uncompressed pixel data readout hasn't been tested yet.
		// Date: 2017-09-12
		int BufferSize = 0;
		fread(&BufferSize, sizeof(int), 1, fid);

		switch (XimStr->BytesPerPixel)
		{
		case 1:	
		{
			__int8 *buffer8 = new __int8[XimStr->ImgWidth * XimStr->ImgHeight];
			memset(buffer8, 0, sizeof(__int8)* XimStr->ImgWidth * XimStr->ImgHeight);
			fread(buffer8, sizeof(__int8), BufferSize, fid);
			for (int ii = 0; ii < XimStr->ImgWidth * XimStr->ImgHeight;ii++)
			{
				XimImg[ii] = int(buffer8[ii]);
			}
			break;
		}
		case 2:
		{
			__int16 *buffer16 = new __int16[XimStr->ImgWidth * XimStr->ImgHeight];
			memset(buffer16, 0, sizeof(__int16)* XimStr->ImgWidth * XimStr->ImgHeight);
			fread(buffer16, sizeof(__int16), BufferSize / 2, fid);
			for (int ii = 0; ii < XimStr->ImgWidth * XimStr->ImgHeight; ii++)
			{
				XimImg[ii] = int(buffer16[ii]);
			}
			break;
		}
		default:
		{
			fread(XimImg, sizeof(int), BufferSize / 4, fid);
			break;
		}
		}
	}

	
	// ******* Stage 2: load the gantry angle from the residual property data ****//
	// Skip histogram
	int tmp = 0;
	fread(&tmp, sizeof(int), 1, fid);
	if (tmp > 0)
	{
		fseek(fid, tmp* sizeof(int), SEEK_CUR);
	}

	// Decode .xim properties
	int nProperties = 0;
	fread(&nProperties, sizeof(int), 1, fid);
	// Property structure is not NULL
	if (nProperties > 0)
	{
		int pName_len = 0;
		// Only load the property name rather than the content
		char pName[64] = { 0 };
		int pType = 0;
		for (int ii = 0; ii < nProperties; ii++)
		{
			// load property name length
			fread(&pName_len, sizeof(int), 1, fid);
			// load property name
			fread(pName, sizeof(char)* pName_len, 1, fid);
			// load property data type
			fread(&pType, sizeof(int), 1, fid);
			
			// extract the Gantry Rotation Angle
			if (!strcmp(pName, "GantryRtn"))
			{
				fread(&(XimStr->GantryRtn), sizeof(double), 1, fid);
				break;
			}
			else
			{
				switch (pType)
				{
				case 0:
				{
					fseek(fid, sizeof(int), SEEK_CUR);
					break;
				}
				case 1:
				{
					fseek(fid, sizeof(double), SEEK_CUR);
					break;
				}
				case 2:
				{
					int skiplen = 0;
					fread(&skiplen, sizeof(int), 1, fid);
					fseek(fid, sizeof(char) * skiplen, SEEK_CUR);
					break;
				}
				case 4:
				{
					int skiplen = 0;
					fread(&skiplen, sizeof(int), 1, fid);
					fseek(fid, sizeof(double) * skiplen, SEEK_CUR);
					break;
				}
				case 5:
				{
					int skiplen = 0;
					fread(&skiplen, sizeof(int), 1, fid);
					fseek(fid, sizeof(int) * skiplen, SEEK_CUR);
					break;
				}
				break;
				}
			}
			// reset all the temporary variables 
			pName_len = 0;
			memset(pName, 0, 64*sizeof(char));
			pType = 0;
		}

	}

	// ********* END of XIM Reading: Close the File Pointer******* //
	if (fclose(fid))
	{
		printf("The file `crt_fopen.c' was not closed\n");
		getchar();
		exit(1);
	}
    
	return 1;
}
