
#pragma once

typedef struct string_info_s {
    char  text[256];      
    float x;              
    float y;              
    int   width;          
    float color[4];       
    char  fontname[64];   
} string_info_t;          

class View3D : public UIWidget
{
    float             m_printfadetime;
    float             m_printalpha;
    str               m_printstring;
    UIReggedMaterial *m_print_mat;
    qboolean          m_locationprint;
    int               m_x_coord;
    int               m_y_coord;
    qboolean          m_letterbox_active;
    Container<string_info_t *> m_printstringlist;

public:
    CLASS_PROTOTYPE(View3D);

protected:
    void Draw(void) override;
    void update(void) override;
    void DrawLetterbox(void);
    void DrawFades(void);
    void Draw2D(void);
    void DrawFPS(void);
    void PrintSound(int channel, const char *name, float vol, int rvol, float pitch, float base, int& line);
    void DrawSoundOverlay(void);
    void CenterPrint(void);
    void LocationPrint(void);
    void OnActivate(Event *ev);
    void OnDeactivate(Event *ev);


public:
    View3D();

    void         ClearCenterPrint(void);
    void         UpdateCenterPrint(const char *s, float alpha);
    void         UpdateLocationPrint(int x, int y, const char *s, float alpha);
    void         AddPrintString(const char *text, float *pos, int width, float *color, const char *fontname);
    void         PrintStringList(void);
    qboolean     LetterboxActive(void);
    void         FrameInitialized(void) override;
    virtual void Pressed(Event *ev) override;
};

class ConsoleView : public UIWidget
{
public:
    CLASS_PROTOTYPE(ConsoleView);

protected:
    void Draw(void) override;
};
