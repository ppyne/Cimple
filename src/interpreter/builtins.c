#include "builtins.h"
#include "../common/error.h"
#include "../common/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

/* Platform-specific includes */
#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <unistd.h>
#  include <spawn.h>
#  include <sys/wait.h>
#  include <fcntl.h>
extern char **environ;
#endif

/* -----------------------------------------------------------------------
 * Sentinel for "not a builtin"
 * ----------------------------------------------------------------------- */
static Value not_a_builtin(void) {
    Value v; v.type = TYPE_UNKNOWN; v.u.i = 0; return v;
}

/* -----------------------------------------------------------------------
 * NFC normalisation stubs (minimal — full implementation would use ICU/libunicode)
 * For the purposes of this implementation we provide UTF-8 code-point counting
 * without full NFC normalisation.  A production implementation should link
 * against a Unicode library.
 * ----------------------------------------------------------------------- */

/* Count UTF-8 code points (no normalisation) */
static int utf8_codepoint_count(const char *s) {
    int count = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if      ((c & 0x80) == 0x00) s += 1;
        else if ((c & 0xE0) == 0xC0) s += 2;
        else if ((c & 0xF0) == 0xE0) s += 3;
        else if ((c & 0xF8) == 0xF0) s += 4;
        else s += 1;
        count++;
    }
    return count;
}

/* Return the code point string at index (UTF-8 multi-byte safe) */
static char *utf8_codepoint_at(const char *s, int index, int line, int col) {
    int total = utf8_codepoint_count(s);
    if (index < 0 || index >= total)
        error_runtime(line, col,
                      "glyphAt: index %d out of bounds (glyphLen=%d)", index, total);
    const char *p = s;
    for (int i = 0; i < index; i++) {
        unsigned char c = (unsigned char)*p;
        if      ((c & 0x80) == 0x00) p += 1;
        else if ((c & 0xE0) == 0xC0) p += 2;
        else if ((c & 0xF0) == 0xE0) p += 3;
        else if ((c & 0xF8) == 0xF0) p += 4;
        else p += 1;
    }
    unsigned char c = (unsigned char)*p;
    int cplen;
    if      ((c & 0x80) == 0x00) cplen = 1;
    else if ((c & 0xE0) == 0xC0) cplen = 2;
    else if ((c & 0xF0) == 0xE0) cplen = 3;
    else if ((c & 0xF8) == 0xF0) cplen = 4;
    else cplen = 1;
    char *result = (char *)cimple_malloc((size_t)cplen + 1);
    memcpy(result, p, (size_t)cplen);
    result[cplen] = '\0';
    return result;
}

/* -----------------------------------------------------------------------
 * File helpers
 * ----------------------------------------------------------------------- */
static char *read_file_str(const char *path, int line, int col) {
    FILE *f = fopen(path, "rb");
    if (!f) error_runtime(line, col, "readFile: cannot open '%s': %s",
                          path, strerror(errno));
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char *)cimple_malloc((size_t)size + 1);
    if ((long)fread(buf, 1, (size_t)size, f) != size) {
        fclose(f);
        free(buf);
        error_runtime(line, col, "readFile: read error on '%s'", path);
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* -----------------------------------------------------------------------
 * exec() — run external command
 * ----------------------------------------------------------------------- */
#ifndef __EMSCRIPTEN__
static Value do_exec(Value *args, int nargs, int line, int col) {
    if (nargs < 1 || !type_is_array(args[0].type))
        error_runtime(line, col, "exec: first argument must be string[]");
    ArrayVal *cmd_arr = args[0].u.arr;
    if (cmd_arr->count == 0)
        error_runtime(line, col, "exec: command array is empty");

    /* Build argv */
    int     argc = cmd_arr->count;
    char  **argv = (char **)cimple_malloc((size_t)(argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; i++)
        argv[i] = cmd_arr->data.strings[i];
    argv[argc] = NULL;

    /* Build environment */
    char **envp = NULL;
    if (nargs >= 2 && type_is_array(args[1].type)) {
        ArrayVal *env_arr = args[1].u.arr;
        int ne = env_arr->count;
#ifdef _WIN32
        /* Collect current environment */
        int base_count = 0;
        LPCH env_block = GetEnvironmentStrings();
        LPCH p = env_block;
        while (*p) { base_count++; while (*p++) {} }
        FreeEnvironmentStrings(env_block);
        envp = (char **)cimple_malloc((size_t)(base_count + ne + 1) * sizeof(char *));
        env_block = GetEnvironmentStrings();
        p = env_block;
        int ei = 0;
        while (*p) { envp[ei++] = cimple_strdup(p); while (*p++) {} }
        FreeEnvironmentStrings(env_block);
        for (int i = 0; i < ne; i++) envp[ei++] = env_arr->data.strings[i];
        envp[ei] = NULL;
#else
        /* Use environ as base, then override */
        int base_count = 0;
        while (environ[base_count]) base_count++;
        envp = (char **)cimple_malloc((size_t)(base_count + ne + 1) * sizeof(char *));
        int ei = 0;
        for (int i = 0; i < base_count; i++) envp[ei++] = environ[i];
        for (int i = 0; i < ne; i++) envp[ei++] = env_arr->data.strings[i];
        envp[ei] = NULL;
#endif
    }

    /* Capture stdout/stderr via pipes */
    char *out_str = NULL;
    char *err_str = NULL;
    int   status  = 0;

#ifdef _WIN32
    HANDLE out_r, out_w, err_r, err_w;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    CreatePipe(&out_r, &out_w, &sa, 0);
    CreatePipe(&err_r, &err_w, &sa, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = out_w;
    si.hStdError  = err_w;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    /* Build command line */
    size_t cmdlen = 0;
    for (int i = 0; i < argc; i++) cmdlen += strlen(argv[i]) + 3;
    char *cmdline = (char *)cimple_malloc(cmdlen + 1);
    cmdline[0] = '\0';
    for (int i = 0; i < argc; i++) {
        if (i > 0) strcat(cmdline, " ");
        strcat(cmdline, "\"");
        strcat(cmdline, argv[i]);
        strcat(cmdline, "\"");
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        error_runtime(line, col, "exec: failed to start '%s'", argv[0]);
    }
    CloseHandle(out_w); CloseHandle(err_w);

    /* Read pipes */
    char tmpbuf[4096];
    DWORD read_bytes;
    size_t out_sz = 0, err_sz = 0;
    char *out_buf = cimple_strdup("");
    char *err_buf = cimple_strdup("");
    while (ReadFile(out_r, tmpbuf, sizeof(tmpbuf)-1, &read_bytes, NULL) && read_bytes > 0) {
        tmpbuf[read_bytes] = '\0';
        out_buf = (char *)cimple_realloc(out_buf, out_sz + read_bytes + 1);
        memcpy(out_buf + out_sz, tmpbuf, read_bytes);
        out_sz += read_bytes;
        out_buf[out_sz] = '\0';
    }
    while (ReadFile(err_r, tmpbuf, sizeof(tmpbuf)-1, &read_bytes, NULL) && read_bytes > 0) {
        tmpbuf[read_bytes] = '\0';
        err_buf = (char *)cimple_realloc(err_buf, err_sz + read_bytes + 1);
        memcpy(err_buf + err_sz, tmpbuf, read_bytes);
        err_sz += read_bytes;
        err_buf[err_sz] = '\0';
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    status = (int)exit_code;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    CloseHandle(out_r); CloseHandle(err_r);
    free(cmdline);
    out_str = out_buf;
    err_str = err_buf;
#else
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0)
        error_runtime(line, col, "exec: pipe() failed");

    pid_t pid = fork();
    if (pid < 0) error_runtime(line, col, "exec: fork() failed");

    if (pid == 0) {
        /* child */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        if (envp) execve(argv[0], argv, envp);
        else      execvp(argv[0], argv);
        _exit(127);
    }
    close(out_pipe[1]); close(err_pipe[1]);

    /* Read stdout */
    size_t out_sz = 0, err_sz = 0;
    char buf[4096];
    out_str = cimple_strdup("");
    err_str = cimple_strdup("");

    for (;;) {
        ssize_t n = read(out_pipe[0], buf, sizeof(buf));
        if (n <= 0) break;
        out_str = (char *)cimple_realloc(out_str, out_sz + (size_t)n + 1);
        memcpy(out_str + out_sz, buf, (size_t)n);
        out_sz += (size_t)n;
        out_str[out_sz] = '\0';
    }
    for (;;) {
        ssize_t n = read(err_pipe[0], buf, sizeof(buf));
        if (n <= 0) break;
        err_str = (char *)cimple_realloc(err_str, err_sz + (size_t)n + 1);
        memcpy(err_str + err_sz, buf, (size_t)n);
        err_sz += (size_t)n;
        err_str[err_sz] = '\0';
    }
    close(out_pipe[0]); close(err_pipe[0]);

    int wstatus;
    waitpid(pid, &wstatus, 0);
    status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
#endif

    if (envp) free(envp);
    free(argv);
    return val_exec(status, out_str, err_str);
}
#else
/* WebAssembly — exec not supported */
static Value do_exec(Value *args, int nargs, int line, int col) {
    (void)args; (void)nargs;
    error_runtime(line, col, "exec: not supported on this platform");
    return val_void();
}
#endif /* __EMSCRIPTEN__ */

/* -----------------------------------------------------------------------
 * now() — milliseconds since Unix epoch
 * ----------------------------------------------------------------------- */
static int64_t get_now_ms(void) {
#ifdef __EMSCRIPTEN__
    extern double emscripten_date_now(void);
    return (int64_t)(emscripten_date_now());
#elif defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    int64_t t = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000LL; /* Jan 1, 1601 → Jan 1, 1970 (100ns units) */
    return t / 10000;          /* → milliseconds */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* -----------------------------------------------------------------------
 * Date helpers (UTC)
 * ----------------------------------------------------------------------- */
static int64_t days_from_civil(int year, int month, int day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)day - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int *year, int *month, int *day) {
    z += 719468;
    const int era = (int)(z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - (int64_t)era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = (int)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    *year = y + (m <= 2);
    *month = (int)m;
    *day = (int)d;
}

static int64_t make_epoch_utc_ms(int year, int month, int day, int hour, int minute, int second) {
    if (month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59) {
        return -1;
    }

    int64_t days = days_from_civil(year, month, day);
    int check_year, check_month, check_day;
    civil_from_days(days, &check_year, &check_month, &check_day);
    if (check_year != year || check_month != month || check_day != day) {
        return -1;
    }

    return (((days * 24 + hour) * 60 + minute) * 60 + second) * 1000;
}

static void epoch_to_tm(int64_t epochMs, struct tm *out) {
    time_t t = (time_t)(epochMs / 1000);
#ifdef _WIN32
    gmtime_s(out, &t);
#elif defined(__EMSCRIPTEN__)
    struct tm *tmp = gmtime(&t);
    if (!tmp) memset(out, 0, sizeof(*out));
    else *out = *tmp;
#else
    gmtime_r(&t, out);
#endif
}

/* -----------------------------------------------------------------------
 * formatDate
 * ----------------------------------------------------------------------- */
static char *format_date(int64_t epochMs, const char *fmt) {
    struct tm tm;
    epoch_to_tm(epochMs, &tm);
    size_t len   = strlen(fmt);
    size_t buflen = len * 4 + 64;
    char  *out   = (char *)cimple_malloc(buflen);
    size_t oi    = 0;

    for (size_t i = 0; i < len && oi < buflen - 10; i++) {
        /* Match tokens */
        if (strncmp(fmt + i, "YYYY", 4) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%04d", tm.tm_year + 1900);
            i += 3;
        } else if (strncmp(fmt + i, "MM", 2) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_mon + 1);
            i += 1;
        } else if (strncmp(fmt + i, "DD", 2) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_mday);
            i += 1;
        } else if (strncmp(fmt + i, "HH", 2) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_hour);
            i += 1;
        } else if (strncmp(fmt + i, "mm", 2) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_min);
            i += 1;
        } else if (strncmp(fmt + i, "ss", 2) == 0) {
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_sec);
            i += 1;
        } else {
            out[oi++] = fmt[i];
        }
    }
    out[oi] = '\0';
    return out;
}

/* -----------------------------------------------------------------------
 * toString helper (no dispatch overhead)
 * ----------------------------------------------------------------------- */
static char *val_to_string(Value *v, int line, int col) {
    char buf[64];
    switch (v->type) {
    case TYPE_INT:
        snprintf(buf, sizeof(buf), "%lld", (long long)v->u.i);
        return cimple_strdup(buf);
    case TYPE_FLOAT:
        if (isnan(v->u.f))  return cimple_strdup("NaN");
        if (isinf(v->u.f))  return cimple_strdup(v->u.f > 0 ? "Infinity" : "-Infinity");
        snprintf(buf, sizeof(buf), "%.17g", v->u.f);
        return cimple_strdup(buf);
    case TYPE_BOOL:
        return cimple_strdup(v->u.b ? "true" : "false");
    case TYPE_STRING:
        return cimple_strdup(v->u.s);
    default:
        error_runtime(line, col, "toString: unsupported type");
        return cimple_strdup("");
    }
}

/* -----------------------------------------------------------------------
 * Main dispatch function
 * ----------------------------------------------------------------------- */
Value builtin_call(const char *name, Value *args, int nargs, int line, int col) {

#define REQUIRE(n) do { if (nargs < (n)) \
    error_runtime(line, col, "%s: expected %d argument(s)", name, (n)); } while(0)
#define ARG_STR(idx)   args[(idx)].u.s
#define ARG_INT(idx)   args[(idx)].u.i
#define ARG_FLOAT(idx) args[(idx)].u.f
#define ARG_BOOL(idx)  args[(idx)].u.b
#define ARG_ARR(idx)   args[(idx)].u.arr

    /* ---- I/O ---- */
    if (strcmp(name, "print") == 0) {
        REQUIRE(1);
        fputs(args[0].u.s, stdout);
        return val_void();
    }

    if (strcmp(name, "input") == 0) {
        /* Read a line from stdin, strip trailing newline */
        size_t cap = 256;
        char  *buf = (char *)cimple_malloc(cap);
        size_t len = 0;
        int    c;
        while ((c = fgetc(stdin)) != EOF && c != '\n') {
            if (len + 1 >= cap) {
                cap *= 2;
                buf = (char *)cimple_realloc(buf, cap);
            }
            buf[len++] = (char)c;
        }
        /* Strip trailing \r */
        if (len > 0 && buf[len - 1] == '\r') len--;
        buf[len] = '\0';
        return val_string_own(buf);
    }

    /* ---- String ---- */
    if (strcmp(name, "len") == 0) {
        REQUIRE(1);
        return val_int((int64_t)strlen(ARG_STR(0)));
    }

    if (strcmp(name, "glyphLen") == 0) {
        REQUIRE(1);
        return val_int(utf8_codepoint_count(ARG_STR(0)));
    }

    if (strcmp(name, "glyphAt") == 0) {
        REQUIRE(2);
        char *s = utf8_codepoint_at(ARG_STR(0), (int)ARG_INT(1), line, col);
        return val_string_own(s);
    }

    if (strcmp(name, "byteAt") == 0) {
        REQUIRE(2);
        const char *s = ARG_STR(0);
        int         slen = (int)strlen(s);
        int         idx  = (int)ARG_INT(1);
        if (idx < 0 || idx >= slen)
            error_runtime(line, col,
                          "byteAt: index %d out of bounds (length=%d)", idx, slen);
        return val_int((unsigned char)s[idx]);
    }

    if (strcmp(name, "substr") == 0) {
        REQUIRE(3);
        const char *s    = ARG_STR(0);
        int         slen = (int)strlen(s);
        int         start = (int)ARG_INT(1);
        int         len2  = (int)ARG_INT(2);
        if (start < 0) error_runtime(line, col, "substr: negative start");
        if (start > slen) start = slen;
        int available = slen - start;
        if (len2 > available) len2 = available;
        if (len2 < 0) len2 = 0;
        char *out = cimple_strndup(s + start, (size_t)len2);
        return val_string_own(out);
    }

    if (strcmp(name, "indexOf") == 0) {
        REQUIRE(2);
        const char *found = strstr(ARG_STR(0), ARG_STR(1));
        return val_int(found ? (int64_t)(found - ARG_STR(0)) : -1);
    }

    if (strcmp(name, "contains") == 0) {
        REQUIRE(2);
        return val_bool(strstr(ARG_STR(0), ARG_STR(1)) != NULL);
    }

    if (strcmp(name, "startsWith") == 0) {
        REQUIRE(2);
        size_t plen = strlen(ARG_STR(1));
        return val_bool(strncmp(ARG_STR(0), ARG_STR(1), plen) == 0);
    }

    if (strcmp(name, "endsWith") == 0) {
        REQUIRE(2);
        size_t slen = strlen(ARG_STR(0));
        size_t plen = strlen(ARG_STR(1));
        if (plen > slen) return val_bool(0);
        return val_bool(strcmp(ARG_STR(0) + slen - plen, ARG_STR(1)) == 0);
    }

    if (strcmp(name, "replace") == 0) {
        REQUIRE(3);
        const char *s   = ARG_STR(0);
        const char *old = ARG_STR(1);
        const char *repl= ARG_STR(2);
        if (old[0] == '\0')
            error_runtime(line, col, "replace: old string must not be empty");
        size_t slen    = strlen(s);
        size_t oldlen  = strlen(old);
        size_t repllen = strlen(repl);
        /* Count occurrences */
        int count = 0;
        const char *p = s;
        while ((p = strstr(p, old))) { count++; p += oldlen; }
        /* Build result */
        size_t newlen = slen + (size_t)count * (repllen - oldlen);
        char *out = (char *)cimple_malloc(newlen + 1);
        char *dst = out;
        p = s;
        while (*p) {
            const char *found = strstr(p, old);
            if (!found) {
                strcpy(dst, p);
                dst += strlen(p);
                break;
            }
            memcpy(dst, p, (size_t)(found - p));
            dst += found - p;
            memcpy(dst, repl, repllen);
            dst += repllen;
            p = found + oldlen;
        }
        *dst = '\0';
        return val_string_own(out);
    }

    if (strcmp(name, "format") == 0) {
        REQUIRE(2);
        const char *tmpl = ARG_STR(0);
        ArrayVal   *arr  = ARG_ARR(1);
        /* Count {} placeholders */
        int ph_count = 0;
        for (const char *p = tmpl; *p; p++)
            if (p[0] == '{' && p[1] == '}') ph_count++;
        if (ph_count != arr->count)
            error_runtime(line, col,
                          "format: %d placeholder(s) but %d argument(s)",
                          ph_count, arr->count);
        /* Build result */
        size_t cap = strlen(tmpl) + 1;
        for (int i = 0; i < arr->count; i++) cap += strlen(arr->data.strings[i]);
        char *out = (char *)cimple_malloc(cap + 64);
        char *dst = out;
        const char *p = tmpl;
        int ai = 0;
        while (*p) {
            if (p[0] == '{' && p[1] == '}') {
                const char *sub = arr->data.strings[ai++];
                size_t sl = strlen(sub);
                memcpy(dst, sub, sl);
                dst += sl;
                p += 2;
            } else {
                *dst++ = *p++;
            }
        }
        *dst = '\0';
        return val_string_own(out);
    }

    if (strcmp(name, "join") == 0) {
        REQUIRE(2);
        ArrayVal *arr = ARG_ARR(0);
        const char *sep = ARG_STR(1);
        size_t seplen = strlen(sep);
        if (arr->count == 0) return val_string("");
        size_t total = 0;
        for (int i = 0; i < arr->count; i++) total += strlen(arr->data.strings[i]);
        total += (size_t)(arr->count - 1) * seplen;
        char *out = (char *)cimple_malloc(total + 1);
        char *dst = out;
        for (int i = 0; i < arr->count; i++) {
            if (i > 0) { memcpy(dst, sep, seplen); dst += seplen; }
            size_t sl = strlen(arr->data.strings[i]);
            memcpy(dst, arr->data.strings[i], sl);
            dst += sl;
        }
        *dst = '\0';
        return val_string_own(out);
    }

    if (strcmp(name, "split") == 0) {
        REQUIRE(2);
        const char *s   = ARG_STR(0);
        const char *sep = ARG_STR(1);
        if (sep[0] == '\0')
            error_runtime(line, col, "split: separator must not be empty");
        Value result = val_array(TYPE_STRING);
        if (s[0] == '\0') {
            Value empty_s = val_string("");
            array_push(result.u.arr, empty_s);
            value_free(&empty_s);
            return result;
        }
        size_t seplen = strlen(sep);
        const char *p = s;
        while (1) {
            const char *found = strstr(p, sep);
            if (!found) {
                Value part = val_string(p);
                array_push(result.u.arr, part);
                value_free(&part);
                break;
            }
            char *part_s = cimple_strndup(p, (size_t)(found - p));
            Value part = val_string_own(part_s);
            array_push(result.u.arr, part);
            value_free(&part);
            p = found + seplen;
        }
        return result;
    }

    if (strcmp(name, "concat") == 0) {
        REQUIRE(1);
        ArrayVal *arr = ARG_ARR(0);
        size_t total = 0;
        for (int i = 0; i < arr->count; i++) total += strlen(arr->data.strings[i]);
        char *out = (char *)cimple_malloc(total + 1);
        char *dst = out;
        for (int i = 0; i < arr->count; i++) {
            size_t sl = strlen(arr->data.strings[i]);
            memcpy(dst, arr->data.strings[i], sl);
            dst += sl;
        }
        *dst = '\0';
        return val_string_own(out);
    }

    /* ---- Conversions ---- */
    if (strcmp(name, "toString") == 0) {
        REQUIRE(1);
        char *s = val_to_string(&args[0], line, col);
        return val_string_own(s);
    }

    if (strcmp(name, "toInt") == 0) {
        REQUIRE(1);
        if (args[0].type == TYPE_FLOAT)
            return val_int((int64_t)args[0].u.f);
        if (args[0].type == TYPE_STRING) {
            char *end;
            int64_t v = strtoll(args[0].u.s, &end, 10);
            if (*args[0].u.s == '\0' || *end != '\0') return val_int(0);
            return val_int(v);
        }
        error_runtime(line, col, "toInt: unsupported type");
        return val_int(0);
    }

    if (strcmp(name, "toFloat") == 0) {
        REQUIRE(1);
        if (args[0].type == TYPE_INT)
            return val_float((double)args[0].u.i);
        if (args[0].type == TYPE_STRING) {
            char *end;
            double v = strtod(args[0].u.s, &end);
            if (*args[0].u.s == '\0' || *end != '\0') return val_float(NAN);
            return val_float(v);
        }
        error_runtime(line, col, "toFloat: unsupported type");
        return val_float(0.0);
    }

    if (strcmp(name, "toBool") == 0) {
        REQUIRE(1);
        const char *s = args[0].u.s;
        if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0) return val_bool(1);
        if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0) return val_bool(0);
        return val_bool(0);
    }

    /* ---- String validation ---- */
    if (strcmp(name, "isIntString") == 0) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        if (!s || !*s) return val_bool(0);
        char *end; strtoll(s, &end, 10);
        return val_bool(*end == '\0');
    }
    if (strcmp(name, "isFloatString") == 0) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        if (!s || !*s) return val_bool(0);
        char *end; strtod(s, &end);
        return val_bool(*end == '\0');
    }
    if (strcmp(name, "isBoolString") == 0) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        return val_bool(strcmp(s,"true")==0 || strcmp(s,"false")==0 ||
                        strcmp(s,"1")==0    || strcmp(s,"0")==0);
    }

    /* ---- Float math ---- */
    if (strcmp(name, "isNaN")              == 0) { REQUIRE(1); return val_bool(isnan(ARG_FLOAT(0))); }
    if (strcmp(name, "isInfinite")         == 0) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0))); }
    if (strcmp(name, "isFinite")           == 0) { REQUIRE(1); return val_bool(isfinite(ARG_FLOAT(0))); }
    if (strcmp(name, "isPositiveInfinity") == 0) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0)) && ARG_FLOAT(0) > 0); }
    if (strcmp(name, "isNegativeInfinity") == 0) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0)) && ARG_FLOAT(0) < 0); }
    if (strcmp(name, "abs")   == 0) { REQUIRE(1); return val_float(fabs(ARG_FLOAT(0))); }
    if (strcmp(name, "min")   == 0) { REQUIRE(2); return val_float(ARG_FLOAT(0) < ARG_FLOAT(1) ? ARG_FLOAT(0) : ARG_FLOAT(1)); }
    if (strcmp(name, "max")   == 0) { REQUIRE(2); return val_float(ARG_FLOAT(0) > ARG_FLOAT(1) ? ARG_FLOAT(0) : ARG_FLOAT(1)); }
    if (strcmp(name, "clamp") == 0) {
        REQUIRE(3);
        double v = ARG_FLOAT(0), lo = ARG_FLOAT(1), hi = ARG_FLOAT(2);
        return val_float(v < lo ? lo : v > hi ? hi : v);
    }
    if (strcmp(name, "floor")      == 0) { REQUIRE(1); return val_float(floor(ARG_FLOAT(0))); }
    if (strcmp(name, "ceil")       == 0) { REQUIRE(1); return val_float(ceil(ARG_FLOAT(0))); }
    if (strcmp(name, "round")      == 0) { REQUIRE(1); return val_float(round(ARG_FLOAT(0))); }
    if (strcmp(name, "trunc")      == 0) { REQUIRE(1); return val_float(trunc(ARG_FLOAT(0))); }
    if (strcmp(name, "fmod")       == 0) { REQUIRE(2); return val_float(fmod(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (strcmp(name, "sqrt")       == 0) { REQUIRE(1); return val_float(sqrt(ARG_FLOAT(0))); }
    if (strcmp(name, "pow")        == 0) { REQUIRE(2); return val_float(pow(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (strcmp(name, "approxEqual")== 0) {
        REQUIRE(3);
        return val_bool(fabs(ARG_FLOAT(0) - ARG_FLOAT(1)) <= ARG_FLOAT(2));
    }
    if (strcmp(name, "sin")  == 0) { REQUIRE(1); return val_float(sin(ARG_FLOAT(0))); }
    if (strcmp(name, "cos")  == 0) { REQUIRE(1); return val_float(cos(ARG_FLOAT(0))); }
    if (strcmp(name, "tan")  == 0) { REQUIRE(1); return val_float(tan(ARG_FLOAT(0))); }
    if (strcmp(name, "asin") == 0) { REQUIRE(1); return val_float(asin(ARG_FLOAT(0))); }
    if (strcmp(name, "acos") == 0) { REQUIRE(1); return val_float(acos(ARG_FLOAT(0))); }
    if (strcmp(name, "atan") == 0) { REQUIRE(1); return val_float(atan(ARG_FLOAT(0))); }
    if (strcmp(name, "atan2")== 0) { REQUIRE(2); return val_float(atan2(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (strcmp(name, "exp")  == 0) { REQUIRE(1); return val_float(exp(ARG_FLOAT(0))); }
    if (strcmp(name, "log")  == 0) { REQUIRE(1); return val_float(log(ARG_FLOAT(0))); }
    if (strcmp(name, "log2") == 0) { REQUIRE(1); return val_float(log2(ARG_FLOAT(0))); }
    if (strcmp(name, "log10")== 0) { REQUIRE(1); return val_float(log10(ARG_FLOAT(0))); }

    /* ---- Integer math ---- */
    if (strcmp(name, "absInt")  == 0) { REQUIRE(1); int64_t v = ARG_INT(0); return val_int(v < 0 ? -v : v); }
    if (strcmp(name, "minInt")  == 0) { REQUIRE(2); return val_int(ARG_INT(0) < ARG_INT(1) ? ARG_INT(0) : ARG_INT(1)); }
    if (strcmp(name, "maxInt")  == 0) { REQUIRE(2); return val_int(ARG_INT(0) > ARG_INT(1) ? ARG_INT(0) : ARG_INT(1)); }
    if (strcmp(name, "clampInt")== 0) {
        REQUIRE(3);
        int64_t v = ARG_INT(0), lo = ARG_INT(1), hi = ARG_INT(2);
        return val_int(v < lo ? lo : v > hi ? hi : v);
    }
    if (strcmp(name, "isEven") == 0) { REQUIRE(1); return val_bool(ARG_INT(0) % 2 == 0); }
    if (strcmp(name, "isOdd")  == 0) { REQUIRE(1); return val_bool(ARG_INT(0) % 2 != 0); }
    if (strcmp(name, "safeDivInt") == 0) {
        REQUIRE(3);
        if (ARG_INT(1) == 0) return val_int(ARG_INT(2));
        return val_int(ARG_INT(0) / ARG_INT(1));
    }
    if (strcmp(name, "safeModInt") == 0) {
        REQUIRE(3);
        if (ARG_INT(1) == 0) return val_int(ARG_INT(2));
        return val_int(ARG_INT(0) % ARG_INT(1));
    }

    /* ---- Array intrinsics ---- */
    if (strcmp(name, "count") == 0) {
        REQUIRE(1);
        return val_int(ARG_ARR(0)->count);
    }
    if (strcmp(name, "arrayPush") == 0) {
        REQUIRE(2);
        array_push(ARG_ARR(0), args[1]);
        return val_void();
    }
    if (strcmp(name, "arrayPop") == 0) {
        REQUIRE(1);
        return array_pop(ARG_ARR(0), line, col);
    }
    if (strcmp(name, "arrayInsert") == 0) {
        REQUIRE(3);
        array_insert(ARG_ARR(0), (int)ARG_INT(1), args[2], line, col);
        return val_void();
    }
    if (strcmp(name, "arrayRemove") == 0) {
        REQUIRE(2);
        array_remove(ARG_ARR(0), (int)ARG_INT(1), line, col);
        return val_void();
    }
    if (strcmp(name, "arrayGet") == 0) {
        REQUIRE(2);
        return array_get(ARG_ARR(0), (int)ARG_INT(1), line, col);
    }
    if (strcmp(name, "arraySet") == 0) {
        REQUIRE(3);
        array_set(ARG_ARR(0), (int)ARG_INT(1), args[2], line, col);
        return val_void();
    }

    /* ---- File I/O ---- */
    if (strcmp(name, "readFile") == 0) {
        REQUIRE(1);
        char *content = read_file_str(ARG_STR(0), line, col);
        return val_string_own(content);
    }
    if (strcmp(name, "writeFile") == 0) {
        REQUIRE(2);
        FILE *f = fopen(ARG_STR(0), "wb");
        if (!f) error_runtime(line, col, "writeFile: cannot open '%s'", ARG_STR(0));
        fputs(ARG_STR(1), f);
        fclose(f);
        return val_void();
    }
    if (strcmp(name, "appendFile") == 0) {
        REQUIRE(2);
        FILE *f = fopen(ARG_STR(0), "ab");
        if (!f) error_runtime(line, col, "appendFile: cannot open '%s'", ARG_STR(0));
        fputs(ARG_STR(1), f);
        fclose(f);
        return val_void();
    }
    if (strcmp(name, "fileExists") == 0) {
        REQUIRE(1);
        FILE *f = fopen(ARG_STR(0), "rb");
        if (f) { fclose(f); return val_bool(1); }
        return val_bool(0);
    }
    if (strcmp(name, "readLines") == 0) {
        REQUIRE(1);
        char *content = read_file_str(ARG_STR(0), line, col);
        Value result  = val_array(TYPE_STRING);
        char *p = content;
        char *start = p;
        while (*p) {
            if (*p == '\n' || (*p == '\r' && p[1] == '\n')) {
                char *end = p;
                char *line_s = cimple_strndup(start, (size_t)(end - start));
                Value lv = val_string_own(line_s);
                array_push(result.u.arr, lv);
                value_free(&lv);
                if (*p == '\r') p++;
                p++;
                start = p;
            } else if (*p == '\r') {
                char *line_s = cimple_strndup(start, (size_t)(p - start));
                Value lv = val_string_own(line_s);
                array_push(result.u.arr, lv);
                value_free(&lv);
                p++;
                start = p;
            } else {
                p++;
            }
        }
        if (start != p) {
            char *line_s = cimple_strndup(start, (size_t)(p - start));
            Value lv = val_string_own(line_s);
            array_push(result.u.arr, lv);
            value_free(&lv);
        }
        free(content);
        return result;
    }
    if (strcmp(name, "writeLines") == 0) {
        REQUIRE(2);
        FILE *f = fopen(ARG_STR(0), "wb");
        if (!f) error_runtime(line, col, "writeLines: cannot open '%s'", ARG_STR(0));
        ArrayVal *arr = ARG_ARR(1);
        for (int i = 0; i < arr->count; i++) {
            fputs(arr->data.strings[i], f);
            if (i < arr->count - 1) fputc('\n', f);
        }
        fclose(f);
        return val_void();
    }

    /* ---- exec ---- */
    if (strcmp(name, "exec") == 0) {
        REQUIRE(1);
        return do_exec(args, nargs, line, col);
    }
    if (strcmp(name, "execStatus") == 0) {
        REQUIRE(1);
        return val_int(args[0].u.exec.status);
    }
    if (strcmp(name, "execStdout") == 0) {
        REQUIRE(1);
        return val_string(args[0].u.exec.out ? args[0].u.exec.out : "");
    }
    if (strcmp(name, "execStderr") == 0) {
        REQUIRE(1);
        return val_string(args[0].u.exec.err ? args[0].u.exec.err : "");
    }

    /* ---- Environment ---- */
    if (strcmp(name, "getEnv") == 0) {
        REQUIRE(2);
#ifdef __EMSCRIPTEN__
        return val_string(ARG_STR(1));
#else
        const char *v = getenv(ARG_STR(0));
        if (!v) return val_string(ARG_STR(1));
        return val_string(v);
#endif
    }

    /* ---- Time ---- */
    if (strcmp(name, "now") == 0) {
        return val_int(get_now_ms());
    }
    if (strcmp(name, "epochToYear")  == 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_year + 1900); }
    if (strcmp(name, "epochToMonth") == 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_mon + 1); }
    if (strcmp(name, "epochToDay")   == 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_mday); }
    if (strcmp(name, "epochToHour")  == 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_hour); }
    if (strcmp(name, "epochToMinute")== 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_min); }
    if (strcmp(name, "epochToSecond")== 0) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_sec); }

    if (strcmp(name, "makeEpoch") == 0) {
        REQUIRE(6);
        int64_t ts = make_epoch_utc_ms((int)ARG_INT(0), (int)ARG_INT(1), (int)ARG_INT(2),
                                       (int)ARG_INT(3), (int)ARG_INT(4), (int)ARG_INT(5));
        return val_int(ts);
    }

    if (strcmp(name, "formatDate") == 0) {
        REQUIRE(2);
        char *s = format_date(ARG_INT(0), ARG_STR(1));
        return val_string_own(s);
    }

    return not_a_builtin();
}
