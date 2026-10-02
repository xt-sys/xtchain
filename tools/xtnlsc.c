/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/xtnlsc.c
 * DESCRIPTION: Lossless NLS and YAML converter
 * DEVELOPERS:  Aiken Harris <harraiken91@gmail.com>
 */

#include "xtchain.h"
#include "unicode.h"


#define XTNLSC_VERSION "1.0.0"

typedef struct
{
    const unsigned char *p;
    size_t n, pos;
    YAML_BUFFER out;
    int reading;
} NlsIO;

typedef struct
{
    const char *name;
    unsigned width;
    char ref;
} LocaleField;

static const LocaleField locale_fields[] = {
    {"sname", 4, 'T'},
    {"sopentypelanguagetag", 4, 'T'},
    {"ilanguage", 2, 0},
    {"unique_lcid", 2, 0},
    {"idigits", 2, 0},
    {"inegnumber", 2, 0},
    {"icurrdigits", 2, 0},
    {"icurrency", 2, 0},
    {"inegcurr", 2, 0},
    {"ilzero", 2, 0},
    {"inotneutral", 2, 0},
    {"ifirstdayofweek", 2, 0},
    {"ifirstweekofyear", 2, 0},
    {"icountry", 2, 0},
    {"imeasure", 2, 0},
    {"idigitsubstitution", 2, 0},
    {"sgrouping", 4, 'T'},
    {"smongrouping", 4, 'T'},
    {"slist", 4, 'T'},
    {"sdecimal", 4, 'T'},
    {"sthousand", 4, 'T'},
    {"scurrency", 4, 'T'},
    {"smondecimalsep", 4, 'T'},
    {"smonthousandsep", 4, 'T'},
    {"spositivesign", 4, 'T'},
    {"snegativesign", 4, 'T'},
    {"s1159", 4, 'T'},
    {"s2359", 4, 'T'},
    {"snativedigits", 4, 'A'},
    {"stimeformat", 4, 'A'},
    {"sshortdate", 4, 'A'},
    {"slongdate", 4, 'A'},
    {"syearmonth", 4, 'A'},
    {"sduration", 4, 'A'},
    {"idefaultlanguage", 2, 0},
    {"idefaultansicodepage", 2, 0},
    {"idefaultcodepage", 2, 0},
    {"idefaultmaccodepage", 2, 0},
    {"idefaultebcdiccodepage", 2, 0},
    {"old_geoid", 2, 0},
    {"ipapersize", 2, 0},
    {"islamic_cal_0", 1, 0},
    {"islamic_cal_1", 1, 0},
    {"scalendartype", 4, 'T'},
    {"sabbrevlangname", 4, 'T'},
    {"siso639langname", 4, 'T'},
    {"senglanguage", 4, 'T'},
    {"snativelangname", 4, 'T'},
    {"sengcountry", 4, 'T'},
    {"snativectryname", 4, 'T'},
    {"sabbrevctryname", 4, 'T'},
    {"siso3166ctryname", 4, 'T'},
    {"sintlsymbol", 4, 'T'},
    {"sengcurrname", 4, 'T'},
    {"snativecurrname", 4, 'T'},
    {"fontsignature", 4, 'F'},
    {"siso639langname2", 4, 'T'},
    {"siso3166ctryname2", 4, 'T'},
    {"sparent", 4, 'T'},
    {"sdayname", 4, 'A'},
    {"sabbrevdayname", 4, 'A'},
    {"smonthname", 4, 'A'},
    {"sabbrevmonthname", 4, 'A'},
    {"sgenitivemonth", 4, 'A'},
    {"sabbrevgenitivemonth", 4, 'A'},
    {"calnames", 4, 'A'},
    {"customsorts", 4, 'A'},
    {"inegativepercent", 2, 0},
    {"ipositivepercent", 2, 0},
    {"unknown1", 2, 0},
    {"ireadinglayout", 2, 0},
    {"unknown2_0", 2, 0},
    {"unknown2_1", 2, 0},
    {"unused1", 4, 0},
    {"sengdisplayname", 4, 'T'},
    {"snativedisplayname", 4, 'T'},
    {"spercent", 4, 'T'},
    {"snan", 4, 'T'},
    {"sposinfinity", 4, 'T'},
    {"sneginfinity", 4, 'T'},
    {"unused2", 4, 0},
    {"serastring", 4, 'T'},
    {"sabbreverastring", 4, 'T'},
    {"unused3", 4, 0},
    {"sconsolefallbackname", 4, 'T'},
    {"sshorttime", 4, 'A'},
    {"sshortestdayname", 4, 'A'},
    {"unused4", 4, 0},
    {"ssortlocale", 4, 'T'},
    {"skeyboardstoinstall", 4, 'T'},
    {"sscripts", 4, 'T'},
    {"srelativelongdate", 4, 'T'},
    {"igeoid", 4, 0},
    {"sshortestam", 4, 'T'},
    {"sshortestpm", 4, 'T'},
    {"smonthday", 4, 'A'},
    {"keyboard_layout", 4, 'T'},
};

typedef struct
{
    NlsIO *x;
    YAML_NODE *needed;
    size_t words;
    const unsigned char *source;
    unsigned char *data, *used;
} Pool;

typedef struct
{
    size_t glyph, range_pos, dbcs_base, flag, wide, end, blocks[256];
    unsigned leads[256], block_count;
    uint32_t size, marker, ranges;
} Codepage;

static char *temporary;
static FILE *output_stream;

typedef void (*Codec)(NlsIO *, YAML_NODE *);

typedef enum
{
    NLS_CHARACTER_TYPES,
    NLS_GEOGRAPHY,
    NLS_CASE_EXCEPTIONS,
    NLS_UNICODE_MAPPINGS,
    NLS_SORT_TABLES_LEGACY,
    NLS_LOCALE_DATABASE,
    NLS_CODEPAGE,
    NLS_CASE_MAP,
    NLS_SORTKEY_LEGACY,
    NLS_FORMAT_COUNT
} NLS_FORMAT;

static YAML_NODE *NlsDecode(const unsigned char *data, size_t len, const char *type);
static YAML_BUFFER NlsEncode(YAML_NODE *root);
static YAML_BUFFER NlsEncodeV1(const unsigned char *data, size_t len);
static void NlsBounds(NlsIO *x, size_t count);
static uint32_t NlsRaw(NlsIO *x, unsigned width, uint32_t value);
static YAML_NODE *NlsField(NlsIO *x, YAML_NODE *parent, const char *key, YAML_NODE *value);
static YAML_NODE *NlsGroup(NlsIO *x, YAML_NODE *parent, const char *key, YAML_NODE_TYPE type);
static YAML_NODE *NlsItem(NlsIO *x, YAML_NODE *seq, size_t i);
static uint32_t NlsNum(NlsIO *x, YAML_NODE *parent, const char *key, unsigned width, int hex);
static void NlsFixed(NlsIO *x, YAML_NODE *parent, const char *key, unsigned units);
static char *NlsUtf8(const unsigned char *p, size_t units);
static YAML_BUFFER NlsUtf16(const char *s);
static void NlsCheckConsumed(YAML_NODE *n);
static void NlsTable(NlsIO *x, YAML_NODE *parent, const char *key, unsigned words, int mode);
static void NlsCtype(NlsIO *x, YAML_NODE *root);
static void NlsGeo(NlsIO *x, YAML_NODE *root);
static void NlsExceptions(NlsIO *x, YAML_NODE *root);
static void NlsUnicode(NlsIO *x, YAML_NODE *root, int embedded);
static void NlsSort(NlsIO *x, YAML_NODE *root);
static void NlsLocale(NlsIO *x, YAML_NODE *root);
static YAML_NODE *NlsDecodeFormat(const unsigned char *p, size_t n, size_t format);
static int NlsProbeFormat(const unsigned char *p, size_t n, size_t format);
static void NlsExtraUnicodeFile(NlsIO *x, YAML_NODE *n);
static int NlsEncodeExtra(const char *kind, YAML_NODE *body, YAML_BUFFER *out);

static const struct
{
    const char *kind;
    Codec codec;
} codecs[NLS_FORMAT_COUNT] = {
    {"character-types", NlsCtype},
    {"geography", NlsGeo},
    {"case-exceptions", NlsExceptions},
    {"unicode-mappings", NlsExtraUnicodeFile},
    {"sort-tables-legacy", NlsSort},
    {"locale-database", NlsLocale},
    {"codepage", NULL},
    {"case-map", NULL},
    {"sortkey-legacy", NULL}
};

static jmp_buf NlsProbeJump;

static
const
char *
NlsGetCharacterName(uint32_t Codepoint)
{
    size_t Low = 0;
    size_t High = sizeof(unicode_names) / sizeof(unicode_names[0]);
    while(Low < High)
    {
        size_t Middle = Low + (High - Low) / 2;
        if(unicode_names[Middle].code < Codepoint)
        {
            Low = Middle + 1;
        }
        else
        {
            High = Middle;
        }
    }
    return Low < sizeof(unicode_names) / sizeof(unicode_names[0]) && unicode_names[Low].code == Codepoint
               ? unicode_names[Low].name
               : "CONTROL / UNASSIGNED";
}

static
void
NlsBounds(NlsIO *x,
          size_t count)
{
    if(x->pos > x->n || count > x->n - x->pos)
    {
        YamlError("truncated NLS structure at byte %zu", x->pos);
    }
}

static
uint32_t
NlsRaw(NlsIO *x,
       unsigned width,
       uint32_t v)
{
    if(width != 1 && width != 2 && width != 4)
    {
        YamlError("internal NLS field width");
    }
    if(x->reading)
    {
        NlsBounds(x, width);
        v = width == 4 ? ReadUInt32(x->p + x->pos) : width == 2 ? ReadUInt16(x->p + x->pos) : x->p[x->pos];
    }
    else
    {
        if(width == 4)
        {
            WriteUInt32(&x->out, v);
        }
        else if(width == 2)
        {
            WriteUInt16(&x->out, v);
        }
        else
        {
            if(v > 255)
            {
                YamlError("byte overflow");
            }
            unsigned char b = (unsigned char)v;
            YamlAppendBuffer(&x->out, &b, 1);
        }
    }
    x->pos += width;
    return v;
}

static
YAML_NODE *
NlsField(NlsIO *x,
         YAML_NODE *parent,
         const char *key,
         YAML_NODE *v)
{
    if(x->reading)
    {
        YamlAddField(parent, key, v);
        return v;
    }
    parent->length++;
    return YamlGetField(parent, key);
}

static
YAML_NODE *
NlsGroup(NlsIO *x,
         YAML_NODE *parent,
         const char *key,
         YAML_NODE_TYPE type)
{
    YAML_NODE *n = NlsField(x, parent, key, x->reading ? YamlCreateNode(type) : NULL);
    if(n->type != type)
    {
        YamlError("invalid structure: %s", key);
    }
    return n;
}

static
YAML_NODE *
NlsItem(NlsIO *x,
        YAML_NODE *seq,
        size_t i)
{
    if(x->reading)
    {
        YAML_NODE *n = YamlCreateNode(YAML_MAP);
        YamlAppendItem(seq, n);
        return n;
    }
    if(i >= seq->count || seq->values[i]->type != YAML_MAP)
    {
        YamlError("invalid record count or type");
    }
    return seq->values[i];
}

static
uint32_t
NlsNum(NlsIO *x,
       YAML_NODE *parent,
       const char *key,
       unsigned width,
       int hex)
{
    uint32_t v;
    if(x->reading)
    {
        v = NlsRaw(x, width, 0);
        NlsField(x, parent, key, hex ? YamlCreateHexadecimal(v, width * 2) : YamlCreateInteger(v));
    }
    else
    {
        YAML_NODE *n = NlsField(x, parent, key, NULL);
        uint32_t max = width == 4 ? UINT32_MAX : width == 2 ? 65535 : 255;
        v = hex ? YamlGetHexadecimal(n, max) : YamlGetUnsigned(n, max);
        NlsRaw(x, width, v);
    }
    return v;
}

static
char *
NlsUtf8(const unsigned char *p,
        size_t units)
{
    char *s = YamlAllocateMemory(units * 4 + 1);
    size_t j = 0;
    for(size_t i = 0; i < units; i++)
    {
        uint32_t c = ReadUInt16(p + i * 2);
        if(!c)
        {
            YamlError("embedded NUL in textual NLS field");
        }
        if(c >= 0xd800 && c <= 0xdbff)
        {
            if(++i >= units)
            {
                YamlError("unpaired UTF-16 surrogate");
            }
            uint32_t lo = ReadUInt16(p + i * 2);
            if(lo < 0xdc00 || lo > 0xdfff)
            {
                YamlError("unpaired UTF-16 surrogate");
            }
            c = 0x10000 + ((c - 0xd800) << 10) + lo - 0xdc00;
        }
        else if(c >= 0xdc00 && c <= 0xdfff)
        {
            YamlError("unpaired UTF-16 surrogate");
        }
        if(c < 0x80)
        {
            s[j++] = (char)c;
        }
        else if(c < 0x800)
        {
            s[j++] = (char)(0xc0 | (c >> 6));
            s[j++] = (char)(0x80 | (c & 63));
        }
        else if(c < 0x10000)
        {
            s[j++] = (char)(0xe0 | (c >> 12));
            s[j++] = (char)(0x80 | ((c >> 6) & 63));
            s[j++] = (char)(0x80 | (c & 63));
        }
        else
        {
            s[j++] = (char)(0xf0 | (c >> 18));
            s[j++] = (char)(0x80 | ((c >> 12) & 63));
            s[j++] = (char)(0x80 | ((c >> 6) & 63));
            s[j++] = (char)(0x80 | (c & 63));
        }
    }
    return s;
}

static
YAML_BUFFER
NlsUtf16(const char *s)
{
    YAML_BUFFER b = {0};
    const unsigned char *p = (const unsigned char *)s;
    while(*p)
    {
        uint32_t c = *p++;
        unsigned more = 0;
        uint32_t min = 0;
        if(c >= 0xf0 && c <= 0xf4)
        {
            c &= 7;
            more = 3;
            min = 0x10000;
        }
        else if(c >= 0xe0 && c <= 0xef)
        {
            c &= 15;
            more = 2;
            min = 0x800;
        }
        else if(c >= 0xc2 && c <= 0xdf)
        {
            c &= 31;
            more = 1;
            min = 0x80;
        }
        else if(c >= 0x80)
        {
            YamlError("invalid UTF-8 string");
        }
        for(unsigned i = 0; i < more; i++)
        {
            if((*p & 0xc0) != 0x80)
            {
                YamlError("invalid UTF-8 string");
            }
            c = (c << 6) | (*p++ & 63);
        }
        if(c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        {
            YamlError("invalid Unicode scalar");
        }
        if(c >= 0x10000)
        {
            c -= 0x10000;
            WriteUInt16(&b, 0xd800 + (c >> 10));
            WriteUInt16(&b, 0xdc00 + (c & 1023));
        }
        else
        {
            WriteUInt16(&b, c);
        }
    }
    return b;
}

static
void
NlsFixed(NlsIO *x,
         YAML_NODE *parent,
         const char *key,
         unsigned units)
{
    if(x->reading)
    {
        NlsBounds(x, 2 * units);
        size_t n = 0;
        while(n < units && ReadUInt16(x->p + x->pos + 2 * n))
        {
            n++;
        }
        if(n == units)
        {
            YamlError("unterminated fixed string: %s", key);
        }
        NlsField(x, parent, key, YamlCreateString(NlsUtf8(x->p + x->pos, n)));
        /* Nonzero padding is represented separately, not silently discarded. */
        YAML_NODE *pad = YamlCreateNode(YAML_MAP);
        for(size_t i = n + 1; i < units; i++)
        {
            if(ReadUInt16(x->p + x->pos + 2 * i))
            {
                YamlAddField(
                    pad, YamlFormatString("%zu", i), YamlCreateHexadecimal(ReadUInt16(x->p + x->pos + 2 * i), 4));
            }
        }
        if(pad->count)
        {
            NlsField(x, parent, YamlFormatString("%s_padding", key), pad);
        }
        x->pos += 2 * units;
    }
    else
    {
        YAML_BUFFER b = NlsUtf16(YamlGetScalar(NlsField(x, parent, key, NULL)));
        if(b.len / 2 >= units)
        {
            YamlError("fixed string too long: %s", key);
        }
        unsigned char *p = YamlAllocateMemory(2 * units);
        memcpy(p, b.data, b.len);
        YAML_NODE *pad = YamlFindField(parent, YamlFormatString("%s_padding", key));
        if(pad)
        {
            parent->length++;
            if(pad->type != YAML_MAP)
            {
                YamlError("invalid string padding");
            }
            for(size_t i = 0; i < pad->count; i++)
            {
                uint32_t at = YamlParseNumber(pad->keys[i], 10, units - 1),
                         v = YamlGetHexadecimal(pad->values[i], 65535);
                if(at <= b.len / 2)
                {
                    YamlError("string padding overlaps text");
                }
                p[2 * at] = (unsigned char)v;
                p[2 * at + 1] = (unsigned char)(v >> 8);
            }
            pad->length = pad->count;
        }
        YamlAppendBuffer(&x->out, p, 2 * units);
        x->pos += 2 * units;
    }
}

static
void
NlsCheckConsumed(YAML_NODE *n)
{
    if(n->type == YAML_MAP && n->length != n->count)
    {
        YamlError("unexpected fields in NLS structure (%zu consumed of %zu)", n->length, n->count);
    }
    if(n->type == YAML_MAP || n->type == YAML_SEQUENCE)
    {
        for(size_t i = 0; i < n->count; i++)
        {
            NlsCheckConsumed(n->values[i]);
        }
    }
}

static
void
NlsExtraUnicodeFile(NlsIO *x,
                    YAML_NODE *n)
{
    NlsUnicode(x, n, 0);
}


static
int
NlsEncodeExtra(const char *kind,
               YAML_NODE *body,
               YAML_BUFFER *out)
{
    for(size_t i = 0; i < sizeof(codecs) / sizeof(codecs[0]); i++)
    {
        if(codecs[i].codec && !strcmp(kind, codecs[i].kind))
        {
            NlsIO x = {0};
            codecs[i].codec(&x, body);
            NlsCheckConsumed(body);
            /* Revalidate the emitted structure before the CLI commits the output. */
            NlsIO check = {.p = x.out.data, .n = x.out.len, .reading = 1};
            YAML_NODE *r = YamlCreateNode(YAML_MAP);
            codecs[i].codec(&check, r);
            if(check.pos != check.n)
            {
                YamlError("encoded NLS has trailing data");
            }
            *out = x.out;
            return 1;
        }
    }
    return 0;
}

/* Unicode, character type and geography tables */

static
uint32_t
NlsTablesCpvalue(YAML_NODE *n)
{
    const char *s = YamlGetScalar(n);
    size_t len = strlen(s);
    if(len < 6 || len > 8 || strncmp(s, "U+", 2))
    {
        YamlError("expected Unicode code point U+XXXX[XX]");
    }
    return YamlParseNumber(s + 2, 16, 0x10ffff);
}

static
void
NlsTablesMark(unsigned char *role,
              size_t count,
              size_t at,
              unsigned width,
              unsigned char r)
{
    if(at > count || width > count - at)
    {
        YamlError("NLS trie offset outside table");
    }
    for(unsigned j = 0; j < width; j++)
    {
        if(role[at + j] && role[at + j] != r)
        {
            YamlError("overlapping NLS trie indices and values");
        }
        role[at + j] = r;
    }
}

static
uint32_t *
NlsTablesGraph(uint32_t *t,
               size_t words,
               unsigned char *role,
               int mode,
               unsigned *limit)
{
    if(words < 256)
    {
        YamlError("NLS trie too short");
    }
    int high = mode == 0 && t[0] >= 1280;
    *limit = high ? 0x110000 : 65536;
    uint32_t *slots = YamlAllocateMemory((size_t)*limit * sizeof(*slots));
    NlsTablesMark(role, words, 0, high ? 1280 : 256, 1);
    for(unsigned c = 0; c < *limit; c++)
    {
        size_t mid, leaf;
        unsigned width;
        if(c < 65536)
        {
            mid = (size_t)t[c >> 8] + ((c >> 4) & 15);
            NlsTablesMark(role, words, mid, 1, 1);
            width = mode == 2 ? 2 : 1;
            leaf = (size_t)t[mid] + width * (c & 15);
        }
        else
        {
            unsigned v = c - 65536;
            mid = (size_t)t[256 + (v >> 10)] + ((v >> 5) & 31);
            NlsTablesMark(role, words, mid, 1, 1);
            width = 2;
            leaf = (size_t)t[mid] + 2 * (v & 31);
        }
        NlsTablesMark(role, words, leaf, width, 2);
        slots[c] = (uint32_t)leaf;
    }
    return slots;
}

static
void
NlsTable(NlsIO *x,
         YAML_NODE *parent,
         const char *key,
         unsigned words,
         int mode)
{
    if(words < 256 || words > 65535)
    {
        YamlError("invalid mapping table length");
    }
    YAML_NODE *r = NlsGroup(x, parent, key, YAML_MAP), *map, *layout, *indices, *unused;
    uint32_t *t = YamlAllocateMemory(words * sizeof(*t));
    unsigned char *role = YamlAllocateMemory(words);
    uint32_t *slots;
    unsigned limit;
    if(x->reading)
    {
        NlsBounds(x, words * 2);
        for(unsigned i = 0; i < words; i++)
        {
            t[i] = NlsRaw(x, 2, 0);
        }
        slots = NlsTablesGraph(t, words, role, mode, &limit);
        NlsField(x, r, "default", YamlCreateString(mode == 0 ? "identity" : mode == 1 ? "zero" : "none"));
        map = NlsGroup(x, r, "map", YAML_MAP);
        layout = NlsGroup(x, r, "binary_layout", YAML_MAP);
        indices = NlsGroup(x, layout, "index_words", YAML_MAP);
        unused = NlsGroup(x, layout, "unused_words", YAML_MAP);
        for(unsigned c = 0; c < limit; c++)
        {
            uint32_t at = slots[c], v = t[at];
            if(c >= 65536)
            {
                v |= t[at + 1] << 16;
            }
            if(mode == 2)
            {
                if(v || t[at + 1])
                {
                    YAML_NODE *pair = YamlCreateNode(YAML_MAP);
                    YamlAddField(pair, "base", YamlCreateCodepoint(t[at + 1]));
                    YamlAddField(pair, "combining", YamlCreateCodepoint(v));
                    YamlAddField(map, YamlFormatString("U+%04X", c), pair);
                }
            }
            else if(v)
            {
                YamlAddField(map,
                             YamlFormatString("U+%04X", c),
                             mode == 0 ? YamlCreateCodepoint(c < 65536 ? (c + v) & 65535 : c + v)
                                       : YamlCreateInteger(v));
            }
        }
        for(unsigned i = 0; i < words; i++)
        {
            if(role[i] == 1)
            {
                YamlAddField(indices, YamlFormatString("%u", i), YamlCreateInteger(t[i]));
            }
            else if(!role[i])
            {
                YamlAddField(unused, YamlFormatString("%u", i), YamlCreateInteger(t[i]));
            }
        }
    }
    else
    {
        const char *def = YamlGetScalar(NlsField(x, r, "default", NULL));
        if(strcmp(def, mode == 0 ? "identity" : mode == 1 ? "zero" : "none"))
        {
            YamlError("invalid mapping default");
        }
        map = NlsGroup(x, r, "map", YAML_MAP);
        layout = NlsGroup(x, r, "binary_layout", YAML_MAP);
        indices = NlsGroup(x, layout, "index_words", YAML_MAP);
        unused = NlsGroup(x, layout, "unused_words", YAML_MAP);
        unsigned char *assigned = YamlAllocateMemory(words);
        YAML_NODE *sets[2] = {indices, unused};
        for(unsigned s = 0; s < 2; s++)
        {
            YAML_NODE *g = sets[s];
            g->length = g->count;
            for(size_t j = 0; j < g->count; j++)
            {
                unsigned at = YamlParseNumber(g->keys[j], 10, words - 1);
                if(assigned[at])
                {
                    YamlError("overlapping mapping metadata");
                }
                assigned[at] = s ? 3 : 1;
                t[at] = YamlGetUnsigned(g->values[j], 65535);
            }
        }
        slots = NlsTablesGraph(t, words, role, mode, &limit);
        for(unsigned i = 0; i < words; i++)
        {
            if(assigned[i] != (role[i] == 1 ? 1 : role[i] == 2 ? 0 : 3))
            {
                YamlError("mapping metadata omits or hides words");
            }
        }
        uint32_t *targets = YamlAllocateMemory((size_t)limit * 2 * sizeof(*targets));
        unsigned char *seen = YamlAllocateMemory(limit);
        map->length = map->count;
        for(size_t j = 0; j < map->count; j++)
        {
            YAML_NODE k = {.type = YAML_SCALAR, .text = map->keys[j]};
            unsigned c = NlsTablesCpvalue(&k);
            if(c >= limit || seen[c])
            {
                YamlError("duplicate or out of range mapping key");
            }
            seen[c] = 1;
            if(mode == 2)
            {
                YAML_NODE *pair = map->values[j];
                targets[c * 2] = YamlGetCodepoint(NlsField(x, pair, "combining", NULL));
                targets[c * 2 + 1] = YamlGetCodepoint(NlsField(x, pair, "base", NULL));
            }
            else if(mode == 1)
            {
                targets[c * 2] = YamlGetUnsigned(map->values[j], 65535);
            }
            else
            {
                uint32_t target = NlsTablesCpvalue(map->values[j]);
                if(c < 65536 && target > 65535)
                {
                    YamlError("BMP target outside BMP");
                }
                targets[c * 2] = c < 65536 ? (target - c) & 65535 : target - c;
            }
        }
        unsigned char *written = YamlAllocateMemory(words);
        for(unsigned c = 0; c < limit; c++)
        {
            unsigned at = slots[c], width = (mode == 2 || c >= 65536) ? 2 : 1;
            uint32_t v[2] = {targets[c * 2] & 65535, mode == 2 ? targets[c * 2 + 1] : targets[c * 2] >> 16};
            for(unsigned j = 0; j < width; j++)
            {
                if(written[at + j] && t[at + j] != v[j])
                {
                    YamlError("mapping edit conflicts with shared layout at U+%04X", c);
                }
                t[at + j] = v[j];
                written[at + j] = 1;
            }
        }
        for(unsigned i = 0; i < words; i++)
        {
            NlsRaw(x, 2, t[i]);
        }
    }
}

static
void
NlsCtype(NlsIO *x,
         YAML_NODE *r)
{
    size_t start = x->pos;
    unsigned size = NlsNum(x, r, "byte_size", 2, 0), types_size = NlsNum(x, r, "class_table_size", 2, 0);
    if(size < 4 || types_size < 2 || (types_size - 2) % 6 || types_size + 2 > size)
    {
        YamlError("invalid CTYPE header");
    }
    unsigned classes = (types_size - 2) / 6;
    if(classes > 256)
    {
        YamlError("too many character classes");
    }
    YAML_NODE *defs = NlsGroup(x, r, "classes", YAML_SEQUENCE);
    if(!x->reading && defs->count != classes)
    {
        YamlError("class count mismatch");
    }
    for(unsigned i = 0; i < classes; i++)
    {
        YAML_NODE *v = NlsItem(x, defs, i);
        NlsNum(x, v, "ctype1", 2, 1);
        NlsNum(x, v, "ctype2", 2, 0);
        NlsNum(x, v, "ctype3", 2, 1);
    }
    unsigned bytes = size - types_size - 2;
    if(bytes < 512)
    {
        YamlError("CTYPE trie too short");
    }
    unsigned char *data = YamlAllocateMemory(bytes), *role = YamlAllocateMemory(bytes),
                  *assigned = YamlAllocateMemory(bytes);
    unsigned *slots = YamlAllocateMemory(65536 * sizeof(*slots));
    YAML_NODE *layout = NlsGroup(x, r, "binary_layout", YAML_MAP),
              *indices = NlsGroup(x, layout, "index_words", YAML_MAP),
              *unused = NlsGroup(x, layout, "unused_bytes", YAML_MAP);
    if(x->reading)
    {
        NlsBounds(x, bytes);
        memcpy(data, x->p + x->pos, bytes);
        x->pos += bytes;
    }
    else
    {
        YAML_NODE *sets[2] = {indices, unused};
        for(unsigned s = 0; s < 2; s++)
        {
            YAML_NODE *g = sets[s];
            g->length = g->count;
            for(size_t j = 0; j < g->count; j++)
            {
                unsigned at = YamlParseNumber(g->keys[j], 10, bytes - 1), width = s ? 1 : 2,
                         v = YamlGetUnsigned(g->values[j], s ? 255 : 65535);
                if(at + width > bytes)
                {
                    YamlError("CTYPE metadata outside trie");
                }
                for(unsigned k = 0; k < width; k++)
                {
                    if(assigned[at + k])
                    {
                        YamlError("CTYPE metadata overlap");
                    }
                    assigned[at + k] = s ? 3 : 1;
                    data[at + k] = (unsigned char)(v >> (8 * k));
                }
            }
        }
    }
    NlsTablesMark(role, bytes, 0, 512, 1);
    for(unsigned c = 0; c < 65536; c++)
    {
        unsigned mid = ReadUInt16(data + 2 * (c >> 8)) + 2 * ((c >> 4) & 15);
        NlsTablesMark(role, bytes, mid, 2, 1);
        unsigned at = ReadUInt16(data + mid) + (c & 15);
        NlsTablesMark(role, bytes, at, 1, 2);
        slots[c] = at;
        if(x->reading && data[at] >= classes)
        {
            YamlError("CTYPE references missing class");
        }
    }
    if(x->reading)
    {
        for(unsigned i = 0; i < bytes; i++)
        {
            if(role[i] == 1)
            {
                if(i + 1 >= bytes || role[i + 1] != 1)
                {
                    YamlError("misaligned CTYPE index");
                }
                YamlAddField(indices, YamlFormatString("%u", i), YamlCreateInteger(ReadUInt16(data + i)));
                i++;
            }
            else if(!role[i])
            {
                YamlAddField(unused, YamlFormatString("%u", i), YamlCreateInteger(data[i]));
            }
        }
    }
    else
    {
        for(unsigned i = 0; i < bytes; i++)
        {
            if(assigned[i] != (role[i] == 1 ? 1 : role[i] == 2 ? 0 : 3))
            {
                YamlError("CTYPE metadata hides or omits bytes");
            }
        }
    }
    YAML_NODE *ranges = NlsGroup(x, r, "character_ranges", YAML_SEQUENCE);
    if(x->reading)
    {
        for(unsigned c = 0; c < 65536;)
        {
            unsigned end = c;
            while(end < 65535 && data[slots[end + 1]] == data[slots[c]])
            {
                end++;
            }
            YAML_NODE *v = YamlCreateNode(YAML_MAP);
            YamlAddField(v, "first", YamlCreateCodepoint(c));
            YamlAddField(v, "last", YamlCreateCodepoint(end));
            YamlAddField(v, "class", YamlCreateInteger(data[slots[c]]));
            YamlAppendItem(ranges, v);
            c = end + 1;
        }
    }
    else
    {
        unsigned next = 0;
        unsigned char *written = YamlAllocateMemory(bytes);
        for(size_t i = 0; i < ranges->count; i++)
        {
            YAML_NODE *v = NlsItem(x, ranges, i);
            unsigned first = YamlGetCodepoint(NlsField(x, v, "first", NULL)),
                     last = YamlGetCodepoint(NlsField(x, v, "last", NULL));
            unsigned cls = YamlGetUnsigned(NlsField(x, v, "class", NULL), classes - 1);
            if(first != next || last < first)
            {
                YamlError("CTYPE ranges must cover BMP exactly in order");
            }
            for(unsigned c = first; c <= last; c++)
            {
                unsigned at = slots[c];
                if(written[at] && data[at] != cls)
                {
                    YamlError("CTYPE edit conflicts with shared leaf");
                }
                written[at] = 1;
                data[at] = (unsigned char)cls;
            }
            next = last + 1;
        }
        if(next != 65536)
        {
            YamlError("incomplete CTYPE ranges");
        }
        YamlAppendBuffer(&x->out, data, bytes);
        x->pos += bytes;
    }
    if(x->pos - start != size)
    {
        YamlError("CTYPE size mismatch");
    }
}

static
void
NlsGeo(NlsIO *x,
       YAML_NODE *r)
{
    size_t start = x->pos;
    NlsFixed(x, r, "signature", 4);
    if(strcmp(YamlGetScalar(YamlGetField(r, "signature")), "geo"))
    {
        YamlError("invalid geography signature");
    }
    unsigned size = NlsNum(x, r, "byte_size", 4, 0), ids_at = NlsNum(x, r, "ids_offset", 4, 0),
             ids = NlsNum(x, r, "id_count", 4, 0), index_at = NlsNum(x, r, "index_offset", 4, 0),
             count = NlsNum(x, r, "index_count", 4, 0);
    if(ids_at != 28 || !ids || index_at < 28 || (index_at - 28) % ids || ids > 100000 || count > 100000)
    {
        YamlError("unsupported geography layout");
    }
    unsigned stride = (index_at - 28) / ids;
    if(stride != 80 && stride != 104)
    {
        YamlError("unsupported geography record size");
    }
    if((uint64_t)index_at + 12ull * count != size)
    {
        YamlError("invalid geography index size");
    }
    YAML_NODE *items = NlsGroup(x, r, "places", YAML_SEQUENCE);
    if(!x->reading && items->count != ids)
    {
        YamlError("geography count mismatch");
    }
    for(unsigned i = 0; i < ids; i++)
    {
        YAML_NODE *v = NlsItem(x, items, i);
        NlsNum(x, v, "geoid", 4, 0);
        NlsFixed(x, v, "latitude", 12);
        NlsFixed(x, v, "longitude", 12);
        NlsNum(x, v, "class", 4, 0);
        NlsNum(x, v, "parent", 4, 0);
        NlsFixed(x, v, "iso2", 4);
        NlsFixed(x, v, "iso3", 4);
        NlsNum(x, v, "un_code", 2, 0);
        NlsNum(x, v, "dial_code", 2, 0);
        if(stride == 104)
        {
            NlsFixed(x, v, "currency_code", 4);
            NlsFixed(x, v, "currency_symbol", 8);
        }
    }
    YAML_NODE *index = NlsGroup(x, r, stride == 80 ? "locale_index" : "name_index", YAML_SEQUENCE);
    if(!x->reading && index->count != count)
    {
        YamlError("geography index count mismatch");
    }
    for(unsigned i = 0; i < count; i++)
    {
        YAML_NODE *v = NlsItem(x, index, i);
        if(stride == 80)
        {
            NlsNum(x, v, "lcid", 4, 1);
            NlsNum(x, v, "geoid", 4, 0);
            NlsNum(x, v, "alternate_lcid", 4, 1);
        }
        else
        {
            NlsFixed(x, v, "name", 4);
            unsigned idx = NlsNum(x, v, "place_index", 4, 0);
            if(idx >= ids)
            {
                YamlError("geography index outside places");
            }
        }
    }
    if(x->pos - start != size)
    {
        YamlError("geography size mismatch");
    }
}

static
void
NlsExceptions(NlsIO *x,
              YAML_NODE *r)
{
    unsigned count = NlsNum(x, r, "locale_count", 4, 0);
    if(count > 10000)
    {
        YamlError("too many exception locales");
    }
    YAML_NODE *items = NlsGroup(x, r, "locales", YAML_SEQUENCE);
    if(!x->reading && items->count != count)
    {
        YamlError("exception count mismatch");
    }
    unsigned *offset = YamlAllocateMemory(count * sizeof(*offset)), *upper = YamlAllocateMemory(count * sizeof(*upper)),
             *lower = YamlAllocateMemory(count * sizeof(*lower));
    unsigned words = 0;
    for(unsigned i = 0; i < count; i++)
    {
        YAML_NODE *v = NlsItem(x, items, i);
        NlsNum(x, v, "lcid", 4, 1);
        offset[i] = NlsNum(x, v, "word_offset", 4, 0);
        upper[i] = NlsNum(x, v, "uppercase_count", 4, 0);
        lower[i] = NlsNum(x, v, "lowercase_count", 4, 0);
        if(offset[i] != words || upper[i] > 65536 || lower[i] > 65536)
        {
            YamlError("unsupported exception layout");
        }
        words += 2 * (upper[i] + lower[i]);
    }
    for(unsigned i = 0; i < count; i++)
    {
        for(unsigned k = 0; k < 2; k++)
        {
            YAML_NODE *v = items->values[i], *map = NlsGroup(x, v, k ? "lowercase" : "uppercase", YAML_SEQUENCE);
            unsigned n = k ? lower[i] : upper[i];
            if(!x->reading && map->count != n)
            {
                YamlError("case exception record count mismatch");
            }
            for(unsigned j = 0; j < n; j++)
            {
                YAML_NODE *e = NlsItem(x, map, j);
                if(x->reading)
                {
                    unsigned c = NlsRaw(x, 2, 0), delta = NlsRaw(x, 2, 0);
                    NlsField(x, e, "source", YamlCreateCodepoint(c));
                    NlsField(x, e, "target", YamlCreateCodepoint((c + delta) & 65535));
                }
                else
                {
                    unsigned c = YamlGetCodepoint(NlsField(x, e, "source", NULL)),
                             target = YamlGetCodepoint(NlsField(x, e, "target", NULL));
                    NlsRaw(x, 2, c);
                    NlsRaw(x, 2, (target - c) & 65535);
                }
            }
        }
    }
}

static
void
NlsUnicode(NlsIO *x,
           YAML_NODE *r,
           int embedded)
{
    static const char *names[] = {"fold_digits",
                                  "compatibility",
                                  "hiragana",
                                  "katakana",
                                  "halfwidth",
                                  "fullwidth",
                                  "traditional_chinese",
                                  "simplified_chinese",
                                  "decomposition"};
    YAML_NODE *sizes = NlsGroup(x, r, "table_word_counts", YAML_MAP);
    for(unsigned i = 0; i < (embedded ? 8u : 9u); i++)
    {
        unsigned n = NlsNum(x, sizes, names[i], 2, 0);
        if(n < 257)
        {
            YamlError("invalid Unicode map length");
        }
        NlsTable(x, r, names[i], n - 1, i == 8 ? 2 : 0);
    }
    if(embedded)
    {
        return;
    }
    YAML_NODE *comp = NlsGroup(x, r, "composition", YAML_MAP);
    unsigned bases_len = NlsNum(x, comp, "base_index_words", 2, 0),
             marks_len = NlsNum(x, comp, "combining_index_words", 2, 0);
    unsigned packed;
    if(x->reading)
    {
        packed = NlsRaw(x, 2, 0);
        NlsField(x, comp, "base_count", YamlCreateInteger(packed & 255));
        NlsField(x, comp, "combining_count", YamlCreateInteger(packed >> 8));
    }
    else
    {
        unsigned a = YamlGetUnsigned(NlsField(x, comp, "base_count", NULL), 255),
                 b = YamlGetUnsigned(NlsField(x, comp, "combining_count", NULL), 255);
        packed = a | (b << 8);
        NlsRaw(x, 2, packed);
    }
    NlsTable(x, comp, "base_indices", bases_len, 1);
    NlsTable(x, comp, "combining_indices", marks_len, 1);
    unsigned base_count = packed & 255, mark_count = packed >> 8;
    YAML_NODE *base_map = YamlGetField(YamlGetField(comp, "base_indices"), "map"),
              *mark_map = YamlGetField(YamlGetField(comp, "combining_indices"), "map");
    const char **base_names = YamlAllocateMemory((base_count + 1) * sizeof(*base_names)),
               **mark_names = YamlAllocateMemory((mark_count + 1) * sizeof(*mark_names));
    YAML_NODE *maps[2] = {base_map, mark_map};
    const char **namesets[2] = {base_names, mark_names};
    unsigned counts[2] = {base_count, mark_count};
    for(unsigned k = 0; k < 2; k++)
    {
        YAML_NODE *m = maps[k];
        for(size_t j = 0; j < m->count; j++)
        {
            unsigned idx = YamlGetUnsigned(m->values[j], counts[k]);
            if(!idx || namesets[k][idx])
            {
                YamlError("composition index must be unique and nonzero");
            }
            namesets[k][idx] = m->keys[j];
        }
        for(unsigned j = 1; j <= counts[k]; j++)
        {
            if(!namesets[k][j])
            {
                YamlError("missing composition index");
            }
        }
    }
    YAML_NODE *pairs = NlsGroup(x, comp, "pairs", YAML_MAP);
    for(unsigned a = 1; a <= base_count; a++)
    {
        for(unsigned b = 1; b <= mark_count; b++)
        {
            char *key = YamlFormatString("%s %s", base_names[a], mark_names[b]);
            unsigned v = 0;
            if(x->reading)
            {
                v = NlsRaw(x, 2, 0);
                if(v)
                {
                    YamlAddField(pairs, key, YamlCreateCodepoint(v));
                }
            }
            else
            {
                YAML_NODE *n = YamlFindField(pairs, key);
                if(n)
                {
                    pairs->length++;
                    v = YamlGetCodepoint(n);
                }
                NlsRaw(x, 2, v);
            }
        }
    }
}

static
void
NlsSortCharacter(NlsIO *x,
                 YAML_NODE *r,
                 const char *key)
{
    if(x->reading)
    {
        NlsField(x, r, key, YamlCreateCodepoint(NlsRaw(x, 2, 0)));
    }
    else
    {
        NlsRaw(x, 2, YamlGetCodepoint(NlsField(x, r, key, NULL)));
    }
}

static
void
NlsSortWeight(NlsIO *x,
              YAML_NODE *r)
{
    YAML_NODE *w = NlsGroup(x, r, "weight", YAML_MAP);
    NlsNum(x, w, "primary", 1, 0);
    NlsNum(x, w, "script", 1, 0);
    NlsNum(x, w, "diacritic", 1, 0);
    NlsNum(x, w, "case", 1, 0);
}

static
YAML_NODE *
NlsSortCounted(NlsIO *x,
               YAML_NODE *r,
               const char *key,
               unsigned width,
               unsigned *count)
{
    YAML_NODE *s = NlsGroup(x, r, key, YAML_SEQUENCE);
    if(!x->reading && s->count > 65535)
    {
        YamlError("too many sort records");
    }
    *count = NlsRaw(x, width, x->reading ? 0 : (unsigned)s->count);
    if(*count > 65535)
    {
        YamlError("too many sort records");
    }
    return s;
}

static
void
NlsSort(NlsIO *x,
        YAML_NODE *r)
{
    unsigned n;
    YAML_NODE *s;
    const char *versions[] = {"nls_versions", "defined_versions"};
    for(unsigned k = 0; k < 2; k++)
    {
        s = NlsSortCounted(x, r, versions[k], 4, &n);
        for(unsigned i = 0; i < n; i++)
        {
            YAML_NODE *v = NlsItem(x, s, i);
            NlsNum(x, v, "version", 4, 1);
            NlsNum(x, v, "reserved", 4, 1);
        }
    }
    const char *lists[] = {"reverse_diacritics", "double_compression"};
    for(unsigned k = 0; k < 2; k++)
    {
        s = NlsSortCounted(x, r, lists[k], 4, &n);
        for(unsigned i = 0; i < n; i++)
        {
            YAML_NODE *v = NlsItem(x, s, i);
            NlsNum(x, v, "lcid", 4, 1);
        }
    }
    s = NlsSortCounted(x, r, "cjk_sort_files", 4, &n);
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "lcid", 4, 1);
        NlsFixed(x, v, "filename", 14);
    }
    s = NlsSortCounted(x, r, "expansions", 4, &n);
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsSortCharacter(x, v, "first");
        NlsSortCharacter(x, v, "second");
    }
    s = NlsSortCounted(x, r, "compression_locales", 4, &n);
    unsigned *offset = YamlAllocateMemory(n * sizeof(*offset)), *two = YamlAllocateMemory(n * sizeof(*two)),
             *three = YamlAllocateMemory(n * sizeof(*three));
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "lcid", 4, 1);
        offset[i] = NlsNum(x, v, "word_offset", 4, 0);
        two[i] = NlsNum(x, v, "two_character_count", 2, 0);
        three[i] = NlsNum(x, v, "three_character_count", 2, 0);
    }
    YAML_NODE *groups = NlsGroup(x, r, "compression_groups", YAML_MAP);
    unsigned cursor = 0, done = 0;
    unsigned char *used = YamlAllocateMemory(n);
    while(done < n)
    {
        unsigned best = UINT32_MAX, idx = 0;
        for(unsigned i = 0; i < n; i++)
        {
            if(!used[i] && offset[i] < best)
            {
                best = offset[i];
                idx = i;
            }
        }
        if(best != cursor)
        {
            YamlError("noncontiguous compression groups");
        }
        for(unsigned i = 0; i < n; i++)
        {
            if(offset[i] == best)
            {
                if(two[i] != two[idx] || three[i] != three[idx])
                {
                    YamlError("conflicting shared compression lengths");
                }
                used[i] = 1;
                done++;
            }
        }
        YAML_NODE *g = NlsGroup(x, groups, YamlFormatString("%u", best), YAML_MAP);
        for(unsigned k = 0; k < 2; k++)
        {
            unsigned count = k ? three[idx] : two[idx];
            YAML_NODE *items = NlsGroup(x, g, k ? "three_characters" : "two_characters", YAML_SEQUENCE);
            if(!x->reading && items->count != count)
            {
                YamlError("compression entry count mismatch");
            }
            for(unsigned j = 0; j < count; j++)
            {
                YAML_NODE *v = NlsItem(x, items, j);
                NlsSortCharacter(x, v, "first");
                NlsSortCharacter(x, v, "second");
                if(k)
                {
                    NlsSortCharacter(x, v, "third");
                    NlsNum(x, v, "padding", 2, 1);
                }
                NlsSortWeight(x, v);
            }
        }
        cursor += 4 * two[idx] + 6 * three[idx];
    }
    s = NlsSortCounted(x, r, "exception_locales", 4, &n);
    offset = YamlAllocateMemory(n * sizeof(*offset));
    unsigned *counts = YamlAllocateMemory(n * sizeof(*counts));
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "lcid", 4, 1);
        offset[i] = NlsNum(x, v, "word_offset", 4, 0);
        counts[i] = NlsNum(x, v, "entry_count", 4, 0);
        if(counts[i] > 65536)
        {
            YamlError("too many sort exceptions");
        }
    }
    groups = NlsGroup(x, r, "exception_groups", YAML_MAP);
    used = YamlAllocateMemory(n);
    cursor = done = 0;
    while(done < n)
    {
        unsigned best = UINT32_MAX, idx = 0;
        for(unsigned i = 0; i < n; i++)
        {
            if(!used[i] && offset[i] < best)
            {
                best = offset[i];
                idx = i;
            }
        }
        if(best != cursor)
        {
            YamlError("noncontiguous exception groups");
        }
        for(unsigned i = 0; i < n; i++)
        {
            if(offset[i] == best)
            {
                if(counts[i] != counts[idx])
                {
                    YamlError("conflicting shared sort exceptions");
                }
                used[i] = 1;
                done++;
            }
        }
        YAML_NODE *items = NlsGroup(x, groups, YamlFormatString("%u", best), YAML_SEQUENCE);
        if(!x->reading && items->count != counts[idx])
        {
            YamlError("sort exception count mismatch");
        }
        for(unsigned j = 0; j < counts[idx]; j++)
        {
            YAML_NODE *v = NlsItem(x, items, j);
            NlsSortCharacter(x, v, "character");
            NlsSortWeight(x, v);
        }
        cursor += 3 * counts[idx];
    }
    s = NlsSortCounted(x, r, "multiple_weights", 2, &n);
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "script", 1, 0);
        NlsNum(x, v, "weight_count", 1, 0);
    }
    s = NlsSortCounted(x, r, "jamo", 4, &n);
    if(n != 256)
    {
        YamlError("unsupported legacy Jamo table size");
    }
    for(unsigned i = 0; i < n; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        for(unsigned j = 0; j < 5; j++)
        {
            NlsNum(x, v, YamlFormatString("weight_%u", j), 1, 0);
        }
        NlsNum(x, v, "offset", 1, 0);
        NlsNum(x, v, "length", 1, 0);
        NlsNum(x, v, "reserved", 1, 0);
    }
    s = NlsGroup(x, r, "jamo_second_characters", YAML_SEQUENCE);
    if(!x->reading && s->count != 256)
    {
        YamlError("invalid second Jamo table length");
    }
    for(unsigned i = 0; i < 256; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsSortCharacter(x, v, "character");
        for(unsigned j = 0; j < 5; j++)
        {
            NlsNum(x, v, YamlFormatString("weight_%u", j), 1, 0);
        }
        NlsNum(x, v, "length", 1, 0);
    }
}


static
const char *
NlsLocalePoolPrefix(char type)
{
    switch(type)
    {
    case 'T':
        return "text_";
    case 'A':
        return "array_";
    case 'F':
        return "font_";
    case 'E':
        return "eras_";
    case 'R':
        return "era_";
    default:
        YamlError("invalid locale pool type");
    }
    return "";
}

static
char *
NlsLocalePoolKey(char type,
                 unsigned offset)
{
    return YamlFormatString("%s%u", NlsLocalePoolPrefix(type), offset);
}

static
unsigned
NlsLocalePoolOffset(const char *key,
                    char type)
{
    const char *prefix = NlsLocalePoolPrefix(type);
    size_t n = strlen(prefix);
    if(strncmp(key, prefix, n))
    {
        YamlError("invalid locale reference: %s", key);
    }
    return YamlParseNumber(key + n, 10, YAML_MAX_BINARY / 2);
}

static
void
NlsLocaleRequire(Pool *p,
                 char type,
                 unsigned offset)
{
    if(offset >= p->words)
    {
        YamlError("locale pool reference outside section");
    }
    char *key = NlsLocalePoolKey(type, offset);
    if(!YamlFindField(p->needed, key))
    {
        YamlAddField(p->needed, key, YamlCreateInteger((unsigned char)type));
    }
}

static
unsigned
NlsLocaleReference(Pool *p,
                   YAML_NODE *r,
                   const char *key,
                   unsigned width,
                   char type)
{
    unsigned offset;
    if(p->x->reading)
    {
        offset = NlsRaw(p->x, width, 0);
        NlsField(p->x, r, key, YamlCreateString(NlsLocalePoolKey(type, offset)));
    }
    else
    {
        offset = NlsLocalePoolOffset(YamlGetScalar(NlsField(p->x, r, key, NULL)), type);
        NlsRaw(p->x, width, offset);
    }
    NlsLocaleRequire(p, type, offset);
    return offset;
}

static
void
NlsLocaleWritePool(Pool *p,
                   size_t byte_offset,
                   const unsigned char *data,
                   size_t bytes)
{
    if(byte_offset > p->words * 2 || bytes > p->words * 2 - byte_offset)
    {
        YamlError("locale value exceeds pool boundaries");
    }
    for(size_t i = 0; i < bytes; i++)
    {
        size_t at = byte_offset + i;
        if(p->used[at] && p->data[at] != data[i])
        {
            YamlError("locale edit conflicts with shared pool storage at byte %zu", at);
        }
        p->used[at] = 1;
        p->data[at] = data[i];
    }
}

static
unsigned
NlsLocalePoolWord(Pool *p,
                  size_t at)
{
    if(at >= p->words)
    {
        YamlError("truncated locale pool value");
    }
    return ReadUInt16(p->source + 2 * at);
}

static
unsigned
NlsLocalePoolDword(Pool *p,
                   size_t at)
{
    return NlsLocalePoolWord(p, at) | (NlsLocalePoolWord(p, at + 1) << 16);
}

static
void
NlsLocalePoolValues(Pool *p,
                    YAML_NODE *r)
{
    NlsIO *x = p->x;
    YAML_NODE *values = NlsGroup(x, r, "values", YAML_MAP);
    /* Arrays can add references to strings; consume the growing work list. */
    for(size_t i = 0; i < p->needed->count; i++)
    {
        const char *key = p->needed->keys[i];
        char type = (char)YamlGetUnsigned(p->needed->values[i], 255);
        unsigned at = NlsLocalePoolOffset(key, type);
        YAML_NODE *v = NlsGroup(x, values, key, YAML_MAP);
        YAML_BUFFER b = {0};
        unsigned len = x->reading ? NlsLocalePoolWord(p, at) : 0;
        if(type == 'T')
        {
            YAML_NODE *text;
            if(x->reading)
            {
                if((size_t)at + len + 1 >= p->words)
                {
                    YamlError("unterminated locale text");
                }
                text = NlsField(x, v, "text", YamlCreateString(NlsUtf8(p->source + 2 * (at + 1), len)));
            }
            else
            {
                text = NlsField(x, v, "text", NULL);
            }
            YAML_BUFFER text16 = NlsUtf16(YamlGetScalar(text));
            if(text16.len / 2 > 65535)
            {
                YamlError("locale text too long");
            }
            WriteUInt16(&b, (uint32_t)(text16.len / 2));
            YamlAppendBuffer(&b, text16.data, text16.len);
            unsigned end;
            if(x->reading)
            {
                end = NlsLocalePoolWord(p, at + len + 1);
                NlsField(x, v, "terminator", YamlCreateInteger(end));
            }
            else
            {
                end = YamlGetUnsigned(NlsField(x, v, "terminator", NULL), 65535);
            }
            if(end)
            {
                YamlError("nonzero locale text terminator");
            }
            WriteUInt16(&b, end);
        }
        else if(type == 'R')
        {
            if(x->reading && len != 6)
            {
                YamlError("unsupported calendar era record");
            }
            WriteUInt16(&b, 6);
            const char *names[] = {
                "era_id", "start_year", "start_month", "start_day", "year_offset", "first_year", "terminator"};
            for(unsigned j = 0; j < 7; j++)
            {
                unsigned val;
                if(x->reading)
                {
                    val = NlsLocalePoolWord(p, at + 1 + j);
                    YAML_NODE *item = YamlCreateInteger(val);
                    if(j == 4 && val >= 32768)
                    {
                        item = YamlCreateString(YamlFormatString("%d", (int)val - 65536));
                        item->IsInteger = 1;
                    }
                    NlsField(x, v, names[j], item);
                }
                else
                {
                    YAML_NODE *item = NlsField(x, v, names[j], NULL);
                    const char *str = YamlGetScalar(item);
                    if(j == 4 && str[0] == '-')
                    {
                        val = (65536 - YamlParseNumber(str + 1, 10, 32768)) & 65535;
                    }
                    else
                    {
                        val = YamlGetUnsigned(item, j == 4 ? 32767 : 65535);
                    }
                }
                if(j == 6 && val)
                {
                    YamlError("invalid era terminator");
                }
                WriteUInt16(&b, val);
            }
        }
        else
        {
            int array = type == 'A' || type == 'E';
            char child = type == 'E' ? 'R' : 'T';
            YAML_NODE *items = NlsGroup(x, v, array ? "items" : "uint32_values", YAML_SEQUENCE);
            if(x->reading)
            {
                if(type == 'F' && len % 2)
                {
                    YamlError("odd font signature size");
                }
                unsigned count = array ? len : len / 2;
                for(unsigned j = 0; j < count; j++)
                {
                    unsigned value = NlsLocalePoolDword(p, at + 1 + 2 * j);
                    if(array)
                    {
                        NlsLocaleRequire(p, child, value);
                        YamlAppendItem(items, YamlCreateString(NlsLocalePoolKey(child, value)));
                    }
                    else
                    {
                        YamlAppendItem(items, YamlCreateHexadecimal(value, 8));
                    }
                }
            }
            if(items->count > (array ? 65535 : 32767))
            {
                YamlError("locale array too large");
            }
            WriteUInt16(&b, (uint32_t)items->count * (array ? 1 : 2));
            for(size_t j = 0; j < items->count; j++)
            {
                unsigned value = array ? NlsLocalePoolOffset(YamlGetScalar(items->values[j]), child)
                                       : YamlGetHexadecimal(items->values[j], UINT32_MAX);
                if(array)
                {
                    NlsLocaleRequire(p, child, value);
                }
                WriteUInt32(&b, value);
            }
        }
        NlsLocaleWritePool(p, 2 * at, b.data, b.len);
    }
    YAML_NODE *unused = NlsGroup(x, r, "unreferenced_words", YAML_MAP);
    if(x->reading)
    {
        for(size_t i = 0; i < p->words; i++)
        {
            if(p->used[2 * i] != p->used[2 * i + 1])
            {
                YamlError("unaligned locale pool coverage");
            }
            if(!p->used[2 * i])
            {
                YamlAddField(unused, YamlFormatString("%zu", i), YamlCreateInteger(NlsLocalePoolWord(p, i)));
            }
        }
    }
    else
    {
        unused->length = unused->count;
        for(size_t i = 0; i < unused->count; i++)
        {
            unsigned at = YamlParseNumber(unused->keys[i], 10, (unsigned)p->words - 1),
                     value = YamlGetUnsigned(unused->values[i], 65535);
            if(p->used[2 * at] || p->used[2 * at + 1])
            {
                YamlError("unused word overlaps locale value");
            }
            unsigned char b[2] = {(unsigned char)value, (unsigned char)(value >> 8)};
            NlsLocaleWritePool(p, 2 * at, b, 2);
        }
        for(size_t i = 0; i < p->words * 2; i++)
        {
            if(!p->used[i])
            {
                YamlError("locale pool has missing storage");
            }
        }
    }
    if(x->reading)
    {
        for(size_t i = 0; i < p->words * 2; i++)
        {
            if(p->used[i] && p->source[i] != p->data[i])
            {
                YamlError("locale pool semantic reconstruction mismatch");
            }
        }
        x->pos += p->words * 2;
    }
    else
    {
        YamlAppendBuffer(&x->out, p->data, p->words * 2);
        x->pos += p->words * 2;
    }
}

static
void
NlsLocalePosition(NlsIO *x,
                  size_t at)
{
    if(x->pos != at)
    {
        YamlError("unsupported locale layout at %zu; expected %zu", x->pos, at);
    }
}

static
void
NlsLocaleLocaleDatabase(NlsIO *x,
                        YAML_NODE *r,
                        size_t section_size)
{
    size_t start = x->pos;
    YAML_NODE *h = NlsGroup(x, r, "header", YAML_MAP);
    NlsNum(x, h, "version_offset", 4, 0);
    NlsNum(x, h, "unknown_04", 4, 1);
    unsigned version = NlsNum(x, h, "version", 4, 0), magic = NlsNum(x, h, "signature", 4, 1);
    if(version != 7 || magic != 0x5344534e)
    {
        YamlError("unsupported locale database version");
    }
    for(unsigned i = 0; i < 3; i++)
    {
        NlsNum(x, h, YamlFormatString("unknown_%02x", 16 + 4 * i), 4, 1);
    }
    NlsNum(x, h, "header_size", 2, 0);
    unsigned lcids = NlsNum(x, h, "lcid_count", 2, 0), locales = NlsNum(x, h, "locale_count", 2, 0),
             locale_size = NlsNum(x, h, "locale_record_size", 2, 0);
    unsigned locale_at = NlsNum(x, h, "locales_offset", 4, 0), names = NlsNum(x, h, "name_count", 2, 0);
    NlsNum(x, h, "padding", 2, 1);
    unsigned ids_at = NlsNum(x, h, "lcids_offset", 4, 0), names_at = NlsNum(x, h, "names_offset", 4, 0);
    NlsNum(x, h, "unknown_34", 4, 1);
    unsigned calendars = NlsNum(x, h, "calendar_count", 2, 0),
             calendar_size = NlsNum(x, h, "calendar_record_size", 2, 0),
             cal_at = NlsNum(x, h, "calendars_offset", 4, 0), strings_at = NlsNum(x, h, "strings_offset", 4, 0);
    for(unsigned i = 0; i < 4; i++)
    {
        NlsNum(x, h, YamlFormatString("unknown_%02x", 68 + 2 * i), 2, 1);
    }
    if(locale_size != 328 || calendar_size != 72 || strings_at > section_size || (section_size - strings_at) % 2)
    {
        YamlError("unsupported locale record layout");
    }
    Pool pool = {.x = x, .needed = YamlCreateNode(YAML_MAP), .words = (section_size - strings_at) / 2};
    if(x->reading)
    {
        NlsBounds(x, section_size - (x->pos - start));
        pool.source = x->p + start + strings_at;
    }
    pool.data = YamlAllocateMemory(pool.words * 2);
    pool.used = YamlAllocateMemory(pool.words * 2);
    NlsLocalePosition(x, start + ids_at);
    YAML_NODE *s = NlsGroup(x, r, "lcid_index", YAML_SEQUENCE);
    if(!x->reading && s->count != lcids)
    {
        YamlError("locale index count mismatch");
    }
    for(unsigned i = 0; i < lcids; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "lcid", 4, 1);
        unsigned idx = NlsNum(x, v, "locale_index", 2, 0);
        if(idx >= locales)
        {
            YamlError("LCID index outside locales");
        }
        NlsLocaleReference(&pool, v, "name", 2, 'T');
    }
    NlsLocalePosition(x, start + names_at);
    s = NlsGroup(x, r, "name_index", YAML_SEQUENCE);
    if(!x->reading && s->count != names)
    {
        YamlError("locale name count mismatch");
    }
    for(unsigned i = 0; i < names; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsLocaleReference(&pool, v, "name", 2, 'T');
        unsigned idx = NlsNum(x, v, "locale_index", 2, 0);
        if(idx >= locales)
        {
            YamlError("name index outside locales");
        }
        NlsNum(x, v, "lcid", 4, 1);
    }
    NlsLocalePosition(x, start + locale_at);
    s = NlsGroup(x, r, "locales", YAML_SEQUENCE);
    if(!x->reading && s->count != locales)
    {
        YamlError("locale count mismatch");
    }
    for(unsigned i = 0; i < locales; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        for(size_t j = 0; j < sizeof(locale_fields) / sizeof(locale_fields[0]); j++)
        {
            const LocaleField *f = &locale_fields[j];
            if(f->ref)
            {
                NlsLocaleReference(&pool, v, f->name, f->width, f->ref);
            }
            else
            {
                NlsNum(x, v, f->name, f->width, 0);
            }
        }
    }
    NlsLocalePosition(x, start + cal_at);
    s = NlsGroup(x, r, "calendars", YAML_SEQUENCE);
    if(!x->reading && s->count != calendars)
    {
        YamlError("calendar count mismatch");
    }
    const char *calfields[] = {"short_date",
                               "year_month",
                               "long_date",
                               "era_names",
                               "year_offset_range",
                               "day_names",
                               "abbreviated_day_names",
                               "month_names",
                               "abbreviated_month_names",
                               "name",
                               "month_day",
                               "abbreviated_era_names",
                               "shortest_day_names",
                               "relative_long_date"};
    for(unsigned i = 0; i < calendars; i++)
    {
        YAML_NODE *v = NlsItem(x, s, i);
        NlsNum(x, v, "calendar_id", 2, 0);
        NlsNum(x, v, "two_digit_year_max", 2, 0);
        for(unsigned j = 0; j < 14; j++)
        {
            NlsLocaleReference(&pool, v, calfields[j], 4, j == 4 ? 'E' : (j == 9 || j == 13) ? 'T' : 'A');
        }
        for(unsigned j = 0; j < 3; j++)
        {
            NlsNum(x, v, YamlFormatString("unused_%u", j), 4, 1);
        }
    }
    NlsLocalePosition(x, start + strings_at);
    YAML_NODE *p = NlsGroup(x, r, "string_pool", YAML_MAP);
    NlsLocalePoolValues(&pool, p);
    NlsLocalePosition(x, start + section_size);
}

static
void
NlsLocale(NlsIO *x,
          YAML_NODE *r)
{
    YAML_NODE *h = NlsGroup(x, r, "sections", YAML_MAP);
    unsigned ctypes = NlsNum(x, h, "character_types_offset", 4, 0);
    for(unsigned i = 0; i < 3; i++)
    {
        if(NlsNum(x, h, YamlFormatString("reserved_%u", i), 4, 1))
        {
            YamlError("unsupported extra locale section");
        }
    }
    unsigned locales = NlsNum(x, h, "locales_offset", 4, 0), charmaps = NlsNum(x, h, "character_maps_offset", 4, 0),
             geo = NlsNum(x, h, "geography_offset", 4, 0), scripts = NlsNum(x, h, "scripts_offset", 4, 0);
    if(ctypes != 32 || ctypes > locales || locales > charmaps || charmaps > geo || geo > scripts ||
       scripts > YAML_MAX_BINARY)
    {
        YamlError("invalid locale section offsets");
    }
    NlsLocalePosition(x, ctypes);
    NlsCtype(x, NlsGroup(x, r, "character_types", YAML_MAP));
    if(x->pos > locales || (locales - x->pos) % 2 || locales - x->pos > 16)
    {
        YamlError("invalid character type alignment");
    }
    size_t padding = (locales - x->pos) / 2;
    YAML_NODE *pad = NlsGroup(x, r, "character_types_alignment", YAML_SEQUENCE);
    if(!x->reading && pad->count != padding)
    {
        YamlError("character type alignment size mismatch");
    }
    for(size_t i = 0; i < padding; i++)
    {
        if(x->reading)
        {
            YamlAppendItem(pad, YamlCreateInteger(NlsRaw(x, 2, 0)));
        }
        else
        {
            NlsRaw(x, 2, YamlGetUnsigned(pad->values[i], 65535));
        }
    }
    NlsLocalePosition(x, locales);
    NlsLocaleLocaleDatabase(x, NlsGroup(x, r, "regional_settings", YAML_MAP), charmaps - locales);
    NlsLocalePosition(x, charmaps);
    NlsUnicode(x, NlsGroup(x, r, "character_maps", YAML_MAP), 1);
    NlsLocalePosition(x, geo);
    NlsGeo(x, NlsGroup(x, r, "geography", YAML_MAP));
    NlsLocalePosition(x, scripts);

    if(x->reading && x->pos != x->n)
    {
        YamlError("unsupported nonempty locale script section");
    }
}


static
int
NlsCodecProbeCp(const unsigned char *p,
                size_t len,
                Codepage *c)
{
    memset(c, 0, sizeof(*c));
    if(len < 546 || ReadUInt16(p) != 13)
    {
        return 0;
    }
    c->size = ReadUInt16(p + 4);
    if(c->size != 1 && c->size != 2)
    {
        return 0;
    }
    c->flag = 26 + 2 * ReadUInt16(p + 26);
    c->wide = c->flag + 2;
    c->end = c->wide + 65536 * c->size;
    if(c->end > len)
    {
        return 0;
    }
    c->marker = ReadUInt16(p + 540);
    c->glyph = 542;
    c->range_pos = 542 + (c->marker ? 512 : 0);
    if(c->range_pos + 2 > c->flag)
    {
        return 0;
    }
    c->ranges = ReadUInt16(p + c->range_pos);
    size_t pos = c->range_pos + 2;
    if(c->ranges)
    {
        if(c->size != 2 || pos + 512 > c->flag)
        {
            return 0;
        }
        c->dbcs_base = pos;
        pos += 512;
        for(unsigned lead = 0; lead < 256; lead++)
        {
            uint32_t off = ReadUInt16(p + c->dbcs_base + 2 * lead);
            if(!off)
            {
                continue;
            }
            size_t addr = c->dbcs_base + off * 2;
            if(addr < pos || addr + 512 > c->flag)
            {
                return 0;
            }
            unsigned i = 0;
            while(i < c->block_count && c->blocks[i] < addr)
            {
                i++;
            }
            if(i < c->block_count && c->blocks[i] == addr)
            {
                continue;
            }
            for(unsigned j = c->block_count; j > i; j--)
            {
                c->blocks[j] = c->blocks[j - 1];
                c->leads[j] = c->leads[j - 1];
            }
            c->blocks[i] = addr;
            c->leads[i] = lead;
            c->block_count++;
        }
        for(unsigned i = 0; i < c->block_count; i++)
        {
            if(c->blocks[i] < pos)
            {
                return 0;
            }
            pos = c->blocks[i] + 512;
        }
    }
    return pos <= c->flag;
}

static
int
NlsCodecCompareU32(const void *a,
                   const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static
uint32_t *
NlsCodecReadValues(const unsigned char *p,
                   size_t count,
                   unsigned width)
{
    uint32_t *values = YamlAllocateMemory(count * sizeof(*values));
    for(size_t i = 0; i < count; i++)
    {
        values[i] = width == 1 ? p[i] : width == 2 ? ReadUInt16(p + 2 * i) : ReadUInt32(p + 4 * i);
    }
    return values;
}

static
uint32_t
NlsCodecMode(const uint32_t *values,
             size_t count)
{
    uint32_t *sorted = YamlAllocateMemory(count * sizeof(*sorted));
    memcpy(sorted, values, count * sizeof(*sorted));
    qsort(sorted, count, sizeof(*sorted), NlsCodecCompareU32);
    size_t best = 0;
    uint32_t result = 0;
    for(size_t i = 0; i < count;)
    {
        size_t end = i + 1;
        while(end < count && sorted[end] == sorted[i])
        {
            end++;
        }
        if(end - i > best)
        {
            best = end - i;
            result = sorted[i];
        }
        i = end;
    }
    return result;
}

static
YAML_NODE *
NlsCodecSparse(const uint32_t *values,
               size_t count,
               int direction,
               unsigned width,
               uint32_t def)
{
    YAML_NODE *n = YamlCreateNode(YAML_MAP), *map = YamlCreateNode(YAML_MAP);
    YamlAddField(n, "default", direction ? YamlCreateHexadecimal(def, width * 2) : YamlCreateCodepoint(def));
    YamlAddField(n, "map", map);
    for(size_t i = 0; i < count; i++)
    {
        if(values[i] != def)
        {
            YamlAddField(map,
                         direction ? YamlFormatString("U+%04X", (unsigned)i) : YamlFormatString("0x%02X", (unsigned)i),
                         direction ? YamlCreateHexadecimal(values[i], width * 2) : YamlCreateCodepoint(values[i]));
        }
    }
    return n;
}

static
uint32_t *
NlsCodecExpand(YAML_NODE *n,
               size_t count,
               int direction,
               uint32_t max)
{
    YamlValidateFields(n, "default map", "");
    uint32_t def =
        direction ? YamlGetHexadecimal(YamlGetField(n, "default"), max) : YamlGetCodepoint(YamlGetField(n, "default"));
    uint32_t *values = YamlAllocateMemory(count * sizeof(*values));
    unsigned char *seen = YamlAllocateMemory(count);
    for(size_t i = 0; i < count; i++)
    {
        values[i] = def;
    }
    YAML_NODE *map = YamlGetField(n, "map");
    if(map->type != YAML_MAP || map->count > count)
    {
        YamlError("invalid sparse mapping");
    }
    for(size_t i = 0; i < map->count; i++)
    {
        YAML_NODE key = {.type = YAML_SCALAR, .text = map->keys[i]};
        uint32_t index = direction ? YamlGetCodepoint(&key) : YamlGetHexadecimal(&key, 255);
        if(index >= count || seen[index])
        {
            YamlError("duplicate or out-of-range normalized mapping key");
        }
        seen[index] = 1;
        values[index] = direction ? YamlGetHexadecimal(map->values[i], max) : YamlGetCodepoint(map->values[i]);
    }
    return values;
}

static
void
NlsCodecWriteValues(YAML_BUFFER *out,
                    const uint32_t *values,
                    size_t count,
                    unsigned width)
{
    for(size_t i = 0; i < count; i++)
    {
        if(width == 1)
        {
            if(values[i] > 255)
            {
                YamlError("byte overflow");
            }
            unsigned char c = (unsigned char)values[i];
            YamlAppendBuffer(out, &c, 1);
        }
        else if(width == 2)
        {
            WriteUInt16(out, values[i]);
        }
        else
        {
            WriteUInt32(out, values[i]);
        }
    }
}

static
void
NlsCodecBlobAppend(YAML_BUFFER *out,
                   YAML_NODE *n)
{
    if(!n || n->type != YAML_BINARY)
    {
        YamlError("expected YAML !!binary");
    }
    YamlAppendBuffer(out, n->bytes, n->length);
}

static
YAML_NODE *
NlsCodecDecodeCp(const unsigned char *p,
                 size_t len,
                 Codepage *c)
{
    YAML_NODE *root = YamlCreateNode(YAML_MAP), *h = YamlCreateNode(YAML_MAP), *ranges = YamlCreateNode(YAML_SEQUENCE),
              *layout = YamlCreateNode(YAML_MAP);
    YamlAddField(root, "header", h);
    YamlAddField(h, "code_page", YamlCreateInteger(ReadUInt16(p + 2)));
    YamlAddField(h, "max_char_size", YamlCreateInteger(c->size));
    YamlAddField(h, "default_encoded", YamlCreateHexadecimal(ReadUInt16(p + 6), c->size * 2));
    YamlAddField(h, "default_unicode", YamlCreateCodepoint(ReadUInt16(p + 8)));
    YamlAddField(h, "translated_default", YamlCreateCodepoint(ReadUInt16(p + 10)));
    YamlAddField(h, "translated_unicode_default", YamlCreateHexadecimal(ReadUInt16(p + 12), c->size * 2));
    unsigned pairs = 6;
    while(pairs && !p[14 + 2 * (pairs - 1)] && !p[15 + 2 * (pairs - 1)])
    {
        pairs--;
    }
    for(unsigned i = 0; i < pairs; i++)
    {
        YAML_NODE *pair = YamlCreateNode(YAML_SEQUENCE);
        YamlAppendItem(pair, YamlCreateHexadecimal(p[14 + i * 2], 2));
        YamlAppendItem(pair, YamlCreateHexadecimal(p[15 + i * 2], 2));
        YamlAppendItem(ranges, pair);
    }
    YamlAddField(h, "lead_byte_ranges", ranges);
    YamlAddField(root, "to_unicode", NlsCodecSparse(NlsCodecReadValues(p + 28, 256, 2), 256, 0, 2, ReadUInt16(p + 8)));
    uint32_t *wide = NlsCodecReadValues(p + c->wide, 65536, c->size);
    YamlAddField(root, "from_unicode", NlsCodecSparse(wide, 65536, 1, c->size, NlsCodecMode(wide, 65536)));
    if(c->marker)
    {
        YamlAddField(
            root, "glyphs", NlsCodecSparse(NlsCodecReadValues(p + c->glyph, 256, 2), 256, 0, 2, ReadUInt16(p + 8)));
    }
    YamlAddField(layout, "glyph_marker", YamlCreateInteger(c->marker));
    YamlAddField(layout, "dbcs_range_count", YamlCreateInteger(c->ranges));
    YamlAddField(layout, "wide_table_flag", YamlCreateInteger(ReadUInt16(p + c->flag)));
    size_t pos = c->range_pos + 2;
    if(c->ranges)
    {
        YAML_NODE *dbcs = YamlCreateNode(YAML_MAP), *leads = YamlCreateNode(YAML_MAP),
                  *tables = YamlCreateNode(YAML_MAP), *storage = YamlCreateNode(YAML_SEQUENCE);
        YamlAddField(root, "dbcs", dbcs);
        YamlAddField(dbcs, "lead_bytes", leads);
        YamlAddField(dbcs, "tables", tables);
        for(unsigned lead = 0; lead < 256; lead++)
        {
            uint32_t off = ReadUInt16(p + c->dbcs_base + lead * 2);
            if(!off)
            {
                continue;
            }
            for(unsigned j = 0; j < c->block_count; j++)
            {
                if(c->blocks[j] == c->dbcs_base + off * 2)
                {
                    YamlAddField(leads,
                                 YamlFormatString("0x%02X", lead),
                                 YamlCreateString(YamlFormatString("DBCS_%02X", c->leads[j])));
                }
            }
        }
        pos = c->dbcs_base + 512;
        for(unsigned i = 0; i < c->block_count; i++)
        {
            if(c->blocks[i] > pos)
            {
                YAML_NODE *gap = YamlCreateNode(YAML_MAP);
                YamlAddField(gap, "padding", YamlCreateBinary(p + pos, c->blocks[i] - pos));
                YamlAppendItem(storage, gap);
            }
            char *name = YamlFormatString("DBCS_%02X", c->leads[i]);
            uint32_t *v = NlsCodecReadValues(p + c->blocks[i], 256, 2);
            YamlAddField(tables, name, NlsCodecSparse(v, 256, 0, 2, NlsCodecMode(v, 256)));
            YAML_NODE *part = YamlCreateNode(YAML_MAP);
            YamlAddField(part, "table", YamlCreateString(name));
            YamlAppendItem(storage, part);
            pos = c->blocks[i] + 512;
        }
        YamlAddField(layout, "dbcs_storage", storage);
    }
    if(pos < c->flag)
    {
        YamlAddField(layout, "pre_wide_padding", YamlCreateBinary(p + pos, c->flag - pos));
    }
    if(c->end < len)
    {
        YamlAddField(layout, "trailing_bytes", YamlCreateBinary(p + c->end, len - c->end));
    }
    YamlAddField(root, "binary_layout", layout);
    return root;
}

static
YAML_BUFFER
NlsCodecEncodeCp(YAML_NODE *root)
{
    YamlValidateFields(root, "header to_unicode from_unicode binary_layout", "glyphs dbcs");
    YAML_NODE *h = YamlGetField(root, "header");
    YamlValidateFields(h,
                       "code_page max_char_size default_encoded default_unicode translated_default "
                       "translated_unicode_default lead_byte_ranges",
                       "");
    unsigned size = YamlGetUnsigned(YamlGetField(h, "max_char_size"), 2);
    if(!size)
    {
        YamlError("max_char_size must be 1 or 2");
    }
    uint32_t max = size == 1 ? 255 : 65535;
    YAML_BUFFER out = {0}, body = {0};
    WriteUInt16(&out, 13);
    WriteUInt16(&out, YamlGetUnsigned(YamlGetField(h, "code_page"), 65535));
    WriteUInt16(&out, size);
    WriteUInt16(&out, YamlGetHexadecimal(YamlGetField(h, "default_encoded"), max));
    WriteUInt16(&out, YamlGetCodepoint(YamlGetField(h, "default_unicode")));
    WriteUInt16(&out, YamlGetCodepoint(YamlGetField(h, "translated_default")));
    WriteUInt16(&out, YamlGetHexadecimal(YamlGetField(h, "translated_unicode_default"), max));
    YAML_NODE *ranges = YamlGetField(h, "lead_byte_ranges");
    if(ranges->type != YAML_SEQUENCE || ranges->count > 6)
    {
        YamlError("expected up to six lead byte pairs");
    }
    unsigned char pairs[12] = {0};
    for(size_t i = 0; i < ranges->count; i++)
    {
        YAML_NODE *pair = ranges->values[i];
        if(pair->type != YAML_SEQUENCE || pair->count != 2)
        {
            YamlError("invalid lead byte pair");
        }
        pairs[i * 2] = (unsigned char)YamlGetHexadecimal(pair->values[0], 255);
        pairs[i * 2 + 1] = (unsigned char)YamlGetHexadecimal(pair->values[1], 255);
    }
    YamlAppendBuffer(&out, pairs, 12);
    NlsCodecWriteValues(&body, NlsCodecExpand(YamlGetField(root, "to_unicode"), 256, 0, 65535), 256, 2);
    YAML_NODE *layout = YamlGetField(root, "binary_layout");
    YamlValidateFields(
        layout, "glyph_marker dbcs_range_count wide_table_flag", "dbcs_storage pre_wide_padding trailing_bytes");
    uint32_t marker = YamlGetUnsigned(YamlGetField(layout, "glyph_marker"), 65535),
             count = YamlGetUnsigned(YamlGetField(layout, "dbcs_range_count"), 65535);
    YAML_NODE *glyphs = YamlFindField(root, "glyphs");
    if(!!marker != !!glyphs)
    {
        YamlError("glyph marker and table disagree");
    }
    WriteUInt16(&body, marker);
    if(glyphs)
    {
        NlsCodecWriteValues(&body, NlsCodecExpand(glyphs, 256, 0, 65535), 256, 2);
    }
    WriteUInt16(&body, count);
    YAML_NODE *dbcs = YamlFindField(root, "dbcs"), *storage = YamlFindField(layout, "dbcs_storage");
    if(!!count != !!dbcs || !!count != !!storage)
    {
        YamlError("DBCS metadata and tables disagree");
    }
    if(count)
    {
        if(size != 2 || storage->type != YAML_SEQUENCE)
        {
            YamlError("invalid DBCS structure");
        }
        YamlValidateFields(dbcs, "lead_bytes tables", "");
        YAML_NODE *tables = YamlGetField(dbcs, "tables"), *leads = YamlGetField(dbcs, "lead_bytes"),
                  *positions = YamlCreateNode(YAML_MAP);
        if(tables->type != YAML_MAP || leads->type != YAML_MAP || tables->count > 256 || leads->count > 256)
        {
            YamlError("invalid DBCS table count");
        }
        YAML_BUFFER block = {0};
        for(size_t i = 0; i < storage->count; i++)
        {
            YAML_NODE *part = storage->values[i];
            if(part->type != YAML_MAP || part->count != 1)
            {
                YamlError("invalid DBCS storage entry");
            }
            YAML_NODE *padding = YamlFindField(part, "padding");
            if(padding)
            {
                NlsCodecBlobAppend(&block, padding);
            }
            else
            {
                YamlValidateFields(part, "table", "");
                const char *name = YamlGetScalar(YamlGetField(part, "table"));
                if(block.len % 2 || block.len / 2 + 256 > 65535)
                {
                    YamlError("DBCS offset overflow or misalignment");
                }
                YamlAddField(positions, name, YamlCreateInteger(256 + (uint32_t)block.len / 2));
                NlsCodecWriteValues(&block, NlsCodecExpand(YamlGetField(tables, name), 256, 0, 65535), 256, 2);
            }
        }
        if(positions->count != tables->count)
        {
            YamlError("DBCS table missing from storage");
        }
        uint32_t offsets[256] = {0};
        unsigned char used[256] = {0};
        YAML_NODE *references = YamlCreateNode(YAML_MAP);
        for(size_t i = 0; i < leads->count; i++)
        {
            YAML_NODE key = {.type = YAML_SCALAR, .text = leads->keys[i]};
            uint32_t lead = YamlGetHexadecimal(&key, 255);
            if(used[lead])
            {
                YamlError("duplicate normalized lead byte");
            }
            used[lead] = 1;
            const char *name = YamlGetScalar(leads->values[i]);
            offsets[lead] = YamlGetUnsigned(YamlGetField(positions, name), 65535);
            if(!YamlFindField(references, name))
            {
                YamlAddField(references, name, YamlCreateInteger(1));
            }
        }
        if(references->count != tables->count)
        {
            YamlError("unreferenced DBCS table");
        }
        NlsCodecWriteValues(&body, offsets, 256, 2);
        YamlAppendBuffer(&body, block.data, block.len);
    }
    YAML_NODE *padding = YamlFindField(layout, "pre_wide_padding");
    if(padding)
    {
        NlsCodecBlobAppend(&body, padding);
    }
    if(body.len % 2 || 1 + body.len / 2 > 65535)
    {
        YamlError("wide table offset overflow or misalignment");
    }
    WriteUInt16(&out, 1 + (uint32_t)body.len / 2);
    YamlAppendBuffer(&out, body.data, body.len);
    WriteUInt16(&out, YamlGetUnsigned(YamlGetField(layout, "wide_table_flag"), 65535));
    NlsCodecWriteValues(&out, NlsCodecExpand(YamlGetField(root, "from_unicode"), 65536, 1, max), 65536, size);
    YAML_NODE *tail = YamlFindField(layout, "trailing_bytes");
    if(tail)
    {
        NlsCodecBlobAppend(&out, tail);
    }
    return out;
}

static
int
NlsCodecCaseGraph(const uint32_t *t,
                  size_t count,
                  unsigned char *roles,
                  uint32_t *slots)
{
    if(count < 256)
    {
        return 0;
    }
    memset(roles, 1, 256);
    for(unsigned c = 0; c < 65536; c++)
    {
        size_t second = t[c >> 8] + ((c >> 4) & 15);
        if(second >= count)
        {
            return 0;
        }
        if(roles[second] == 2)
        {
            return 0;
        }
        roles[second] = 1;
        size_t third = t[second] + (c & 15);
        if(third >= count || roles[third] == 1)
        {
            return 0;
        }
        roles[third] = 2;
        if(slots)
        {
            slots[c] = (uint32_t)third;
        }
    }
    return 1;
}

static
int
NlsCodecProbeCase(const unsigned char *p,
                  size_t len)
{
    if(len < 6 || len % 2 || ReadUInt16(p) != 1)
    {
        return 0;
    }
    size_t pos = 2;
    for(unsigned i = 0; i < 2; i++)
    {
        if(pos + 2 > len)
        {
            return 0;
        }
        uint32_t size = ReadUInt16(p + pos);
        if(size < 257 || pos + 2 * size > len)
        {
            return 0;
        }
        uint32_t *t = NlsCodecReadValues(p + pos + 2, size - 1, 2);
        unsigned char *roles = YamlAllocateMemory(size - 1);
        if(!NlsCodecCaseGraph(t, size - 1, roles, NULL))
        {
            return 0;
        }
        pos += size * 2;
    }
    return pos == len;
}

static
YAML_NODE *
NlsCodecDecodeCase(const unsigned char *p)
{
    YAML_NODE *root = YamlCreateNode(YAML_MAP), *layout = YamlCreateNode(YAML_MAP);
    YamlAddField(layout, "version", YamlCreateInteger(1));
    size_t pos = 2;
    const char *labels[] = {"uppercase", "lowercase"};
    for(unsigned i = 0; i < 2; i++)
    {
        uint32_t count = ReadUInt16(p + pos) - 1;
        uint32_t *t = NlsCodecReadValues(p + pos + 2, count, 2), *slots = YamlAllocateMemory(65536 * sizeof(*slots));
        unsigned char *roles = YamlAllocateMemory(count);
        if(!NlsCodecCaseGraph(t, count, roles, slots))
        {
            YamlError("invalid case graph");
        }
        YAML_NODE *record = YamlCreateNode(YAML_MAP), *map = YamlCreateNode(YAML_MAP), *meta = YamlCreateNode(YAML_MAP),
                  *indices = YamlCreateNode(YAML_MAP), *unused = YamlCreateNode(YAML_MAP);
        YamlAddField(record, "default", YamlCreateString("identity"));
        YamlAddField(record, "map", map);
        for(unsigned c = 0; c < 65536; c++)
        {
            if(t[slots[c]])
            {
                YamlAddField(map, YamlFormatString("U+%04X", c), YamlCreateCodepoint((c + t[slots[c]]) & 65535));
            }
        }
        for(unsigned j = 0; j < count; j++)
        {
            if(roles[j] == 1)
            {
                YamlAddField(indices, YamlFormatString("%u", j), YamlCreateInteger(t[j]));
            }
            else if(!roles[j])
            {
                YamlAddField(unused, YamlFormatString("%u", j), YamlCreateInteger(t[j]));
            }
        }
        YamlAddField(meta, "word_count", YamlCreateInteger(count));
        YamlAddField(meta, "index_words", indices);
        YamlAddField(meta, "unused_words", unused);
        YamlAddField(root, labels[i], record);
        YamlAddField(layout, labels[i], meta);
        pos += (count + 1) * 2;
    }
    YamlAddField(root, "binary_layout", layout);
    return root;
}

static
YAML_BUFFER
NlsCodecEncodeCase(YAML_NODE *root)
{
    YamlValidateFields(root, "uppercase lowercase binary_layout", "");
    YAML_NODE *layout = YamlGetField(root, "binary_layout");
    YamlValidateFields(layout, "version uppercase lowercase", "");
    if(YamlGetUnsigned(YamlGetField(layout, "version"), 65535) != 1)
    {
        YamlError("unsupported case version");
    }
    YAML_BUFFER out = {0};
    WriteUInt16(&out, 1);
    const char *labels[] = {"uppercase", "lowercase"};
    for(unsigned which = 0; which < 2; which++)
    {
        YAML_NODE *record = YamlGetField(root, labels[which]), *meta = YamlGetField(layout, labels[which]);
        YamlValidateFields(record, "default map", "");
        if(strcmp(YamlGetScalar(YamlGetField(record, "default")), "identity"))
        {
            YamlError("case default must be identity");
        }
        YamlValidateFields(meta, "word_count index_words unused_words", "");
        uint32_t count = YamlGetUnsigned(YamlGetField(meta, "word_count"), 65534);
        if(count < 256)
        {
            YamlError("case table too short");
        }
        uint32_t *table = YamlAllocateMemory(count * sizeof(*table)),
                 *slots = YamlAllocateMemory(65536 * sizeof(*slots));
        unsigned char *assigned = YamlAllocateMemory(count), *roles = YamlAllocateMemory(count);
        const char *groups[] = {"index_words", "unused_words"};
        for(unsigned g = 0; g < 2; g++)
        {
            YAML_NODE *group = YamlGetField(meta, groups[g]);
            if(group->type != YAML_MAP)
            {
                YamlError("invalid case metadata");
            }
            for(size_t i = 0; i < group->count; i++)
            {
                uint32_t idx = YamlParseNumber(group->keys[i], 10, count - 1);
                if(assigned[idx])
                {
                    YamlError("overlapping case metadata");
                }
                assigned[idx] = (unsigned char)(g ? 3 : 1);
                table[idx] = YamlGetUnsigned(group->values[i], 65535);
            }
        }
        if(!NlsCodecCaseGraph(table, count, roles, slots))
        {
            YamlError("invalid case graph");
        }
        for(unsigned i = 0; i < count; i++)
        {
            if(assigned[i] != (roles[i] == 1 ? 1 : roles[i] == 2 ? 0 : 3))
            {
                YamlError("case metadata hides or omits words");
            }
        }
        uint32_t *targets = YamlAllocateMemory(65536 * sizeof(*targets));
        unsigned char *seen = YamlAllocateMemory(65536);
        for(unsigned c = 0; c < 65536; c++)
        {
            targets[c] = c;
        }
        YAML_NODE *map = YamlGetField(record, "map");
        if(map->type != YAML_MAP || map->count > 65536)
        {
            YamlError("invalid case map");
        }
        for(size_t i = 0; i < map->count; i++)
        {
            YAML_NODE key = {.type = YAML_SCALAR, .text = map->keys[i]};
            uint32_t c = YamlGetCodepoint(&key);
            if(seen[c])
            {
                YamlError("duplicate normalized case key");
            }
            seen[c] = 1;
            targets[c] = YamlGetCodepoint(map->values[i]);
        }
        unsigned char *written = YamlAllocateMemory(count);
        for(unsigned c = 0; c < 65536; c++)
        {
            uint32_t slot = slots[c], delta = (targets[c] - c) & 65535;
            if(written[slot] && table[slot] != delta)
            {
                YamlError("case edit conflicts with shared layout at U+%04X", c);
            }
            table[slot] = delta;
            written[slot] = 1;
        }
        WriteUInt16(&out, count + 1);
        NlsCodecWriteValues(&out, table, count, 2);
    }
    return out;
}

static
YAML_NODE *
NlsDecodeFormat(const unsigned char *p,
                size_t len,
                size_t format)
{
    if(codecs[format].codec)
    {
        NlsIO x = {.p = p, .n = len, .reading = 1};
        YAML_NODE *body = YamlCreateNode(YAML_MAP);
        codecs[format].codec(&x, body);
        if(x.pos != len)
        {
            YamlError("trailing data in %s at byte %zu", codecs[format].kind, x.pos);
        }
        return body;
    }
    if(format == NLS_CODEPAGE)
    {
        Codepage cp;
        if(!NlsCodecProbeCp(p, len, &cp))
        {
            YamlError("invalid codepage structure");
        }
        return NlsCodecDecodeCp(p, len, &cp);
    }
    if(format == NLS_CASE_MAP)
    {
        if(!NlsCodecProbeCase(p, len))
        {
            YamlError("invalid case-map structure");
        }
        return NlsCodecDecodeCase(p);
    }
    if(format == NLS_SORTKEY_LEGACY && len == 4 + 65536 * 4)
    {
        YAML_NODE *body = YamlCreateNode(YAML_MAP);
        YamlAddField(body, "header", YamlCreateInteger(ReadUInt32(p)));
        uint32_t *values = NlsCodecReadValues(p + 4, 65536, 4);
        YamlAddField(body, "weights", NlsCodecSparse(values, 65536, 1, 4, NlsCodecMode(values, 65536)));
        return body;
    }
    YamlError("invalid sortkey-legacy size");
    return NULL;
}

static
void
NlsRejectFormat(void)
{
    longjmp(NlsProbeJump, 1);
}

static
int
NlsProbeFormat(const unsigned char *p,
               size_t len,
               size_t format)
{
    /* Cheap header checks avoid allocating trees for unrelated formats. */
    if(len < 4)
    {
        return 0;
    }
    switch(format)
    {
        case NLS_CHARACTER_TYPES:
            if(ReadUInt16(p) != len)
            {
                return 0;
            }
            break;
        case NLS_GEOGRAPHY:
            if(len < 28 || memcmp(p, "g\0e\0o\0\0", 8))
            {
                return 0;
            }
            break;
        case NLS_CASE_EXCEPTIONS:
            if(ReadUInt32(p) > 10000 || ReadUInt32(p) > (len - 4) / 16)
            {
                return 0;
            }
            break;
        case NLS_UNICODE_MAPPINGS:
            if(ReadUInt16(p) < 257 || (size_t)ReadUInt16(p) * 2 > len)
            {
                return 0;
            }
            break;
        case NLS_SORT_TABLES_LEGACY:
            if(ReadUInt32(p) > 65535 || ReadUInt32(p) > (len - 4) / 8)
            {
                return 0;
            }
            break;
        case NLS_LOCALE_DATABASE:
            if(len < 32 || ReadUInt32(p) != 32)
            {
                return 0;
            }
            break;
        case NLS_CODEPAGE:
            if(ReadUInt16(p) != 13)
            {
                return 0;
            }
            break;
        case NLS_CASE_MAP:
            if(ReadUInt16(p) != 1)
            {
                return 0;
            }
            break;
        case NLS_SORTKEY_LEGACY:
            /* This legacy raw table has no identifying signature. */
            return len == 4 + 65536 * 4;
    }

    YAML_ALLOCATION *checkpoint = YamlAllocations;
    void (*previous_handler)(void) = YamlValidationError;
    if(setjmp(NlsProbeJump))
    {
        YamlValidationError = previous_handler;
        YamlReleaseAllocations(checkpoint);
        return 0;
    }
    YamlValidationError = NlsRejectFormat;
    NlsDecodeFormat(p, len, format);
    YamlValidationError = previous_handler;
    YamlReleaseAllocations(checkpoint);
    return 1;
}

static
YAML_NODE *
NlsDecode(const unsigned char *p,
          size_t len,
          const char *type)
{
    size_t count = NLS_FORMAT_COUNT;
    size_t selected = count;
    if(type)
    {
        for(size_t i = 0; i < count; i++)
        {
            if(!strcmp(type, codecs[i].kind))
            {
                selected = i;
                break;
            }
        }
        if(selected == count)
        {
            YamlError("unsupported NLS type: %s", type);
        }
    }
    else
    {
        for(size_t i = 0; i < count; i++)
        {
            if(NlsProbeFormat(p, len, i))
            {
                if(selected != count)
                {
                    YamlError("ambiguous NLS format (%s or %s); use --type KIND",
                              codecs[selected].kind, codecs[i].kind);
                }
                selected = i;
            }
        }
        if(selected == count)
        {
            YamlError("unsupported NLS format or corrupt structure (no opaque fallback)");
        }
    }
    YAML_NODE *body = NlsDecodeFormat(p, len, selected);
    YAML_NODE *root = YamlCreateNode(YAML_MAP);
    YamlAddField(root, "format", YamlCreateString("xtnlsc"));
    YamlAddField(root, "version", YamlCreateInteger(2));
    YamlAddField(root, "kind", YamlCreateString(codecs[selected].kind));
    for(size_t i = 0; i < body->count; i++)
    {
        YamlAddField(root, body->keys[i], body->values[i]);
    }
    return root;
}

static
YAML_BUFFER
NlsEncode(YAML_NODE *root)
{
    if(strcmp(YamlGetScalar(YamlGetField(root, "format")), "xtnlsc") ||
       YamlGetUnsigned(YamlGetField(root, "version"), 65535) != 2)
    {
        YamlError("expected xtnlsc YAML version 2");
    }
    const char *kind = YamlGetScalar(YamlGetField(root, "kind"));
    YAML_NODE *body = YamlCreateNode(YAML_MAP);
    for(size_t i = 0; i < root->count; i++)
    {
        if(strcmp(root->keys[i], "format") && strcmp(root->keys[i], "version") && strcmp(root->keys[i], "kind"))
        {
            YamlAddField(body, root->keys[i], root->values[i]);
        }
    }
    if(!strcmp(kind, "codepage"))
    {
        return NlsCodecEncodeCp(body);
    }
    if(!strcmp(kind, "case-map"))
    {
        return NlsCodecEncodeCase(body);
    }
    YAML_BUFFER out = {0};
    if(NlsEncodeExtra(kind, body, &out))
    {
        return out;
    }
    if(!strcmp(kind, "sortkey-legacy"))
    {
        YamlValidateFields(body, "header weights", "");
        WriteUInt32(&out, YamlGetUnsigned(YamlGetField(body, "header"), UINT32_MAX));
        NlsCodecWriteValues(&out, NlsCodecExpand(YamlGetField(body, "weights"), 65536, 1, UINT32_MAX), 65536, 4);
    }
    else if(!strcmp(kind, "opaque"))
    {
        YamlValidateFields(body, "data", "");
        NlsCodecBlobAppend(&out, YamlGetField(body, "data"));
    }
    else
    {
        YamlError("unsupported YAML kind: %s", kind);
    }
    return out;
}

static
char *
NlsLegacyTrim(char *s)
{
    while(isspace((unsigned char)*s))
    {
        s++;
    }
    size_t n = strlen(s);
    while(n && isspace((unsigned char)s[n - 1]))
    {
        s[--n] = 0;
    }
    return s;
}

static
YAML_BUFFER
NlsEncodeV1(const unsigned char *data,
            size_t len)
{
    char *text = YamlAllocateMemory(len + 1);
    memcpy(text, data, len);
    if(strlen(text) != len)
    {
        YamlError("NUL in legacy text");
    }
    YAML_BUFFER out = {0};
    YAML_NODE *names = YamlCreateNode(YAML_MAP);
    unsigned header = 0, width = 0;
    uint32_t size = 0, count = 0, used = 0;
    size_t line = 0;
    char *cursor = text;
    while(*cursor)
    {
        char *next = strchr(cursor, '\n');
        if(next)
        {
            *next++ = 0;
        }
        char *comment = strchr(cursor, '#');
        if(comment)
        {
            *comment = 0;
        }
        char *s = NlsLegacyTrim(cursor);
        line++;
        if(*s)
        {
            if(header < 3)
            {
                if(!header && strcmp(s, "XTNLS 1"))
                {
                    YamlError("legacy line %zu: expected XTNLS 1", line);
                }
                if(header == 1 && strcmp(s, "KIND codepage") && strcmp(s, "KIND case-map") &&
                   strcmp(s, "KIND sortkey-legacy") && strcmp(s, "KIND opaque"))
                {
                    YamlError("invalid legacy KIND");
                }
                if(header == 2)
                {
                    if(strncmp(s, "SIZE ", 5))
                    {
                        YamlError("missing legacy SIZE");
                    }
                    size = YamlParseNumber(s + 5, 10, YAML_MAX_BINARY);
                }
                header++;
            }
            else if(*s == '[')
            {
                if(used != count)
                {
                    YamlError("incomplete legacy section");
                }
                char name[128], offset[17], type[4], items[11];
                int end = 0;
                if(sscanf(s,
                          "[%127[A-Za-z0-9_] offset=0x%16[0-9a-fA-F] type=%3s count=%10[0-9]]%n",
                          name,
                          offset,
                          type,
                          items,
                          &end) != 4 ||
                   !end || s[end] || !isalpha((unsigned char)name[0]))
                {
                    YamlError("invalid legacy section at line %zu", line);
                }
                if(YamlParseNumber(offset, 16, YAML_MAX_BINARY) != out.len)
                {
                    YamlError("legacy section gap or overlap");
                }
                width = !strcmp(type, "u8") ? 1 : !strcmp(type, "u16") ? 2 : !strcmp(type, "u32") ? 4 : 0;
                count = YamlParseNumber(items, 10, YAML_MAX_BINARY);
                used = 0;
                if(!width || !count || (uint64_t)count * width + out.len > size)
                {
                    YamlError("invalid legacy section size");
                }
                YamlAddField(names, name, YamlCreateInteger(1));
            }
            else
            {
                if(!width)
                {
                    YamlError("legacy data outside a section");
                }
                char *colon = strchr(s, ':');
                if(!colon)
                {
                    YamlError("invalid legacy data row");
                }
                *colon = 0;
                if(YamlParseNumber(NlsLegacyTrim(s), 16, YAML_MAX_BINARY) != used)
                {
                    YamlError("noncontiguous legacy index");
                }
                char *payload = NlsLegacyTrim(colon + 1), *star = strchr(payload, '*');
                uint32_t repeat = 1;
                if(star)
                {
                    *star = 0;
                    repeat = YamlParseNumber(NlsLegacyTrim(star + 1), 10, YAML_MAX_BINARY);
                    if(!repeat)
                    {
                        YamlError("zero repeat");
                    }
                }
                payload = NlsLegacyTrim(payload);
                unsigned tokens = 0;
                while(*payload)
                {
                    char *stop = payload;
                    while(*stop && !isspace((unsigned char)*stop))
                    {
                        stop++;
                    }
                    char save = *stop;
                    *stop = 0;
                    uint32_t value = YamlParseNumber(payload, 16, width == 1 ? 255 : width == 2 ? 65535 : UINT32_MAX);
                    tokens++;
                    if((star && tokens > 1) || repeat > count - used)
                    {
                        YamlError("legacy row exceeds section");
                    }
                    for(uint32_t j = 0; j < repeat; j++)
                    {
                        if(width == 1)
                        {
                            unsigned char c = (unsigned char)value;
                            YamlAppendBuffer(&out, &c, 1);
                        }
                        else if(width == 2)
                        {
                            WriteUInt16(&out, value);
                        }
                        else
                        {
                            WriteUInt32(&out, value);
                        }
                    }
                    used += repeat;
                    if(!save)
                    {
                        break;
                    }
                    payload = NlsLegacyTrim(stop + 1);
                }
                if(!tokens)
                {
                    YamlError("empty legacy row");
                }
            }
        }
        if(!next)
        {
            break;
        }
        cursor = next;
    }
    if(header != 3 || used != count || out.len != size)
    {
        YamlError("incomplete legacy document");
    }
    return out;
}

static
void
NlsMainCleanFile(void)
{
    if(output_stream)
    {
        fclose(output_stream);
    }
    if(temporary)
    {
        remove(temporary);
    }
}

static
unsigned char *
NlsMainReadFile(const char *path,
                size_t limit,
                size_t *length)
{
    FILE *f = fopen(path, "rb");
    if(!f)
    {
        YamlError("cannot open %s: %s", path, strerror(errno));
    }
    /* Read incrementally: pipes and files changed during a read are bounded too. */
    size_t cap = 65536, len = 0;
    unsigned char *data = YamlAllocateMemory(cap + 1);
    for(;;)
    {
        if(len == cap)
        {
            size_t newcap = cap < limit / 2 ? cap * 2 : limit + 1;
            if(cap >= limit + 1)
            {
                fclose(f);
                YamlError("input exceeds size limit");
            }
            unsigned char *next = YamlAllocateMemory(newcap + 1);
            memcpy(next, data, len);
            data = next;
            cap = newcap;
        }
        size_t n = fread(data + len, 1, cap - len, f);
        len += n;
        if(len > limit)
        {
            fclose(f);
            YamlError("input exceeds size limit");
        }
        if(!n)
        {
            if(ferror(f))
            {
                fclose(f);
                YamlError("cannot read %s", path);
            }
            break;
        }
    }
    if(fclose(f))
    {
        YamlError("cannot close input");
    }
    data[len] = 0;
    *length = len;
    return data;
}

static
void
NlsMainUsage(FILE *f)
{
    fputs("xtnlsc " XTNLSC_VERSION " - lossless NLS / YAML converter\n"
          "Usage: xtnlsc (-d|--decode|-e|--encode) -i INPUT -o OUTPUT [--type KIND]\n"
          "  -d, --decode       NLS to YAML\n"
          "  -e, --encode       YAML v2 or legacy XTNLS v1 to NLS\n"
          "  -i, --input PATH   Input file\n"
          "  -o, --output PATH  Output file\n"
          "  -t, --type KIND    Decode as KIND when content is ambiguous\n"
          "                    Normally detected from content, regardless of filename\n"
          "                    Kinds: character-types, geography, case-exceptions,\n"
          "                    unicode-mappings, sort-tables-legacy, locale-database,\n"
          "                    codepage, case-map, sortkey-legacy\n"
          "  -h, --help         Show help\n"
          "      --version      Show version\n",
          f);
}

static
int
NlsMainArgumentError(const char *message)
{
    fprintf(stderr, "xtnlsc: %s\n", message);
    NlsMainUsage(stderr);
    return 2;
}

int
main(int argc,
     char **argv)
{
    YamlProgramName = "xtnlsc";
    YamlCharacterName = NlsGetCharacterName;
    atexit(YamlCleanup);
    atexit(NlsMainCleanFile);
    const char *input = NULL, *output = NULL, *type = NULL;
    int mode = 0;
    for(int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if(!strcmp(arg, "--version"))
        {
            puts("xtnlsc " XTNLSC_VERSION);
            return 0;
        }
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            NlsMainUsage(stdout);
            return 0;
        }
        if(!strcmp(arg, "-d") || !strcmp(arg, "--decode") || !strcmp(arg, "-e") || !strcmp(arg, "--encode"))
        {
            if(mode)
            {
                return NlsMainArgumentError("select exactly one mode");
            }
            mode = !strcmp(arg, "-d") || !strcmp(arg, "--decode") ? 1 : 2;
        }
        else if(!strcmp(arg, "-t") || !strcmp(arg, "--type"))
        {
            if(i + 1 >= argc || type)
            {
                return NlsMainArgumentError("missing or duplicate NLS type");
            }
            type = argv[++i];
        }
        else if(!strcmp(arg, "-i") || !strcmp(arg, "--input") || !strcmp(arg, "-o") || !strcmp(arg, "--output"))
        {
            if(i + 1 >= argc)
            {
                return NlsMainArgumentError("missing filename");
            }
            const char **slot = !strcmp(arg, "-i") || !strcmp(arg, "--input") ? &input : &output;
            if(*slot)
            {
                return NlsMainArgumentError("duplicate input/output argument");
            }
            *slot = argv[++i];
        }
        else
        {
            return NlsMainArgumentError("unknown argument");
        }
    }
    if(!mode || !input || !output)
    {
        return NlsMainArgumentError("mode, input and output are required");
    }
    if(type && mode != 1)
    {
        return NlsMainArgumentError("--type is only valid in decode mode");
    }
    struct stat a, b;
    if(!strcmp(input, output) ||
       (!stat(input, &a) && !stat(output, &b) && a.st_ino && a.st_dev == b.st_dev && a.st_ino == b.st_ino))
    {
        YamlError("input and output must be different files");
    }
    size_t length;
    unsigned char *data = NlsMainReadFile(input, mode == 1 ? YAML_MAX_BINARY : YAML_MAX_TEXT, &length);
    YAML_NODE *root = NULL;
    YAML_BUFFER result = {0};
    if(mode == 1)
    {
        root = NlsDecode(data, length, type);
    }
    else
    {
        unsigned char *start = data;
        size_t remaining = length;
        if(remaining >= 3 && !memcmp(start, "\xEF\xBB\xBF", 3))
        {
            start += 3;
            remaining -= 3;
        }
        while(remaining && (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t'))
        {
            start++;
            remaining--;
        }
        result = remaining >= 7 && !memcmp(start, "XTNLS 1", 7) ? NlsEncodeV1(start, remaining)
                                                                : NlsEncode(YamlReadDocument(start, remaining));
    }
    for(unsigned attempt = 0; attempt < 100; attempt++)
    {
        char *candidate = YamlFormatString("%s.xtnlsc-%ld-%u.tmp", output, (long)XTCHAIN_PROCESS_ID(), attempt);
        output_stream = fopen(candidate, "wbx");
        if(output_stream)
        {
            temporary = candidate;
            break;
        }
        if(errno != EEXIST)
        {
            YamlError("cannot create output temporary file: %s", strerror(errno));
        }
    }
    if(!output_stream)
    {
        YamlError("cannot allocate temporary output filename");
    }
    if(mode == 1)
    {
        YamlWriteDocument(output_stream, root, "xtnlsc " XTNLSC_VERSION);
    }
    else if(result.len && fwrite(result.data, 1, result.len, output_stream) != result.len)
    {
        YamlError("cannot write binary output");
    }
    if(fflush(output_stream))
    {
        YamlError("cannot flush output");
    }
#ifdef _WIN32
    if(_commit(_fileno(output_stream)))
    {
        YamlError("cannot sync output");
    }
#else
    if(fsync(fileno(output_stream)))
    {
        YamlError("cannot sync output");
    }
#endif
    FILE *finished = output_stream;
    output_stream = NULL;
    if(fclose(finished))
    {
        YamlError("cannot close output");
    }
#ifdef _WIN32
    if(!MoveFileExA(temporary, output, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        YamlError("cannot replace output file");
    }
#else
    if(rename(temporary, output))
    {
        YamlError("cannot replace output: %s", strerror(errno));
    }
#endif
    temporary = NULL;
    fprintf(stderr, "xtnlsc: %s -> %s\n", mode == 1 ? YamlGetScalar(YamlGetField(root, "kind")) : "binary", output);
    return 0;
}
