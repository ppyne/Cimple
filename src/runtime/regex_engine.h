#ifndef CIMPLE_REGEX_ENGINE_H
#define CIMPLE_REGEX_ENGINE_H

#include "value.h"

struct RegExpVal {
    char *pattern;
    char *flags;
    void *nfa;
    int flag_i;
    int flag_m;
    int flag_s;
    int group_count;
};

struct RegExpMatchVal {
    int ok;
    int start;
    int end;
    int group_count;
    char **groups;
};

RegExpVal *regex_compile_value(const char *pattern, const char *flags, int line, int col);
RegExpVal *regex_copy_value(const RegExpVal *src);
void regex_free_value(RegExpVal *re);

RegExpMatchVal *regex_match_empty(void);
RegExpMatchVal *regex_match_copy(const RegExpMatchVal *src);
void regex_match_free(RegExpMatchVal *m);

int regex_glyph_len(const char *s);
RegExpMatchVal *regex_find_value(const RegExpVal *re, const char *input, int start, int line, int col);
int regex_test_value(const RegExpVal *re, const char *input, int start, int line, int col);
RegExpMatchVal **regex_find_all_values(const RegExpVal *re, const char *input,
                                       int start, int max, int *out_count,
                                       int line, int col);
char *regex_replace_value(const RegExpVal *re, const char *input,
                          const char *replacement, int start,
                          int replace_all, int max,
                          int line, int col);
char **regex_split_value(const RegExpVal *re, const char *input,
                         int start, int max_parts, int *out_count,
                         int line, int col);

#endif
