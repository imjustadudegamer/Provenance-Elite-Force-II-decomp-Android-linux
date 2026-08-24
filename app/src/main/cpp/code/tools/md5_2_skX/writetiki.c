


#include "md5_2_skX.h"

char *extractTIKIDataPath(const char *tikiName) {
	static char out[MAX_TOOLPATH];
	strcpy(out,tikiName);
	stripExt(out);
	backSlashesToSlashes(out);
	if(out[0] && out[strlen(out)-1] != '/') {
		strcat(out,"/");
	}
	return out;
}

void writeTIKI(tModel_t *m,const char *outFName) {
	FILE *out;
	int i;
	char path[MAX_TOOLPATH];
	const char *skelmodel;
	const char *p;
	char surf[24];

	p = extractTIKIDataPath(outFName);
	p = getGamePath(p);
	if(p) {
		strcpy(path,p);
	} else {
		path[0] = 0;
	}
	skelmodel = getFName(outSKDMesh);

	T_Printf("Writing MoHAA .tik file %s...\n",outFName);

	out = F_Open(outFName,"w");
	if(out == 0) {
		T_Printf("Cannot open %s for writing\n",outFName);
		return;
	}

	fprintf(out,"//////////////////////////////////////////////////////////////////////////\n");
	fprintf(out,"//\n");
	fprintf(out,"// Exported by md5_2_skX\n");
	fprintf(out,"//\n");
	fprintf(out,"//////////////////////////////////////////////////////////////////////////\n");
fprintf(out,"\n");
	fprintf(out,"TIKI\n");
fprintf(out,"\n");
	fprintf(out,"setup\n");
	fprintf(out,"{\n");
		fprintf(out,"\tscale 1.0\n");
		fprintf(out,"\n");
		fprintf(out,"\tpath %s\n",path);
		fprintf(out,"\tskelmodel %s\n",skelmodel);
		for(i = 0; i < m->numSurfaces; i++) {
			Q_strncpyz(surf,m->surfs[i].name,sizeof(surf));
			fprintf(out,"\tsurface %s shader %s\n",surf,m->surfs[i].name);
		}
		fprintf(out,"\n");
	fprintf(out,"}\n");
fprintf(out,"\n");
	fprintf(out,"init\n");
	fprintf(out,"{\n");
		fprintf(out,"\tserver\n");
		fprintf(out,"\t{\n");
			fprintf(out,"\t\tclassname Animate\n");
		fprintf(out,"\t}\n");
		fprintf(out,"\tclient\n");
		fprintf(out,"\t{\n");
			fprintf(out,"\t\t// setup emitters / cache images here...\n");
		fprintf(out,"\t}\n");
	fprintf(out,"}\n");
fprintf(out,"\n");
	fprintf(out,"animations\n");
	fprintf(out,"{\n");
		for(i = 0; i < numAnims; i++) {
			char alias[MAX_TOOLPATH];

			if(anims[i] == 0) {
				continue;
			}
			p = getFName(outSKCAnim[i]);

			strcpy(alias,p);
			stripExt(alias);

			fprintf(out,"\t%s %s\n",alias,p);
		}
	fprintf(out,"}\n");
fprintf(out,"\n");
	fclose(out);

	T_Printf("Wrote MoHAA TIKI file %s\n",outFName);
}


