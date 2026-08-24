
#include <stdio.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "trilib.h"
#include "l3dslib.h"

#define MAIN3DS       0x4D4D
#define EDIT3DS       0x3D3D  
#define EDIT_OBJECT   0x4000
#define OBJ_TRIMESH   0x4100
#define TRI_VERTEXL   0x4110
#define TRI_FACEL1    0x4120

#define MAXVERTS	2000

typedef struct {
	int	v[4];
} tri;

float	fverts[MAXVERTS][3];
tri		tris[MAXTRIANGLES];

int	bytesread, level, numtris, totaltris;
int	vertsfound, trisfound;

triangle_t	*ptri;


void StoreAliasTriangles (void)
{
	int		i, j, k;

	if ((totaltris + numtris) > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0; i<numtris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			for (k=0 ; k<3 ; k++)
			{
				ptri[i+totaltris].verts[j][k] = fverts[tris[i].v[j]][k];
			}
		}
	}
	
	totaltris += numtris;
	numtris = 0;
	vertsfound = 0;
	trisfound = 0;
}


int ParseVertexL (FILE *input)
{
	int				i, j, startbytesread, numverts;
	unsigned short	tshort;

	if (vertsfound)
		Error ("Error: Multiple vertex chunks");

	vertsfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&tshort, sizeof(tshort), 1, input);
	bytesread += sizeof(tshort);
	numverts = (int)tshort;

	if (numverts > MAXVERTS)
		Error ("Error: Too many vertices");

	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread(&fverts[i][j], sizeof(float), 1, input);
			bytesread += sizeof(float);
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


int ParseFaceL1 (FILE *input)
{

	int				i, j, startbytesread;
	unsigned short	tshort;

	if (trisfound)
		Error ("Error: Multiple face chunks");

	trisfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&tshort, sizeof(tshort), 1, input);
	bytesread += sizeof(tshort);
	numtris = (int)tshort;

	if (numtris > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0 ; i<numtris ; i++)
	{
		for (j=0 ; j<4 ; j++)
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread(&tshort, sizeof(tshort), 1, input);
			bytesread += sizeof(tshort);
			tris[i].v[j] = (int)tshort;
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


int ParseChunk (FILE *input)
{
#define BLOCK_SIZE	4096
	char			temp[BLOCK_SIZE];
	unsigned short	type;
	int				i, length, w, t, retval;

	level++;
	retval = 0;

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&type, sizeof(type), 1, input);
	bytesread += sizeof(type);

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread (&length, sizeof(length), 1, input);
	bytesread += sizeof(length);
	w = length - 6;

	switch (type)
	{
	case TRI_VERTEXL:
		w -= ParseVertexL (input);
		goto ParseSubchunk;

	case TRI_FACEL1:
		w -= ParseFaceL1 (input);
		goto ParseSubchunk;

	case EDIT_OBJECT:
		i = 0;

		do
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread (&temp[i], 1, 1, input);
			i++;
			w--;
			bytesread++;
		} while (temp[i-1]);

	case MAIN3DS:
	case OBJ_TRIMESH:
	case EDIT3DS:
ParseSubchunk:
		while (w > 0)
		{
			w -= ParseChunk (input);
		}

		retval = length;
		goto Done;

	default:
		while (w > 0)
		{
			t = w;

			if (t > BLOCK_SIZE)
				t = BLOCK_SIZE;

			if (feof(input))
				Error ("Error: unexpected end of file");

			fread (&temp, t, 1, input);
			bytesread += t;

			w -= t;
		}

		retval = length;
		goto Done;
	}

Done:
	level--;
	return retval;
}


void Load3DSTriangleList (char *filename, triangle_t **pptri, int *numtriangles)
{
	FILE        *input;
	short int	tshort;

	bytesread = 0;
	level = 0;
	numtris = 0;
	totaltris = 0;
	vertsfound = 0;
	trisfound = 0;

	if ((input = fopen(filename, "rb")) == 0) {
		fprintf(stderr,"reader: could not open file '%s'\n", filename);
		exit(0);
	}

	fread(&tshort, sizeof(tshort), 1, input);

	if ((tshort != MAIN3DS) && (tshort != EDIT3DS)) {
		fprintf(stderr,"File is not a 3DS file.\n");
		exit(0);
	}

	fseek(input, 0, SEEK_SET);

	ptri = malloc (MAXTRIANGLES * sizeof(triangle_t));

	*pptri = ptri;

	ParseChunk (input);

	if (vertsfound || trisfound)
		Error ("Incomplete triangle set");

	*numtriangles = totaltris;

	fclose (input);
}

