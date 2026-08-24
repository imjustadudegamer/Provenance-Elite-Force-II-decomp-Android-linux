

#pragma once
#if defined( GAME_DLL )
#include "../fgame/str.h"
#else

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cinttypes>

#ifdef _WIN32
#    pragma warning(disable : 4710) 
#endif

#ifndef Q_PRINTF_FUNC
#    ifdef __GNUC__
#        ifdef __MINGW32__
#            define Q_PRINTF_FUNC(fmt, va) \
	__attribute__((format(gnu_printf, fmt, va))) \
	__attribute__((format(ms_printf, fmt, va)))
#        else
#            define Q_PRINTF_FUNC(fmt, va) __attribute__((format(printf, fmt, va)))
#        endif
#    else
#        define Q_PRINTF_FUNC(fmt, va)
#    endif
#endif

void TestStringClass();

class strdata
{
public:
    // NOTE: the initialiser list order must follow the member declaration order
    strdata()
        : data(NULL)
        , refcount(0)
        , alloced(0)
        , len(0)
    {}

    ~strdata()
    {
        if (data) {
            delete[] data;
        }
    }

    void AddRef() { refcount++; }

    bool DelRef() 
    {
        refcount--;
        if (refcount < 0) {
            delete this;
            return true;
        }

        return false;
    }

    char  *data;
    int    refcount;
    size_t alloced;
    size_t len;
};

class str
{
protected:
    friend class Archiver;
    strdata *m_data;
    void     EnsureAlloced(size_t, bool keepold = true);
    void     EnsureDataWritable();

public:
    ~str();
    str();
    str(const char *text);
    str(const char *text, size_t len);
    str(const str& string);
    str(const str& string, size_t start, size_t end);
    str(const char ch);
    str(const int num);
    str(const float num);
    str(const unsigned int num);
    str(const long num);
    str(const unsigned long num);
    str(const long long num);
    str(const unsigned long long num);

    str(str&& string);
    str& operator=(str&& string);

    size_t      length(void) const;
    const char *c_str(void) const;

    void append(const char *text);
    void append(const str& text);

    char  operator[](intptr_t index) const;
    char& operator[](intptr_t index);

    void operator=(const str& text);
    void operator=(const char *text);

    friend str operator+(const str& a, const str& b);
    friend str operator+(const str& a, const char *b);
    friend str operator+(const char *a, const str& b);

    friend str operator+(const str& a, const float b);
    friend str operator+(const str& a, const int b);
    friend str operator+(const str& a, const unsigned b);
    friend str operator+(const str& a, const bool b);
    friend str operator+(const str& a, const char b);

    str& operator+=(const str& a);
    str& operator+=(const char *a);
    str& operator+=(const float a);
    str& operator+=(const char a);
    str& operator+=(const int a);
    str& operator+=(const unsigned a);
    str& operator+=(const bool a);

    str& operator-=(int c);
    str& operator--(int);

    friend bool operator==(const str& a, const str& b);
    friend bool operator==(const str& a, const char *b);
    friend bool operator==(const char *a, const str& b);

    friend bool operator!=(const str& a, const str& b);
    friend bool operator!=(const str& a, const char *b);
    friend bool operator!=(const char *a, const str& b);

    operator const char *() const;

    int icmpn(const char *text, size_t n) const;
    int icmpn(const str& text, size_t n) const;
    int icmp(const char *text) const;
    int icmp(const str& text) const;
    int cmpn(const char *text, size_t n) const;
    int cmpn(const str& text, size_t n) const;

    void tolower(void);
    void toupper(void);

    static char *tolower(char *s1);
    static char *toupper(char *s1);

    static int icmpn(const char *s1, const char *s2, size_t n);
    static int icmp(const char *s1, const char *s2);
    static int cmpn(const char *s1, const char *s2, size_t n);
    static int cmp(const char *s1, const char *s2);

    static void snprintf(char *dst, int size, const char *fmt, ...) Q_PRINTF_FUNC(3, 4);
    void        strip(void);

    static bool isNumeric(const char *str);
    bool        isNumeric(void) const;

    void CapLength(size_t newlen);

    void        BackSlashesToSlashes();
    void        SlashesToBackSlashes();
    void        DefaultExtension(const char *extension);
    const char *GetExtension() const;
    void        StripExtension();
    void        SkipFile();
    void        SkipPath();
};

char *strstrip(char *string);
char *strlwc(char *string);

inline char str::operator[](intptr_t index) const
{
    assert(m_data);

    if (!m_data) {
        return 0;
    }

    assert((index >= 0) && (index < (int)m_data->len));

    if ((index < 0) || (index >= (int)m_data->len)) {
        return 0;
    }

    return m_data->data[index];
}

inline size_t str::length(void) const
{
    return (m_data != NULL) ? m_data->len : 0;
}

inline str::~str()
{
    if (m_data) {
        m_data->DelRef();
        m_data = NULL;
    }
}

inline str::str()
    : m_data(NULL)
{}

inline str::str(const char *text)
    : m_data(NULL)
{
    size_t len;

    assert(text);
    if (text && *text) {
        len = strlen(text);

        if (len) {
            EnsureAlloced(len + 1);
            strcpy(m_data->data, text);
            m_data->len = len;
        }
    }
}

inline str::str(const char *text, size_t len)
    : m_data(NULL)
{
    assert(text);
    if (*text && len) {
        EnsureAlloced(len + 1);
        strncpy(m_data->data, text, len + 1);
        m_data->data[len]= 0;
        m_data->len = len;
    }
}

inline str::str(const str& text)
    : m_data(NULL)
{
    if (text.m_data) {
        text.m_data->AddRef();
    }

    if (m_data) {
        m_data->DelRef();
    }

    m_data = text.m_data;
}

inline str::str(const str& text, size_t start, size_t end)
    : m_data(NULL)
{
    size_t i;
    size_t len;

    if (end > text.length()) {
        end = text.length();
    }

    if (start > text.length()) {
        start = text.length();
    }

    if (end >= start) {
        len = end - start;
    } else {
        len = 0;
    }

    if (len > 0) {
        EnsureAlloced(len + 1);

        for (i = 0; i < len; i++) {
            m_data->data[i] = text[start + i];
        }

        m_data->data[len] = 0;
        m_data->len       = len;
    }
}

inline str::str(const char ch)
    : m_data(NULL)
{
    EnsureAlloced(2);

    m_data->data[0] = ch;
    m_data->data[1] = 0;
    m_data->len     = 1;
}

inline str::str(const float num)
    : m_data(NULL)
{
    char   text[32];
    size_t len;

    snprintf(text, sizeof(text), "%.3f", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const int num)
    : m_data(NULL)
{
    char   text[32];
    size_t len;

    snprintf(text, sizeof(text), "%d", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const unsigned int num)
    : m_data(NULL)
{
    char   text[32];
    size_t len;

    snprintf(text, sizeof(text), "%u", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const long num)
    : m_data(NULL)
{
    char   text[64];
    size_t len;

    snprintf(text, sizeof(text), "%ld", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const unsigned long num)
    : m_data(NULL)
{
    char   text[64];
    size_t len;

    snprintf(text, sizeof(text), "%lu", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const long long num)
    : m_data(NULL)
{
    char   text[64];
    size_t len;

    snprintf(text, sizeof(text), "%lld", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline str::str(const unsigned long long num)
    : m_data(NULL)
{
    char   text[64];
    size_t len;

    snprintf(text, sizeof(text), "%llu", num);
    len = strlen(text);
    EnsureAlloced(len + 1);
    strcpy(m_data->data, text);
    m_data->len = len;
}

inline const char *str::c_str(void) const
{
    if (m_data) {
        return m_data->data;
    } else {
        return "";
    }
}

inline str::str(str&& string)
    : m_data(string.m_data)
{
    string.m_data = NULL;
}

inline str& str::operator=(str&& string)
{
    if (m_data) {
        m_data->DelRef();
    }

    m_data        = string.m_data;
    string.m_data = NULL;

    return *this;
}

inline void str::append(const char *text)
{
    size_t len;

    assert(text);

    if (text && *text) {
        len = length();
        len += strlen(text);
        EnsureAlloced(len + 1);

        strcat(m_data->data, text);
        m_data->len = len;
    }
}

inline void str::append(const str& text)
{
    append(text.c_str());
}

inline char& str::operator[](intptr_t index)
{
    static char dummy = 0;
    assert(m_data);

    EnsureDataWritable();

    if (!m_data) {
        return dummy;
    }

    assert((index >= 0) && (index < (int)m_data->len));

    if ((index < 0) || (index >= (int)m_data->len)) {
        return dummy;
    }

    return m_data->data[index];
}

inline void str::operator=(const str& text)
{
    if (text.m_data) {
        text.m_data->AddRef();
    }

    if (m_data) {
        m_data->DelRef();
    }

    m_data = text.m_data;
}

inline void str::operator=(const char *text)
{
    size_t len;

    assert(text);

    if (m_data) {
        if (text && text == m_data->data) {
            return; 
        }

        m_data->DelRef();
        m_data = NULL;
    }

    if (text && *text) {
        len = strlen(text);

        m_data          = new strdata;
        m_data->len     = len;
        m_data->alloced = len + 1;
        m_data->data    = new char[len + 1];
        strcpy(m_data->data, text);
    }
}

inline str operator+(const str& a, const str& b)
{
    str result(a);

    result.append(b);

    return result;
}

inline str operator+(const str& a, const char *b)
{
    str result(a);

    result.append(b);

    return result;
}

inline str operator+(const char *a, const str& b)
{
    str result(a);

    result.append(b);

    return result;
}

inline str operator+(const str& a, const bool b)
{
    str result(a);

    result.append(b ? "true" : "false");

    return result;
}

inline str operator+(const str& a, const char b)
{
    char text[2];

    text[0] = b;
    text[1] = 0;

    return a + text;
}

inline str& str::operator+=(const str& a)
{
    append(a);
    return *this;
}

inline str& str::operator+=(const char *a)
{
    append(a);
    return *this;
}

inline str& str::operator+=(const char a)
{
    char text[2];

    text[0] = a;
    text[1] = 0;
    append(text);

    return *this;
}

inline str& str::operator+=(const bool a)
{
    append(a ? "true" : "false");
    return *this;
}

inline bool operator==(const str& a, const str& b)
{
    return (!strcmp(a.c_str(), b.c_str()));
}

inline bool operator==(const str& a, const char *b)
{
    assert(b);
    if (!b) {
        return false;
    }
    return (!strcmp(a.c_str(), b));
}

inline bool operator==(const char *a, const str& b)
{
    assert(a);
    if (!a) {
        return false;
    }
    return (!strcmp(a, b.c_str()));
}

inline bool operator!=(const str& a, const str& b)
{
    return !(a == b);
}

inline bool operator!=(const str& a, const char *b)
{
    return !(a == b);
}

inline bool operator!=(const char *a, const str& b)
{
    return !(a == b);
}

inline int str::icmpn(const char *text, size_t n) const
{
    assert(text);

    return str::icmpn(c_str(), text, n);
}

inline int str::icmpn(const str& text, size_t n) const
{
    return str::icmpn(c_str(), text.c_str(), n);
}

inline int str::icmp(const char *text) const
{
    assert(text);

    return str::icmp(c_str(), text);
}

inline int str::icmp(const str& text) const
{
    return str::icmp(c_str(), text.c_str());
}

inline int str::cmpn(const char *text, size_t n) const
{
    assert(text);

    return str::cmpn(c_str(), text, n);
}

inline int str::cmpn(const str& text, size_t n) const
{
    return str::cmpn(c_str(), text.c_str(), n);
}

inline void str::tolower(void)
{
    if (m_data) {
        EnsureDataWritable();

        str::tolower(m_data->data);
    }
}

inline void str::toupper(void)
{
    if (m_data) {
        EnsureDataWritable();

        str::toupper(m_data->data);
    }
}

inline bool str::isNumeric(void) const
{
    assert(m_data);
    return str::isNumeric(m_data->data);
}

inline str::operator const char *(void) const
{
    return c_str();
}

str GetCapitalized(const str& value);

#endif 