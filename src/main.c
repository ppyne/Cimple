#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "common/error.h"
#include "common/memory.h"
#include "ast/ast.h"
#include "parser/parser_helper.h"
#include "semantic/semantic.h"
#include "interpreter/interpreter.h"
#include "lexer/lexer.h"

typedef struct {
    char **items;
    int count;
    int cap;
} StringList;

typedef struct {
    char *path;
    int line;
    int col;
} ImportSpec;

typedef struct {
    ImportSpec *items;
    int count;
    int cap;
} ImportList;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

typedef struct {
    SourceMapEntry *items;
    int count;
    int cap;
} SourceMapList;

typedef struct {
    char *merged;
    SourceMapEntry *map;
    int map_count;
    char *root_display;
} ResolvedProgram;

typedef struct {
    StrBuf merged;
    SourceMapList map;
    StringList visited;
    StringList stack;
    const char *root_dir;
} ResolveState;

static char *read_source(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = (char *)cimple_malloc((size_t)size + 1);
    if ((long)fread(buf, 1, (size_t)size, f) != size) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void normalise_newlines(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\r' && r[1] == '\n') r++;
        *w++ = *r++;
    }
    *w = '\0';
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s run   <file.ci> [args...]\n"
            "  %s check <file.ci>\n"
            "\n"
            "Commands:\n"
            "  run    Parse, analyse and execute the program.\n"
            "  check  Parse and analyse only (no execution).\n",
            prog, prog);
}

static void strlist_push(StringList *list, const char *s) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = (char **)cimple_realloc(list->items, (size_t)list->cap * sizeof(char *));
    }
    list->items[list->count++] = cimple_strdup(s);
}

static int strlist_index_of(StringList *list, const char *s) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], s) == 0) return i;
    }
    return -1;
}

static void strlist_pop(StringList *list) {
    if (list->count == 0) return;
    free(list->items[--list->count]);
}

static void strlist_free(StringList *list) {
    for (int i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
}

static void importlist_push(ImportList *list, const char *path, int line, int col) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = (ImportSpec *)cimple_realloc(list->items, (size_t)list->cap * sizeof(ImportSpec));
    }
    list->items[list->count].path = cimple_strdup(path);
    list->items[list->count].line = line;
    list->items[list->count].col = col;
    list->count++;
}

static void importlist_free(ImportList *list) {
    for (int i = 0; i < list->count; i++) free(list->items[i].path);
    free(list->items);
}

static void sb_init(StrBuf *sb) {
    sb->cap = 1024;
    sb->len = 0;
    sb->data = (char *)cimple_malloc(sb->cap);
    sb->data[0] = '\0';
}

static void sb_reserve(StrBuf *sb, size_t extra) {
    size_t needed = sb->len + extra + 1;
    while (needed > sb->cap) sb->cap *= 2;
    sb->data = (char *)cimple_realloc(sb->data, sb->cap);
}

static void sb_append(StrBuf *sb, const char *s) {
    size_t n = strlen(s);
    sb_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

static int count_lines(const char *s) {
    int lines = 1;
    for (const char *p = s; *p; p++) if (*p == '\n') lines++;
    return lines;
}

static void sourcemap_push(SourceMapList *map, int merged_start, int merged_end,
                           int source_start, const char *display_file) {
    if (map->count == map->cap) {
        map->cap = map->cap ? map->cap * 2 : 16;
        map->items = (SourceMapEntry *)cimple_realloc(map->items,
            (size_t)map->cap * sizeof(SourceMapEntry));
    }
    map->items[map->count].merged_line_start = merged_start;
    map->items[map->count].merged_line_end = merged_end;
    map->items[map->count].source_line_start = source_start;
    map->items[map->count].display_file = cimple_strdup(display_file);
    map->count++;
}

static void sourcemap_free(SourceMapList *map) {
    for (int i = 0; i < map->count; i++) free((char *)map->items[i].display_file);
    free(map->items);
}

static char *path_dirname(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return cimple_strdup(".");
    if (slash == path) return cimple_strndup(path, 1);
    return cimple_strndup(path, (size_t)(slash - path));
}

static char *normalize_path_copy(const char *path) {
    char **parts = NULL;
    int count = 0;
    int cap = 0;
    int absolute = path[0] == '/';
    const char *p = path;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (count > 0 && strcmp(parts[count - 1], "..") != 0) {
                free(parts[--count]);
            } else if (!absolute) {
                if (count == cap) {
                    cap = cap ? cap * 2 : 8;
                    parts = (char **)cimple_realloc(parts, (size_t)cap * sizeof(char *));
                }
                parts[count++] = cimple_strndup(start, len);
            }
            continue;
        }
        if (count == cap) {
            cap = cap ? cap * 2 : 8;
            parts = (char **)cimple_realloc(parts, (size_t)cap * sizeof(char *));
        }
        parts[count++] = cimple_strndup(start, len);
    }

    StrBuf sb;
    sb_init(&sb);
    if (absolute) sb_append(&sb, "/");
    for (int i = 0; i < count; i++) {
        if ((absolute && sb.len > 1) || (!absolute && sb.len > 0)) sb_append(&sb, "/");
        sb_append(&sb, parts[i]);
        free(parts[i]);
    }
    free(parts);
    if (sb.len == 0) sb_append(&sb, absolute ? "/" : ".");
    return sb.data;
}

static char *resolve_existing_path(const char *base_dir, const char *path) {
    char *joined;
    if (path[0] == '/') {
        joined = cimple_strdup(path);
    } else {
        size_t need = strlen(base_dir) + strlen(path) + 2;
        joined = (char *)cimple_malloc(need);
        snprintf(joined, need, "%s/%s", base_dir, path);
    }
    char *resolved = normalize_path_copy(joined);
    free(joined);
    FILE *f = fopen(resolved, "rb");
    if (!f) {
        free(resolved);
        return NULL;
    }
    fclose(f);
    return resolved;
}

static char *display_path(const char *root_dir, const char *abs_path) {
    size_t root_len = strlen(root_dir);
    if (strncmp(root_dir, abs_path, root_len) == 0 && abs_path[root_len] == '/') {
        return cimple_strdup(abs_path + root_len + 1);
    }
    return cimple_strdup(abs_path);
}

static void blank_span_preserve_lines(char *buf, int start, int end) {
    for (int i = start; i < end; i++) {
        if (buf[i] != '\n') buf[i] = ' ';
    }
}

static void fail_formatted(ErrorKind kind, const char *file, int line, int col,
                           const char *hint, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    error_print_at_file(kind, file, line, col, msg, hint);
    exit(kind == ERR_RUNTIME ? 2 : 1);
}

static int path_has_ci_extension(const char *path) {
    size_t len = strlen(path);
    return len >= 3 && strcmp(path + len - 3, ".ci") == 0;
}

static void fail_circular_import(ResolveState *st, const char *abs_path) {
    char *disp = display_path(st->root_dir, abs_path);
    StrBuf chain;
    sb_init(&chain);
    for (int i = 0; i < st->stack.count; i++) {
        char *part = display_path(st->root_dir, st->stack.items[i]);
        if (i > 0) sb_append(&chain, " -> ");
        sb_append(&chain, part);
        free(part);
    }
    if (chain.len > 0) sb_append(&chain, " -> ");
    sb_append(&chain, disp);
    StrBuf indication;
    sb_init(&indication);
    sb_append(&indication, "Import chain: ");
    sb_append(&indication, chain.data);
    fail_formatted(ERR_SEMANTIC, disp, 1, 1,
                   indication.data,
                   "Circular import detected: '%s'", disp);
}

static void validate_and_collect_imports(const char *display_file, const char *source,
                                         char *body_out, ImportList *imports) {
    Lexer lex;
    lexer_init(&lex, source);
    int top_level_depth = 0;
    int seen_non_import = 0;

    for (;;) {
        Token tok = lexer_next(&lex);
        if (tok.type == TOK_EOF) break;
        if (tok.type == TOK_LBRACE) {
            top_level_depth++;
            seen_non_import = 1;
            continue;
        }
        if (tok.type == TOK_RBRACE) {
            if (top_level_depth > 0) top_level_depth--;
            seen_non_import = 1;
            continue;
        }
        if (tok.type == TOK_KW_IMPORT) {
            if (top_level_depth > 0) {
                fail_formatted(ERR_SYNTAX, display_file, tok.line, tok.col,
                               "Import directives are only valid at the top level.",
                               "'import' is not allowed inside a function or block");
            }
            if (seen_non_import) {
                fail_formatted(ERR_SYNTAX, display_file, tok.line, tok.col,
                               "Move all import directives to the top of the file.",
                               "'import' must appear before any declaration");
            }
            Token path_tok = lexer_next(&lex);
            if (path_tok.type != TOK_STRING_LIT) {
                fail_formatted(ERR_SYNTAX, display_file, tok.line, tok.col, NULL,
                               "Expected string literal after 'import'");
            }
            Token semi_tok = lexer_next(&lex);
            if (semi_tok.type != TOK_SEMICOLON) {
                fail_formatted(ERR_SYNTAX, display_file, semi_tok.line, semi_tok.col, NULL,
                               "Missing ';' after import declaration");
            }
            if (!path_has_ci_extension(path_tok.v.sval)) {
                fail_formatted(ERR_SEMANTIC, display_file, path_tok.line, path_tok.col,
                               "Only .ci files can be imported.",
                               "Import path must end with '.ci'");
            }
            importlist_push(imports, path_tok.v.sval, path_tok.line, path_tok.col);
            blank_span_preserve_lines(body_out, tok.start_offset, semi_tok.end_offset);
            free(path_tok.v.sval);
            continue;
        }
        seen_non_import = 1;
        if (tok.type == TOK_IDENT || tok.type == TOK_STRING_LIT) free(tok.v.sval);
    }
}

static void append_file_segment(ResolveState *st, const char *display_file, const char *text) {
    int merged_start = (int)count_lines(st->merged.data);
    if (st->merged.len == 0) merged_start = 1;
    sb_append(&st->merged, text);
    if (st->merged.len == 0 || st->merged.data[st->merged.len - 1] != '\n') sb_append(&st->merged, "\n");
    int line_count = count_lines(text);
    sourcemap_push(&st->map, merged_start, merged_start + line_count - 1, 1, display_file);
}

static void resolve_file_recursive(ResolveState *st, const char *abs_path, int depth) {
    if (depth > 32) {
        char *disp = display_path(st->root_dir, abs_path);
        fail_formatted(ERR_SEMANTIC, disp, 1, 1,
                       "Simplify your import graph.",
                       "Import depth limit exceeded (max 32)");
    }
    if (strlist_index_of(&st->visited, abs_path) >= 0) return;
    int stack_index = strlist_index_of(&st->stack, abs_path);
    if (stack_index >= 0) {
        fail_circular_import(st, abs_path);
    }

    char *source = read_source(abs_path);
    char *disp = display_path(st->root_dir, abs_path);
    if (!source) {
        fail_formatted(ERR_SEMANTIC, disp, 1, 1, NULL,
                       "Cannot import file: '%s'", disp);
    }
    normalise_newlines(source);
    char *body = cimple_strdup(source);
    ImportList imports = {0};
    strlist_push(&st->stack, abs_path);
    error_init(disp);
    validate_and_collect_imports(disp, source, body, &imports);

    char *dir = path_dirname(abs_path);
    for (int i = 0; i < imports.count; i++) {
        char *import_abs = resolve_existing_path(dir, imports.items[i].path);
        if (!import_abs) {
            fail_formatted(ERR_SEMANTIC, disp, imports.items[i].line, imports.items[i].col,
                           "File not found or not readable.",
                           "Cannot import file: '%s'", imports.items[i].path);
        }
        resolve_file_recursive(st, import_abs, depth + 1);
        free(import_abs);
    }

    append_file_segment(st, disp, body);
    strlist_push(&st->visited, abs_path);
    strlist_pop(&st->stack);

    free(dir);
    free(disp);
    free(body);
    free(source);
    importlist_free(&imports);
}

static ResolvedProgram resolve_program_sources(const char *root_path) {
    ResolvedProgram out = {0};
    char *root_abs = resolve_existing_path(".", root_path);
    if (!root_abs) {
        fprintf(stderr, "[ERROR] cannot open '%s': %s\n", root_path, strerror(errno));
        exit(1);
    }
    char *root_dir = path_dirname(root_abs);
    out.root_display = display_path(root_dir, root_abs);
    ResolveState st = {0};
    st.root_dir = root_dir;
    sb_init(&st.merged);
    resolve_file_recursive(&st, root_abs, 1);
    out.merged = st.merged.data;
    out.map = st.map.items;
    out.map_count = st.map.count;
    strlist_free(&st.visited);
    strlist_free(&st.stack);
    free(root_dir);
    free(root_abs);
    return out;
}

static const char *map_file_for_line(const SourceMapEntry *map, int count, int line) {
    for (int i = 0; i < count; i++) {
        if (line >= map[i].merged_line_start && line <= map[i].merged_line_end) {
            return map[i].display_file;
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    const char *filepath = argv[2];
    int do_run = (strcmp(cmd, "run") == 0);
    int do_check = (strcmp(cmd, "check") == 0);

    if (!do_run && !do_check) {
        fprintf(stderr, "[ERROR] unknown command '%s'\n", cmd);
        usage(argv[0]);
        return 1;
    }

    error_init(filepath);
    ResolvedProgram resolved = resolve_program_sources(filepath);
    error_init(filepath);
    error_set_source_map(resolved.map, resolved.map_count);

    AstNode *program = parse_program(resolved.merged);
    free(resolved.merged);
    if (!program) {
        sourcemap_free((SourceMapList *)&(SourceMapList){ .items = resolved.map, .count = resolved.map_count });
        return 1;
    }

    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *decl = decls->items[i];
        if (decl->kind == NODE_FUNC_DECL &&
            strcmp(decl->u.func_decl.name, "main") == 0) {
            const char *decl_file = map_file_for_line(resolved.map, resolved.map_count, decl->line);
            if (decl_file && strcmp(decl_file, resolved.root_display) != 0) {
                error_print_at_file(ERR_SEMANTIC, decl_file, decl->line, decl->col,
                                    "'main' cannot be defined in an imported file", NULL);
                ast_free(program);
                sourcemap_free((SourceMapList *)&(SourceMapList){ .items = resolved.map, .count = resolved.map_count });
                free(resolved.root_display);
                return 1;
            }
        }
    }

    int sem_errors = semantic_check(program);
    if (sem_errors) {
        ast_free(program);
        sourcemap_free((SourceMapList *)&(SourceMapList){ .items = resolved.map, .count = resolved.map_count });
        free(resolved.root_display);
        return 1;
    }

    if (do_check) {
        printf("OK\n");
        ast_free(program);
        sourcemap_free((SourceMapList *)&(SourceMapList){ .items = resolved.map, .count = resolved.map_count });
        free(resolved.root_display);
        return 0;
    }

    int user_argc = argc - 3;
    char **user_argv = argv + 3;
    int exit_code = interp_run(program, user_argc, user_argv);
    ast_free(program);
    sourcemap_free((SourceMapList *)&(SourceMapList){ .items = resolved.map, .count = resolved.map_count });
    free(resolved.root_display);
    return exit_code;
}
