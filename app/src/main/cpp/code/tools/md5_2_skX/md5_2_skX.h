
#ifndef __MD5_2_SKX_H__
#define __MD5_2_SKX_H__

#define _CRT_SECURE_NO_WARNINGS

#include "../../qcommon/q_shared.h"
#include "../../qcommon/tiki_local.h"

#define MD5_2_SKX_VERSION "0.1"

#define MAX_TOOLPATH 512

typedef struct tBone_s {
	char name[32];
	int parent; 
} tBone_t;

typedef struct {
	unsigned int indexes[3];
} tTri_t;

typedef struct {
	vec3_t offset;
	float boneWeight;
	int boneNum;
} tWeight_t;

typedef struct {
	vec2_t texCoords;
	vec3_t normal;
	vec3_t absXYZ; 
	int firstWeight;
	int numWeights;
	tWeight_t *weights;
} tVert_t;

typedef struct {
	char name[MAX_TOOLPATH];
	int numVerts;
	int numWeights;
	int numTris;
	tVert_t *verts;
	tTri_t *tris;
	tWeight_t *weights;
} tSurf_t;

typedef struct tModel_s {
	char fname[MAX_TOOLPATH];
	int numSurfaces;
	int numBones;
	tSurf_t *surfs;
	tBone_t *bones;
	bone_t *baseFrame;
	vec3_t baseFrameMins;
	vec3_t baseFrameMaxs;
	float baseFrameRadius;
} tModel_t;


typedef struct {
	vec3_t mins;
	vec3_t maxs;
	float *components;
} tFrame_t;

typedef struct {
	char name[MAX_TOOLPATH];
	int parent;
	int firstComponent;
	int componentBits;
} tAnimBone_t;

typedef struct {
	char fname[MAX_TOOLPATH];
	float frameRate;
	int numFrames;
	int numBones;
	int numAnimatedComponents;
	bone_t *baseFrame;
	tAnimBone_t *boneData;
	tFrame_t *frames;
} tAnim_t;

enum
{
	COMPONENT_BIT_TX = 1 << 0,
	COMPONENT_BIT_TY = 1 << 1,
	COMPONENT_BIT_TZ = 1 << 2,
	COMPONENT_BIT_QX = 1 << 3,
	COMPONENT_BIT_QY = 1 << 4,
	COMPONENT_BIT_QZ = 1 << 5
};

extern qboolean verbose;
extern qboolean noLimits;
extern qboolean createTIK;
extern char outTIKI[MAX_TOOLPATH];
extern char outSKDMesh[MAX_TOOLPATH];
extern char outSKCAnim[MAX_TOOLPATH][256];
extern int numAnims;
extern tAnim_t *anims[256];
char inAnimFNames[MAX_TOOLPATH][256];
extern tModel_t *mainModel;

void CalcModelNormals(tModel_t *m);
void CalcModelXYZVerticesForBones(tModel_t *m, bone_t *bones);
void CalcModelBB(tModel_t *m, bone_t *bones, vec3_t outMins, vec3_t outMaxs, float *outRadius);
void CalcModelBBFromBaseframe(tModel_t *m);

tModel_t *loadMD5Mesh(const char *fname);
tAnim_t *loadMD5Anim(const char *fname);
void writeMD5Mesh(tModel_t *m, const char *outFName);
void writeMD5Anim(tAnim_t *m, const char *outFName);
bone_t *setupMD5AnimBones(tAnim_t *a, int frameNum);
bone_t *setupMD5MeshBones(tModel_t *mod);
void md5AnimateBones(tModel_t *m, bone_t *bones);
// NOTE: given quat must be normalized!
qboolean FixQuatForMD5_P(quat_t q); 

void writeSKL(tModel_t *m, tAnim_t *a, const char *outFName);

void writeTIKI(tModel_t *m, const char *outFName);
char *extractTIKIDataPath(const char *tikiName);

tModel_t *readSKD(const char *fname, float scale);
tAnim_t *appendSKC(tModel_t *m, const char *fname, float scale);
void loadTIKI(const char *fname);

void WriteOBJ_FromBaseFrame(const char *outFName, tModel_t *m);
void WriteOBJ_FromAnim(const char *outFName, tModel_t *m, tAnim_t *a);

void T_Printf(const char *format, ...);
void T_VerbPrintf(const char *format, ...);
void T_Error(const char *format, ...);

int F_LoadBuf(const char *fname, byte **out);
void F_FreeBuf(byte *b);
FILE *F_Open(const char *fname, const char *mode);
qboolean F_Exists(const char *fname);

void stripExt(char *s);
const char *strchr_r(const char *s, char c, char c2);
void backSlashesToSlashes(char *s);
const char *getGamePath(const char *s);
const char *getFName(const char *s);
void *T_Malloc(unsigned int size);
void InverseBoneQuats(bone_t *bones, int numBones);

#endif 
