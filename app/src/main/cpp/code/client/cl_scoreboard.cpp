

#include "client.h"
#include "cl_scoreboard.h"

#define SB_ADDROW_PREFIX   "globalwidgetcommand dmscore addrow " 
#define SB_QUOTE           "\""                                  
#define SB_NAME_KEY        "name"                                
#define SB_QQ              "\" \""                               
#define SB_QUOTE_NL        "\"\n"                                
#define SB_EMPTY4          "\"\" \"\" \"\" \"\" \""              
#define SB_DELETE_ALL      "globalwidgetcommand dmscore deleteallitems" 
#define SB_PLAYERS_TITLE   "$$Players$$"                         
#define SB_SPECTATOR_TITLE "$$Spectators$$"                      
#define SB_SELECT_ROW      "globalwidgetcommand dmscore selectrowwithdata 13 %d" 
#define SB_ADD_HUD         "ui_addhud dm_scoreboard"             
#define SB_STATUS_TITLE    "globalwidgetcommand dm_status title \"\""   
#define SB_RED_HIDE        "globalwidgetcommand dmredteam hide"  
#define SB_BLUE_HIDE       "globalwidgetcommand dmblueteam hide" 
#define SB_SPECTATOR_NAME  "spectator"                           
#define SB_Q2Q             "\"  \""                              
#define SB_RED_NAME        "Red"                                 
#define SB_SPECTATOR_CELLS "\" \"\" \"\" \"\" \"\" \""           
#define SB_RED_TEAM_TITLE  "$$RedTeam$$"                         
#define SB_BLUE_TEAM_TITLE "$$BlueTeam$$"                        
#define SB_RED_TEAM_TITLEF "globalwidgetcommand dmredteam title \"$$RedShirts$$ %d\""  
#define SB_BLUE_TEAM_TITLEF "globalwidgetcommand dmblueteam title \"$$BlueShirts$$ %d\"" 
#define SB_RED_SHOW        "globalwidgetcommand dmredteam show"  
#define SB_BLUE_SHOW       "globalwidgetcommand dmblueteam show" 
#define SB_BGCOLOR_FMT     "globalwidgetcommand dmscore bgrowcolor \"%f\" \"%f\" \"%f\" \"%f\"" 
#define SB_EMPTY_ROW       "globalwidgetcommand dmscore addrow \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"%s\" \"\" \"\" \"\" \"\" \"\"" 
#define SB_BGCOLOR_RESET   "globalwidgetcommand dmscore bgrowcolor \"0.0\" \"0.0\" \"0.0\" \"0.5\"" 
#define SB_EMPTY_ROW_NO_TITLE "globalwidgetcommand dmscore addrow \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"EMPTY\" \"\" \"\" \"\" \"\" \"\" \"\"" 
#define SB_EMPTY_CELL      "\"EMPTY\" "                          
#define SB_QS              "\" "                                 
#define SB_REMOVE_HUD      "ui_removehud dm_scoreboard"          
#define SB_SCORE_CMD       "score"                               

#define SB_TIME_COLON      ":"                                   
#define SB_TIME_ZERO       "0"                                   

#define CS_SCOREBOARD_ICONS (1056)
#define CS_SCOREBOARD_PLAYER (1216)

#define MAX_ICONS 6

ScoreboardTeamEntry::ScoreboardTeamEntry()
    : score(0)
    , frags(0)
    , deaths(0)
{
}

str ScoreboardTeamEntry::getRowString(void) const
{
    return rowString;
}

int scoreCompareFunction(const void *a, const void *b)
{
    const ScoreboardTeamEntry *ea = (const ScoreboardTeamEntry *)a;
    const ScoreboardTeamEntry *eb = (const ScoreboardTeamEntry *)b;

    if (eb->score != ea->score) {
        return eb->score - ea->score;
    }

    if (eb->frags != ea->frags) {
        return eb->frags - ea->frags;
    }

    return ea->deaths - eb->deaths;
}

Scoreboard::Scoreboard()
    : m_lastRequestTime(0)
    , m_shown(false)
{
}

Scoreboard::~Scoreboard() {}

void Scoreboard::formatTimeString(str& text, int seconds) const
{
    text = va("%d", seconds / 60);
    text += SB_TIME_COLON;

    if (seconds % 60 < 10) {
        text += SB_TIME_ZERO;
    }

    text += seconds % 60;
}

void Scoreboard::addIcons(str& row, int index, int stride, int first, int base) const
{
    for (int slot = 0; slot < MAX_ICONS; slot++) {
        int n = atoi(Cmd_Argv(index * stride + first + base + slot));

        if (n > 0) {
            row += SB_QUOTE;
            const char *cs = CL_ConfigString(n + CS_SCOREBOARD_ICONS);
            if (cs) {
                row += cs;
            }
            row += SB_QS;
        } else {
            row += SB_EMPTY_CELL;
        }
    }
}

void Scoreboard::addTeamScoresToScoreboard(
    const str&                 title,
    int                        maxRows,
    const ScoreboardTeamEntry *entries,
    int                        numEntries,
    const UColor&              headerBackground,
    const UColor&              rowBackground
    )
{
    Cbuf_ExecuteText(
        EXEC_NOW,
        va(SB_BGCOLOR_FMT, headerBackground.r, headerBackground.g, headerBackground.b, headerBackground.a));

    Cbuf_ExecuteText(EXEC_NOW, va(SB_EMPTY_ROW, title.c_str()));

    Cbuf_ExecuteText(
        EXEC_NOW,
        va(SB_BGCOLOR_FMT, rowBackground.r, rowBackground.g, rowBackground.b, rowBackground.a));

    int count = numEntries;
    if (maxRows != -1 && maxRows < numEntries) {
        count = maxRows;
    }

    for (int i = 0; i < count; i++) {
        Cbuf_ExecuteText(EXEC_NOW, va("%s", entries[i].getRowString().c_str()));
    }

    Cbuf_ExecuteText(EXEC_NOW, SB_BGCOLOR_RESET);
    Cbuf_ExecuteText(EXEC_NOW, SB_EMPTY_ROW_NO_TITLE);
}

void Scoreboard::processDeathMatchScores(int count)
{
    str                row;
    str                timeText;
    ScoreboardTeamEntry players[MAX_SCOREBOARD_ROWS];
    ScoreboardTeamEntry spectators[MAX_SCOREBOARD_ROWS];
    int                numPlayers = 0;
    int                numSpectators = 0;

    for (int i = 0, arg = 3; i < count; i++, arg += 13) {
        int         clientNum = atoi(Cmd_Argv(arg));
        const char *info      = CL_ConfigString(clientNum + CS_SCOREBOARD_PLAYER);
        int         ping      = atoi(Cmd_Argv(arg + 4));
        int         seconds   = atoi(Cmd_Argv(arg + 5));

        formatTimeString(timeText, seconds);

        row = SB_ADDROW_PREFIX;
        addIcons(row, i, 13, 3, 7);
        row += SB_QUOTE;
        row += Info_ValueForKey(info, SB_NAME_KEY);
        row += SB_QQ;

        if (ping) {
            row += SB_EMPTY4;
            row += clientNum;
            row += SB_QUOTE;

            spectators[numSpectators].rowString = row;
            numSpectators++;
        } else {
            row += Cmd_Argv(arg + 1);
            row += SB_QQ;
            row += Cmd_Argv(arg + 2);
            row += SB_QQ;
            row += Cmd_Argv(arg + 3);
            row += SB_QQ;
            row += timeText;
            row += SB_QQ;
            row += Cmd_Argv(arg + 6);
            row += SB_QQ;
            row += clientNum;
            row += SB_QUOTE_NL;

            players[numPlayers].score     = atoi(Cmd_Argv(arg + 1));
            players[numPlayers].frags     = atoi(Cmd_Argv(arg + 2));
            players[numPlayers].deaths    = atoi(Cmd_Argv(arg + 3));
            players[numPlayers].rowString = row;
            numPlayers++;
        }
    }

    Cbuf_ExecuteText(EXEC_NOW, SB_DELETE_ALL);

    qsort(players, numPlayers, sizeof(ScoreboardTeamEntry), scoreCompareFunction);

    str playersTitle = SB_PLAYERS_TITLE;
    addTeamScoresToScoreboard(
        playersTitle,
        -1,
        players,
        numPlayers,
        UColor(0.3f, 0.0f, 0.3f, 0.5f),
        UColor(0.0f, 0.0f, 0.0f, 0.5f));

    str spectatorsTitle = SB_SPECTATOR_TITLE;
    addTeamScoresToScoreboard(
        spectatorsTitle,
        -1,
        spectators,
        numSpectators,
        UColor(0.5f, 0.5f, 0.5f, 1.0f),
        UColor(0.0f, 0.0f, 0.0f, 0.5f));

    Cbuf_ExecuteText(EXEC_NOW, va(SB_SELECT_ROW, cl.snap.ps.clientNum));

    if (!m_shown) {
        Cbuf_ExecuteText(EXEC_NOW, SB_ADD_HUD);
        Cbuf_ExecuteText(EXEC_NOW, SB_STATUS_TITLE);
        Cbuf_ExecuteText(EXEC_NOW, SB_RED_HIDE);
        Cbuf_ExecuteText(EXEC_NOW, SB_BLUE_HIDE);
        m_shown = true;
    }
}

void Scoreboard::processTeamMatchScores(int count)
{
    str                 row;
    str                 timeText;
    str                 teamName;
    ScoreboardTeamEntry red[MAX_SCOREBOARD_ROWS];
    ScoreboardTeamEntry blue[MAX_SCOREBOARD_ROWS];
    ScoreboardTeamEntry spectators[MAX_SCOREBOARD_ROWS];
    int                 numRed = 0;
    int                 numBlue = 0;
    int                 numSpectators = 0;

    int redScore  = atoi(Cmd_Argv(3));
    int blueScore = atoi(Cmd_Argv(4));

    bool redFirst = redScore > blueScore;

    for (int i = 0, arg = 5; i < count; i++, arg += 13) {
        int         clientNum = atoi(Cmd_Argv(arg));
        const char *info      = CL_ConfigString(clientNum + CS_SCOREBOARD_PLAYER);
        int         seconds   = atoi(Cmd_Argv(arg + 5));

        formatTimeString(timeText, seconds);

        row = SB_ADDROW_PREFIX;
        addIcons(row, i, 13, 5, 7);
        row += SB_QUOTE;
        row += Info_ValueForKey(info, SB_NAME_KEY);
        row += SB_QQ;

        teamName = Cmd_Argv(arg + 4);

        if (!strcmp(teamName.c_str(), SB_SPECTATOR_NAME)) {
            row += SB_SPECTATOR_CELLS;
            row += clientNum;
            row += SB_QUOTE;

            spectators[numSpectators].rowString = row;
            numSpectators++;
            continue;
        }

        row += Cmd_Argv(arg + 1);
        row += SB_QQ;
        row += Cmd_Argv(arg + 2);
        row += SB_Q2Q;
        row += Cmd_Argv(arg + 3);
        row += SB_QQ;
        row += timeText;
        row += SB_QQ;
        row += Cmd_Argv(arg + 6);
        row += SB_QQ;
        row += clientNum;
        row += SB_QUOTE;

        ScoreboardTeamEntry& table = strcmp(teamName.c_str(), SB_RED_NAME) == 0 ? red[numRed] : blue[numBlue];

        table.score     = atoi(Cmd_Argv(arg + 1));
        table.rowString = row;

        if (!strcmp(teamName.c_str(), SB_RED_NAME)) {
            numRed++;
        } else {
            numBlue++;
        }
    }

    qsort(red, numRed, sizeof(ScoreboardTeamEntry), scoreCompareFunction);
    qsort(blue, numBlue, sizeof(ScoreboardTeamEntry), scoreCompareFunction);

    Cbuf_ExecuteText(EXEC_NOW, SB_DELETE_ALL);

    str redTitle   = SB_RED_TEAM_TITLE;
    str blueTitle  = SB_BLUE_TEAM_TITLE;

    if (redFirst) {
        addTeamScoresToScoreboard(
            redTitle, 10, red, numRed, UColor(1.0f, 0.0f, 0.0f, 1.0f), UColor(0.3f, 0.0f, 0.0f, 0.5f));
        addTeamScoresToScoreboard(
            blueTitle, 10, blue, numBlue, UColor(0.0f, 0.0f, 1.0f, 1.0f), UColor(0.0f, 0.0f, 0.3f, 0.5f));
    } else {
        addTeamScoresToScoreboard(
            blueTitle, 10, blue, numBlue, UColor(0.0f, 0.0f, 1.0f, 1.0f), UColor(0.0f, 0.0f, 0.3f, 0.5f));
        addTeamScoresToScoreboard(
            redTitle, 10, red, numRed, UColor(1.0f, 0.0f, 0.0f, 1.0f), UColor(0.3f, 0.0f, 0.0f, 0.5f));
    }

    str spectatorsTitle = SB_SPECTATOR_TITLE;
    addTeamScoresToScoreboard(
        spectatorsTitle,
        -1,
        spectators,
        numSpectators,
        UColor(0.5f, 0.5f, 0.5f, 1.0f),
        UColor(0.0f, 0.0f, 0.0f, 0.5f));

    Cbuf_ExecuteText(EXEC_NOW, va(SB_SELECT_ROW, cl.snap.ps.clientNum));

    if (!m_shown) {
        Cbuf_ExecuteText(EXEC_NOW, SB_STATUS_TITLE);
        Cbuf_ExecuteText(EXEC_NOW, va(SB_RED_TEAM_TITLEF, redScore));
        Cbuf_ExecuteText(EXEC_NOW, va(SB_BLUE_TEAM_TITLEF, blueScore));
        Cbuf_ExecuteText(EXEC_NOW, SB_RED_SHOW);
        Cbuf_ExecuteText(EXEC_NOW, SB_BLUE_SHOW);
        Cbuf_ExecuteText(EXEC_NOW, SB_ADD_HUD);
    }
}

static Scoreboard *scoreboard = NULL;

qboolean UI_ParseScores(void)
{
    int count = atoi(Cmd_Argv(2));

    if (count > MAX_SCOREBOARD_ROWS) {
        count = MAX_SCOREBOARD_ROWS;
    }

    if (atoi(Cmd_Argv(1))) {
        scoreboard->processTeamMatchScores(count);
    } else {
        scoreboard->processDeathMatchScores(count);
    }

    return qtrue;
}

void UI_ShowScoreboard_f(void)
{
    if (!scoreboard) {
        return;
    }

    unsigned int lastTime = (unsigned int)scoreboard->lastRequestTime();

    if ((int)(lastTime + 2000) <= cls.realtime) {
        scoreboard->setLastRequestTime(cls.realtime);
        CL_AddReliableCommand(SB_SCORE_CMD, qfalse);
    }
}

void UI_HideScoreboard_f(void)
{
    if (!scoreboard) {
        return;
    }

    scoreboard->setLastRequestTime(0);
    Cbuf_ExecuteText(EXEC_NOW, SB_REMOVE_HUD);
    scoreboard->setShown(false);
}

void UI_CreateScoreboard(void)
{
    if (!scoreboard) {
        scoreboard = new Scoreboard;
    }
}

void UI_DeleteScoreboard(void)
{
    if (scoreboard) {
        delete scoreboard;
        scoreboard = NULL;
    }
}
