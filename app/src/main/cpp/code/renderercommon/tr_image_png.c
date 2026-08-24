/*
===========================================================================
ioquake3 png decoder
Copyright (C) 2007,2008 Joerg Dietrich

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tr_common.h"

#include "puff.h"

#ifndef INT_MAX
#define INT_MAX 0x1fffffff
#endif



#define Q3IMAGE_BYTESPERPIXEL (4)



#define PNG_Signature "\x89\x50\x4E\x47\xD\xA\x1A\xA"
#define PNG_Signature_Size (8)


struct PNG_ChunkHeader
{
	uint32_t Length;
	uint32_t Type;
};

#define PNG_ChunkHeader_Size (8)

typedef uint32_t PNG_ChunkCRC;

#define PNG_ChunkCRC_Size (4)


#define MAKE_CHUNKTYPE(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d)))

#define PNG_ChunkType_IHDR MAKE_CHUNKTYPE('I', 'H', 'D', 'R')
#define PNG_ChunkType_PLTE MAKE_CHUNKTYPE('P', 'L', 'T', 'E')
#define PNG_ChunkType_IDAT MAKE_CHUNKTYPE('I', 'D', 'A', 'T')
#define PNG_ChunkType_IEND MAKE_CHUNKTYPE('I', 'E', 'N', 'D')
#define PNG_ChunkType_tRNS MAKE_CHUNKTYPE('t', 'R', 'N', 'S')


struct PNG_Chunk_IHDR
{
	uint32_t Width;
	uint32_t Height;
	uint8_t  BitDepth;
	uint8_t  ColourType;
	uint8_t  CompressionMethod;
	uint8_t  FilterMethod;
	uint8_t  InterlaceMethod;
};

#define PNG_Chunk_IHDR_Size (13)


#define PNG_ColourType_Grey      (0)
#define PNG_ColourType_True      (2)
#define PNG_ColourType_Indexed   (3)
#define PNG_ColourType_GreyAlpha (4)
#define PNG_ColourType_TrueAlpha (6)


#define PNG_NumColourComponents_Grey      (1)
#define PNG_NumColourComponents_True      (3)
#define PNG_NumColourComponents_Indexed   (1)
#define PNG_NumColourComponents_GreyAlpha (2)
#define PNG_NumColourComponents_TrueAlpha (4)


#define PNG_BitDepth_1  ( 1)
#define PNG_BitDepth_2  ( 2)
#define PNG_BitDepth_4  ( 4)
#define PNG_BitDepth_8  ( 8)
#define PNG_BitDepth_16 (16)


#define PNG_CompressionMethod_0 (0)


#define PNG_FilterMethod_0 (0)


#define PNG_FilterType_None    (0)
#define PNG_FilterType_Sub     (1)
#define PNG_FilterType_Up      (2)
#define PNG_FilterType_Average (3)
#define PNG_FilterType_Paeth   (4)


#define PNG_InterlaceMethod_NonInterlaced (0)
#define PNG_InterlaceMethod_Interlaced    (1)


#define PNG_Adam7_NumPasses (7)


struct PNG_ZlibHeader
{
	uint8_t CompressionMethod;
	uint8_t Flags;
};

#define PNG_ZlibHeader_Size (2)


#define PNG_ZlibCheckValue_Size (4)



struct BufferedFile
{
	byte *Buffer;
	int   Length;
	byte *Ptr;
	int   BytesLeft;
};


static struct BufferedFile *ReadBufferedFile(const char *name)
{
	struct BufferedFile *BF;
	union {
		byte *b;
		void *v;
	} buffer;


	if(!name)
	{
		return(NULL);
	}


	BF = ri.Malloc(sizeof(struct BufferedFile));
	if(!BF)
	{
		return(NULL);
	}


	BF->Length    = 0;
	BF->Buffer    = NULL;
	BF->Ptr       = NULL;
	BF->BytesLeft = 0;


	BF->Length = ri.FS_ReadFile((char *) name, &buffer.v);
	BF->Buffer = buffer.b;


	if(!(BF->Buffer && (BF->Length > 0)))
	{
		ri.Free(BF);

		return(NULL);
	}


	BF->Ptr       = BF->Buffer;
	BF->BytesLeft = BF->Length;

	return(BF);
}


static void CloseBufferedFile(struct BufferedFile *BF)
{
	if(BF)
	{
		if(BF->Buffer)
		{
			ri.FS_FreeFile(BF->Buffer);
		}

		ri.Free(BF);
	}
}


static void *BufferedFileRead(struct BufferedFile *BF, unsigned Length)
{
	void *RetVal;


	if(!(BF && Length))
	{
		return(NULL);
	}


	if(Length > BF->BytesLeft)
	{
		return(NULL);
	}


	RetVal = BF->Ptr;


	BF->Ptr       += Length;
	BF->BytesLeft -= Length;

	return(RetVal);
}


static qboolean BufferedFileRewind(struct BufferedFile *BF, unsigned Offset)
{
	unsigned BytesRead; 


	if(!BF)
	{
		return(qfalse);
	}


	if(Offset == (unsigned)-1)
	{
		BF->Ptr       = BF->Buffer;
		BF->BytesLeft = BF->Length;

		return(qtrue);
	}


	BytesRead = BF->Ptr - BF->Buffer;


	if(Offset > BytesRead)
	{
		return(qfalse);
	}


	BF->Ptr       -= Offset;
	BF->BytesLeft += Offset;

	return(qtrue);
}


static qboolean BufferedFileSkip(struct BufferedFile *BF, unsigned Offset)
{

	if(!BF)
	{
		return(qfalse);
	}


	if(Offset > BF->BytesLeft)
	{
		return(qfalse);
	}


	BF->Ptr       += Offset;
	BF->BytesLeft -= Offset;

	return(qtrue);
}


static qboolean FindChunk(struct BufferedFile *BF, uint32_t ChunkType)
{
	struct PNG_ChunkHeader *CH;

	uint32_t Length;
	uint32_t Type;


	if(!BF)
	{
		return(qfalse);
	}


	while(qtrue)
	{

		CH = BufferedFileRead(BF, PNG_ChunkHeader_Size);
		if(!CH)
		{
			return(qfalse);
		}


		Length = BigLong(CH->Length);
		Type   = BigLong(CH->Type);


		if(Type == ChunkType)
		{

			BufferedFileRewind(BF, PNG_ChunkHeader_Size);

			break;
		}
		else
		{

			if(Length)
			{
				if(!BufferedFileSkip(BF, Length + PNG_ChunkCRC_Size))
				{
					return(qfalse);
				}  
			}
		}
	}

	return(qtrue);
}


static uint32_t DecompressIDATs(struct BufferedFile *BF, uint8_t **Buffer)
{
	uint8_t  *DecompressedData;
	uint32_t  DecompressedDataLength;

	uint8_t  *CompressedData;
	uint8_t  *CompressedDataPtr;
	uint32_t  CompressedDataLength;

	struct PNG_ChunkHeader *CH;

	uint32_t Length;
	uint32_t Type;

	int BytesToRewind;

	int32_t   puffResult;
	uint8_t  *puffDest;
	uint32_t  puffDestLen;
	uint8_t  *puffSrc;
	uint32_t  puffSrcLen;


	if(!(BF && Buffer))
	{
		return((uint32_t)-1);
	}


	DecompressedData = NULL;
	*Buffer = DecompressedData;

	CompressedData = NULL;
	CompressedDataLength = 0;

	BytesToRewind = 0;


	if(!FindChunk(BF, PNG_ChunkType_IDAT))
	{
		return((uint32_t)-1);
	}


	while(qtrue)
	{

		CH = BufferedFileRead(BF, PNG_ChunkHeader_Size);
		if(!CH)
		{

			BufferedFileRewind(BF, BytesToRewind);

			return((uint32_t)-1);
		}


		Length = BigLong(CH->Length);
		Type   = BigLong(CH->Type);


		if(!(Type == PNG_ChunkType_IDAT))
		{
			BufferedFileRewind(BF, PNG_ChunkHeader_Size); 

			break;
		}


		BytesToRewind += PNG_ChunkHeader_Size;


		if(Length)
		{
			if(!BufferedFileSkip(BF, Length + PNG_ChunkCRC_Size))
			{
				BufferedFileRewind(BF, BytesToRewind);

				return((uint32_t)-1);
			}

			BytesToRewind += Length + PNG_ChunkCRC_Size;
			CompressedDataLength += Length;
		} 
	}

	BufferedFileRewind(BF, BytesToRewind);

	CompressedData = ri.Malloc(CompressedDataLength);
	if(!CompressedData)
	{
		return((uint32_t)-1);
	}

	CompressedDataPtr = CompressedData;


	while(qtrue)
	{

		CH = BufferedFileRead(BF, PNG_ChunkHeader_Size);
		if(!CH)
		{
			ri.Free(CompressedData); 

			return((uint32_t)-1);
		}


		Length = BigLong(CH->Length);
		Type   = BigLong(CH->Type);


		if(!(Type == PNG_ChunkType_IDAT))
		{
			BufferedFileRewind(BF, PNG_ChunkHeader_Size); 

			break;
		}


		if(Length)
		{
			uint8_t *OrigCompressedData;

			OrigCompressedData = BufferedFileRead(BF, Length);
			if(!OrigCompressedData)
			{
				ri.Free(CompressedData); 

				return((uint32_t)-1);
			}

			if(!BufferedFileSkip(BF, PNG_ChunkCRC_Size))
			{
				ri.Free(CompressedData); 

				return((uint32_t)-1);
			}

			memcpy(CompressedDataPtr, OrigCompressedData, Length);
			CompressedDataPtr += Length;
		} 
	}


	puffDest    = NULL;
	puffDestLen = 0;


	puffSrc    = CompressedData + PNG_ZlibHeader_Size;
	puffSrcLen = CompressedDataLength - PNG_ZlibHeader_Size - PNG_ZlibCheckValue_Size;


	puffResult = puff(puffDest, &puffDestLen, puffSrc, &puffSrcLen);
	if(!((puffResult == 0) && (puffDestLen > 0)))
	{
		ri.Free(CompressedData);

		return((uint32_t)-1);
	}


	DecompressedData = ri.Malloc(puffDestLen);
	if(!DecompressedData)
	{
		ri.Free(CompressedData);

		return((uint32_t)-1);
	}


	puffDest   = DecompressedData;
	puffSrc    = CompressedData + PNG_ZlibHeader_Size;
	puffSrcLen = CompressedDataLength - PNG_ZlibHeader_Size - PNG_ZlibCheckValue_Size;


	puffResult = puff(puffDest, &puffDestLen, puffSrc, &puffSrcLen);


	ri.Free(CompressedData);


	if(!((puffResult == 0) && (puffDestLen > 0)))
	{
		ri.Free(DecompressedData);

		return((uint32_t)-1);
	}


	DecompressedDataLength = puffDestLen;
	*Buffer = DecompressedData;

	return(DecompressedDataLength);
}


static uint8_t PredictPaeth(uint8_t a, uint8_t b, uint8_t c)
{

	uint8_t Pr;
	int p;
	int pa, pb, pc;

	p  = ((int) a) + ((int) b) - ((int) c);
	pa = abs(p - ((int) a));
	pb = abs(p - ((int) b));
	pc = abs(p - ((int) c));

	if((pa <= pb) && (pa <= pc))
	{
		Pr = a;
	}
	else if(pb <= pc)
	{
		Pr = b;
	}
	else
	{
		Pr = c;
	}

	return(Pr);

}


static qboolean UnfilterImage(uint8_t  *DecompressedData, 
		uint32_t  ImageHeight,
		uint32_t  BytesPerScanline, 
		uint32_t  BytesPerPixel)
{
	uint8_t   *DecompPtr;
	uint8_t   FilterType;
	uint8_t  *PixelLeft, *PixelUp, *PixelUpLeft;
	uint32_t  w, h, p;


	uint8_t Zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};


	if(!(DecompressedData && BytesPerPixel))
	{
		return(qfalse);
	}


	if((!ImageHeight) || (!BytesPerScanline))
	{
		return(qtrue);
	}


	DecompPtr = DecompressedData;



	for(h = 0; h < ImageHeight; h++)
	{

		FilterType = *DecompPtr;
		DecompPtr++;


		PixelLeft = Zeros;


		if(h > 0)
		{
			PixelUp = DecompPtr - (BytesPerScanline + 1);
		}
		else
		{
			PixelUp = Zeros;
		}


		PixelUpLeft = Zeros;


		for(w = 0; w < (BytesPerScanline / BytesPerPixel); w++)
		{

			for(p = 0; p < BytesPerPixel; p++)
			{
				switch(FilterType)
				{ 
					case PNG_FilterType_None :
					{

						break;
					}

					case PNG_FilterType_Sub :
					{
						DecompPtr[p] += PixelLeft[p];

						break;
					}

					case PNG_FilterType_Up :
					{
						DecompPtr[p] += PixelUp[p];

						break;
					}

					case PNG_FilterType_Average :
					{
						DecompPtr[p] += ((uint8_t) ((((uint16_t) PixelLeft[p]) + ((uint16_t) PixelUp[p])) / 2));

						break;
					}

					case PNG_FilterType_Paeth :
					{
						DecompPtr[p] += PredictPaeth(PixelLeft[p], PixelUp[p], PixelUpLeft[p]);

						break;
					}

					default :
					{
						return(qfalse);
					}
				}
			}

			PixelLeft = DecompPtr;


			if(h > 0)
			{
				PixelUpLeft = DecompPtr - (BytesPerScanline + 1);
			}


			DecompPtr += BytesPerPixel;


			if(h > 0)
			{
				PixelUp = DecompPtr - (BytesPerScanline + 1);
			}
		}
	}

	return(qtrue);
}


static qboolean ConvertPixel(struct PNG_Chunk_IHDR *IHDR,
		byte                  *OutPtr,
		uint8_t               *DecompPtr,
		qboolean               HasTransparentColour,
		uint8_t               *TransparentColour,
		uint8_t               *OutPal)
{

	if(!(IHDR && OutPtr && DecompPtr && TransparentColour && OutPal))
	{
		return(qfalse);
	}

	switch(IHDR->ColourType)
	{
		case PNG_ColourType_Grey :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_1 :
				case PNG_BitDepth_2 :
				case PNG_BitDepth_4 :
				{
					uint8_t Step;
					uint8_t GreyValue;

					Step = 0xFF / ((1 << IHDR->BitDepth) - 1);

					GreyValue = DecompPtr[0] * Step;

					OutPtr[0] = GreyValue;
					OutPtr[1] = GreyValue;
					OutPtr[2] = GreyValue;
					OutPtr[3] = 0xFF;


					if(HasTransparentColour)
					{
						if(TransparentColour[1] == DecompPtr[0])
						{
							OutPtr[3] = 0x00;
						}
					}


					break;
				}

				case PNG_BitDepth_8 :
				case PNG_BitDepth_16 :
				{
					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[0];
					OutPtr[2] = DecompPtr[0];
					OutPtr[3] = 0xFF;


					if(HasTransparentColour)
					{
						if(IHDR->BitDepth == PNG_BitDepth_8)
						{
							if(TransparentColour[1] == DecompPtr[0])
							{
								OutPtr[3] = 0x00;
							}
						}
						else
						{
							if((TransparentColour[0] == DecompPtr[0]) && (TransparentColour[1] == DecompPtr[1]))
							{
								OutPtr[3] = 0x00;
							}
						}
					}

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_True :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				{
					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[1];
					OutPtr[2] = DecompPtr[2];
					OutPtr[3] = 0xFF;


					if(HasTransparentColour)
					{
						if((TransparentColour[1] == DecompPtr[0]) &&
								(TransparentColour[3] == DecompPtr[1]) &&
								(TransparentColour[5] == DecompPtr[2]))
						{
							OutPtr[3] = 0x00;
						}
					}

					break;
				}

				case PNG_BitDepth_16 :
				{

					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[2];
					OutPtr[2] = DecompPtr[4];
					OutPtr[3] = 0xFF;


					if(HasTransparentColour)
					{
						if((TransparentColour[0] == DecompPtr[0]) && (TransparentColour[1] == DecompPtr[1]) &&
								(TransparentColour[2] == DecompPtr[2]) && (TransparentColour[3] == DecompPtr[3]) &&
								(TransparentColour[4] == DecompPtr[4]) && (TransparentColour[5] == DecompPtr[5]))
						{
							OutPtr[3] = 0x00;
						}
					}

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_Indexed :
		{
			OutPtr[0] = OutPal[DecompPtr[0] * Q3IMAGE_BYTESPERPIXEL + 0];
			OutPtr[1] = OutPal[DecompPtr[0] * Q3IMAGE_BYTESPERPIXEL + 1];
			OutPtr[2] = OutPal[DecompPtr[0] * Q3IMAGE_BYTESPERPIXEL + 2];
			OutPtr[3] = OutPal[DecompPtr[0] * Q3IMAGE_BYTESPERPIXEL + 3];

			break;
		}

		case PNG_ColourType_GreyAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				{
					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[0];
					OutPtr[2] = DecompPtr[0];
					OutPtr[3] = DecompPtr[1];

					break;
				}

				case PNG_BitDepth_16 :
				{

					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[0];
					OutPtr[2] = DecompPtr[0];
					OutPtr[3] = DecompPtr[2];

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_TrueAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				{
					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[1];
					OutPtr[2] = DecompPtr[2];
					OutPtr[3] = DecompPtr[3];

					break;
				}

				case PNG_BitDepth_16 :
				{

					OutPtr[0] = DecompPtr[0];
					OutPtr[1] = DecompPtr[2];
					OutPtr[2] = DecompPtr[4];
					OutPtr[3] = DecompPtr[6];

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		default :
		{
			return(qfalse);
		}
	}

	return(qtrue);
}



static qboolean DecodeImageNonInterlaced(struct PNG_Chunk_IHDR *IHDR,
		byte                  *OutBuffer, 
		uint8_t               *DecompressedData,
		uint32_t               DecompressedDataLength,
		qboolean               HasTransparentColour,
		uint8_t               *TransparentColour,
		uint8_t               *OutPal)
{
	uint32_t IHDR_Width;
	uint32_t IHDR_Height;
	uint32_t BytesPerScanline, BytesPerPixel, PixelsPerByte;
	uint32_t  w, h, p;
	byte *OutPtr;
	uint8_t *DecompPtr;


	if(!(IHDR && OutBuffer && DecompressedData && DecompressedDataLength && TransparentColour && OutPal))
	{
		return(qfalse);
	}


	IHDR_Width  = BigLong(IHDR->Width);
	IHDR_Height = BigLong(IHDR->Height);


	switch(IHDR->ColourType)
	{
		case PNG_ColourType_Grey :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_1 :
				case PNG_BitDepth_2 :
				case PNG_BitDepth_4 :
				{
					BytesPerPixel    = 1;
					PixelsPerByte    = 8 / IHDR->BitDepth;

					break;
				}

				case PNG_BitDepth_8  :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_Grey;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_True :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8  :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_True;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_Indexed :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_1 :
				case PNG_BitDepth_2 :
				case PNG_BitDepth_4 :
				{
					BytesPerPixel    = 1;
					PixelsPerByte    = 8 / IHDR->BitDepth;

					break;
				}

				case PNG_BitDepth_8 :
				{
					BytesPerPixel    = PNG_NumColourComponents_Indexed;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_GreyAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_GreyAlpha;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_TrueAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_TrueAlpha;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		default :
		{
			return(qfalse);
		}
	}


	BytesPerScanline = (IHDR_Width * BytesPerPixel + (PixelsPerByte - 1)) / PixelsPerByte;


	if(!(DecompressedDataLength == ((BytesPerScanline + 1) * IHDR_Height)))
	{
		return(qfalse);
	}


	if(!UnfilterImage(DecompressedData, IHDR_Height, BytesPerScanline, BytesPerPixel))
	{
		return(qfalse);
	}


	OutPtr = OutBuffer;
	DecompPtr = DecompressedData;


	for(h = 0; h < IHDR_Height; h++)
	{

		uint32_t CurrPixel;


		DecompPtr++;


		CurrPixel = 0;

		for(w = 0; w < (BytesPerScanline / BytesPerPixel); w++)
		{
			if(PixelsPerByte > 1)
			{
				uint8_t  Mask;
				uint32_t Shift;
				uint8_t  SinglePixel;

				for(p = 0; p < PixelsPerByte; p++)
				{
					if(CurrPixel < IHDR_Width)
					{
						Mask  = (1 << IHDR->BitDepth) - 1;
						Shift = (PixelsPerByte - 1 - p) * IHDR->BitDepth;

						SinglePixel = ((DecompPtr[0] & (Mask << Shift)) >> Shift);

						if(!ConvertPixel(IHDR, OutPtr, &SinglePixel, HasTransparentColour, TransparentColour, OutPal))
						{
							return(qfalse);
						}

						OutPtr += Q3IMAGE_BYTESPERPIXEL;
						CurrPixel++;
					}
				}

			}
			else
			{
				if(!ConvertPixel(IHDR, OutPtr, DecompPtr, HasTransparentColour, TransparentColour, OutPal))
				{
					return(qfalse);
				}


				OutPtr += Q3IMAGE_BYTESPERPIXEL;
			}

			DecompPtr += BytesPerPixel;
		}
	}

	return(qtrue);
}


static qboolean DecodeImageInterlaced(struct PNG_Chunk_IHDR *IHDR,
		byte                  *OutBuffer, 
		uint8_t               *DecompressedData,
		uint32_t               DecompressedDataLength,
		qboolean               HasTransparentColour,
		uint8_t               *TransparentColour,
		uint8_t               *OutPal)
{
	uint32_t IHDR_Width;
	uint32_t IHDR_Height;
	uint32_t BytesPerScanline[PNG_Adam7_NumPasses], BytesPerPixel, PixelsPerByte;
	uint32_t PassWidth[PNG_Adam7_NumPasses], PassHeight[PNG_Adam7_NumPasses];
	uint32_t WSkip[PNG_Adam7_NumPasses], WOffset[PNG_Adam7_NumPasses], HSkip[PNG_Adam7_NumPasses], HOffset[PNG_Adam7_NumPasses];
	uint32_t w, h, p, a;
	byte *OutPtr;
	uint8_t *DecompPtr;
	uint32_t TargetLength;


	if(!(IHDR && OutBuffer && DecompressedData && DecompressedDataLength && TransparentColour && OutPal))
	{
		return(qfalse);
	}


	IHDR_Width  = BigLong(IHDR->Width);
	IHDR_Height = BigLong(IHDR->Height);


	WSkip[0]   = 8;
	WOffset[0] = 0;
	HSkip[0]   = 8;
	HOffset[0] = 0;

	WSkip[1]   = 8;
	WOffset[1] = 4;
	HSkip[1]   = 8;
	HOffset[1] = 0;

	WSkip[2]   = 4;
	WOffset[2] = 0;
	HSkip[2]   = 8;
	HOffset[2] = 4;

	WSkip[3]   = 4;
	WOffset[3] = 2;
	HSkip[3]   = 4;
	HOffset[3] = 0;

	WSkip[4]   = 2;
	WOffset[4] = 0;
	HSkip[4]   = 4;
	HOffset[4] = 2;

	WSkip[5]   = 2;
	WOffset[5] = 1;
	HSkip[5]   = 2;
	HOffset[5] = 0;

	WSkip[6]   = 1;
	WOffset[6] = 0;
	HSkip[6]   = 2;
	HOffset[6] = 1;


	PassWidth[0]  = (IHDR_Width  + 7) / 8;
	PassHeight[0] = (IHDR_Height + 7) / 8;

	PassWidth[1]  = (IHDR_Width  + 3) / 8;
	PassHeight[1] = (IHDR_Height + 7) / 8;

	PassWidth[2]  = (IHDR_Width  + 3) / 4;
	PassHeight[2] = (IHDR_Height + 3) / 8;

	PassWidth[3]  = (IHDR_Width  + 1) / 4;
	PassHeight[3] = (IHDR_Height + 3) / 4;

	PassWidth[4]  = (IHDR_Width  + 1) / 2;
	PassHeight[4] = (IHDR_Height + 1) / 4;

	PassWidth[5]  = (IHDR_Width  + 0) / 2;
	PassHeight[5] = (IHDR_Height + 1) / 2;

	PassWidth[6]  = (IHDR_Width  + 0) / 1;
	PassHeight[6] = (IHDR_Height + 0) / 2;


	switch(IHDR->ColourType)
	{
		case PNG_ColourType_Grey :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_1 :
				case PNG_BitDepth_2 :
				case PNG_BitDepth_4 :
				{
					BytesPerPixel    = 1;
					PixelsPerByte    = 8 / IHDR->BitDepth;

					break;
				}

				case PNG_BitDepth_8  :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_Grey;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_True :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8  :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_True;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_Indexed :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_1 :
				case PNG_BitDepth_2 :
				case PNG_BitDepth_4 :
				{
					BytesPerPixel    = 1;
					PixelsPerByte    = 8 / IHDR->BitDepth;

					break;
				}

				case PNG_BitDepth_8 :
				{
					BytesPerPixel    = PNG_NumColourComponents_Indexed;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_GreyAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_GreyAlpha;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		case PNG_ColourType_TrueAlpha :
		{
			switch(IHDR->BitDepth)
			{
				case PNG_BitDepth_8 :
				case PNG_BitDepth_16 :
				{
					BytesPerPixel    = (IHDR->BitDepth / 8) * PNG_NumColourComponents_TrueAlpha;
					PixelsPerByte    = 1;

					break;
				}

				default :
				{
					return(qfalse);
				}
			}

			break;
		}

		default :
		{
			return(qfalse);
		}
	}


	for(a = 0; a < PNG_Adam7_NumPasses; a++)
	{
		BytesPerScanline[a] = (PassWidth[a] * BytesPerPixel + (PixelsPerByte - 1)) / PixelsPerByte;
	}


	TargetLength = 0;

	for(a = 0; a < PNG_Adam7_NumPasses; a++)
	{
		TargetLength += ((BytesPerScanline[a] + (BytesPerScanline[a] ? 1 : 0)) * PassHeight[a]);
	}


	if(!(DecompressedDataLength == TargetLength))
	{
		return(qfalse);
	}


	DecompPtr = DecompressedData;

	for(a = 0; a < PNG_Adam7_NumPasses; a++)
	{
		if(!UnfilterImage(DecompPtr, PassHeight[a], BytesPerScanline[a], BytesPerPixel))
		{
			return(qfalse);
		}

		DecompPtr += ((BytesPerScanline[a] + (BytesPerScanline[a] ? 1 : 0)) * PassHeight[a]);
	}


	DecompPtr = DecompressedData;


	for(a = 0; a < PNG_Adam7_NumPasses; a++)
	{
		for(h = 0; h < PassHeight[a]; h++)
		{

			uint32_t CurrPixel;


			if(BytesPerScanline[a])
			{
				DecompPtr++;
			}


			CurrPixel = 0;

			for(w = 0; w < (BytesPerScanline[a] / BytesPerPixel); w++)
			{
				if(PixelsPerByte > 1)
				{
					uint8_t  Mask;
					uint32_t Shift;
					uint8_t  SinglePixel;

					for(p = 0; p < PixelsPerByte; p++)
					{
						if(CurrPixel < PassWidth[a])
						{
							Mask  = (1 << IHDR->BitDepth) - 1;
							Shift = (PixelsPerByte - 1 - p) * IHDR->BitDepth;

							SinglePixel = ((DecompPtr[0] & (Mask << Shift)) >> Shift);

							OutPtr = OutBuffer + (((((h * HSkip[a]) + HOffset[a]) * IHDR_Width) + ((CurrPixel * WSkip[a]) + WOffset[a])) * Q3IMAGE_BYTESPERPIXEL);

							if(!ConvertPixel(IHDR, OutPtr, &SinglePixel, HasTransparentColour, TransparentColour, OutPal))
							{
								return(qfalse);
							}

							CurrPixel++;
						}
					}

				}
				else
				{
					OutPtr = OutBuffer + (((((h * HSkip[a]) + HOffset[a]) * IHDR_Width) + ((w * WSkip[a]) + WOffset[a])) * Q3IMAGE_BYTESPERPIXEL);

					if(!ConvertPixel(IHDR, OutPtr, DecompPtr, HasTransparentColour, TransparentColour, OutPal))
					{
						return(qfalse);
					}
				}

				DecompPtr += BytesPerPixel;
			}
		}
	}

	return(qtrue);
}


void R_LoadPNG(const char *name, byte **pic, int *width, int *height)
{
	struct BufferedFile *ThePNG;
	byte *OutBuffer;
	uint8_t *Signature;
	struct PNG_ChunkHeader *CH;
	uint32_t ChunkHeaderLength;
	uint32_t ChunkHeaderType;
	struct PNG_Chunk_IHDR *IHDR;
	uint32_t IHDR_Width;
	uint32_t IHDR_Height;
	PNG_ChunkCRC *CRC;
	uint8_t *InPal;
	uint8_t *DecompressedData;
	uint32_t DecompressedDataLength;
	uint32_t i;


	uint8_t OutPal[1024];


	qboolean HasTransparentColour = qfalse;
	uint8_t TransparentColour[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


	if(!(name && pic))
	{
		return;
	}


	*pic = NULL;

	if(width)
	{
		*width = 0;
	}

	if(height)
	{
		*height = 0;
	}


	ThePNG = ReadBufferedFile(name);
	if(!ThePNG)
	{
		return;
	}           


	Signature = BufferedFileRead(ThePNG, PNG_Signature_Size);
	if(!Signature)
	{
		CloseBufferedFile(ThePNG);

		return;
	}


	if(memcmp(Signature, PNG_Signature, PNG_Signature_Size))
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	CH = BufferedFileRead(ThePNG, PNG_ChunkHeader_Size);
	if(!CH)
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	ChunkHeaderLength = BigLong(CH->Length);
	ChunkHeaderType   = BigLong(CH->Type);


	if(!((ChunkHeaderType == PNG_ChunkType_IHDR) && (ChunkHeaderLength == PNG_Chunk_IHDR_Size)))
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	IHDR = BufferedFileRead(ThePNG, PNG_Chunk_IHDR_Size);
	if(!IHDR)
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	CRC = BufferedFileRead(ThePNG, PNG_ChunkCRC_Size);
	if(!CRC)
	{
		CloseBufferedFile(ThePNG);

		return; 
	}



	IHDR_Width  = BigLong(IHDR->Width);
	IHDR_Height = BigLong(IHDR->Height);


	if(!((IHDR_Width > 0) && (IHDR_Height > 0))
	|| IHDR_Width > INT_MAX / Q3IMAGE_BYTESPERPIXEL / IHDR_Height)
	{
		CloseBufferedFile(ThePNG);

		ri.Printf( PRINT_WARNING, "%s: invalid image size\n", name );

		return; 
	}



	if(!((IHDR->CompressionMethod == PNG_CompressionMethod_0) && (IHDR->FilterMethod == PNG_FilterMethod_0)))
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	if(!((IHDR->InterlaceMethod == PNG_InterlaceMethod_NonInterlaced)  || (IHDR->InterlaceMethod == PNG_InterlaceMethod_Interlaced)))
	{
		CloseBufferedFile(ThePNG);

		return;
	}


	if(IHDR->ColourType == PNG_ColourType_Indexed)
	{

		if(!FindChunk(ThePNG, PNG_ChunkType_PLTE))
		{
			CloseBufferedFile(ThePNG);

			return;
		}


		CH = BufferedFileRead(ThePNG, PNG_ChunkHeader_Size);
		if(!CH)
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		ChunkHeaderLength = BigLong(CH->Length);
		ChunkHeaderType   = BigLong(CH->Type);


		if(!(ChunkHeaderType == PNG_ChunkType_PLTE))
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		if(ChunkHeaderLength % 3)
		{
			CloseBufferedFile(ThePNG);

			return;   
		}


		InPal = BufferedFileRead(ThePNG, ChunkHeaderLength);
		if(!InPal)
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		CRC = BufferedFileRead(ThePNG, PNG_ChunkCRC_Size);
		if(!CRC)
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		for(i = 0; i < 256; i++)
		{
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 0] = 0x00;
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 1] = 0x00;
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 2] = 0x00;
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 3] = 0xFF;  
		}


		for(i = 0; i < (ChunkHeaderLength / 3); i++)
		{
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 0] = InPal[i*3+0];
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 1] = InPal[i*3+1];
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 2] = InPal[i*3+2];
			OutPal[i * Q3IMAGE_BYTESPERPIXEL + 3] = 0xFF;
		}
	}



	if(FindChunk(ThePNG, PNG_ChunkType_tRNS))
	{
		uint8_t *Trans;


		CH = BufferedFileRead(ThePNG, PNG_ChunkHeader_Size);
		if(!CH)
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		ChunkHeaderLength = BigLong(CH->Length);
		ChunkHeaderType   = BigLong(CH->Type);


		if(!(ChunkHeaderType == PNG_ChunkType_tRNS))
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		Trans = BufferedFileRead(ThePNG, ChunkHeaderLength);
		if(!Trans)
		{
			CloseBufferedFile(ThePNG);

			return;  
		}


		CRC = BufferedFileRead(ThePNG, PNG_ChunkCRC_Size);
		if(!CRC)
		{
			CloseBufferedFile(ThePNG);

			return; 
		}


		switch(IHDR->ColourType)
		{
			case PNG_ColourType_Grey :
			{
				if(ChunkHeaderLength != 2)
				{
					CloseBufferedFile(ThePNG);

					return;    
				}

				HasTransparentColour = qtrue;


				TransparentColour[0] = Trans[0];
				TransparentColour[1] = Trans[1];

				break;
			}

			case PNG_ColourType_True :
			{
				if(ChunkHeaderLength != 6)
				{
					CloseBufferedFile(ThePNG);

					return;    
				}

				HasTransparentColour = qtrue;


				TransparentColour[0] = Trans[0];
				TransparentColour[1] = Trans[1];
				TransparentColour[2] = Trans[2];
				TransparentColour[3] = Trans[3];
				TransparentColour[4] = Trans[4];
				TransparentColour[5] = Trans[5];

				break;
			}

			case PNG_ColourType_Indexed :
			{

				if(ChunkHeaderLength > 256)
				{
					CloseBufferedFile(ThePNG);

					return;    
				}

				HasTransparentColour = qtrue;


				for(i = 0; i < ChunkHeaderLength; i++)
				{
					OutPal[i * Q3IMAGE_BYTESPERPIXEL + 3] = Trans[i];
				}

				break;
			}


			default :
			{
				CloseBufferedFile(ThePNG);

				return;
			}
		} 
	}


	if(!BufferedFileRewind(ThePNG, (unsigned)-1))
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	if(!BufferedFileSkip(ThePNG, PNG_Signature_Size))
	{
		CloseBufferedFile(ThePNG);

		return; 
	}


	DecompressedDataLength = DecompressIDATs(ThePNG, &DecompressedData);
	if(!(DecompressedDataLength && DecompressedData))
	{
		CloseBufferedFile(ThePNG);

		return;
	}


	OutBuffer = ri.Malloc(IHDR_Width * IHDR_Height * Q3IMAGE_BYTESPERPIXEL); 
	if(!OutBuffer)
	{
		ri.Free(DecompressedData); 
		CloseBufferedFile(ThePNG);

		return;  
	}


	switch(IHDR->InterlaceMethod)
	{
		case PNG_InterlaceMethod_NonInterlaced :
		{
			if(!DecodeImageNonInterlaced(IHDR, OutBuffer, DecompressedData, DecompressedDataLength, HasTransparentColour, TransparentColour, OutPal))
			{
				ri.Free(OutBuffer); 
				ri.Free(DecompressedData); 
				CloseBufferedFile(ThePNG);

				return;
			}

			break;
		}

		case PNG_InterlaceMethod_Interlaced :
		{
			if(!DecodeImageInterlaced(IHDR, OutBuffer, DecompressedData, DecompressedDataLength, HasTransparentColour, TransparentColour, OutPal))
			{
				ri.Free(OutBuffer); 
				ri.Free(DecompressedData); 
				CloseBufferedFile(ThePNG);

				return;
			}

			break;
		}

		default :
		{
			ri.Free(OutBuffer); 
			ri.Free(DecompressedData); 
			CloseBufferedFile(ThePNG);

			return;
		}
	}


	*pic = OutBuffer;


	if(width)
	{
		*width = IHDR_Width;
	}

	if(height)
	{
		*height = IHDR_Height;
	}


	ri.Free(DecompressedData); 


	CloseBufferedFile(ThePNG);
}
