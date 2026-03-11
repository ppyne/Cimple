#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common/error.h"
#include "common/memory.h"
#include "ast/ast.h"
#include "parser/parser_helper.h"
#include "semantic/semantic.h"
#include "interpreter/interpreter.h"

/* -----------------------------------------------------------------------
 * Read the entire contents of a file into a NUL-terminated buffer.
 * Returns NULL on error.
 * ----------------------------------------------------------------------- */
static char *read_source(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[ERROR] cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
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

/* -----------------------------------------------------------------------
 * Normalise \r\n → \n in-place.
 * ----------------------------------------------------------------------- */
static void normalise_newlines(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\r' && r[1] == '\n') { r++; }
        *w++ = *r++;
    }
    *w = '\0';
}

/* -----------------------------------------------------------------------
 * Usage
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd      = argv[1];
    const char *filepath = argv[2];
    int  do_run          = (strcmp(cmd, "run")   == 0);
    int  do_check        = (strcmp(cmd, "check") == 0);

    if (!do_run && !do_check) {
        fprintf(stderr, "[ERROR] unknown command '%s'\n", cmd);
        usage(argv[0]);
        return 1;
    }

    /* Read source */
    error_init(filepath);
    char *source = read_source(filepath);
    if (!source) return 1;
    normalise_newlines(source);

    /* Parse */
    AstNode *program = parse_program(source);
    free(source);

    if (!program) {
        /* parse_program already printed a fatal error */
        return 1;
    }

    /* Semantic analysis */
    int sem_errors = semantic_check(program);
    if (sem_errors) {
        ast_free(program);
        return 1;
    }

    if (do_check) {
        printf("OK\n");
        ast_free(program);
        return 0;
    }

    /* Run — pass user arguments (skip "cimple", "run", "<file>") */
    int user_argc   = argc - 3;
    char **user_argv = argv + 3;

    int exit_code = interp_run(program, user_argc, user_argv);
    ast_free(program);
    return exit_code;
}
