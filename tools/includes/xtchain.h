/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/xtchain.h
 * DESCRIPTION: Common header for XTchain tools
 * DEVELOPERS:  Martin Storsjo <martin@martin.st>
 *              Rafal Kupiec <belliash@codingworkshop.eu.org>
 *              Aiken Harris <harraiken91@gmail.com>
 */

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <dirent.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>


#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#define XTCHAIN_PROCESS_ID _getpid
#define PATH_SEP '\\'
#else
#define XTCHAIN_PROCESS_ID getpid
#define PATH_SEP '/'
#endif


#define SECTOR_SIZE     512
#define _T(x)           x

#define YAML_MAX_BINARY (64u * 1024u * 1024u)
#define YAML_MAX_TEXT (512u * 1024u * 1024u)

typedef struct MBR_PARTITION {
    uint8_t BootFlag;       // 0x80 = bootable, 0x00 = non-boot
    uint8_t StartCHS [3];   // CHS address
    uint8_t Type;           // Partition type
    uint8_t EndCHS[3];      // CHS address
    uint32_t StartLBA;      // Start sector
    uint32_t Size;          // Sectors count
} MBR_PARTITION, *PMBR_PARTITION;

typedef struct _RESERVED_SECTOR_INFO
{
    int SectorNumber;
    const char* Description;
} RESERVED_SECTOR_INFO, *PRESERVED_SECTOR_INFO;

typedef struct _YAML_BUFFER
{
    unsigned char *data;
    size_t len, cap;
} YAML_BUFFER;

typedef enum _YAML_NODE_TYPE
{
    YAML_SCALAR,
    YAML_MAP,
    YAML_SEQUENCE,
    YAML_BINARY
} YAML_NODE_TYPE;
typedef struct _YAML_NODE YAML_NODE;

struct _YAML_NODE
{
    YAML_NODE_TYPE type;
    char *text;
    int IsInteger;
    unsigned char *bytes;
    size_t length;
    char **keys;
    YAML_NODE **values;
    size_t count, cap, *slots, slots_cap;
};

typedef struct YAML_ALLOCATION
{
    void *p;
    struct YAML_ALLOCATION *next;
} YAML_ALLOCATION;

typedef struct YAML_LINE
{
    char *s;
    size_t n, indent, number;
} YAML_LINE;

typedef struct YAML_PARSER
{
    YAML_LINE *lines;
    size_t count, at, col;
} YAML_PARSER;

typedef struct YAML_INLINE
{
    const char *s;
    size_t n, at, line;
} YAML_INLINE;

static const char *YamlProgramName = "xtchain";
static const char *(*YamlCharacterName)(uint32_t Codepoint) = NULL;
static YAML_ALLOCATION *YamlAllocations;
static void (*YamlValidationError)(void);

static inline void YamlWriterEmit(FILE *f, YAML_NODE *n, unsigned depth);
static inline YAML_NODE * YamlParserBlock(YAML_PARSER *p, size_t indent, unsigned depth);

static
inline
char *
_tcsrchrs(const char *str,
          char char1,
          char char2)
{
    char *ptr1 = strrchr(str, char1);
    char *ptr2 = strrchr(str, char2);

    if(!ptr1)
    {
        return ptr2;
    }

    if(!ptr2)
    {
        return ptr1;
    }

    if(ptr1 < ptr2)
    {
        return ptr2;
    }

    return ptr1;
}

static
inline
void
split_argv(const char *argv0,
           char **dir_ptr,
           char **basename_ptr,
           char **target_ptr,
           char **exe_ptr)
{
    const char *sep = _tcsrchrs(argv0, '/', '\\');
    const char *basename_ptr_const = argv0;
    char *dir = strdup(_T(""));

    if(sep)
    {
        dir = strdup(argv0);
        dir[sep + 1 - argv0] = '\0';
        basename_ptr_const = sep + 1;
    }

    char *basename = strdup(basename_ptr_const);
    char *period = strchr(basename, '.');

    if(period)
    {
        *period = '\0';
    }

    char *target = strdup(basename);
    char *dash = strrchr(target, '-');
    char *exe = basename;

    if(dash)
    {
        *dash = '\0';
        exe = dash + 1;
    }
    else
    {
        target = NULL;
    }

    if(dir_ptr)
    {
        *dir_ptr = dir;
    }

    if(basename_ptr)
    {
        *basename_ptr = basename;
    }

    if(target_ptr)
    {
        *target_ptr = target;
    }

    if(exe_ptr)
    {
        *exe_ptr = exe;
    }
}


/* Release only the allocations made after a validation checkpoint. */
static
inline
void
YamlReleaseAllocations(YAML_ALLOCATION *checkpoint)
{
    while(YamlAllocations != checkpoint)
    {
        YAML_ALLOCATION *a = YamlAllocations;
        YamlAllocations = a->next;
        free(a->p);
        free(a);
    }
}

/* A per-command arena ensures cleanup on every validation failure as well. */
static
inline
void
YamlCleanup(void)
{
    YamlReleaseAllocations(NULL);
}

static
inline
void
YamlError(const char *format,
          ...)
{
    if(YamlValidationError)
    {
        YamlValidationError();
    }
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "%s: error: ", YamlProgramName);
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static
inline
void *
YamlAllocateMemory(size_t n)
{
    YAML_ALLOCATION *a = malloc(sizeof(*a));
    void *p = calloc(n ? n : 1, 1);
    if(!a || !p)
    {
        free(a);
        free(p);
        /* Resource exhaustion is fatal, even during format detection. */
        YamlValidationError = NULL;
        YamlError("out of memory");
    }
    a->p = p;
    a->next = YamlAllocations;
    YamlAllocations = a;
    return p;
}

static
inline
char *
YamlCopyString(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = YamlAllocateMemory(n);
    memcpy(p, s, n);
    return p;
}

static
inline
char *
YamlFormatString(const char *format,
                 ...)
{
    if(!format)
    {
        YamlError("null format string");
    }
    va_list ap, aq;
    va_start(ap, format);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, format, aq);
    va_end(aq);
    if(n < 0)
    {
        YamlError("formatting failed");
    }
    char *p = YamlAllocateMemory((size_t)n + 1);
    vsnprintf(p, (size_t)n + 1, format, ap);
    va_end(ap);
    return p;
}

static
inline
void
YamlAppendBuffer(YAML_BUFFER *b,
                 const void *data,
                 size_t n)
{
    if(n > YAML_MAX_BINARY || b->len > YAML_MAX_BINARY - n)
    {
        YamlError("binary exceeds 64 MiB");
    }
    if(b->len + n > b->cap)
    {
        size_t cap = b->cap ? b->cap : 1024;
        while(cap < b->len + n)
        {
            cap *= 2;
        }
        unsigned char *p = YamlAllocateMemory(cap);
        if(b->len)
        {
            memcpy(p, b->data, b->len);
        }
        b->data = p;
        b->cap = cap;
    }
    if(n)
    {
        memcpy(b->data + b->len, data, n);
    }
    b->len += n;
}

static
inline
void
WriteUInt16(YAML_BUFFER *b,
            uint32_t v)
{
    if(v > 65535)
    {
        YamlError("value does not fit 16 bits");
    }
    unsigned char p[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    YamlAppendBuffer(b, p, 2);
}

static
inline
void
WriteUInt32(YAML_BUFFER *b,
            uint32_t v)
{
    unsigned char p[4] = {
        (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    YamlAppendBuffer(b, p, 4);
}

static
inline
uint32_t
ReadUInt16(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static
inline
uint32_t
ReadUInt32(const unsigned char *p)
{
    return ReadUInt16(p) | (ReadUInt16(p + 2) << 16);
}

static
inline
YAML_NODE *
YamlCreateNode(YAML_NODE_TYPE type)
{
    YAML_NODE *n = YamlAllocateMemory(sizeof(*n));
    n->type = type;
    return n;
}

static
inline
YAML_NODE *
YamlCreateString(const char *s)
{
    YAML_NODE *n = YamlCreateNode(YAML_SCALAR);
    n->text = YamlCopyString(s);
    return n;
}

static
inline
YAML_NODE *
YamlCreateInteger(uint32_t v)
{
    YAML_NODE *n = YamlCreateString(YamlFormatString("%u", v));
    n->IsInteger = 1;
    return n;
}

static
inline
YAML_NODE *
YamlCreateHexadecimal(uint32_t v,
                      unsigned width)
{
    return YamlCreateString(YamlFormatString("0x%0*X", (int)width, v));
}

static
inline
YAML_NODE *
YamlCreateCodepoint(uint32_t v)
{
    return YamlCreateString(YamlFormatString("U+%04X", v));
}

static
inline
YAML_NODE *
YamlCreateBinary(const void *data,
                 size_t n)
{
    YAML_NODE *v = YamlCreateNode(YAML_BINARY);
    v->bytes = YamlAllocateMemory(n);
    if(n)
    {
        memcpy(v->bytes, data, n);
    }
    v->length = n;
    return v;
}

static
inline
size_t
YamlWriterHash(const char *s)
{
    size_t h = 2166136261u;
    for(; *s; s++)
    {
        h = (h ^ (unsigned char)*s) * 16777619u;
    }
    return h;
}

static
inline
void
YamlWriterReserve(YAML_NODE *n)
{
    if(n->count == n->cap)
    {
        size_t cap = n->cap ? n->cap * 2 : 8;
        if(cap > 2097152)
        {
            YamlError("too many YAML entries");
        }
        YAML_NODE **v = YamlAllocateMemory(cap * sizeof(*v));
        char **k = YamlAllocateMemory(cap * sizeof(*k));
        if(n->count)
        {
            memcpy(v, n->values, n->count * sizeof(*v));
            memcpy(k, n->keys, n->count * sizeof(*k));
        }
        n->values = v;
        n->keys = k;
        n->cap = cap;
    }
}

static
inline
void
YamlAddField(YAML_NODE *n,
             const char *key,
             YAML_NODE *v)
{
    if(!n || n->type != YAML_MAP)
    {
        YamlError("expected mapping");
    }
    if(!n->slots_cap || (n->count + 1) * 2 >= n->slots_cap)
    {
        size_t cap = n->slots_cap ? n->slots_cap * 2 : 16;
        size_t *slots = YamlAllocateMemory(cap * sizeof(*slots));
        for(size_t i = 0; i < n->count; i++)
        {
            size_t s = YamlWriterHash(n->keys[i]) & (cap - 1);
            while(slots[s])
            {
                s = (s + 1) & (cap - 1);
            }
            slots[s] = i + 1;
        }
        n->slots = slots;
        n->slots_cap = cap;
    }
    size_t s = YamlWriterHash(key) & (n->slots_cap - 1);
    while(n->slots[s])
    {
        if(!strcmp(n->keys[n->slots[s] - 1], key))
        {
            YamlError("duplicate YAML key: %s", key);
        }
        s = (s + 1) & (n->slots_cap - 1);
    }
    YamlWriterReserve(n);
    n->keys[n->count] = YamlCopyString(key);
    n->values[n->count] = v;
    n->slots[s] = ++n->count;
}

static
inline
void
YamlAppendItem(YAML_NODE *n,
               YAML_NODE *v)
{
    if(!n || n->type != YAML_SEQUENCE)
    {
        YamlError("expected sequence");
    }
    YamlWriterReserve(n);
    n->values[n->count++] = v;
}

static
inline
YAML_NODE *
YamlFindField(YAML_NODE *n,
              const char *key)
{
    if(!n || n->type != YAML_MAP)
    {
        YamlError("expected mapping for %s", key);
    }
    if(!n->slots_cap)
    {
        return NULL;
    }
    size_t s = YamlWriterHash(key) & (n->slots_cap - 1);
    while(n->slots[s])
    {
        if(!strcmp(n->keys[n->slots[s] - 1], key))
        {
            return n->values[n->slots[s] - 1];
        }
        s = (s + 1) & (n->slots_cap - 1);
    }
    return NULL;
}

static
inline
YAML_NODE *
YamlGetField(YAML_NODE *n,
             const char *key)
{
    YAML_NODE *v = YamlFindField(n, key);
    if(!v)
    {
        YamlError("missing field: %s", key);
    }
    return v;
}

static
inline
int
YamlWriterInList(const char *list,
                 const char *key)
{
    size_t n = strlen(key);
    while(*list)
    {
        size_t len = strcspn(list, " ");
        if(n == len && !memcmp(list, key, n))
        {
            return 1;
        }
        list += len;
        while(*list == ' ')
        {
            list++;
        }
    }
    return 0;
}

static
inline
void
YamlValidateFields(YAML_NODE *n,
                   const char *required,
                   const char *allowed)
{
    if(!n || n->type != YAML_MAP)
    {
        YamlError("expected mapping");
    }
    for(size_t i = 0; i < n->count; i++)
    {
        if(!YamlWriterInList(required, n->keys[i]) && !YamlWriterInList(allowed, n->keys[i]))
        {
            YamlError("unexpected field: %s", n->keys[i]);
        }
    }
    char *r = YamlCopyString(required), *p = r;
    while(*p)
    {
        char *end = strchr(p, ' ');
        if(end)
        {
            *end = 0;
        }
        (void)YamlGetField(n, p);
        if(!end)
        {
            break;
        }
        p = end + 1;
    }
}

const
char *
YamlGetScalar(YAML_NODE *n)
{
    if(!n || n->type != YAML_SCALAR)
    {
        YamlError("expected scalar");
    }
    return n->text;
}

static
inline
uint32_t
YamlParseNumber(const char *s,
                unsigned base,
                uint32_t max)
{
    uint64_t YamlWriterValue = 0;
    if(!*s)
    {
        YamlError("empty number");
    }
    for(const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
        unsigned digit = *p >= '0' && *p <= '9'   ? *p - '0'
                         : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10
                         : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10
                                                  : 99;
        if(digit >= base || YamlWriterValue > (UINT32_MAX - digit) / base)
        {
            YamlError("invalid number: %s", s);
        }
        YamlWriterValue = YamlWriterValue * base + digit;
    }
    if(YamlWriterValue > max)
    {
        YamlError("number out of range: %s", s);
    }
    return (uint32_t)YamlWriterValue;
}

static
inline
uint32_t
YamlGetUnsigned(YAML_NODE *n,
                uint32_t max)
{
    return YamlParseNumber(YamlGetScalar(n), 10, max);
}

static
inline
uint32_t
YamlGetHexadecimal(YAML_NODE *n,
                   uint32_t max)
{
    const char *s = YamlGetScalar(n);
    if(strncmp(s, "0x", 2) || strlen(s) > 10)
    {
        YamlError("expected 0x... encoded value");
    }
    return YamlParseNumber(s + 2, 16, max);
}

static
inline
uint32_t
YamlGetCodepoint(YAML_NODE *n)
{
    const char *s = YamlGetScalar(n);
    if(strlen(s) != 6 || strncmp(s, "U+", 2))
    {
        YamlError("expected U+XXXX");
    }
    return YamlParseNumber(s + 2, 16, 65535);
}

static
inline
int
YamlWriterBase64Digit(unsigned char c)
{
    if(c >= 'A' && c <= 'Z')
    {
        return c - 'A';
    }
    if(c >= 'a' && c <= 'z')
    {
        return c - 'a' + 26;
    }
    if(c >= '0' && c <= '9')
    {
        return c - '0' + 52;
    }
    return c == '+' ? 62 : c == '/' ? 63 : c == '=' ? 64 : -1;
}

static
inline
YAML_NODE *
YamlReadBinary(const unsigned char *s,
               size_t n)
{
    YAML_BUFFER b = {0};
    unsigned q[4], used = 0;
    int ended = 0;
    for(size_t i = 0; i < n; i++)
    {
        if(isspace(s[i]))
        {
            continue;
        }
        int d = YamlWriterBase64Digit(s[i]);
        if(d < 0 || ended)
        {
            YamlError("invalid Base64");
        }
        q[used++] = (unsigned)d;
        if(used == 4)
        {
            if(q[0] > 63 || q[1] > 63 || (q[2] == 64 && q[3] != 64))
            {
                YamlError("invalid Base64 padding");
            }
            unsigned char bytes[3] = {(unsigned char)((q[0] << 2) | (q[1] >> 4)),
                                      (unsigned char)((q[1] << 4) | (q[2] >> 2)),
                                      (unsigned char)((q[2] << 6) | q[3])};
            size_t count = q[2] == 64 ? 1 : q[3] == 64 ? 2 : 3;
            if((count == 1 && (q[1] & 15)) || (count == 2 && (q[2] & 3)))
            {
                YamlError("noncanonical Base64 padding bits");
            }
            YamlAppendBuffer(&b, bytes, count);
            ended = count != 3;
            used = 0;
        }
    }
    if(used)
    {
        YamlError("incomplete Base64 group");
    }
    return YamlCreateBinary(b.data, b.len);
}

static
inline
void
YamlWriterIndent(FILE *f,
                 unsigned n)
{
    while(n--)
    {
        fputc(' ', f);
    }
}

static
inline
void
YamlWriterQuoted(FILE *f,
                 const char *s)
{
    int escaped = 0;
    for(const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
        if(*p < 0x20 || *p == 0x7f || *p >= 0x80)
        {
            escaped = 1;
        }
    }
    if(escaped)
    {
        fputc('"', f);
        for(const unsigned char *p = (const unsigned char *)s; *p; p++)
        {
            if(*p == '"' || *p == '\\')
            {
                fputc('\\', f);
                fputc(*p, f);
            }
            else if(*p < 0x20 || *p == 0x7f)
            {
                fprintf(f, "\\u%04X", *p);
            }
            else if(*p >= 0x80)
            {
                uint32_t c = *p;
                unsigned extra;
                if(c < 0xe0)
                {
                    c &= 31;
                    extra = 1;
                }
                else if(c < 0xf0)
                {
                    c &= 15;
                    extra = 2;
                }
                else
                {
                    c &= 7;
                    extra = 3;
                }
                while(extra--)
                {
                    p++;
                    c = (c << 6) | (*p & 63);
                }
                fprintf(f, c <= 65535 ? "\\u%04X" : "\\U%08X", c);
            }
            else
            {
                fputc(*p, f);
            }
        }
        fputc('"', f);
    }
    else
    {
        fputc('\'', f);
        for(; *s; s++)
        {
            fputc(*s, f);
            if(*s == '\'')
            {
                fputc('\'', f);
            }
        }
        fputc('\'', f);
    }
}

static
inline
int
YamlWriterPlain(const char *s)
{
    if(!*s)
    {
        return 0;
    }
    const char *reserved[] = {"null", "true", "false", "yes", "no", "on", "off", "y", "n"};
    for(size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
    {
        size_t j = 0;
        while(s[j] && reserved[i][j] && tolower((unsigned char)s[j]) == reserved[i][j])
        {
            j++;
        }
        if(!s[j] && !reserved[i][j])
        {
            return 0;
        }
    }
    for(const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
        if(!isalnum(*p) && *p != '_' && *p != '-' && *p != '+')
        {
            return 0;
        }
    }
    return strncmp(s, "0x", 2) != 0;
}

static
inline
void
YamlWriterOutputScalar(FILE *f,
                       YAML_NODE *n)
{
    if(n->IsInteger ||
       (YamlWriterPlain(n->text) && !isdigit((unsigned char)n->text[0]) && n->text[0] != '-' && n->text[0] != '+'))
    {
        fputs(n->text, f);
    }
    else
    {
        YamlWriterQuoted(f, n->text);
    }
}

static
inline
int
YamlWriterCodepoint(const char *s)
{
    if(!s || strlen(s) != 6 || strncmp(s, "U+", 2))
    {
        return -1;
    }
    for(unsigned i = 2; i < 6; i++)
    {
        if(!isxdigit((unsigned char)s[i]))
        {
            return -1;
        }
    }
    return (int)strtoul(s + 2, NULL, 16);
}

static
inline
void
YamlWriterComment(FILE *f,
                  const char *key,
                  YAML_NODE *v)
{
    int c = v->type == YAML_SCALAR ? YamlWriterCodepoint(v->text) : -1;
    if(c < 0)
    {
        c = YamlWriterCodepoint(key);
    }
    if(c >= 0 && YamlCharacterName)
    {
        fprintf(f, "  # %s", YamlCharacterName((uint32_t)c));
    }
}

static
inline
void
YamlWriterValue(FILE *f,
                YAML_NODE *v,
                unsigned depth,
                const char *key)
{
    if(v->type == YAML_SCALAR)
    {
        fputc(' ', f);
        YamlWriterOutputScalar(f, v);
        YamlWriterComment(f, key, v);
        fputc('\n', f);
    }
    else if(v->type == YAML_BINARY)
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        if(!v->length)
        {
            fputs(" !!binary ''\n", f);
            return;
        }
        fputs(" !!binary |\n", f);
        unsigned column = 0;
        for(size_t i = 0; i < v->length; i += 3)
        {
            uint32_t bits = (uint32_t)v->bytes[i] << 16;
            if(i + 1 < v->length)
            {
                bits |= (uint32_t)v->bytes[i + 1] << 8;
            }
            if(i + 2 < v->length)
            {
                bits |= v->bytes[i + 2];
            }
            char chars[4] = {alphabet[bits >> 18],
                             alphabet[(bits >> 12) & 63],
                             i + 1 < v->length ? alphabet[(bits >> 6) & 63] : '=',
                             i + 2 < v->length ? alphabet[bits & 63] : '='};
            if(!column)
            {
                YamlWriterIndent(f, depth);
            }
            fwrite(chars, 1, 4, f);
            column += 4;
            if(column == 76)
            {
                fputc('\n', f);
                column = 0;
            }
        }
        if(column)
        {
            fputc('\n', f);
        }
    }
    else if(!v->count)
    {
        fputs(v->type == YAML_MAP ? " {}\n" : " []\n", f);
    }
    else
    {
        fputc('\n', f);
        YamlWriterEmit(f, v, depth);
    }
}

static
inline
void
YamlWriterEmit(FILE *f,
               YAML_NODE *n,
               unsigned depth)
{
    for(size_t i = 0; i < n->count; i++)
    {
        YamlWriterIndent(f, depth);
        if(n->type == YAML_MAP)
        {
            if(YamlWriterPlain(n->keys[i]))
            {
                fputs(n->keys[i], f);
            }
            else
            {
                YamlWriterQuoted(f, n->keys[i]);
            }
            fputc(':', f);
        }
        else
        {
            fputc('-', f);
        }
        YamlWriterValue(f, n->values[i], depth + 2, n->type == YAML_MAP ? n->keys[i] : NULL);
    }
}

static
inline
void
YamlWriteDocument(FILE *f,
                  YAML_NODE *root,
                  const char *Generator)
{
    if(Generator)
    {
        fprintf(f, "# Generated by %s. Do not edit manually.\n", Generator);
    }
    YamlWriterEmit(f, root, 0);
    if(ferror(f))
    {
        YamlError("cannot write YAML output");
    }
}

/* Bounded YAML parser: block and flow collections, quoted scalars and binary values */

/* Bounded YAML subset with explicit scalar handling and no implicit type resolver */

static
inline
void
YamlParserError(size_t line,
                const char *why)
{
    YamlError("YAML line %zu: %s", line, why);
}

static
inline
int
YamlParserSpace(char c)
{
    return c == ' ' || c == '\t';
}

static
inline
void
YamlParserWs(YAML_INLINE *p)
{
    while(p->at < p->n && YamlParserSpace(p->s[p->at]))
    {
        p->at++;
    }
}

static
inline
void
YamlParserDepthCheck(unsigned depth)
{
    if(depth > 32)
    {
        YamlError("YAML nesting exceeds 32 levels");
    }
}

static
inline
char *
YamlParserSlice(const char *s,
                size_t n)
{
    char *v = YamlAllocateMemory(n + 1);
    if(n)
    {
        memcpy(v, s, n);
    }
    return v;
}

static
inline
void
YamlParserUtf8(YAML_BUFFER *b,
               uint32_t c,
               size_t line)
{
    unsigned char v[4];
    size_t n;
    if(!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
    {
        YamlParserError(line, "invalid Unicode escape");
    }
    if(c < 0x80)
    {
        v[0] = (unsigned char)c;
        n = 1;
    }
    else if(c < 0x800)
    {
        v[0] = 0xc0 | (c >> 6);
        v[1] = 0x80 | (c & 63);
        n = 2;
    }
    else if(c < 0x10000)
    {
        v[0] = 0xe0 | (c >> 12);
        v[1] = 0x80 | ((c >> 6) & 63);
        v[2] = 0x80 | (c & 63);
        n = 3;
    }
    else
    {
        v[0] = 0xf0 | (c >> 18);
        v[1] = 0x80 | ((c >> 12) & 63);
        v[2] = 0x80 | ((c >> 6) & 63);
        v[3] = 0x80 | (c & 63);
        n = 4;
    }
    YamlAppendBuffer(b, v, n);
}

static
inline
char *
YamlParserQuoted(YAML_INLINE *p)
{
    char quote = p->s[p->at++];
    YAML_BUFFER b = {0};
    while(p->at < p->n)
    {
        unsigned char c = (unsigned char)p->s[p->at++];
        if(c == (unsigned char)quote)
        {
            if(quote == '\'' && p->at < p->n && p->s[p->at] == '\'')
            {
                p->at++;
                YamlAppendBuffer(&b, &c, 1);
                continue;
            }
            return YamlParserSlice((const char *)b.data, b.len);
        }
        if(quote == '"' && c == '\\')
        {
            if(p->at == p->n)
            {
                YamlParserError(p->line, "unfinished quoted escape");
            }
            c = (unsigned char)p->s[p->at++];
            uint32_t cp = c;
            switch(c)
            {
            case 'a':
                cp = 7;
                break;
            case 'b':
                cp = 8;
                break;
            case 't':
                cp = 9;
                break;
            case 'n':
                cp = 10;
                break;
            case 'v':
                cp = 11;
                break;
            case 'f':
                cp = 12;
                break;
            case 'r':
                cp = 13;
                break;
            case 'e':
                cp = 27;
                break;
            case 'N':
                cp = 0x85;
                break;
            case '_':
                cp = 0xa0;
                break;
            case 'L':
                cp = 0x2028;
                break;
            case 'P':
                cp = 0x2029;
                break;
            case ' ':
            case '"':
            case '/':
            case '\\':
                break;
            case 'x':
            case 'u':
            case 'U':
            {
                unsigned digits = c == 'x' ? 2 : c == 'u' ? 4 : 8;
                cp = 0;
                if(p->n - p->at < digits)
                {
                    YamlParserError(p->line, "short Unicode escape");
                }
                while(digits--)
                {
                    unsigned char h = (unsigned char)p->s[p->at++];
                    unsigned d = h >= '0' && h <= '9'   ? h - '0'
                                 : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                 : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                        : 99;
                    if(d > 15)
                    {
                        YamlParserError(p->line, "invalid Unicode escape");
                    }
                    cp = cp * 16 + d;
                }
                break;
            }
            default:
                YamlParserError(p->line, "unsupported quoted escape");
            }
            YamlParserUtf8(&b, cp, p->line);
        }
        else
        {
            YamlAppendBuffer(&b, &c, 1);
        }
    }
    YamlParserError(p->line, "unterminated quote; multiline strings are not supported");
    return NULL;
}

static
inline
char *
YamlParserTextValue(YAML_INLINE *p,
                    int key,
                    int flow)
{
    YamlParserWs(p);
    if(p->at == p->n)
    {
        YamlParserError(p->line, "missing scalar");
    }
    char c = p->s[p->at];
    if(c == '\'' || c == '"')
    {
        return YamlParserQuoted(p);
    }
    if(strchr("!&*|>%@`{}[],?#", c) ||
       ((c == '-' || c == ':') && (p->at + 1 == p->n || YamlParserSpace(p->s[p->at + 1]))))
    {
        YamlParserError(p->line, "unsupported scalar, tag, anchor or alias");
    }
    size_t start = p->at;
    while(p->at < p->n)
    {
        c = p->s[p->at];
        if(c == '#' && (p->at == start || YamlParserSpace(p->s[p->at - 1])))
        {
            break;
        }
        if(flow && strchr(",[]{}", c))
        {
            break;
        }
        if(c == ':' &&
           (key || p->at + 1 == p->n || YamlParserSpace(p->s[p->at + 1]) || (flow && strchr(",[]{}", p->s[p->at + 1]))))
        {
            break;
        }
        p->at++;
    }
    size_t end = p->at;
    while(end > start && YamlParserSpace(p->s[end - 1]))
    {
        end--;
    }
    if(end == start)
    {
        YamlParserError(p->line, "missing scalar");
    }
    return YamlParserSlice(p->s + start, end - start);
}

static
inline
YAML_NODE *
YamlParserInlineValue(YAML_INLINE *p,
                      unsigned depth,
                      int flow)
{
    YamlParserDepthCheck(depth);
    YamlParserWs(p);
    if(p->at == p->n)
    {
        YamlParserError(p->line, "missing value");
    }
    char open = p->s[p->at];
    if(open == '[' || open == '{')
    {
        YAML_NODE *n = YamlCreateNode(open == '[' ? YAML_SEQUENCE : YAML_MAP);
        char close = open == '[' ? ']' : '}';
        p->at++;
        YamlParserWs(p);
        if(p->at < p->n && p->s[p->at] == close)
        {
            p->at++;
            return n;
        }
        for(;;)
        {
            if(n->type == YAML_MAP)
            {
                YamlParserWs(p);
                int quoted_key = p->at < p->n && (p->s[p->at] == '\'' || p->s[p->at] == '"');
                char *key = YamlParserTextValue(p, 1, 1);
                YamlParserWs(p);
                if(p->at == p->n || p->s[p->at++] != ':')
                {
                    YamlParserError(p->line, "expected ':' in flow mapping");
                }
                if(!quoted_key && p->at < p->n && !YamlParserSpace(p->s[p->at]) && !strchr("[{", p->s[p->at]))
                {
                    YamlParserError(p->line, "plain flow mapping keys require separation after ':'");
                }
                if(!strcmp(key, "<<"))
                {
                    YamlParserError(p->line, "YAML merge keys are not supported");
                }
                YamlAddField(n, key, YamlParserInlineValue(p, depth + 1, 1));
            }
            else
            {
                YamlAppendItem(n, YamlParserInlineValue(p, depth + 1, 1));
            }
            YamlParserWs(p);
            if(p->at == p->n)
            {
                YamlParserError(p->line, "unterminated flow collection; multiline flow is not supported");
            }
            char c = p->s[p->at++];
            if(c == close)
            {
                return n;
            }
            if(c != ',')
            {
                YamlParserError(p->line, "expected comma or closing delimiter");
            }
            YamlParserWs(p);
            if(p->at < p->n && p->s[p->at] == close)
            {
                p->at++;
                return n;
            }
        }
    }
    if(p->n - p->at >= 8 && !memcmp(p->s + p->at, "!!binary", 8) &&
       (p->at + 8 == p->n || YamlParserSpace(p->s[p->at + 8])))
    {
        p->at += 8;
        char *s = YamlParserTextValue(p, 0, flow);
        return YamlReadBinary((const unsigned char *)s, strlen(s));
    }
    return YamlCreateString(YamlParserTextValue(p, 0, flow));
}

static
inline
void
YamlParserFinish(YAML_INLINE *p)
{
    size_t before = p->at;
    YamlParserWs(p);
    if(p->at == p->n)
    {
        return;
    }
    if(p->s[p->at] == '#' && p->at > before)
    {
        return;
    }
    /* Plain scalars consume whitespace before stopping at a comment. */
    if(p->s[p->at] == '#' && p->at && YamlParserSpace(p->s[p->at - 1]))
    {
        return;
    }
    YamlParserError(p->line, "unexpected trailing content");
}

static
inline
void
YamlParserSkip(YAML_PARSER *p)
{
    while(p->at < p->count)
    {
        YAML_LINE *l = &p->lines[p->at];
        if(l->indent != l->n && l->s[l->indent] != '#')
        {
            break;
        }
        p->at++;
    }
    if(p->at < p->count)
    {
        p->col = p->lines[p->at].indent;
    }
}

static
inline
void
YamlParserNext(YAML_PARSER *p)
{
    p->at++;
    YamlParserSkip(p);
}

static
inline
int
YamlParserDash(const YAML_LINE *l,
               size_t col)
{
    return col < l->n && l->s[col] == '-' && (col + 1 == l->n || YamlParserSpace(l->s[col + 1]));
}

static
inline
int
YamlParserEmpty(const YAML_INLINE *p)
{
    return p->at == p->n || p->s[p->at] == '#';
}

static
inline
YAML_NODE *
YamlParserBlockValue(YAML_PARSER *p,
                     YAML_INLINE *v,
                     size_t parent,
                     unsigned depth)
{
    YamlParserDepthCheck(depth);
    YamlParserWs(v);
    if(YamlParserEmpty(v))
    {
        YamlParserNext(p);
        if(p->at == p->count || p->col < parent || (p->col == parent && !YamlParserDash(&p->lines[p->at], p->col)))
        {
            YamlParserError(v->line, "missing value; implicit null is not supported");
        }
        return YamlParserBlock(p, p->col, depth);
    }
    if(v->n - v->at >= 8 && !memcmp(v->s + v->at, "!!binary", 8) &&
       (v->at + 8 == v->n || YamlParserSpace(v->s[v->at + 8])))
    {
        YAML_INLINE tail = *v;
        tail.at += 8;
        YamlParserWs(&tail);
        if(tail.at < tail.n && tail.s[tail.at] == '|')
        {
            tail.at++;
            if(tail.at < tail.n && (tail.s[tail.at] == '-' || tail.s[tail.at] == '+'))
            {
                tail.at++;
            }
            YamlParserFinish(&tail);
            YAML_BUFFER b = {0};
            size_t content_indent = 0;
            p->at++;
            while(p->at < p->count)
            {
                YAML_LINE *l = &p->lines[p->at];
                if(l->indent == l->n)
                {
                    p->at++;
                    continue;
                }
                if(l->indent <= parent)
                {
                    break;
                }
                if(!content_indent)
                {
                    content_indent = l->indent;
                }
                if(l->indent != content_indent)
                {
                    YamlParserError(l->number, "inconsistent binary block indentation");
                }
                YamlAppendBuffer(&b, l->s + l->indent, l->n - l->indent);
                p->at++;
            }
            YamlParserSkip(p);
            return YamlReadBinary(b.data, b.len);
        }
    }
    YAML_NODE *n = YamlParserInlineValue(v, depth, 0);
    YamlParserFinish(v);
    YamlParserNext(p);
    return n;
}

static
inline
YAML_NODE *
YamlParserBlock(YAML_PARSER *p,
                size_t indent,
                unsigned depth)
{
    YamlParserDepthCheck(depth);
    YAML_LINE *l = &p->lines[p->at];
    int seq = YamlParserDash(l, p->col);
    YAML_NODE *n = YamlCreateNode(seq ? YAML_SEQUENCE : YAML_MAP);
    while(p->at < p->count && p->col == indent)
    {
        l = &p->lines[p->at];
        if(!indent && ((!strcmp(l->s, "---")) || (!strcmp(l->s, "..."))))
        {
            break;
        }
        if(YamlParserDash(l, p->col) != seq)
        {
            break;
        }
        YAML_INLINE v = {l->s, l->n, p->col, l->number};
        if(seq)
        {
            v.at++;
            YamlParserWs(&v);
            /* Compact sequence/map entries, as emitted by PyYAML. */
            int compact = YamlParserDash(l, v.at);
            if(!compact && !YamlParserEmpty(&v) && !strchr("[{!&*", v.s[v.at]))
            {
                YAML_INLINE probe = v;
                (void)YamlParserTextValue(&probe, 1, 0);
                YamlParserWs(&probe);
                if(probe.at < probe.n && probe.s[probe.at] == ':' &&
                   (probe.at + 1 == probe.n || YamlParserSpace(probe.s[probe.at + 1])))
                {
                    compact = 1;
                }
            }
            if(compact)
            {
                p->col = v.at;
                YamlAppendItem(n, YamlParserBlock(p, v.at, depth + 1));
            }
            else
            {
                if(YamlParserEmpty(&v))
                {
                    YamlParserNext(p);
                    if(p->at == p->count || p->col <= indent)
                    {
                        YamlParserError(v.line, "missing sequence item");
                    }
                    YamlAppendItem(n, YamlParserBlock(p, p->col, depth + 1));
                }
                else
                {
                    YamlAppendItem(n, YamlParserBlockValue(p, &v, indent, depth + 1));
                }
            }
        }
        else
        {
            char *key = YamlParserTextValue(&v, 1, 0);
            YamlParserWs(&v);
            if(v.at == v.n || v.s[v.at++] != ':' || (v.at < v.n && !YamlParserSpace(v.s[v.at])))
            {
                YamlParserError(v.line, "expected block mapping 'key: value'");
            }
            if(!strcmp(key, "<<"))
            {
                YamlParserError(v.line, "YAML merge keys are not supported");
            }
            YamlAddField(n, key, YamlParserBlockValue(p, &v, indent, depth + 1));
        }
    }
    if(p->at < p->count && p->col > indent)
    {
        YamlParserError(p->lines[p->at].number, "unexpected indentation or multiline scalar");
    }
    return n;
}

static
inline
void
YamlParserValidateUtf8(const unsigned char *s,
                       size_t len)
{
    for(size_t i = 0; i < len;)
    {
        uint32_t c = s[i++], minimum = 0;
        unsigned extra = 0;
        if(c >= 0xc2 && c <= 0xdf)
        {
            c &= 31;
            extra = 1;
            minimum = 0x80;
        }
        else if(c >= 0xe0 && c <= 0xef)
        {
            c &= 15;
            extra = 2;
            minimum = 0x800;
        }
        else if(c >= 0xf0 && c <= 0xf4)
        {
            c &= 7;
            extra = 3;
            minimum = 0x10000;
        }
        else if(c >= 0x80)
        {
            YamlError("invalid UTF-8 in YAML input");
        }
        if(extra > len - i)
        {
            YamlError("truncated UTF-8 in YAML input");
        }
        while(extra--)
        {
            unsigned b = s[i++];
            if((b & 0xc0) != 0x80)
            {
                YamlError("invalid UTF-8 in YAML input");
            }
            c = (c << 6) | (b & 63);
        }
        if(c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        {
            YamlError("invalid UTF-8 codepoint");
        }
        if((c < 32 && c != 9 && c != 10 && c != 13) || (c >= 0x7f && c <= 0x9f && c != 0x85) || c == 0xfffe ||
           c == 0xffff)
        {
            YamlError("non-printable character in YAML input");
        }
        if(c == 0x85 || c == 0x2028 || c == 0x2029 || c == 0xfeff)
        {
            YamlError("unsupported YAML line break or embedded BOM");
        }
    }
}

static
inline
YAML_NODE *
YamlReadDocument(const unsigned char *data,
                 size_t len)
{
    if(len > YAML_MAX_TEXT)
    {
        YamlError("YAML input exceeds 512 MiB");
    }
    YamlParserValidateUtf8(data, len);
    size_t count = 1;
    for(size_t i = 0; i < len; i++)
    {
        if(data[i] == '\n')
        {
            count++;
        }
    }
    if(count > 2097152)
    {
        YamlError("too many YAML lines");
    }
    YAML_PARSER p = {YamlAllocateMemory(count * sizeof(YAML_LINE)), 0, 0, 0};
    char *storage = YamlParserSlice((const char *)data, len);
    size_t start = 0;
    for(size_t i = 0; i <= len; i++)
    {
        if(i < len && storage[i] != '\n')
        {
            continue;
        }
        size_t end = i;
        if(end > start && storage[end - 1] == '\r')
        {
            end--;
        }
        while(end > start && YamlParserSpace(storage[end - 1]))
        {
            end--;
        }
        storage[end] = 0;
        YAML_LINE *l = &p.lines[p.count++];
        l->s = storage + start;
        l->n = end - start;
        l->number = p.count;
        while(l->indent < l->n && l->s[l->indent] == ' ')
        {
            l->indent++;
        }
        if(l->indent < l->n && l->s[l->indent] == '\t')
        {
            YamlParserError(l->number, "tabs in indentation are not supported");
        }
        if(memchr(l->s, '\r', l->n))
        {
            YamlParserError(l->number, "use LF or CRLF line endings");
        }
        start = i + 1;
    }
    YamlParserSkip(&p);
    if(p.at < p.count && !p.col && !strcmp(p.lines[p.at].s, "---"))
    {
        YamlParserNext(&p);
    }
    if(p.at == p.count)
    {
        YamlError("empty YAML document");
    }
    if(p.col)
    {
        YamlParserError(p.lines[p.at].number, "root must start at column one");
    }
    YAML_NODE *root;
    if(p.lines[p.at].s[0] == '{' || p.lines[p.at].s[0] == '[')
    {
        YAML_INLINE v = {p.lines[p.at].s, p.lines[p.at].n, 0, p.lines[p.at].number};
        root = YamlParserInlineValue(&v, 0, 0);
        YamlParserFinish(&v);
        YamlParserNext(&p);
    }
    else
    {
        root = YamlParserBlock(&p, 0, 0);
    }
    if(p.at < p.count && !p.col && !strcmp(p.lines[p.at].s, "..."))
    {
        YamlParserNext(&p);
    }
    if(p.at != p.count)
    {
        YamlParserError(p.lines[p.at].number, "unexpected content or multiple YAML documents");
    }
    return root;
}
