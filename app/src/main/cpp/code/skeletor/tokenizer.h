

#pragma once

class Tokenizer
{
    char *pBuffer;
    char *pEnd;
    int   iLength;
    char *pCurrent;
    int   iLine;
    int   iError;
    bool  fTokenReady;
    char  szToken[256];

public:
    Tokenizer();
    Tokenizer(const char *pInputBuffer, int iBufferLength);

    void   SetBuffer(const char *pInputBuffer, int iBufferLength);
    void   Reset();
    int    GetLineNumber();
    bool   HasError();
    int    GetError();
    bool   SkipToEOL();
    bool   CheckOverflow();
    bool   SkipWhiteSpace(bool fCrossLine);
    bool   AtComment();
    void   SkipNonToken(bool fCrossLine);
    bool   TokenAvailable(bool fCrossLine);
    bool   CommentAvailable(bool fCrossLine);
    void   UnGetToken();
    bool   AtString(bool fCrossLine);
    char  *GetToken(bool fCrossLine);
    char  *GetLine(bool fCrossLine);
    char  *GetRaw();
    char  *GetString(bool fCrossLine);
    bool   GetSpecific(const char *szString);
    int    GetInteger(bool fCrossLine);
    double GetDouble(bool fCrossLine);
    float  GetFloat(bool fCrossLine);
    int    LinesInFile();
    char  *Token();
    char  *GetCurrentPointer();
};
