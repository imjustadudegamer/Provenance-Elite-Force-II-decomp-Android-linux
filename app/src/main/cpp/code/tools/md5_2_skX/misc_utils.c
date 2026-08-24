
#include "md5_2_skX.h"


void T_Printf(const char *format, ...) {
	char	buffer[4096];
	va_list	argptr;

	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	printf(buffer);
}

void T_VerbPrintf(const char *format, ...) {
	char	buffer[4096];
	va_list	argptr;

	if(verbose == qfalse)
		return;

	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	T_Printf(buffer);
}

void T_Error(const char *format, ...) {
	char	buffer[4096];
	va_list	argptr;

	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);

	T_Printf("ERROR:");
	T_Printf(buffer);
	T_Printf("\n");
	system("pause");
	exit(-1);
}


void QDECL Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	T_Error( "%s", text);
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	T_Printf ("%s", text);
}


int F_LoadBuf(const char *fname, byte **out) {
	FILE *f;
	int len;
	byte *buf;

	f = fopen(fname,"rb");

	if(f == 0) {
		*out = 0;
		return -1; 
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = malloc(len+1);
	fread(buf,1,len,f);
	buf[len] = 0; 

	*out = buf;

	fclose(f);
	return len;
}

void F_FreeBuf(byte *b) {
	free(b);
}

void MakeDirSafe(const char *fpath) {
	char buf[MAX_TOOLPATH];
	char fixed[MAX_TOOLPATH];
	const char *p;
	strcpy(fixed,fpath);
	backSlashesToSlashes(fixed);

	p = fixed;
	while(*p) {
		if(*p == '/') {
			memcpy(buf,fixed,p - fixed);
			buf[p-fixed] = 0;
			_mkdir(buf);
		}
		p++;
	}
}

FILE *F_Open(const char *fname, const char *mode) {
	FILE *r;
	MakeDirSafe(fname);
	r = fopen(fname,mode);
	return r;
}

qboolean F_Exists(const char *fname) {
	FILE *f;

	f = fopen(fname,"rb");

	if(f == 0)
		return qfalse;

	fclose(f);
	return qtrue;
}


void stripExt(char *s) {
	int l;
	char *p;

	l = strlen(s);

	p = s + l - 1;
	while(p != s) {
		if(*p == '.') {
			*p = 0;
			return;
		}
		if(*p == '/' || *p == '\\')
			return;
		p--;
	}
}
const char *strchr_r(const char *s, char c, char c2) {
	const char *p = s + strlen(s);
	while(p != s) {
		if(*p == c || *p == c2)
			return p;
		p--;
	}
	return 0;
}
void backSlashesToSlashes(char *s) {
	while(*s) {
		if(*s == '\\') {
			*s = '/';
		}
		s++;
	}
}
const char *getGamePath(const char *s) {
	char buf[MAX_TOOLPATH];
	const char *p;

	strcpy(buf,s);
	backSlashesToSlashes(buf);
	p = buf;

	while(*p) {
		if(!Q_stricmpn(p,"/main",strlen("/main"))) {
			const char *r;
			r = strchr(p+1,'/');
			r++;
			return r;
		}
		p++;
	}

	p = buf;

	while(*p) {
		if(!Q_stricmpn(p,"/models/",strlen("/models/"))) {
			const char *r;
			r = p;
			r++;
			return r;
		}
		p++;
	}

	return 0;
}

const char *getFName(const char *s) {
	const char *p;

	p = strchr_r(s,'/','\\');

	if(p == 0)
		return s;

	p++;

	return p;
}


void *T_Malloc(unsigned int size) {
	void *r = malloc(size);
	memset(r,0,size);
	return r;
}


void PrintBoneOr(bone_t *b) {
}

void InverseBoneQuats(bone_t *bones, int numBones) {
	int i;
	for(i = 0; i < numBones; i++) {
		QuatInverse(bones[i].q);
	}
}
