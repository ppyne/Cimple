#include "regex_engine.h"
#include "../common/error.h"
#include "../common/memory.h"
#include "utf8proc.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define REGEX_MAX_QUANT 10000
#define REGEX_MAX_DEPTH 256
#define REGEX_INF (-1)

typedef enum {
    RX_EMPTY,
    RX_LITERAL,
    RX_DOT,
    RX_BOL,
    RX_EOL,
    RX_CLASS,
    RX_SEQ,
    RX_ALT,
    RX_REPEAT,
    RX_GROUP
} RegexNodeKind;

typedef enum {
    CLS_LITERAL,
    CLS_RANGE,
    CLS_DIGIT,
    CLS_NOT_DIGIT,
    CLS_WORD,
    CLS_NOT_WORD,
    CLS_SPACE,
    CLS_NOT_SPACE
} ClassItemKind;

typedef struct {
    ClassItemKind kind;
    char *a;
    char *b;
} ClassItem;

typedef struct RegexNode RegexNode;
struct RegexNode {
    RegexNodeKind kind;
    union {
        struct { char *glyph; } lit;
        struct {
            int negated;
            ClassItem *items;
            int count;
            int cap;
        } cls;
        struct {
            RegexNode **items;
            int count;
            int cap;
        } list;
        struct {
            RegexNode *left;
            RegexNode *right;
        } alt;
        struct {
            RegexNode *child;
            int min;
            int max;
            int greedy;
        } rep;
        struct {
            int index;
            RegexNode *child;
        } group;
    } u;
};

typedef struct {
    const char *pattern;
    const char *cur;
    int line;
    int col;
    int depth;
    int group_count;
} RegexParser;

typedef struct {
    int start;
    int end;
    int set;
} Capture;

typedef struct {
    char *normalized;
    char **glyphs;
    int *offsets;
    int count;
} GlyphString;

typedef struct {
    RegexNode *root;
} RegexProgram;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} RegexSb;

static void regex_syntax_error(int line, int col, const char *msg) {
    error_runtime(line, col, "RegExpSyntax: %s", msg);
}

static void regex_limit_error(int line, int col, const char *msg) {
    error_runtime(line, col, "RegExpLimit: %s", msg);
}

static void regex_range_error(int line, int col, const char *msg) {
    error_runtime(line, col, "RegExpRange: %s", msg);
}

static char *nfc_normalize(const char *s) {
    uint8_t *out = NULL;
    utf8proc_map((const utf8proc_uint8_t *)s, 0, &out,
                 UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_COMPOSE);
    if (out) return (char *)out;
    return cimple_strdup(s ? s : "");
}

int regex_glyph_len(const char *s) {
    GlyphString gs;
    gs.normalized = nfc_normalize(s ? s : "");
    utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(gs.normalized);
    utf8proc_ssize_t pos = 0;
    int count = 0;
    while (pos < len) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t r = utf8proc_iterate((const utf8proc_uint8_t *)gs.normalized + pos,
                                              len - pos, &cp);
        if (r <= 0) break;
        pos += r;
        count++;
    }
    free(gs.normalized);
    return count;
}

static GlyphString glyph_string_make(const char *s) {
    GlyphString gs;
    gs.normalized = nfc_normalize(s ? s : "");
    gs.glyphs = NULL;
    gs.offsets = NULL;
    gs.count = 0;
    utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(gs.normalized);
    utf8proc_ssize_t pos = 0;
    int cap = 0;
    while (pos < len) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t r = utf8proc_iterate((const utf8proc_uint8_t *)gs.normalized + pos,
                                              len - pos, &cp);
        if (r <= 0) break;
        if (gs.count == cap) {
            cap = cap ? cap * 2 : 8;
            gs.glyphs = (char **)cimple_realloc(gs.glyphs, (size_t)cap * sizeof(char *));
            gs.offsets = (int *)cimple_realloc(gs.offsets, (size_t)(cap + 1) * sizeof(int));
        }
        gs.offsets[gs.count] = (int)pos;
        gs.glyphs[gs.count] = cimple_strndup(gs.normalized + pos, (size_t)r);
        pos += r;
        gs.count++;
    }
    gs.offsets = (int *)cimple_realloc(gs.offsets, (size_t)(gs.count + 1) * sizeof(int));
    gs.offsets[gs.count] = (int)strlen(gs.normalized);
    return gs;
}

static void glyph_string_free(GlyphString *gs) {
    if (!gs) return;
    for (int i = 0; i < gs->count; i++) free(gs->glyphs[i]);
    free(gs->glyphs);
    free(gs->offsets);
    free(gs->normalized);
    gs->glyphs = NULL;
    gs->offsets = NULL;
    gs->normalized = NULL;
    gs->count = 0;
}

static char *glyph_substring(const GlyphString *gs, int start, int end) {
    if (start < 0) start = 0;
    if (end < start) end = start;
    if (end > gs->count) end = gs->count;
    return cimple_strndup(gs->normalized + gs->offsets[start],
                          (size_t)(gs->offsets[end] - gs->offsets[start]));
}

static RegexNode *node_new(RegexNodeKind kind) {
    RegexNode *n = ALLOC(RegexNode);
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    return n;
}

/* node_empty is kept for future use (empty alternation branch) */
static RegexNode *node_empty(void) __attribute__((unused));
static RegexNode *node_empty(void) { return node_new(RX_EMPTY); }

static RegexNode *node_literal(char *glyph) {
    RegexNode *n = node_new(RX_LITERAL);
    n->u.lit.glyph = glyph;
    return n;
}

static RegexNode *node_dot(void) { return node_new(RX_DOT); }
static RegexNode *node_bol(void) { return node_new(RX_BOL); }
static RegexNode *node_eol(void) { return node_new(RX_EOL); }

static RegexNode *node_class(int negated) {
    RegexNode *n = node_new(RX_CLASS);
    n->u.cls.negated = negated;
    return n;
}

static void class_add(RegexNode *cls, ClassItem item) {
    if (cls->u.cls.count == cls->u.cls.cap) {
        cls->u.cls.cap = cls->u.cls.cap ? cls->u.cls.cap * 2 : 4;
        cls->u.cls.items = (ClassItem *)cimple_realloc(cls->u.cls.items,
                            (size_t)cls->u.cls.cap * sizeof(ClassItem));
    }
    cls->u.cls.items[cls->u.cls.count++] = item;
}

static RegexNode *node_seq(void) { return node_new(RX_SEQ); }
static RegexNode *node_alt(RegexNode *l, RegexNode *r) {
    RegexNode *n = node_new(RX_ALT);
    n->u.alt.left = l;
    n->u.alt.right = r;
    return n;
}
static RegexNode *node_repeat(RegexNode *child, int min, int max, int greedy) {
    RegexNode *n = node_new(RX_REPEAT);
    n->u.rep.child = child;
    n->u.rep.min = min;
    n->u.rep.max = max;
    n->u.rep.greedy = greedy;
    return n;
}
static RegexNode *node_group(int index, RegexNode *child) {
    RegexNode *n = node_new(RX_GROUP);
    n->u.group.index = index;
    n->u.group.child = child;
    return n;
}

static void seq_add(RegexNode *seq, RegexNode *child) {
    if (!child) return;
    if (seq->u.list.count == seq->u.list.cap) {
        seq->u.list.cap = seq->u.list.cap ? seq->u.list.cap * 2 : 4;
        seq->u.list.items = (RegexNode **)cimple_realloc(seq->u.list.items,
                             (size_t)seq->u.list.cap * sizeof(RegexNode *));
    }
    seq->u.list.items[seq->u.list.count++] = child;
}

static void regex_node_free(RegexNode *n) {
    if (!n) return;
    switch (n->kind) {
    case RX_LITERAL:
        free(n->u.lit.glyph);
        break;
    case RX_CLASS:
        for (int i = 0; i < n->u.cls.count; i++) {
            free(n->u.cls.items[i].a);
            free(n->u.cls.items[i].b);
        }
        free(n->u.cls.items);
        break;
    case RX_SEQ:
        for (int i = 0; i < n->u.list.count; i++) regex_node_free(n->u.list.items[i]);
        free(n->u.list.items);
        break;
    case RX_ALT:
        regex_node_free(n->u.alt.left);
        regex_node_free(n->u.alt.right);
        break;
    case RX_REPEAT:
        regex_node_free(n->u.rep.child);
        break;
    case RX_GROUP:
        regex_node_free(n->u.group.child);
        break;
    default:
        break;
    }
    free(n);
}

static RegexNode *parse_expr(RegexParser *p);

static int peek_ch(RegexParser *p) { return (unsigned char)*p->cur; }
static int consume_ch(RegexParser *p) { return (unsigned char)*p->cur++; }
static int at_end(RegexParser *p) { return *p->cur == '\0'; }

static char *parse_raw_glyph(RegexParser *p) {
    utf8proc_int32_t cp;
    utf8proc_ssize_t r = utf8proc_iterate((const utf8proc_uint8_t *)p->cur,
                                          (utf8proc_ssize_t)strlen(p->cur), &cp);
    if (r <= 0) regex_syntax_error(p->line, p->col, "invalid UTF-8 sequence");
    char *out = cimple_strndup(p->cur, (size_t)r);
    p->cur += r;
    return out;
}

static char *parse_escape_literal(RegexParser *p, int in_class, int *special_kind) {
    if (at_end(p)) regex_syntax_error(p->line, p->col, "trailing escape");
    int c = consume_ch(p);
    if (isdigit(c)) regex_syntax_error(p->line, p->col, "backreferences are not supported");
    *special_kind = 0;
    switch (c) {
    case 'd': *special_kind = CLS_DIGIT; return NULL;
    case 'D': *special_kind = CLS_NOT_DIGIT; return NULL;
    case 'w': *special_kind = CLS_WORD; return NULL;
    case 'W': *special_kind = CLS_NOT_WORD; return NULL;
    case 's': *special_kind = CLS_SPACE; return NULL;
    case 'S': *special_kind = CLS_NOT_SPACE; return NULL;
    default: {
        char buf[2] = { (char)c, '\0' };
        (void)in_class;
        return cimple_strdup(buf);
    }
    }
}

static RegexNode *parse_class(RegexParser *p) {
    consume_ch(p); /* [ */
    int negated = 0;
    if (peek_ch(p) == '^') { consume_ch(p); negated = 1; }
    RegexNode *cls = node_class(negated);
    int first = 1;
    while (!at_end(p) && peek_ch(p) != ']') {
        char *a = NULL, *b = NULL;
        int special = 0;
        if (peek_ch(p) == '\\') {
            consume_ch(p);
            a = parse_escape_literal(p, 1, &special);
        } else {
            a = parse_raw_glyph(p);
        }
        if (special) {
            ClassItem item = { (ClassItemKind)special, NULL, NULL };
            class_add(cls, item);
            first = 0;
            continue;
        }
        if (!first && strcmp(a, "-") == 0 && peek_ch(p) == ']') {
            ClassItem item = { CLS_LITERAL, a, NULL };
            class_add(cls, item);
            break;
        }
        if (peek_ch(p) == '-' && p->cur[1] && p->cur[1] != ']') {
            consume_ch(p);
            int special2 = 0;
            if (peek_ch(p) == '\\') {
                consume_ch(p);
                b = parse_escape_literal(p, 1, &special2);
                if (special2) regex_syntax_error(p->line, p->col, "invalid class range");
            } else {
                b = parse_raw_glyph(p);
            }
            ClassItem item = { CLS_RANGE, a, b };
            class_add(cls, item);
        } else {
            ClassItem item = { CLS_LITERAL, a, NULL };
            class_add(cls, item);
        }
        first = 0;
    }
    if (at_end(p) || peek_ch(p) != ']') regex_syntax_error(p->line, p->col, "unterminated character class");
    consume_ch(p);
    return cls;
}

static RegexNode *parse_atom(RegexParser *p) {
    if (p->depth++ > REGEX_MAX_DEPTH) regex_limit_error(p->line, p->col, "compilation depth exceeded");
    RegexNode *n = NULL;
    if (at_end(p)) regex_syntax_error(p->line, p->col, "unexpected end of pattern");
    if (peek_ch(p) == '(') {
        consume_ch(p);
        if (peek_ch(p) == '?') {
            consume_ch(p);
            if (peek_ch(p) == ':') {
                consume_ch(p);
                n = parse_expr(p);
            } else {
                regex_syntax_error(p->line, p->col, "lookaround is not supported");
            }
        } else {
            int idx = ++p->group_count;
            n = node_group(idx, parse_expr(p));
        }
        if (peek_ch(p) != ')') regex_syntax_error(p->line, p->col, "missing ')' ");
        consume_ch(p);
        p->depth--;
        return n;
    }
    switch (peek_ch(p)) {
    case '.': consume_ch(p); n = node_dot(); break;
    case '^': consume_ch(p); n = node_bol(); break;
    case '$': consume_ch(p); n = node_eol(); break;
    case '[': n = parse_class(p); break;
    case '\\': {
        consume_ch(p);
        int special = 0;
        char *lit = parse_escape_literal(p, 0, &special);
        if (special) {
            RegexNode *cls = node_class(0);
            ClassItem item = { (ClassItemKind)special, NULL, NULL };
            class_add(cls, item);
            n = cls;
        } else {
            n = node_literal(lit);
        }
        break;
    }
    default:
        n = node_literal(parse_raw_glyph(p));
        break;
    }
    p->depth--;
    return n;
}

static int parse_decimal(RegexParser *p) {
    if (!isdigit(peek_ch(p))) regex_syntax_error(p->line, p->col, "expected decimal number in quantifier");
    int v = 0;
    while (isdigit(peek_ch(p))) {
        int d = consume_ch(p) - '0';
        if (v > REGEX_MAX_QUANT / 10) regex_limit_error(p->line, p->col, "quantifier too large");
        v = v * 10 + d;
        if (v > REGEX_MAX_QUANT) regex_limit_error(p->line, p->col, "quantifier too large");
    }
    return v;
}

static RegexNode *parse_repeat(RegexParser *p) {
    RegexNode *atom = parse_atom(p);
    if (at_end(p)) return atom;
    int min = -1, max = -1, greedy = 1;
    int c = peek_ch(p);
    if (c == '*') { consume_ch(p); min = 0; max = REGEX_INF; }
    else if (c == '+') { consume_ch(p); min = 1; max = REGEX_INF; }
    else if (c == '?') { consume_ch(p); min = 0; max = 1; }
    else if (c == '{') {
        consume_ch(p);
        min = parse_decimal(p);
        if (peek_ch(p) == '}') {
            consume_ch(p);
            max = min;
        } else if (peek_ch(p) == ',') {
            consume_ch(p);
            if (peek_ch(p) == '}') {
                consume_ch(p);
                max = REGEX_INF;
            } else {
                max = parse_decimal(p);
                if (peek_ch(p) != '}') regex_syntax_error(p->line, p->col, "unterminated quantifier");
                consume_ch(p);
            }
        } else {
            regex_syntax_error(p->line, p->col, "invalid quantifier");
        }
        if (max != REGEX_INF && min > max) regex_limit_error(p->line, p->col, "invalid quantifier range");
    }
    if (min < 0) return atom;
    if (peek_ch(p) == '?') { consume_ch(p); greedy = 0; }
    return node_repeat(atom, min, max, greedy);
}

static int concat_stops_at(int c) {
    return c == '\0' || c == ')' || c == '|';
}

static RegexNode *parse_concat(RegexParser *p) {
    RegexNode *seq = node_seq();
    while (!concat_stops_at(peek_ch(p)))
        seq_add(seq, parse_repeat(p));
    if (seq->u.list.count == 0) return seq;
    if (seq->u.list.count == 1) {
        RegexNode *only = seq->u.list.items[0];
        free(seq->u.list.items);
        free(seq);
        return only;
    }
    return seq;
}

static RegexNode *parse_expr(RegexParser *p) {
    RegexNode *left = parse_concat(p);
    while (peek_ch(p) == '|') {
        consume_ch(p);
        RegexNode *right = parse_concat(p);
        left = node_alt(left, right);
    }
    return left;
}

static char *canonical_flags(const char *flags, int *fi, int *fm, int *fs, int line, int col) {
    *fi = *fm = *fs = 0;
    for (const char *p = flags ? flags : ""; *p; p++) {
        if (*p == 'i') *fi = 1;
        else if (*p == 'm') *fm = 1;
        else if (*p == 's') *fs = 1;
        else regex_syntax_error(line, col, "unknown flag");
    }
    char buf[4];
    int k = 0;
    if (*fi) buf[k++] = 'i';
    if (*fm) buf[k++] = 'm';
    if (*fs) buf[k++] = 's';
    buf[k] = '\0';
    return cimple_strdup(buf);
}

static int glyph_eq_ascii_fold(const char *a, const char *b, int flag_i) {
    if (!flag_i) return strcmp(a, b) == 0;
    if (strlen(a) == 1 && strlen(b) == 1) {
        unsigned char ca = (unsigned char)a[0];
        unsigned char cb = (unsigned char)b[0];
        if (ca < 128 && cb < 128) return tolower(ca) == tolower(cb);
    }
    return strcmp(a, b) == 0;
}

static int glyph_is_ascii_single(const char *g, char *out) {
    if (strlen(g) == 1 && (unsigned char)g[0] < 128) { if (out) *out = g[0]; return 1; }
    return 0;
}

static int glyph_matches_shortcut(const char *g, ClassItemKind kind) {
    char c;
    int ascii = glyph_is_ascii_single(g, &c);
    int is_digit = ascii && c >= '0' && c <= '9';
    int is_word = ascii && (isalnum((unsigned char)c) || c == '_');
    int is_space = strcmp(g, " ") == 0 || strcmp(g, "\t") == 0 || strcmp(g, "\n") == 0 || strcmp(g, "\r") == 0 || strcmp(g, "\f") == 0 || strcmp(g, "\v") == 0;
    switch (kind) {
    case CLS_DIGIT: return is_digit;
    case CLS_NOT_DIGIT: return !is_digit;
    case CLS_WORD: return is_word;
    case CLS_NOT_WORD: return !is_word;
    case CLS_SPACE: return is_space;
    case CLS_NOT_SPACE: return !is_space;
    default: return 0;
    }
}

static int class_matches(const RegexNode *cls, const char *glyph, const RegExpVal *re) {
    int matched = 0;
    for (int i = 0; i < cls->u.cls.count; i++) {
        ClassItem *it = &cls->u.cls.items[i];
        if (it->kind == CLS_LITERAL) {
            if (glyph_eq_ascii_fold(glyph, it->a, re->flag_i)) { matched = 1; break; }
        } else if (it->kind == CLS_RANGE) {
            char ga, gb, gg;
            if (glyph_is_ascii_single(it->a, &ga) && glyph_is_ascii_single(it->b, &gb) && glyph_is_ascii_single(glyph, &gg)) {
                if (re->flag_i) {
                    ga = (char)tolower((unsigned char)ga);
                    gb = (char)tolower((unsigned char)gb);
                    gg = (char)tolower((unsigned char)gg);
                }
                if (gg >= ga && gg <= gb) { matched = 1; break; }
            }
        } else if (glyph_matches_shortcut(glyph, it->kind)) {
            matched = 1; break;
        }
    }
    return cls->u.cls.negated ? !matched : matched;
}

static int is_line_break(const char *g) { return strcmp(g, "\n") == 0; }

typedef struct {
    Capture *caps;
    int count;
} CapState;

static Capture *caps_clone(const Capture *src, int count) {
    Capture *out = count ? ALLOC_N(Capture, count) : NULL;
    if (count) memcpy(out, src, (size_t)count * sizeof(Capture));
    return out;
}

static int match_node(const RegexNode *node, const GlyphString *gs, const RegExpVal *re,
                      int pos, Capture *caps, int cap_count, int *out_pos);

static int match_seq_items(const RegexNode *seq, int idx, const GlyphString *gs,
                           const RegExpVal *re, int pos, Capture *caps,
                           int cap_count, int *out_pos);

static int match_repeat_grow(const RegexNode *rep, const RegexNode *seq, int next_idx,
                             const GlyphString *gs, const RegExpVal *re,
                             int pos, Capture *caps, int cap_count,
                             int reps_done, int *out_pos) {
    int max = rep->u.rep.max;
    int greedy = rep->u.rep.greedy;
    if (!greedy && reps_done >= rep->u.rep.min) {
        Capture *probe = caps_clone(caps, cap_count);
        int probe_pos = 0;
        if (match_seq_items(seq, next_idx, gs, re, pos, probe, cap_count, &probe_pos)) {
            memcpy(caps, probe, (size_t)cap_count * sizeof(Capture));
            free(probe);
            *out_pos = probe_pos;
            return 1;
        }
        free(probe);
    }
    if (max != REGEX_INF && reps_done >= max) {
        if (reps_done >= rep->u.rep.min)
            return match_seq_items(seq, next_idx, gs, re, pos, caps, cap_count, out_pos);
        return 0;
    }
    Capture *next_caps = caps_clone(caps, cap_count);
    int next_pos = 0;
    if (match_node(rep->u.rep.child, gs, re, pos, next_caps, cap_count, &next_pos)) {
        if (next_pos != pos && match_repeat_grow(rep, seq, next_idx, gs, re, next_pos,
                                                 next_caps, cap_count, reps_done + 1, out_pos)) {
            memcpy(caps, next_caps, (size_t)cap_count * sizeof(Capture));
            free(next_caps);
            return 1;
        }
        if (next_pos == pos && reps_done + 1 >= rep->u.rep.min) {
            if (match_seq_items(seq, next_idx, gs, re, pos, next_caps, cap_count, out_pos)) {
                memcpy(caps, next_caps, (size_t)cap_count * sizeof(Capture));
                free(next_caps);
                return 1;
            }
        }
    }
    free(next_caps);
    if (reps_done >= rep->u.rep.min && greedy)
        return match_seq_items(seq, next_idx, gs, re, pos, caps, cap_count, out_pos);
    return 0;
}

static int match_seq_items(const RegexNode *seq, int idx, const GlyphString *gs,
                           const RegExpVal *re, int pos, Capture *caps,
                           int cap_count, int *out_pos) {
    if (idx >= seq->u.list.count) { *out_pos = pos; return 1; }
    const RegexNode *item = seq->u.list.items[idx];
    if (item->kind == RX_REPEAT)
        return match_repeat_grow(item, seq, idx + 1, gs, re, pos, caps, cap_count, 0, out_pos);
    Capture *copy = caps_clone(caps, cap_count);
    int next = 0;
    if (match_node(item, gs, re, pos, copy, cap_count, &next) &&
        match_seq_items(seq, idx + 1, gs, re, next, copy, cap_count, out_pos)) {
        memcpy(caps, copy, (size_t)cap_count * sizeof(Capture));
        free(copy);
        return 1;
    }
    free(copy);
    return 0;
}

static int match_node(const RegexNode *node, const GlyphString *gs, const RegExpVal *re,
                      int pos, Capture *caps, int cap_count, int *out_pos) {
    switch (node->kind) {
    case RX_EMPTY:
        *out_pos = pos;
        return 1;
    case RX_LITERAL:
        if (pos < gs->count && glyph_eq_ascii_fold(gs->glyphs[pos], node->u.lit.glyph, re->flag_i)) {
            *out_pos = pos + 1;
            return 1;
        }
        return 0;
    case RX_DOT:
        if (pos >= gs->count) return 0;
        if (!re->flag_s && is_line_break(gs->glyphs[pos])) return 0;
        *out_pos = pos + 1;
        return 1;
    case RX_BOL:
        if (pos == 0 || (re->flag_m && pos > 0 && is_line_break(gs->glyphs[pos - 1]))) {
            *out_pos = pos;
            return 1;
        }
        return 0;
    case RX_EOL:
        if (pos == gs->count || (re->flag_m && pos < gs->count && is_line_break(gs->glyphs[pos]))) {
            *out_pos = pos;
            return 1;
        }
        return 0;
    case RX_CLASS:
        if (pos < gs->count && class_matches(node, gs->glyphs[pos], re)) {
            *out_pos = pos + 1;
            return 1;
        }
        return 0;
    case RX_SEQ:
        return match_seq_items(node, 0, gs, re, pos, caps, cap_count, out_pos);
    case RX_ALT: {
        Capture *left = caps_clone(caps, cap_count);
        int next = 0;
        if (match_node(node->u.alt.left, gs, re, pos, left, cap_count, &next)) {
            memcpy(caps, left, (size_t)cap_count * sizeof(Capture));
            free(left);
            *out_pos = next;
            return 1;
        }
        free(left);
        Capture *right = caps_clone(caps, cap_count);
        if (match_node(node->u.alt.right, gs, re, pos, right, cap_count, &next)) {
            memcpy(caps, right, (size_t)cap_count * sizeof(Capture));
            free(right);
            *out_pos = next;
            return 1;
        }
        free(right);
        return 0;
    }
    case RX_REPEAT: {
        RegexNode wrap;
        memset(&wrap, 0, sizeof(wrap));
        wrap.kind = RX_SEQ;
        wrap.u.list.count = 1;
        wrap.u.list.items = (RegexNode **)&node;
        return match_seq_items(&wrap, 0, gs, re, pos, caps, cap_count, out_pos);
    }
    case RX_GROUP: {
        int idx = node->u.group.index;
        Capture *copy = caps_clone(caps, cap_count);
        int next = 0;
        if (idx >= 0 && idx < cap_count) {
            copy[idx].start = pos;
            copy[idx].set = 1;
        }
        if (match_node(node->u.group.child, gs, re, pos, copy, cap_count, &next)) {
            if (idx >= 0 && idx < cap_count) copy[idx].end = next;
            memcpy(caps, copy, (size_t)cap_count * sizeof(Capture));
            free(copy);
            *out_pos = next;
            return 1;
        }
        free(copy);
        return 0;
    }
    }
    return 0;
}

static RegExpMatchVal *match_at(const RegExpVal *re, const GlyphString *gs, int start_pos) {
    int cap_count = re->group_count + 1;
    Capture *caps = cap_count ? ALLOC_N(Capture, cap_count) : NULL;
    for (int i = 0; i < cap_count; i++) { caps[i].start = caps[i].end = 0; caps[i].set = 0; }
    int end_pos = 0;
    caps[0].start = start_pos;
    caps[0].set = 1;
    if (!match_node(((RegexProgram *)re->nfa)->root, gs, re, start_pos, caps, cap_count, &end_pos)) {
        free(caps);
        return regex_match_empty();
    }
    caps[0].end = end_pos;
    RegExpMatchVal *m = ALLOC(RegExpMatchVal);
    m->ok = 1;
    m->start = start_pos;
    m->end = end_pos;
    m->group_count = cap_count;
    m->groups = cap_count ? ALLOC_N(char *, cap_count) : NULL;
    for (int i = 0; i < cap_count; i++) {
        if (!caps[i].set) m->groups[i] = cimple_strdup("");
        else m->groups[i] = glyph_substring(gs, caps[i].start, caps[i].end);
    }
    free(caps);
    return m;
}

RegExpMatchVal *regex_match_empty(void) {
    RegExpMatchVal *m = ALLOC(RegExpMatchVal);
    m->ok = 0;
    m->start = 0;
    m->end = 0;
    m->group_count = 0;
    m->groups = NULL;
    return m;
}

RegExpMatchVal *regex_match_copy(const RegExpMatchVal *src) {
    if (!src) return regex_match_empty();
    RegExpMatchVal *m = ALLOC(RegExpMatchVal);
    m->ok = src->ok;
    m->start = src->start;
    m->end = src->end;
    m->group_count = src->group_count;
    m->groups = src->group_count ? ALLOC_N(char *, src->group_count) : NULL;
    for (int i = 0; i < src->group_count; i++) m->groups[i] = cimple_strdup(src->groups[i]);
    return m;
}

void regex_match_free(RegExpMatchVal *m) {
    if (!m) return;
    for (int i = 0; i < m->group_count; i++) free(m->groups[i]);
    free(m->groups);
    free(m);
}

static char *expand_replacement(const RegExpMatchVal *m, const char *replacement) {
    size_t cap = 64, len = 0;
    char *out = (char *)cimple_malloc(cap);
    out[0] = '\0';
    for (size_t i = 0; replacement[i]; ) {
        if (replacement[i] != '$') {
            if (len + 2 > cap) { cap *= 2; out = (char *)cimple_realloc(out, cap); }
            out[len++] = replacement[i++];
            out[len] = '\0';
            continue;
        }
        if (replacement[i + 1] == '$') {
            if (len + 2 > cap) { cap *= 2; out = (char *)cimple_realloc(out, cap); }
            out[len++] = '$';
            out[len] = '\0';
            i += 2;
            continue;
        }
        if (isdigit((unsigned char)replacement[i + 1])) {
            int num = replacement[i + 1] - '0';
            i += 2;
            if (isdigit((unsigned char)replacement[i]) && num < 10) {
                num = num * 10 + (replacement[i] - '0');
                i++;
            }
            const char *piece = (m && num < m->group_count) ? m->groups[num] : "";
            size_t n = strlen(piece);
            while (len + n + 1 > cap) { cap *= 2; out = (char *)cimple_realloc(out, cap); }
            memcpy(out + len, piece, n);
            len += n;
            out[len] = '\0';
            continue;
        }
        /* $X where X is not a digit and not '$': emit literal '$'.
         * Also handles trailing '$' (replacement[i+1] == '\0') — only advance
         * past the '$', not past the null terminator (which would be UB). */
        if (len + 2 > cap) { cap *= 2; out = (char *)cimple_realloc(out, cap); }
        out[len++] = '$';
        out[len] = '\0';
        i++;
        /* Emit the following character as-is unless it is the string terminator */
        if (replacement[i]) {
            if (len + 2 > cap) { cap *= 2; out = (char *)cimple_realloc(out, cap); }
            out[len++] = replacement[i++];
            out[len] = '\0';
        }
    }
    return out;
}

RegExpVal *regex_compile_value(const char *pattern, const char *flags, int line, int col) {
    RegexParser p;
    p.pattern = pattern ? pattern : "";
    p.cur = p.pattern;
    p.line = line;
    p.col = col;
    p.depth = 0;
    p.group_count = 0;
    RegexNode *root = parse_expr(&p);
    if (!at_end(&p)) regex_syntax_error(line, col, "unexpected trailing input");
    RegExpVal *re = ALLOC(RegExpVal);
    memset(re, 0, sizeof(*re));
    re->pattern = cimple_strdup(pattern ? pattern : "");
    re->flags = canonical_flags(flags ? flags : "", &re->flag_i, &re->flag_m, &re->flag_s, line, col);
    re->group_count = p.group_count;
    RegexProgram *prog = ALLOC(RegexProgram);
    prog->root = root;
    re->nfa = prog;
    return re;
}

RegExpVal *regex_copy_value(const RegExpVal *src) {
    if (!src) return NULL;
    return regex_compile_value(src->pattern, src->flags, 0, 0);
}

void regex_free_value(RegExpVal *re) {
    if (!re) return;
    free(re->pattern);
    free(re->flags);
    if (re->nfa) {
        RegexProgram *prog = (RegexProgram *)re->nfa;
        regex_node_free(prog->root);
        free(prog);
    }
    free(re);
}

static void check_start_range(const char *input, int start, int line, int col) {
    int glen = regex_glyph_len(input);
    if (start < 0 || start > glen)
        regex_range_error(line, col, "start out of bounds");
}

RegExpMatchVal *regex_find_value(const RegExpVal *re, const char *input, int start, int line, int col) {
    check_start_range(input, start, line, col);
    GlyphString gs = glyph_string_make(input);
    RegExpMatchVal *m = NULL;
    for (int pos = start; pos <= gs.count; pos++) {
        m = match_at(re, &gs, pos);
        if (m->ok) break;
        regex_match_free(m);
        m = NULL;
    }
    if (!m) m = regex_match_empty();
    glyph_string_free(&gs);
    return m;
}

int regex_test_value(const RegExpVal *re, const char *input, int start, int line, int col) {
    RegExpMatchVal *m = regex_find_value(re, input, start, line, col);
    int ok = m->ok;
    regex_match_free(m);
    return ok;
}

RegExpMatchVal **regex_find_all_values(const RegExpVal *re, const char *input,
                                       int start, int max, int *out_count,
                                       int line, int col) {
    check_start_range(input, start, line, col);
    if (max < -1) regex_range_error(line, col, "max must be >= -1");
    *out_count = 0;
    if (max == 0) return NULL;
    GlyphString gs = glyph_string_make(input);
    RegExpMatchVal **items = NULL;
    int count = 0, cap = 0;
    int pos = start;
    while (pos <= gs.count && (max < 0 || count < max)) {
        RegExpMatchVal *m = NULL;
        int found_pos = pos;
        for (; found_pos <= gs.count; found_pos++) {
            m = match_at(re, &gs, found_pos);
            if (m->ok) break;
            regex_match_free(m);
            m = NULL;
        }
        if (!m) break;
        if (count == cap) {
            cap = cap ? cap * 2 : 4;
            items = (RegExpMatchVal **)cimple_realloc(items, (size_t)cap * sizeof(RegExpMatchVal *));
        }
        items[count++] = m;
        pos = m->end;
        if (m->start == m->end) pos = m->end + 1;
    }
    glyph_string_free(&gs);
    *out_count = count;
    return items;
}

char *regex_replace_value(const RegExpVal *re, const char *input,
                          const char *replacement, int start,
                          int replace_all, int max,
                          int line, int col) {
    check_start_range(input, start, line, col);
    if (replace_all && max < -1) regex_range_error(line, col, "max must be >= -1");
    if (replace_all && max == 0) return cimple_strdup(input ? input : "");
    GlyphString gs = glyph_string_make(input);
    RegexSb sb;
    sb.data = (char *)cimple_malloc(1); sb.data[0] = '\0'; sb.len = 0; sb.cap = 1;
#define SB_ENSURE(add) do { size_t need = sb.len + (size_t)(add) + 1; while (need > sb.cap) { sb.cap = sb.cap ? sb.cap * 2 : 64; sb.data = (char *)cimple_realloc(sb.data, sb.cap); } } while (0)
#define SB_APPEND_STR(str) do { const char *_s=(str); size_t _n=strlen(_s); SB_ENSURE(_n); memcpy(sb.data+sb.len,_s,_n); sb.len+=_n; sb.data[sb.len]='\0'; } while (0)
    SB_APPEND_STR(glyph_substring(&gs, 0, start));
    int pos = start;
    int replaced = 0;
    while (pos <= gs.count && (!replace_all || max < 0 || replaced < max)) {
        RegExpMatchVal *m = NULL;
        int found_pos = pos;
        for (; found_pos <= gs.count; found_pos++) {
            m = match_at(re, &gs, found_pos);
            if (m->ok) break;
            regex_match_free(m);
            m = NULL;
        }
        if (!m) break;
        char *between = glyph_substring(&gs, pos, m->start);
        SB_APPEND_STR(between);
        free(between);
        char *rep = expand_replacement(m, replacement ? replacement : "");
        SB_APPEND_STR(rep);
        free(rep);
        replaced++;
        pos = m->end;
        if (m->start == m->end) {
            if (pos < gs.count) {
                char *g = glyph_substring(&gs, pos, pos + 1);
                SB_APPEND_STR(g);
                free(g);
            }
            pos++;
        }
        regex_match_free(m);
        if (!replace_all) break;
    }
    if (pos <= gs.count) {
        char *tail = glyph_substring(&gs, pos, gs.count);
        SB_APPEND_STR(tail);
        free(tail);
    }
    char *out = sb.data;
    glyph_string_free(&gs);
    return out;
#undef SB_ENSURE
#undef SB_APPEND_STR
}

char **regex_split_value(const RegExpVal *re, const char *input,
                         int start, int max_parts, int *out_count,
                         int line, int col) {
    check_start_range(input, start, line, col);
    if (max_parts < -1) regex_range_error(line, col, "maxParts must be >= -1");
    *out_count = 0;
    if (max_parts == 0) return NULL;
    GlyphString gs = glyph_string_make(input);
    if (max_parts == 1) {
        char **parts = ALLOC_N(char *, 1);
        parts[0] = glyph_substring(&gs, start, gs.count);
        glyph_string_free(&gs);
        *out_count = 1;
        return parts;
    }
    char **parts = NULL;
    int count = 0, cap = 0;
    int pos = start;
    while (pos <= gs.count && (max_parts < 0 || count + 1 < max_parts)) {
        RegExpMatchVal *m = NULL;
        int found_pos = pos;
        for (; found_pos <= gs.count; found_pos++) {
            m = match_at(re, &gs, found_pos);
            if (m->ok) break;
            regex_match_free(m);
            m = NULL;
        }
        if (!m) break;
        if (count == cap) {
            cap = cap ? cap * 2 : 4;
            parts = (char **)cimple_realloc(parts, (size_t)cap * sizeof(char *));
        }
        parts[count++] = glyph_substring(&gs, pos, m->start);
        pos = m->end;
        if (m->start == m->end) pos = m->end + 1;
        regex_match_free(m);
    }
    if (count == cap) {
        cap = cap ? cap + 1 : 1;
        parts = (char **)cimple_realloc(parts, (size_t)cap * sizeof(char *));
    }
    parts[count++] = glyph_substring(&gs, pos <= gs.count ? pos : gs.count, gs.count);
    glyph_string_free(&gs);
    *out_count = count;
    return parts;
}
