/*
 *  This is a modified version of Mark Adlers work,
 *  see below for the original copyright.
 *  2006 - Joerg Dietrich <dietrich_joerg@gmx.de>
 */



#include <setjmp.h>             
#include "puff.h"		

#define local static            

#define MAXBITS 15              
#define MAXLCODES 286           
#define MAXDCODES 30            
#define MAXCODES (MAXLCODES+MAXDCODES)  
#define FIXLCODES 288           

struct state {
    uint8_t *out;         
    uint32_t outlen;       
    uint32_t outcnt;       

    uint8_t *in;          
    uint32_t inlen;        
    uint32_t incnt;        
    int32_t bitbuf;                 
    int32_t bitcnt;                 

    jmp_buf env;
};

local int32_t bits(struct state *s, int32_t need)
{
    int32_t val;           

    val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->incnt == s->inlen) longjmp(s->env, 1);   
        val |= (int32_t)(s->in[s->incnt++]) << s->bitcnt;  
        s->bitcnt += 8;
    }

    s->bitbuf = (int32_t)(val >> need);
    s->bitcnt -= need;

    return (int32_t)(val & ((1L << need) - 1));
}

local int32_t stored(struct state *s)
{
    uint32_t len;       

    s->bitbuf = 0;
    s->bitcnt = 0;

    if (s->incnt + 4 > s->inlen) return 2;      
    len = s->in[s->incnt++];
    len |= s->in[s->incnt++] << 8;
    if (s->in[s->incnt++] != (~len & 0xff) ||
        s->in[s->incnt++] != ((~len >> 8) & 0xff))
        return -2;                              

    if (s->incnt + len > s->inlen) return 2;    
    if (s->out != NULL) {
        if (s->outcnt + len > s->outlen)
            return 1;                           
        while (len--)
            s->out[s->outcnt++] = s->in[s->incnt++];
    }
    else {                                      
        s->outcnt += len;
        s->incnt += len;
    }

    return 0;
}

struct huffman {
    int16_t *count;       
    int16_t *symbol;      
};

local int32_t decode(struct state *s, struct huffman *h)
{
    int32_t len;            
    int32_t code;           
    int32_t first;          
    int32_t count;          
    int32_t index;          
    int32_t bitbuf;         
    int32_t left;           
    int16_t *next;        

    bitbuf = s->bitbuf;
    left = s->bitcnt;
    code = first = index = 0;
    len = 1;
    next = h->count + 1;
    while (1) {
        while (left--) {
            code |= bitbuf & 1;
            bitbuf >>= 1;
            count = *next++;
            if (code < first + count) { 
                s->bitbuf = bitbuf;
                s->bitcnt = (s->bitcnt - len) & 7;
                return h->symbol[index + (code - first)];
            }
            index += count;             
            first += count;
            first <<= 1;
            code <<= 1;
            len++;
        }
        left = (MAXBITS+1) - len;
        if (left == 0) break;
        if (s->incnt == s->inlen) longjmp(s->env, 1);   
        bitbuf = s->in[s->incnt++];
        if (left > 8) left = 8;
    }
    return -9;                          
}

local int32_t construct(struct huffman *h, int16_t *length, int32_t n)
{
    int32_t symbol;         
    int32_t len;            
    int32_t left;           
    int16_t offs[MAXBITS+1];      

    for (len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        (h->count[length[symbol]])++;   
    if (h->count[0] == n)               
        return 0;                       

    left = 1;                           
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;                     
        left -= h->count[len];          
        if (left < 0) return left;      
    }                                   

    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];

    for (symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = symbol;

    return left;
}

local int32_t codes(struct state *s,
                struct huffman *lencode,
                struct huffman *distcode)
{
    int32_t symbol;         
    int32_t len;            
    uint32_t dist;          
    static const int16_t lens[29] = { 
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int16_t lext[29] = { 
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int16_t dists[30] = { 
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
    static const int16_t dext[30] = { 
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};

    do {
        symbol = decode(s, lencode);
        if (symbol < 0) return symbol;  
        if (symbol < 256) {             
            if (s->out != NULL) {
                if (s->outcnt == s->outlen) return 1;
                s->out[s->outcnt] = symbol;
            }
            s->outcnt++;
        }
        else if (symbol > 256) {        
            symbol -= 257;
            if (symbol >= 29) return -9;        
            len = lens[symbol] + bits(s, lext[symbol]);

            symbol = decode(s, distcode);
            if (symbol < 0) return symbol;      
            dist = dists[symbol] + bits(s, dext[symbol]);
            if (dist > s->outcnt)
                return -10;     

            if (s->out != NULL) {
                if (s->outcnt + len > s->outlen) return 1;
                while (len--) {
                    s->out[s->outcnt] = s->out[s->outcnt - dist];
                    s->outcnt++;
                }
            }
            else
                s->outcnt += len;
        }
    } while (symbol != 256);            

    return 0;
}

local int32_t fixed(struct state *s)
{
    static int32_t virgin = 1;
    static int16_t lencnt[MAXBITS+1], lensym[FIXLCODES];
    static int16_t distcnt[MAXBITS+1], distsym[MAXDCODES];
    static struct huffman lencode = {lencnt, lensym};
    static struct huffman distcode = {distcnt, distsym};

    if (virgin) {
        int32_t symbol;
        int16_t lengths[FIXLCODES];

        for (symbol = 0; symbol < 144; symbol++)
            lengths[symbol] = 8;
        for (; symbol < 256; symbol++)
            lengths[symbol] = 9;
        for (; symbol < 280; symbol++)
            lengths[symbol] = 7;
        for (; symbol < FIXLCODES; symbol++)
            lengths[symbol] = 8;
        construct(&lencode, lengths, FIXLCODES);

        for (symbol = 0; symbol < MAXDCODES; symbol++)
            lengths[symbol] = 5;
        construct(&distcode, lengths, MAXDCODES);

        virgin = 0;
    }

    return codes(s, &lencode, &distcode);
}

local int32_t dynamic(struct state *s)
{
    int32_t nlen, ndist, ncode;             
    int32_t index;                          
    int32_t err;                            
    int16_t lengths[MAXCODES];            
    int16_t lencnt[MAXBITS+1], lensym[MAXLCODES];         
    int16_t distcnt[MAXBITS+1], distsym[MAXDCODES];       
    struct huffman lencode = {lencnt, lensym};          
    struct huffman distcode = {distcnt, distsym};       
    static const int16_t order[19] =      
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    nlen = bits(s, 5) + 257;
    ndist = bits(s, 5) + 1;
    ncode = bits(s, 4) + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES)
        return -3;                      

    for (index = 0; index < ncode; index++)
        lengths[order[index]] = bits(s, 3);
    for (; index < 19; index++)
        lengths[order[index]] = 0;

    err = construct(&lencode, lengths, 19);
    if (err != 0) return -4;            

    index = 0;
    while (index < nlen + ndist) {
        int32_t symbol;             
        int32_t len;                

        symbol = decode(s, &lencode);
        if (symbol < 16)                
            lengths[index++] = symbol;
        else {                          
            len = 0;                    
            if (symbol == 16) {         
                if (index == 0) return -5;      
                len = lengths[index - 1];       
                symbol = 3 + bits(s, 2);
            }
            else if (symbol == 17)      
                symbol = 3 + bits(s, 3);
            else                        
                symbol = 11 + bits(s, 7);
            if (index + symbol > nlen + ndist)
                return -6;              
            while (symbol--)            
                lengths[index++] = len;
        }
    }

    err = construct(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        return -7;      

    err = construct(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        return -8;      

    return codes(s, &lencode, &distcode);
}

int32_t puff(uint8_t  *dest,           
             uint32_t *destlen,        
             uint8_t  *source,         
             uint32_t *sourcelen)      
{
    struct state s;             
    int32_t last, type;             
    int32_t err;                    

    s.out = dest;
    s.outlen = *destlen;                
    s.outcnt = 0;

    s.in = source;
    s.inlen = *sourcelen;
    s.incnt = 0;
    s.bitbuf = 0;
    s.bitcnt = 0;

    if (setjmp(s.env) != 0)             
        err = 2;                        
    else {
        do {
            last = bits(&s, 1);         
            type = bits(&s, 2);         
            err = type == 0 ? stored(&s) :
                  (type == 1 ? fixed(&s) :
                   (type == 2 ? dynamic(&s) :
                    -1));               
            if (err != 0) break;        
        } while (!last);
    }

    if (err <= 0) {
        *destlen = s.outcnt;
        *sourcelen = s.incnt;
    }
    return err;
}
