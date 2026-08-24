#pragma once

#include "str.h"
#include "../uilib/ucolor.h"

class ScoreboardTeamEntry
{
public:
    ScoreboardTeamEntry();

    str getRowString(void) const;

    int score;        
    int frags;        
    int deaths;       
    str rowString;    
};

int scoreCompareFunction(const void *a, const void *b);

class Scoreboard
{
public:
    Scoreboard();
    ~Scoreboard();

    void processDeathMatchScores(int count);
    void processTeamMatchScores(int count);

    void addTeamScoresToScoreboard(
        const str&                 title,
        int                        maxRows,
        const ScoreboardTeamEntry *entries,
        int                        numEntries,
        const UColor&              headerBackground,
        const UColor&              rowBackground
        );

    void addIcons(str& row, int index, int stride, int first, int base) const;
    void formatTimeString(str& text, int seconds) const;

    int  lastRequestTime(void) const { return m_lastRequestTime; }
    void setLastRequestTime(int t) { m_lastRequestTime = t; }
    bool shown(void) const { return m_shown; }
    void setShown(bool shown) { m_shown = shown; }

private:
    str  m_rowA;              
    str  m_rowB;              
    int  m_lastRequestTime;   
    bool m_shown;             
};

#define MAX_SCOREBOARD_ROWS 128

qboolean UI_ParseScores(void);

void UI_CreateScoreboard(void);
void UI_DeleteScoreboard(void);
void UI_ShowScoreboard_f(void);
void UI_HideScoreboard_f(void);
