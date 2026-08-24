#pragma once

#include "str.h"
#include "container.h"
#include "script.h"

// therefore the decompile's, not alphabetical.

#define MAX_FOOTPRINT_MATERIALS 12

enum footprintMaterial_t {
    FOOTPRINT_DIRT = 0,        
    FOOTPRINT_ROCK,            
    FOOTPRINT_METAL,           
    FOOTPRINT_GRILL,           
    FOOTPRINT_ORGANIC,         
    FOOTPRINT_SQUISHY,         
    FOOTPRINT_SAND,            
    FOOTPRINT_SNOW,            
    FOOTPRINT_METAL_DUCT,      
    FOOTPRINT_METAL_HOLLOW,    
    FOOTPRINT_CARPET,          
    FOOTPRINT_DEFAULT,         
};

class FootprintBoneShaderPair_s
{
public:
    FootprintBoneShaderPair_s();
    ~FootprintBoneShaderPair_s();

    str   boneName;    
    str   shader;      
    float radius;      
};

class FootprintFX_s
{
public:
    FootprintFX_s();
    ~FootprintFX_s();

    str                                   footprintLeft;    
    str                                   footprintRight;   
    str                                   cloud;            
    Container<FootprintBoneShaderPair_s>  boneShaderPairs;  
};

class FootprintMap_s
{
public:
    ~FootprintMap_s();

    str            name;       
    FootprintFX_s  fx[MAX_FOOTPRINT_MATERIALS];    
};

class Script;

FootprintMap_s *CreateNewFootprintMap(const str& name, Script& script);
void            ParseFootprintSurfaceData(FootprintMap_s *map, Script& script);
void            ParseFootprintFXData(FootprintFX_s *fx, Script& script);

void CG_InitFootprintMap(void);
void CG_ClearFootprintMap(void);

typedef struct surfaceinfo_s {
    int     contents;      
    int     surfaceFlags;   
    vec3_t  endpos;        
    trace_t trace;         
    int     reserved;      
    qboolean hit;          
} surfaceinfo_t;

surfaceinfo_t *CG_GetSurfaceInfo(centity_t *cent, const char *tagname);

// The footstep chain, retail-exact.  Both FX cores take the TRACE, not the
void CG_Footstep(centity_t *cent, const char *tagname, const char *creaturetype);
void CG_PlayWaterFX(const trace_t *trace, float volume);
void CG_PlaySurfaceFX(
    const trace_t *trace,
    int            unused,
    unsigned int   surfaceFlags,
    const char    *tagname,
    const char    *creaturetype
);
void CG_PlaySurfaceSound(centity_t *cent, const char *prefix, int channel, float volume, float minDistance);
