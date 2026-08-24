
#include "../../tiki/tiki_shared.h"

#define	MAX_ENTITIES	1023		
#define MAX_POINTS     32
#define MAX_SPRITES    1024

#define MAX_MARK_FRAGMENTS   128
#define MAX_MARK_POLYVERTS   384

#define RDF_FULLBRIGHT		2		

typedef struct
{
    float pos[2];
    float size[2];
} letterloc_t;

#define FONT_MAX_PAGES 16 

typedef struct fontheader_sgl_s
{
    int               indirection[256];
    letterloc_t       locations[256];
    char              name[64];
    float             height;
    float             aspectRatio;
    void              *shader;
    int               trhandle; 
    int               numShaderPages;
    void              *shaderPages[FONT_MAX_PAGES];
    int               pageForLoc[256]; 
} fontheader_sgl_t;

typedef struct {
    unsigned short cp;
    unsigned char index;
    unsigned char loc;
} fontchartable_t;

typedef struct fontheader_s {
    int numPages;
    int charTableLength;
    fontchartable_t* charTable;
    short codePage;
    fontheader_sgl_t* sgl[32];
    char name[64];
} fontheader_t;

typedef enum {
    lensflare = (1 << 0),
    viewlensflare = (1 << 1),
    additive = (1 << 2)
} dlighttype_t;

#if !defined _WIN32

#define _3DFX_DRIVER_NAME	"libMesaVoodooGL.so.3.1"
#define OPENGL_DRIVER_NAME	"libGL.so"

#else

#define _3DFX_DRIVER_NAME	"3dfxvgl"
#define OPENGL_DRIVER_NAME	"opengl32"

#endif	
