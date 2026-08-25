

#include "tr_local.h"

#define MAX_LOADED_FONTS 255

typedef struct {
    int          num;
    fontheader_t fonts[MAX_LOADED_FONTS];
} fontPool_t;

typedef struct {
    int              num;
    fontheader_sgl_t fonts[MAX_LOADED_FONTS];
} fontSglPool_t;

static fontPool_t    *s_fontPool    = NULL;
static fontSglPool_t *s_sglPool     = NULL;
static float s_fontHeightScale = 1.0;
static float s_fontGeneralScale = 1.0;
static float s_fontZ = 0.0;

static fontPool_t *FontPool(void)
{
    if (!s_fontPool) {
        s_fontPool    = (fontPool_t *)ri.Malloc(sizeof(fontPool_t));
        s_fontPool->num = 0;
        memset(s_fontPool->fonts, 0, sizeof(s_fontPool->fonts));
    }
    return s_fontPool;
}

static fontSglPool_t *SglPool(void)
{
    if (!s_sglPool) {
        s_sglPool      = (fontSglPool_t *)ri.Malloc(sizeof(fontSglPool_t));
        s_sglPool->num = 0;
        memset(s_sglPool->fonts, 0, sizeof(s_sglPool->fonts));
    }
    return s_sglPool;
}

#define s_loadedFonts     (FontPool()->fonts)
#define s_numLoadedFonts  (FontPool()->num)
#define s_loadedFonts_sgl    (SglPool()->fonts)
#define s_numLoadedFonts_sgl (SglPool()->num)

// Retail has NO font teardown.  RE_Shutdown @0x0001A1E0 never touches the font
void R_ShutdownFont() {}

void R_SetFontHeightScale(float scale)
{
    s_fontHeightScale = scale;
}

void R_SetFontScale(float scale)
{
    s_fontGeneralScale = scale;
}

void R_SetFontZ(float zed)
{
    s_fontZ = zed;
}

static int CodeSearch(const fontheader_t* font, unsigned short uch) {
    int mid;
    int l, r;

    r = font->charTableLength;
    l = 0;
    while (l < r) {
        mid = (l + r) / 2;
        
        if (font->charTable[mid].cp > uch) {
            r = (l + r) / 2;
            continue;
        }

        if (uch == font->charTable[mid].cp) {
            return (l + r) / 2;
        }

        l = mid + 1;
    }

    if (uch != font->charTable[l].cp) {
        return -1;
    }

    return l;
}

static qboolean DBCSIsLeadByte(const fontheader_t* font, unsigned short uch) {
    switch (font->codePage) {
    case 932:
        return ((uch >= 0x81) && (uch <= 0x9F)) ||
            ((uch >= 0xE0) && (uch <= 0xFC));
    case 936:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 949:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 950:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 1361:
        return
            ((uch >= 0x84) && (uch <= 0xD3)) ||
            ((uch >= 0xD8) && (uch <= 0xDE)) ||
            ((uch >= 0xE0) && (uch <= 0xF9));
    }
    return false;
}

fontheader_sgl_t* R_LoadFont_sgl(const char* name)
{
    int i;
    char* theFile;
    fontheader_sgl_t* header;
    char* ref;
    const char* token;
    qboolean error;

    error = qfalse;

    for (i = 0; i < s_numLoadedFonts_sgl; i++)
    {
        header = &s_loadedFonts_sgl[i];
        if (!Q_stricmp(name, header->name)) {
            return header;
        }
    }

    if (s_numLoadedFonts >= MAX_LOADED_FONTS)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Too many fonts loaded!  Couldn't load %s\n", name);
        return NULL;
    }

    if (ri.FS_ReadFile(va("fonts/%s.RitualFont", name), (void**)&theFile) == -1)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Couldn't load font %s\n", name);
        return NULL;
    }

    header = &s_loadedFonts_sgl[s_numLoadedFonts_sgl];
    header->height = 0.0;
    header->aspectRatio = 0.0;
    header->numShaderPages = 1;
    memset(header->pageForLoc, 0, sizeof(header->pageForLoc));
    Q_strncpyz(header->name, name, sizeof(header->name));

    ref = theFile;
    while (ref && !error)
    {
        token = COM_Parse(&ref);
        if (!Q_stricmp(token, "RitFont"))
        {
            continue;
        }

        if (!Q_stricmp(token, "indirections"))
        {
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "{"))
            {
                error = qtrue;
                break;
            }

            for (i = 0; i < 256; i++)
            {
                token = COM_Parse(&ref);
                if (!token[0]) {
                    error = qtrue;
                    break;
                }

                header->indirection[i] = atoi(token);
            }

            if (error) {
                break;
            }

            token = COM_Parse(&ref);
            if (Q_stricmp(token, "}"))
            {
                error = qtrue;
                break;
            }
        }
        else if (!Q_stricmp(token, "locations"))
        {
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "{"))
            {
                error = qtrue;
                break;
            }

            for (i = 0; i < 256; i++)
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{")) {
                    error = qtrue;
                    break;
                }

                if (header->aspectRatio == 0.0)
                {
                    ri.Printf(PRINT_WARNING, "WARNING: aspect decl must be before locations in font '%s'", name);
                    break;
                }

                header->locations[i].pos[0] = atof(COM_Parse(&ref)) / 256.0;
                header->locations[i].pos[1] = atof(COM_Parse(&ref)) * header->aspectRatio / 256.0;
                header->locations[i].size[0] = atof(COM_Parse(&ref)) / 256.0;
                header->locations[i].size[1] = atof(COM_Parse(&ref)) * header->aspectRatio / 256.0;
            
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    error = qtrue;
                    break;
                }
            }

            if (error) {
                break;
            }

            token = COM_Parse(&ref);
            if (Q_stricmp(token, "}"))
            {
                error = qtrue;
                break;
            }
        }
        else if (!Q_stricmp(token, "height"))
        {
            token = COM_Parse(&ref);
            header->height = atof(token);
        }
        else if (!Q_stricmp(token, "aspect"))
        {
            token = COM_Parse(&ref);
            header->aspectRatio = atof(token);
        }
        else if (!Q_stricmp(token, "shadercount"))
        {
            token = COM_Parse(&ref);
            header->numShaderPages = atoi(token);
            if (header->numShaderPages < 1) {
                header->numShaderPages = 1;
            } else if (header->numShaderPages > FONT_MAX_PAGES) {
                ri.Printf(PRINT_WARNING, "LoadFont: font '%s' shadercount %d exceeds FONT_MAX_PAGES(%d); clamping\n",
                    name, header->numShaderPages, FONT_MAX_PAGES);
                header->numShaderPages = FONT_MAX_PAGES;
            }
        }
        else if (!Q_stricmp(token, "shaderindices"))
        {
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "{")) {
                error = qtrue;
                break;
            }
            for (i = 0; i < 256; i++) {
                token = COM_Parse(&ref);
                if (!token[0]) {
                    error = qtrue;
                    break;
                }
                header->pageForLoc[i] = atoi(token);
            }
            if (error) {
                break;
            }
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "}")) {
                error = qtrue;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (token[0])
    {
        ri.Printf(PRINT_WARNING, "WARNING: Unknown token '%s' parsing font '%s'\n", token, name);
        error = qtrue;
    }

    R_LoadFontShader(header);
    if (!header->height || !header->aspectRatio) {
        error = qtrue;
    }

    ri.FS_FreeFile(theFile);
    if (error)
    {
        ri.Printf(3, "WARNING: Error parsing font %s.\n", name);
        return NULL;
    }
    else
    {
        s_numLoadedFonts_sgl++;
        return header;
    }

    return NULL;
}

fontheader_t* R_LoadFont(const char* name) {
    int i;
    char* theFile;
    fontheader_t* header;
    char* ref;
    const char* token;
    const char* loadName;
    qboolean error;
    char* pRitFontNames[32];

    error = qfalse;

    for (i = 0; i < s_numLoadedFonts; i++)
    {
        header = &s_loadedFonts[i];
        if (!Q_stricmp(name, header->name)) {
            return header;
        }
    }

    if (s_numLoadedFonts >= MAX_LOADED_FONTS)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Too many fonts loaded!  Couldn't load %s\n", name);
        return NULL;
    }

    loadName = name;
    if (ri.FS_ReadFile(va("fonts/%s.RitualFont", loadName), (void**)&theFile) == -1)
    {
        loadName = "Tahoma-10";
        if (!Q_stricmp(name, loadName)
            || ri.FS_ReadFile(va("fonts/%s.RitualFont", loadName), (void**)&theFile) == -1)
        {
            ri.Printf(PRINT_WARNING, "LoadFont: Couldn't load font %s\n", name);
            return NULL;
        }
    }

    memset(pRitFontNames, 0, sizeof(pRitFontNames));
    ref = theFile;
    header = &s_loadedFonts[s_numLoadedFonts];

    token = COM_Parse(&ref);
    if (Q_stricmp(token, "RitFontList"))
    {
        if (Q_stricmp(token, "RitFont"))
        {
            ri.Printf(PRINT_WARNING, "LoadFont: Not actual font %s\n", name);
            return NULL;
        }
        header->numPages = 0;
    }
    else
    {
        while (ref && !error)
        {
            token = COM_Parse(&ref);
            if (!Q_stricmp(token, "CodePage"))
            {
                header->codePage = atoi(COM_Parse(&ref));
            }
            else if (!Q_stricmp(token, "Chars"))
            {
                header->charTableLength = atoi(COM_Parse(&ref));
                header->charTable = (fontchartable_t*)ri.Malloc(header->charTableLength * sizeof(fontchartable_t));
                if (!header->charTable)
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Couldn't alloc mem %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else if (!Q_stricmp(token, "Pages"))
            {
                header->numPages = atoi(COM_Parse(&ref));
            }
            else if (!Q_stricmp(token, "RitFontName"))
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }

                for (i = 0; i < header->numPages; i++)
                {
                    token = COM_Parse(&ref);
                    if (!token[0])
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                        error = qtrue;
                        break;
                    }

                    pRitFontNames[i] = (char*)ri.Malloc(strlen(token) + 1);
                    if (!pRitFontNames[i])
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Couldn't alloc mem %s\n", name);
                        error = qtrue;
                        break;
                    }

                    strcpy(pRitFontNames[i], token);
                }

                if (error) {
                    break;
                }

                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else if (!Q_stricmp(token, "CharTable"))
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }

                for (i = 0; i < header->charTableLength; i++) {
                    token = COM_Parse(&ref);
                    if (Q_stricmp(token, "{"))
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                        error = qtrue;
                        break;
                    }

                    header->charTable[i].cp = atoi(COM_Parse(&ref));
                    header->charTable[i].index = atoi(COM_Parse(&ref));
                    header->charTable[i].loc = atoi(COM_Parse(&ref));
                    COM_Parse(&ref);

                    token = COM_Parse(&ref);
                    if (Q_stricmp(token, "}"))
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                        error = qtrue;
                        break;
                    }
                }

                if (error) {
                    break;
                }

                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else
            {
                ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                error = qtrue;
                break;
            }
        }
    }

    ri.FS_FreeFile(theFile);

    if (!header->numPages)
    {
        header->charTableLength = 0;
        header->charTable = NULL;
        header->sgl[0] = R_LoadFont_sgl(loadName);
        if (!header->sgl[0])
        {
            ri.Printf(PRINT_WARNING, "LoadFont: failed %s\n", name);
            return NULL;
        }
    }
    else
    {
        for (i = 0; i < header->numPages; i++)
        {
            header->sgl[i] = R_LoadFont_sgl(pRitFontNames[i]);
            if (!header->sgl[i])
            {
                ri.Printf(PRINT_WARNING, "LoadFont: failed %s(%s)\n", pRitFontNames[i], name);
                error = qtrue;
                break;
            }
        }

        for (i = 0; i < header->numPages; i++) {
            ri.Free(pRitFontNames[i]);
        }

        if (error)
        {
            if (header->charTable) {
                ri.Free(header->charTable);
            }

            header->numPages = 0;
            header->charTableLength = 0;
            header->charTable = NULL;
        }
    }

    strcpy(header->name, name);
    s_numLoadedFonts++;
    return header;
}

void R_LoadFontShader(fontheader_sgl_t* font)
{
    int   i;
    int   page;
    int   save;
    char  pagebase[64];
    char  filename[80];

    save = r_sequencenumber;
    r_sequencenumber = -1;

    if (font->numShaderPages < 1) {
        font->numShaderPages = 1;
    }

    for (page = 0; page < font->numShaderPages; page++) {
        shader_t*  fontshader;
        qboolean   ef2 = qfalse;
        int        e;
        char       probe[96];
        static const char *exts[] = { ".dds", ".tga", ".jpg", ".png", ".tex" };

        if (page == 0) {
            Q_strncpyz(pagebase, font->name, sizeof(pagebase));
        } else {
            Com_sprintf(pagebase, sizeof(pagebase), "%s-%d", font->name, page);
        }

        for (e = 0; e < 5; e++) {
            Com_sprintf(probe, sizeof(probe), "sysimg/fonts/%s%s", pagebase, exts[e]);
            if (ri.FS_ReadFile(probe, NULL) > 0) {
                ef2 = qtrue;
                break;
            }
        }
        Com_sprintf(filename, sizeof(filename), ef2 ? "sysimg/fonts/%s" : "gfx/fonts/%s", pagebase);

        fontshader = R_FindShader(filename, -1, qfalse, qfalse, qfalse, qfalse);
        if (!fontshader) {
            ri.Error(ERR_DROP, "Could not load font shader for %s\n", filename);
        }
        font->shaderPages[page] = fontshader;

        if (fontshader->numUnfoggedPasses > 0) {
            for (i = 0; i < fontshader->numUnfoggedPasses; i++) {
                if (fontshader->unfoggedStages[0] != NULL && fontshader->unfoggedStages[0]->active) {
                    fontshader->unfoggedStages[0]->rgbGen = CGEN_GLOBAL_COLOR;
                    fontshader->unfoggedStages[0]->alphaGen = AGEN_GLOBAL_ALPHA;
                }
            }
        }
    }

    font->shader = font->shaderPages[0]; 
    r_sequencenumber = save;
    font->trhandle = r_sequencenumber;
}

void R_DrawString_sgl(fontheader_sgl_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen) {
    float charHeight;
    float startx, starty;
    int i;
    int curPage; 
    float fWidthScale, fHeightScale;

    i = 0;
    startx = x;
    starty = y;
    if (pvVirtualScreen) {
        if (pvVirtualScreen[0]) {
            fWidthScale = pvVirtualScreen[0];
        } else {
            fWidthScale = (double)glConfig.vidWidth / 640.0;
        }

        if (pvVirtualScreen[1]) {
            fHeightScale = pvVirtualScreen[1];
        } else {
            fHeightScale = (double)glConfig.vidHeight / 480.0;
        }
    }

    if (!font) {
        return;
    }

    R_IssuePendingRenderCommands();

    if (font->trhandle != r_sequencenumber) {
        font->shader = NULL;
    }

    if (!font->shader) {
        R_LoadFontShader(font);
    }

    charHeight = s_fontHeightScale * font->height * s_fontGeneralScale;
    curPage = -1;

    for (i = 0; text[i]; i++) {
        unsigned char c;
        int indirected;
        letterloc_t* loc;

        c = text[i];

        if (maxlen != -1 && i >= maxlen) {
            break;
        }

        switch (c)
        {
        case '\t':
            indirected = font->indirection[32];
            if (indirected == -1) {
                ri.Printf(PRINT_DEVELOPER, "R_DrawString: no space-character in font!\n");
            } else {
                x = s_fontGeneralScale * font->locations[indirected].size[0] * 256.0 * 3.0 + x;
            }
            break;

        case '\n':
            starty = charHeight + starty;
            x = startx;
            y = starty;
            break;

        case '\r':
            x = startx;
            break;

        default:
            if (c == '^') {
                int next = (signed char)text[i + 1];

                if (next >= '0' && next <= '9') {
                    if (curPage != -1) {
                        RB_EndSurface();
                        curPage = -1;
                    }
                    R_ChangeTextColor(atoi(&text[i + 1]));
                    i++;
                    break;
                }
            }

            indirected = font->indirection[c];
            if (indirected == -1)
            {
                ri.Printf(PRINT_DEVELOPER, "R_DrawString: no 0x%02x-character in font!\n", c);
                indirected = font->indirection[(unsigned char)'?'];
                if (indirected == -1) {
                    ri.Printf(PRINT_DEVELOPER, "R_DrawString: no '?' character in font!\n");
                    break;
				}
				font->indirection[c] = indirected;
            }

            {
                int page = (font->numShaderPages > 1) ? font->pageForLoc[indirected] : 0;
                if (page < 0 || page >= font->numShaderPages) {
                    page = 0;
                }
                if (page != curPage) {
                    if (curPage != -1) {
                        RB_EndSurface();
                    }
                    RB_BeginSurface((shader_t*)font->shaderPages[page], 0);
                    curPage = page;
                }
            }

            RB_CHECKOVERFLOW(4, 6);

            loc = &font->locations[indirected];

            tess.texCoords[tess.numVertexes][0][0] = loc->pos[0];
            tess.texCoords[tess.numVertexes][0][1] = loc->pos[1];
            tess.texCoords[tess.numVertexes + 1][0][0] = loc->size[0] + loc->pos[0];
            tess.texCoords[tess.numVertexes + 1][0][1] = loc->pos[1];
            tess.texCoords[tess.numVertexes + 2][0][0] = loc->pos[0];
            tess.texCoords[tess.numVertexes + 2][0][1] = loc->size[1] + loc->pos[1];
            tess.texCoords[tess.numVertexes + 3][0][0] = loc->size[0] + loc->pos[0];
            tess.texCoords[tess.numVertexes + 3][0][1] = loc->size[1] + loc->pos[1];

            tess.xyz[tess.numVertexes][0] = x;
            tess.xyz[tess.numVertexes][1] = y;
            tess.xyz[tess.numVertexes][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 1][0] = x + s_fontGeneralScale * loc->size[0] * 256.0;
            tess.xyz[tess.numVertexes + 1][1] = y;
            tess.xyz[tess.numVertexes + 1][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 2][0] = x;
            tess.xyz[tess.numVertexes + 2][1] = y + charHeight;
            tess.xyz[tess.numVertexes + 2][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 3][0] = x + s_fontGeneralScale * loc->size[0] * 256.0;
            tess.xyz[tess.numVertexes + 3][1] = y + charHeight;
            tess.xyz[tess.numVertexes + 3][2] = s_fontZ;

            tess.indexes[tess.numIndexes] = tess.numVertexes;
            tess.indexes[tess.numIndexes + 1] = tess.numVertexes + 1;
            tess.indexes[tess.numIndexes + 2] = tess.numVertexes + 2;
            tess.indexes[tess.numIndexes + 3] = tess.numVertexes + 1;
            tess.indexes[tess.numIndexes + 4] = tess.numVertexes + 3;
            tess.indexes[tess.numIndexes + 5] = tess.numVertexes + 2;

            if (pvVirtualScreen)
            {
                tess.xyz[tess.numVertexes][0] *= fWidthScale;
                tess.xyz[tess.numVertexes][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 1][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 1][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 2][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 2][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 3][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 3][1] *= fHeightScale;
            }

            x += s_fontGeneralScale * loc->size[0] * 256.0;
            tess.numVertexes += 4;
            tess.numIndexes += 6;
            break;
        }
    }

    if (curPage != -1) {
        RB_EndSurface();
    }
}

void R_DrawString(fontheader_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen) {
    int i;
    int code;
    unsigned short uch;
    char buffer[512];
    size_t buflen;
    int cursgl;
    float curX, curY;
    float curHeight;
    
    if (!font->numPages) {
        if (font->sgl[0]) {
            R_DrawString_sgl(font->sgl[0], text, x, y, maxlen, pvVirtualScreen);
        }
        return;
    }

    if (maxlen < 0) {
        maxlen = strlen(text);
    }
    
    curX = x;
    curY = y;
    curHeight = 0.f;
    cursgl = -1;
    buflen = 0;

    i = 0;
    while(i < maxlen) {
        fontchartable_t* ct;

        uch = text[i];
        i++;

        if (DBCSIsLeadByte(font, uch)) {
            uch = (uch << 8) | text[i];
            i++;
        }

        if (!uch) {
            break;
        }

        if (uch == '\n' || uch == '\r') {
            buffer[buflen] = 0;
            R_DrawString_sgl(font->sgl[cursgl], buffer, curX, curY, maxlen, pvVirtualScreen);
            
            curX = x;
            curHeight = 0.f;
            buflen = 0;
            if (uch == '\n') {
                curY += font->sgl[0]->height * s_fontGeneralScale * s_fontHeightScale;
            }

            continue;
        }

        if (uch == '^') {
            int next = (signed char)text[i];

            if (next >= '0' && next <= '9') {
                if (buflen) {
                    buffer[buflen] = 0;
                    R_DrawString_sgl(font->sgl[cursgl], buffer, curX, curY, maxlen, pvVirtualScreen);

                    curX += curHeight;
                    curHeight = 0.f;
                    buflen = 0;
                }

                R_ChangeTextColor(atoi(&text[i]));
                i++;
                continue;
            }
        }

        code = CodeSearch(font, uch);
        if (code < 0) {
            continue;
        }

        if (cursgl == -1) {
            cursgl = font->charTable[code].index;
        }

        ct = &font->charTable[code];
        if (cursgl != ct->index || buflen >= ARRAY_LEN(buffer) - 2) {
            buffer[buflen] = 0;
            R_DrawString_sgl(font->sgl[cursgl], buffer, curX, curY, maxlen, pvVirtualScreen);

            curX += curHeight;
            curHeight = 0.f;
            buflen = 0;
            ct = &font->charTable[code];
            cursgl = ct->index;
        }

        buffer[buflen++] = ct->loc;
        curHeight += font->sgl[cursgl]->locations[ct->loc].size[0] * 256.0f;
    }

    if (buflen)
    {
        buffer[buflen] = 0;
        R_DrawString_sgl(font->sgl[cursgl], buffer, curX, y, buflen, pvVirtualScreen);
    }
}

void R_DrawFloatingString_sgl(fontheader_sgl_t* font, const char* text, const vec3_t org, const vec4_t color, float scale, int maxlen) {
    shader_t* fontshader;
    qhandle_t fsh;
    float charWidth, charHeight;
    int i;
    vec3_t pos;
    polyVert_t verts[4];

    if (!font) {
        return;
    }

    R_IssuePendingRenderCommands();
    if (font->trhandle != r_sequencenumber) {
        font->shader = NULL;
    }

    if (!font->shader) {
        R_LoadFontShader(font);
    }

    i = 0;
    fontshader = (shader_t*)font->shader;
    fsh = 0;

    for (i = 0; i < tr.numShaders; i++)
    {
        if (fontshader == tr.shaders[i])
        {
            fsh = i;
            break;
        }
    }

    i = 0;
    charHeight = font->height * s_fontHeightScale * s_fontGeneralScale * scale;
    VectorCopy(org, pos);

    for (i = 0; text[i]; i++) {
        unsigned char c;
        int indirected;
        letterloc_t* loc;
        
        c = text[i];
        indirected = font->indirection[c];
        if (indirected == -1)
        {
            ri.Printf(PRINT_ALL, "R_DrawFloatingString: no 0x%02x-character in font!\n", c);
            continue;
        }

        loc = &font->locations[indirected];

        verts[0].modulate[0] = (int)(color[0] * 255.0);
        verts[0].modulate[1] = (int)(color[1] * 255.0);
        verts[0].modulate[2] = (int)(color[2] * 255.0);
        verts[0].modulate[3] = (int)(color[3] * 255.0);
        verts[1].modulate[0] = verts[0].modulate[0];
        verts[1].modulate[1] = verts[0].modulate[1];
        verts[1].modulate[2] = verts[0].modulate[2];
        verts[1].modulate[3] = verts[0].modulate[3];
        verts[2].modulate[0] = verts[0].modulate[0];
        verts[2].modulate[1] = verts[0].modulate[1];
        verts[2].modulate[2] = verts[0].modulate[2];
        verts[2].modulate[3] = verts[0].modulate[3];
        verts[3].modulate[0] = verts[0].modulate[0];
        verts[3].modulate[1] = verts[0].modulate[1];
        verts[3].modulate[2] = verts[0].modulate[2];
        verts[3].modulate[3] = verts[0].modulate[3];

        verts[0].st[0] = loc->pos[0];
        verts[0].st[1] = loc->pos[1];
        verts[1].st[0] = loc->pos[0] + font->locations[indirected].size[0];
        verts[1].st[1] = loc->pos[1];
        verts[2].st[0] = verts[1].st[0];
        verts[2].st[1] = loc->pos[1] + loc->size[1];
        verts[3].st[0] = loc->pos[0];
        verts[3].st[1] = verts[2].st[1];
        VectorCopy(pos, verts[3].xyz);

        charWidth = font->locations[indirected].size[0] * 256.0 * s_fontGeneralScale * scale;
        verts[2].xyz[0] = pos[0] + tr.refdef.viewaxis[1][0] * -charWidth;
        verts[2].xyz[1] = pos[1] + tr.refdef.viewaxis[1][1] * -charWidth;
        verts[2].xyz[2] = pos[2] + tr.refdef.viewaxis[1][2] * -charWidth;
        verts[1].xyz[0] = verts[2].xyz[0] + charHeight * tr.refdef.viewaxis[2][0];
        verts[1].xyz[1] = verts[2].xyz[1] + charHeight * tr.refdef.viewaxis[2][1];
        verts[1].xyz[2] = verts[2].xyz[2] + charHeight * tr.refdef.viewaxis[2][2];
        verts[0].xyz[1] = verts[1].xyz[1] + tr.refdef.viewaxis[1][1] * charWidth;
        verts[0].xyz[2] = verts[1].xyz[2] + tr.refdef.viewaxis[1][2] * charWidth;
        verts[0].xyz[0] = verts[1].xyz[0] + tr.refdef.viewaxis[1][0] * charWidth;
    
        if (RE_AddPolyToScene(fsh, 4, verts, 0)) {
            ++tr.refdef.numPolys;
        }

        pos[0] = verts[2].xyz[0];
        pos[1] = verts[2].xyz[1];
        pos[2] = verts[2].xyz[2];
    }
}

void R_DrawFloatingString(fontheader_t* font, const char* text, const vec3_t org, const vec4_t color, float scale, int maxlen) {
    return R_DrawFloatingString_sgl(font->sgl[0], text, org, color, scale, maxlen);
}

float R_GetFontHeight(const fontheader_t* font)
{
    if (!font || !font->sgl[0]) {
        return 0.0;
    }

    return font->sgl[0]->height * s_fontGeneralScale * s_fontHeightScale;
}

float R_GetFontStringWidth_sgl(const fontheader_sgl_t* font, const char* s)
{
    float widths;
    int i;

    widths = 0.0;

    if (!font) {
        return 0.0;
    }

    for (i = 0; s[i]; i++)
    {
        int indirected;
        unsigned char c = s[i];

        if (c == '\t')
        {
            indirected = font->indirection[32];
            if (indirected != -1) {
                widths += font->locations[indirected].size[0] * 3.0;
            } else {
                ri.Printf(PRINT_ALL, "R_GetFontStringWidth: no space-character in font!\n");
            }
        }
        else
        {
            indirected = font->indirection[c];
            if (indirected != -1) {
                widths += font->locations[indirected].size[0];
            } else {
                ri.Printf(PRINT_ALL, "R_GetFontStringWidth: no 0x%02x-character in font!\n", c);
            }
        }
    }

    return widths * s_fontGeneralScale * 256.0;
}

float R_GetFontStringWidth(const fontheader_t* font, const char* s)
{
    int i;
    int code;
    fontchartable_t* ct;
    float width = 0.f;

    if (!font->numPages) {
        return R_GetFontStringWidth_sgl(font->sgl[0], s);
    }

    i = 0;
    while(s[i]) {
        unsigned char uch = s[i++];

        if (DBCSIsLeadByte(font, uch)) {
            uch = (uch << 8) | s[i++];
            if (!uch) {
                break;
            }
        }

        if (uch == '\t') {
            code = CodeSearch(font, ' ');
            if (code >= 0) {
                ct = &font->charTable[code];
                width += font->sgl[ct->index]->locations[ct->loc].size[0] * 3.f;
            }
        }
        else {
            code = CodeSearch(font, uch);
            if (code >= 0) {
                ct = &font->charTable[code];
                width += font->sgl[ct->index]->locations[ct->loc].size[0];
            }
        }
    }

    return width;
}
