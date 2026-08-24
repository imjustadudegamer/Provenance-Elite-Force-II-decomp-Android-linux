/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "../corepp/str.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#    pragma warning(disable : 4244) 
#    pragma warning(disable : 4710) 
#endif

static const int STR_ALLOC_GRAN = 20;

char *str::tolower(char *s1)
{
    char *s;

    s = s1;
    while (*s) {
        *s = ::tolower(*s);
        s++;
    }

    return s1;
}

char *str::toupper(char *s1)
{
    char *s;

    s = s1;
    while (*s) {
        *s = ::toupper(*s);
        s++;
    }

    return s1;
}

int str::icmpn(const char *s1, const char *s2, size_t n)
{
    int c1;
    int c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--) {
            return 0;
        }

        if (c1 != c2) {
            if (c1 >= 'a' && c1 <= 'z') {
                c1 -= ('a' - 'A');
            }

            if (c2 >= 'a' && c2 <= 'z') {
                c2 -= ('a' - 'A');
            }

            if (c1 < c2) {
                return -1;
            } else if (c1 > c2) {
                return 1;
            }
        }
    } while (c1);

    return 0;
}

int str::icmp(const char *s1, const char *s2)
{
    int c1;
    int c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2) {
            if (c1 >= 'a' && c1 <= 'z') {
                c1 -= ('a' - 'A');
            }

            if (c2 >= 'a' && c2 <= 'z') {
                c2 -= ('a' - 'A');
            }

            if (c1 < c2) {
                return -1;
            } else if (c1 > c2) {
                return 1;
            }
        }
    } while (c1);

    return 0;
}

int str::cmpn(const char *s1, const char *s2, size_t n)
{
    int c1;
    int c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--) {
            return 0;
        }

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
    } while (c1);

    return 0;
}

int str::cmp(const char *s1, const char *s2)
{
    int c1;
    int c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
    } while (c1);

    return 0;
}

bool str::isNumeric(const char *str)
{
    size_t len;
    size_t i;
    bool   dot;

    if (*str == '-') {
        str++;
    }

    dot = false;
    len = strlen(str);
    for (i = 0; i < len; i++) {
        if (!isdigit(str[i])) {
            if ((str[i] == '.') && !dot) {
                dot = true;
                continue;
            }
            return false;
        }
    }

    return true;
}

str operator+(const str& a, const float b)
{
    char text[20];

    str result(a);

    snprintf(text, sizeof(text), "%f", b);
    result.append(text);

    return result;
}

str operator+(const str& a, const int b)
{
    char text[20];

    str result(a);

    snprintf(text, sizeof(text), "%d", b);
    result.append(text);

    return result;
}

str operator+(const str& a, const unsigned b)
{
    char text[20];

    str result(a);

    snprintf(text, sizeof(text), "%u", b);
    result.append(text);

    return result;
}

str& str::operator+=(const float a)
{
    char text[20];

    snprintf(text, sizeof(text), "%f", a);
    append(text);

    return *this;
}

str& str::operator+=(const int a)
{
    char text[20];

    snprintf(text, sizeof(text), "%d", a);
    append(text);

    return *this;
}

str& str::operator+=(const unsigned a)
{
    char text[20];

    snprintf(text, sizeof(text), "%u", a);
    append(text);

    return *this;
}

str& str::operator-=(int c)
{
    if (!m_data) {
        return *this;
    }

    if (!m_data->len) {
        return *this;
    }

    if (m_data->len >= (size_t)c) {
        m_data->len -= c;
    } else {
        m_data->len = 0;
    }

    EnsureDataWritable();

    m_data->data[m_data->len] = 0;

    return *this;
}

str& str::operator--(int)
{
    *this -= 1;
    return *this;
}

void str::CapLength(size_t newlen)
{
    assert(m_data);

    if (length() <= newlen) {
        return;
    }

    EnsureDataWritable();

    m_data->data[newlen] = 0;
    m_data->len          = newlen;
}

void str::EnsureDataWritable(void)
{
    strdata *olddata;
    size_t   len;

    if (!m_data) {
        return;
    }

    if (!m_data->refcount) {
        return;
    }

    olddata = m_data;
    len     = length();

    m_data = new strdata;

    EnsureAlloced(len + 1, false);
    strncpy(m_data->data, olddata->data, len + 1);
    m_data->len = len;

    olddata->DelRef();
}

void str::EnsureAlloced(size_t amount, bool keepold)
{
    if (!m_data) {
        if (amount > 1) {
            m_data = new strdata;

            m_data->data    = new char[amount];
            m_data->alloced = amount;

            m_data->data[0] = '\0';

            return;
        }

        if (!m_data) {
            return;
        }
    }

    EnsureDataWritable();

    char *newbuffer;
    bool  wasalloced = (m_data->alloced != 0);

    if (amount < m_data->alloced) {
        return;
    }

    assert(amount);

    if (amount == 1) {
        m_data->alloced = 1;
    } else {
        size_t newsize, mod;

        mod = amount % STR_ALLOC_GRAN;

        if (!mod) {
            newsize = amount;
        } else {
            newsize = amount + STR_ALLOC_GRAN - mod;
        }

        m_data->alloced = newsize;
    }

    newbuffer = new char[m_data->alloced];

    if (wasalloced && keepold) {
        strcpy(newbuffer, m_data->data);
    }

    if (m_data->data) {
        delete[] m_data->data;
    }

    m_data->data = newbuffer;
}

void str::BackSlashesToSlashes(void)
{
    size_t i;

    EnsureDataWritable();

    for (i = 0; i < m_data->len; i++) {
        if (m_data->data[i] == '\\') {
            m_data->data[i] = '/';
        }
    }
}

void str::SlashesToBackSlashes(void)
{
    size_t i;

    EnsureDataWritable();

    for (i = 0; i < m_data->len; i++) {
        if (m_data->data[i] == '/') {
            m_data->data[i] = '\\';
        }
    }
}

void str::DefaultExtension(const char *extension)
{
    EnsureDataWritable();

    const char *src = m_data->data + m_data->len - 1;

    while (*src != '/' && src != m_data->data) {
        if (*src == '.') {
            return;
        }
        src--;
    }

    append(".");
    append(extension);
}

const char *str::GetExtension() const
{
    size_t length, i;

    length = m_data->len - 1;
    i      = length;

    while (m_data->data[i] != '.') {
        i--;
        if (m_data->data[i] == '/' || i == 0) {
            return ""; 
        }
    }

    return &m_data->data[i + 1];
}

void str::StripExtension()
{
    EnsureDataWritable();

    size_t i = m_data->len;
    while (i > 0 && m_data->data[i] != '.') {
        i--;
        if (m_data->data[i] == '/') {
            return; 
        }
    }
    if (i) {
        m_data->len               = i;
        m_data->data[m_data->len] = 0;

        EnsureDataWritable();
    }
}

void str::SkipFile()
{
    EnsureDataWritable();

    size_t i = m_data->len;
    while (i > 0 && m_data->data[i] != '/' && m_data->data[i] != '\\') {
        i--;
    }
    m_data->len               = i;
    m_data->data[m_data->len] = 0;

    EnsureDataWritable();
}

void str::SkipPath()
{
    EnsureDataWritable();

    const char *pathname = m_data->data;
    const char *last;

    last = m_data->data;
    while (*pathname) {
        if (*pathname == '/' || *pathname == '\\') {
            last = pathname + 1;
        }
        pathname++;
    }

    size_t lastpos = last - m_data->data;
    if (lastpos > 0) {
        size_t length = m_data->len - lastpos;
        for (size_t i = 0; i < length; i++) {
            m_data->data[i] = last[i];
        }

        m_data->len          = length;
        m_data->data[length] = 0;

        EnsureDataWritable();
    }
}

void str::snprintf(char *dst, int size, const char *fmt, ...)
{
    char    buffer[0x10000];
    int     len;
    va_list argptr;

    va_start(argptr, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

    (void)len;

    strncpy(dst, buffer, size - 1);
}

void str::strip(void)
{
    char  *last;
    char  *s;
    size_t i;

    if (m_data == NULL || m_data->data == NULL) {
        return;
    }
    s = m_data->data;
    while (isspace((int)*s) && *s) {
        s++;
    }

    last = s + m_data->len - (s - m_data->data);
    while (last > s) {
        if (!isspace((int)*(last - 1))) {
            break;
        }
        last--;
    }

    *last = '\0';

    m_data->len = last - s;
    for (i = 0; i < m_data->len + 1; i++) {
        m_data->data[i] = s[i];
    }

    EnsureDataWritable();
}

char *strstrip(char *string)
{
    char *last;
    char *s;

    s = string;
    while (isspace((int)*s) && *s) {
        s++;
    }

    last = s + strlen(s) - (s - string);
    while (last > s) {
        if (!isspace((int)*(last - 1))) {
            break;
        }
        last--;
    }

    *last = '\0';

    return s;
}

char *strlwc(char *string)
{
    char *s = string;
    while (*s) {
        *s = tolower(*s);
        s++;
    }

    return s;
}

#ifdef _WIN32
#    pragma warning(disable : 4189) 
#endif

void TestStringClass(void)
{
    char ch;        
    str *t;         
    str  a;         
    str  b;         
    str  c("test"); 
    str  d(c);      
    str  e(nullptr);
    size_t i; 

    i = a.length(); 
    i = c.length(); 

    const char *s1 = a.c_str(); 
    const char *s2 = c.c_str(); 

    t = new str(); 
    delete t;      

    b = "test";          
    t = new str("test"); 
    delete t;            

    a = c;         
    a = NULL;      
    a = c + d;     
    a = c + "wow"; 
    a = c + nullptr;
    a = "this" + d; 
    a = nullptr + d;
    a += c;     
    a += "wow"; 
    a += nullptr;

    a  = "test";  
    ch = a[0];    
    ch = a[-1];   
    ch = a[1000]; 
    ch = a[0];    
    ch = a[1];    
    ch = a[2];    
    ch = a[3];    
    ch = a[4];    
    ch = a[5];    

    a[1]  = 'b'; 
    a[-1] = 'b'; 
    a[0]  = '0'; 
    a[1]  = '1'; 
    a[2]  = '2'; 
    a[3]  = '3'; 
    a[4]  = '4'; 
    a[5]  = '5'; 
    a[7]  = '7'; 

    a = "test"; 
    b = "no";   

    i = (a == b); 
    i = (a == c); 

    i = (a == "blow"); 
    i = (a == "test"); 
    i =
        (a
         == NULL); 

    i = ("test" == b); 
    i = ("test" == a); 
    i =
        (NULL
         == a); 

    i = (a != b); 
    i = (a != c); 

    i = (a != "blow"); 
    i = (a != "test"); 
    i =
        (a
         != NULL); 

    i = ("test" != b); 
    i = ("test" != a); 
    i =
        (NULL
         != a); 

    a = "test"; 
    b = a;      

    a = "not"; 

    a = b; 

    a += b; 

    a = b;

    a[1] = '1'; 

    (void)ch;
    (void)i;
    (void)s1;
    (void)s2;
}

str GetCapitalized(const str& value)
{
    str newValue;

    if (!value.length()) {
        return "";
    }

    newValue += (char)toupper(value[0]);
    newValue += value.c_str() + 1;
    return newValue;
}

#ifdef _WIN32
#    pragma warning(default : 4189) 
#    pragma warning(disable : 4514) 
#endif
