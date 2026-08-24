
#pragma once

#define EDITFIELD_BUFSIZE 256

class EditField
{
public:
    char m_buffer[EDITFIELD_BUFSIZE];
    int  m_cursor;

    EditField();
};

inline EditField::EditField()
{
    m_buffer[0] = 0;
    m_cursor    = 0;
}
