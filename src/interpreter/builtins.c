#include "builtins.h"
#include "interpreter.h"
#include "../common/error.h"
#include "../common/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include "../runtime/regex_engine.h"

/* Platform-specific includes */
#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  include <conio.h>
#else
#  include <sys/ioctl.h>
#  include <sys/select.h>
#  include <termios.h>
#  include <signal.h>
#  include <unistd.h>
#  include <spawn.h>
#  include <sys/wait.h>
#  include <fcntl.h>
#  include <sys/types.h>
extern char **environ;
#endif

static int g_temp_path_counter = 0;

enum {
    TERM_KEY_NONE = 0,
    TERM_KEY_CHAR = 1,
    TERM_KEY_ENTER = 2,
    TERM_KEY_ESC = 3,
    TERM_KEY_TAB = 4,
    TERM_KEY_BACKSPACE = 5,
    TERM_KEY_DELETE = 6,
    TERM_KEY_UP = 7,
    TERM_KEY_DOWN = 8,
    TERM_KEY_LEFT = 9,
    TERM_KEY_RIGHT = 10,
    TERM_KEY_HOME = 11,
    TERM_KEY_END = 12,
    TERM_KEY_PAGE_UP = 13,
    TERM_KEY_PAGE_DOWN = 14,
    TERM_KEY_RESIZE = 15
};

typedef struct {
    int enabled;
#ifdef _WIN32
    DWORD in_mode;
    DWORD out_mode;
#else
    struct termios saved;
#endif
} TerminalRawState;

static TerminalRawState g_terminal_raw_state = {0};
#ifndef _WIN32
static volatile sig_atomic_t g_terminal_resize_pending = 0;
#endif

typedef struct {
    const char *type;
    const char *path;
} RuntimeErrorOverride;

static RuntimeErrorOverride runtime_error_override_push(const char *type, const char *path) {
    RuntimeErrorOverride saved = { NULL, NULL };
    if (g_current_interp) {
        saved.type = g_current_interp->runtime_error_type;
        saved.path = g_current_interp->runtime_error_path;
        g_current_interp->runtime_error_type = type;
        g_current_interp->runtime_error_path = path;
    }
    return saved;
}

static void runtime_error_override_pop(RuntimeErrorOverride saved) {
    if (g_current_interp) {
        g_current_interp->runtime_error_type = saved.type;
        g_current_interp->runtime_error_path = saved.path;
    }
}

#ifndef _WIN32
static void terminal_sigwinch_handler(int sig) {
    (void)sig;
    g_terminal_resize_pending = 1;
}
#endif

static int terminal_is_tty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

static void terminal_require_available(int line, int col) {
#ifdef __EMSCRIPTEN__
    error_runtime(line, col, "terminal API is not available on this platform");
#else
    if (!terminal_is_tty())
        error_runtime(line, col, "terminal is not interactive");
#endif
}

static Value make_terminal_size_value(int width, int height) {
    Value out = val_struct("TerminalSize", 2);
    out.u.st->fields[0].name = cimple_strdup("width");
    out.u.st->fields[0].type = TYPE_INT;
    out.u.st->fields[0].value = val_int(width);
    out.u.st->fields[1].name = cimple_strdup("height");
    out.u.st->fields[1].type = TYPE_INT;
    out.u.st->fields[1].value = val_int(height);
    return out;
}

static Value make_key_event_value(int kind, int code, const char *text,
                                  int ctrl, int alt, int shift) {
    Value out = val_struct("KeyEvent", 6);
    out.u.st->fields[0].name = cimple_strdup("kind");
    out.u.st->fields[0].type = TYPE_INT;
    out.u.st->fields[0].value = val_int(kind);
    out.u.st->fields[1].name = cimple_strdup("code");
    out.u.st->fields[1].type = TYPE_INT;
    out.u.st->fields[1].value = val_int(code);
    out.u.st->fields[2].name = cimple_strdup("text");
    out.u.st->fields[2].type = TYPE_STRING;
    out.u.st->fields[2].value = val_string(text ? text : "");
    out.u.st->fields[3].name = cimple_strdup("ctrl");
    out.u.st->fields[3].type = TYPE_BOOL;
    out.u.st->fields[3].value = val_bool(ctrl);
    out.u.st->fields[4].name = cimple_strdup("alt");
    out.u.st->fields[4].type = TYPE_BOOL;
    out.u.st->fields[4].value = val_bool(alt);
    out.u.st->fields[5].name = cimple_strdup("shift");
    out.u.st->fields[5].type = TYPE_BOOL;
    out.u.st->fields[5].value = val_bool(shift);
    return out;
}

/* -----------------------------------------------------------------------
 * Sentinel for "not a builtin"
 * ----------------------------------------------------------------------- */
static Value not_a_builtin(void) {
    Value v; v.type = TYPE_UNKNOWN; v.u.i = 0; return v;
}

typedef enum {
    BI_NONE = 0,
    BI_PRINT,
    BI_INPUT,
    BI_TERM_IS_TTY,
    BI_TERM_GET_SIZE,
    BI_TERM_CLEAR,
    BI_TERM_CLEAR_LINE,
    BI_TERM_MOVE_CURSOR,
    BI_TERM_WRITE_AT,
    BI_TERM_HIDE_CURSOR,
    BI_TERM_SHOW_CURSOR,
    BI_TERM_FLUSH,
    BI_TERM_BEEP,
    BI_TERM_SET_STYLE,
    BI_TERM_RESET_STYLE,
    BI_TERM_ENABLE_RAW_MODE,
    BI_TERM_DISABLE_RAW_MODE,
    BI_TERM_READ_KEY,
    BI_TERM_POLL_KEY,
    BI_LEN,
    BI_GLYPH_LEN,
    BI_GLYPH_AT,
    BI_BYTE_AT,
    BI_SUBSTR,
    BI_INDEX_OF,
    BI_CONTAINS,
    BI_STARTS_WITH,
    BI_ENDS_WITH,
    BI_REPLACE,
    BI_FORMAT,
    BI_JOIN,
    BI_SPLIT,
    BI_CONCAT,
    BI_TRIM,
    BI_TRIM_LEFT,
    BI_TRIM_RIGHT,
    BI_TO_UPPER,
    BI_TO_LOWER,
    BI_CAPITALIZE,
    BI_PAD_LEFT,
    BI_PAD_RIGHT,
    BI_REPEAT,
    BI_LAST_INDEX_OF,
    BI_COUNT_OCCURRENCES,
    BI_IS_BLANK,
    BI_REGEX_COMPILE,
    BI_REGEX_PATTERN,
    BI_REGEX_FLAGS,
    BI_REGEX_TEST,
    BI_REGEX_FIND,
    BI_REGEX_FIND_ALL,
    BI_REGEX_REPLACE,
    BI_REGEX_REPLACE_ALL,
    BI_REGEX_SPLIT,
    BI_REGEX_OK,
    BI_REGEX_START,
    BI_REGEX_END,
    BI_REGEX_GROUPS,
    BI_TO_STRING,
    BI_TO_INT,
    BI_TO_FLOAT,
    BI_TO_BOOL,
    BI_BYTE_TO_INT,
    BI_INT_TO_BYTE,
    BI_STRING_TO_BYTES,
    BI_BYTES_TO_STRING,
    BI_INT_TO_BYTES,
    BI_FLOAT_TO_BYTES,
    BI_BYTES_TO_INT,
    BI_BYTES_TO_FLOAT,
    BI_IS_INT_STRING,
    BI_IS_FLOAT_STRING,
    BI_IS_BOOL_STRING,
    BI_IS_NAN,
    BI_IS_INFINITE,
    BI_IS_FINITE,
    BI_IS_POSITIVE_INFINITY,
    BI_IS_NEGATIVE_INFINITY,
    BI_ABS,
    BI_MIN,
    BI_MAX,
    BI_CLAMP,
    BI_FLOOR,
    BI_CEIL,
    BI_ROUND,
    BI_TRUNC,
    BI_FMOD,
    BI_SQRT,
    BI_POW,
    BI_APPROX_EQUAL,
    BI_SIN,
    BI_COS,
    BI_TAN,
    BI_ASIN,
    BI_ACOS,
    BI_ATAN,
    BI_ATAN2,
    BI_EXP,
    BI_LOG,
    BI_LOG2,
    BI_LOG10,
    BI_ABS_INT,
    BI_MIN_INT,
    BI_MAX_INT,
    BI_CLAMP_INT,
    BI_IS_EVEN,
    BI_IS_ODD,
    BI_SAFE_DIV_INT,
    BI_SAFE_MOD_INT,
    BI_COUNT,
    BI_MAP_HAS,
    BI_MAP_REMOVE,
    BI_MAP_KEYS,
    BI_MAP_VALUES,
    BI_MAP_SIZE,
    BI_MAP_CLEAR,
    BI_ARRAY_PUSH,
    BI_ARRAY_POP,
    BI_ARRAY_INSERT,
    BI_ARRAY_REMOVE,
    BI_ARRAY_GET,
    BI_ARRAY_SET,
    BI_READ_FILE,
    BI_WRITE_FILE,
    BI_APPEND_FILE,
    BI_READ_FILE_BYTES,
    BI_WRITE_FILE_BYTES,
    BI_APPEND_FILE_BYTES,
    BI_FILE_EXISTS,
    BI_TEMP_PATH,
    BI_REMOVE,
    BI_CHMOD,
    BI_CWD,
    BI_COPY,
    BI_MOVE,
    BI_IS_READABLE,
    BI_IS_WRITABLE,
    BI_IS_EXECUTABLE,
    BI_IS_DIRECTORY,
    BI_DIRNAME,
    BI_BASENAME,
    BI_FILENAME,
    BI_EXTENSION,
    BI_READ_LINES,
    BI_WRITE_LINES,
    BI_EXEC,
    BI_EXEC_STATUS,
    BI_EXEC_STDOUT,
    BI_EXEC_STDERR,
    BI_GET_ENV,
    BI_NOW,
    BI_EPOCH_TO_YEAR,
    BI_EPOCH_TO_MONTH,
    BI_EPOCH_TO_DAY,
    BI_EPOCH_TO_HOUR,
    BI_EPOCH_TO_MINUTE,
    BI_EPOCH_TO_SECOND,
    BI_MAKE_EPOCH,
    BI_FORMAT_DATE,
    /* Utility */
    BI_ASSERT,
    BI_RAND_INT,
    BI_RAND_FLOAT,
    BI_SLEEP
} BuiltinId;

typedef struct {
    const char *name;
    BuiltinId id;
} BuiltinNameEntry;

static const BuiltinNameEntry BUILTIN_NAME_TABLE[] = {
    {"abs", BI_ABS},
    {"absInt", BI_ABS_INT},
    {"assert", BI_ASSERT},
    {"acos", BI_ACOS},
    {"appendFile", BI_APPEND_FILE},
    {"approxEqual", BI_APPROX_EQUAL},
    {"arrayGet", BI_ARRAY_GET},
    {"arrayInsert", BI_ARRAY_INSERT},
    {"arrayPop", BI_ARRAY_POP},
    {"arrayPush", BI_ARRAY_PUSH},
    {"arrayRemove", BI_ARRAY_REMOVE},
    {"arraySet", BI_ARRAY_SET},
    {"asin", BI_ASIN},
    {"atan", BI_ATAN},
    {"atan2", BI_ATAN2},
    {"basename", BI_BASENAME},
    {"byteToInt", BI_BYTE_TO_INT},
    {"byteAt", BI_BYTE_AT},
    {"bytesToFloat", BI_BYTES_TO_FLOAT},
    {"bytesToInt", BI_BYTES_TO_INT},
    {"bytesToString", BI_BYTES_TO_STRING},
    {"ceil", BI_CEIL},
    {"chmod", BI_CHMOD},
    {"clamp", BI_CLAMP},
    {"clampInt", BI_CLAMP_INT},
    {"concat", BI_CONCAT},
    {"contains", BI_CONTAINS},
    {"countOccurrences", BI_COUNT_OCCURRENCES},
    {"copy", BI_COPY},
    {"cos", BI_COS},
    {"count", BI_COUNT},
    {"mapClear", BI_MAP_CLEAR},
    {"mapHas", BI_MAP_HAS},
    {"mapKeys", BI_MAP_KEYS},
    {"mapRemove", BI_MAP_REMOVE},
    {"mapSize", BI_MAP_SIZE},
    {"mapValues", BI_MAP_VALUES},
    {"cwd", BI_CWD},
    {"dirname", BI_DIRNAME},
    {"endsWith", BI_ENDS_WITH},
    {"epochToDay", BI_EPOCH_TO_DAY},
    {"epochToHour", BI_EPOCH_TO_HOUR},
    {"epochToMinute", BI_EPOCH_TO_MINUTE},
    {"epochToMonth", BI_EPOCH_TO_MONTH},
    {"epochToSecond", BI_EPOCH_TO_SECOND},
    {"epochToYear", BI_EPOCH_TO_YEAR},
    {"exec", BI_EXEC},
    {"execStatus", BI_EXEC_STATUS},
    {"execStderr", BI_EXEC_STDERR},
    {"execStdout", BI_EXEC_STDOUT},
    {"exp", BI_EXP},
    {"extension", BI_EXTENSION},
    {"fileExists", BI_FILE_EXISTS},
    {"filename", BI_FILENAME},
    {"floor", BI_FLOOR},
    {"fmod", BI_FMOD},
    {"format", BI_FORMAT},
    {"formatDate", BI_FORMAT_DATE},
    {"getEnv", BI_GET_ENV},
    {"glyphAt", BI_GLYPH_AT},
    {"glyphLen", BI_GLYPH_LEN},
    {"indexOf", BI_INDEX_OF},
    {"input", BI_INPUT},
    {"intToByte", BI_INT_TO_BYTE},
    {"intToBytes", BI_INT_TO_BYTES},
    {"isBoolString", BI_IS_BOOL_STRING},
    {"isDirectory", BI_IS_DIRECTORY},
    {"isEven", BI_IS_EVEN},
    {"isExecutable", BI_IS_EXECUTABLE},
    {"isFinite", BI_IS_FINITE},
    {"isFloatString", BI_IS_FLOAT_STRING},
    {"isInfinite", BI_IS_INFINITE},
    {"isIntString", BI_IS_INT_STRING},
    {"isNaN", BI_IS_NAN},
    {"isNegativeInfinity", BI_IS_NEGATIVE_INFINITY},
    {"isOdd", BI_IS_ODD},
    {"isPositiveInfinity", BI_IS_POSITIVE_INFINITY},
    {"isBlank", BI_IS_BLANK},
    {"isReadable", BI_IS_READABLE},
    {"isWritable", BI_IS_WRITABLE},
    {"join", BI_JOIN},
    {"lastIndexOf", BI_LAST_INDEX_OF},
    {"len", BI_LEN},
    {"log", BI_LOG},
    {"log10", BI_LOG10},
    {"log2", BI_LOG2},
    {"makeEpoch", BI_MAKE_EPOCH},
    {"max", BI_MAX},
    {"maxInt", BI_MAX_INT},
    {"min", BI_MIN},
    {"minInt", BI_MIN_INT},
    {"move", BI_MOVE},
    {"now", BI_NOW},
    {"padLeft", BI_PAD_LEFT},
    {"padRight", BI_PAD_RIGHT},
    {"pow", BI_POW},
    {"print", BI_PRINT},
    {"readFile", BI_READ_FILE},
    {"readFileBytes", BI_READ_FILE_BYTES},
    {"readLines", BI_READ_LINES},
    {"remove", BI_REMOVE},
    {"regexCompile", BI_REGEX_COMPILE},
    {"regexEnd", BI_REGEX_END},
    {"regexFind", BI_REGEX_FIND},
    {"regexFindAll", BI_REGEX_FIND_ALL},
    {"regexFlags", BI_REGEX_FLAGS},
    {"regexGroups", BI_REGEX_GROUPS},
    {"regexOk", BI_REGEX_OK},
    {"regexPattern", BI_REGEX_PATTERN},
    {"regexReplace", BI_REGEX_REPLACE},
    {"regexReplaceAll", BI_REGEX_REPLACE_ALL},
    {"regexSplit", BI_REGEX_SPLIT},
    {"regexStart", BI_REGEX_START},
    {"regexTest", BI_REGEX_TEST},
    {"replace", BI_REPLACE},
    {"repeat", BI_REPEAT},
    {"randFloat", BI_RAND_FLOAT},
    {"randInt", BI_RAND_INT},
    {"round", BI_ROUND},
    {"safeDivInt", BI_SAFE_DIV_INT},
    {"safeModInt", BI_SAFE_MOD_INT},
    {"sin", BI_SIN},
    {"sleep", BI_SLEEP},
    {"split", BI_SPLIT},
    {"sqrt", BI_SQRT},
    {"startsWith", BI_STARTS_WITH},
    {"substr", BI_SUBSTR},
    {"tan", BI_TAN},
    {"tempPath", BI_TEMP_PATH},
    {"termBeep", BI_TERM_BEEP},
    {"termClear", BI_TERM_CLEAR},
    {"termClearLine", BI_TERM_CLEAR_LINE},
    {"termDisableRawMode", BI_TERM_DISABLE_RAW_MODE},
    {"termEnableRawMode", BI_TERM_ENABLE_RAW_MODE},
    {"termFlush", BI_TERM_FLUSH},
    {"termGetSize", BI_TERM_GET_SIZE},
    {"termHideCursor", BI_TERM_HIDE_CURSOR},
    {"termIsTTY", BI_TERM_IS_TTY},
    {"termMoveCursor", BI_TERM_MOVE_CURSOR},
    {"termPollKey", BI_TERM_POLL_KEY},
    {"termReadKey", BI_TERM_READ_KEY},
    {"termResetStyle", BI_TERM_RESET_STYLE},
    {"termSetStyle", BI_TERM_SET_STYLE},
    {"termShowCursor", BI_TERM_SHOW_CURSOR},
    {"termWriteAt", BI_TERM_WRITE_AT},
    {"toLower", BI_TO_LOWER},
    {"toBool", BI_TO_BOOL},
    {"toUpper", BI_TO_UPPER},
    {"stringToBytes", BI_STRING_TO_BYTES},
    {"toFloat", BI_TO_FLOAT},
    {"toInt", BI_TO_INT},
    {"toString", BI_TO_STRING},
    {"trim", BI_TRIM},
    {"trimLeft", BI_TRIM_LEFT},
    {"trimRight", BI_TRIM_RIGHT},
    {"capitalize", BI_CAPITALIZE},
    {"trunc", BI_TRUNC},
    {"writeFile", BI_WRITE_FILE},
    {"appendFileBytes", BI_APPEND_FILE_BYTES},
    {"floatToBytes", BI_FLOAT_TO_BYTES},
    {"writeFileBytes", BI_WRITE_FILE_BYTES},
    {"writeLines", BI_WRITE_LINES}
};

static BuiltinId builtin_lookup_id(const char *name) {
    for (size_t i = 0; i < sizeof(BUILTIN_NAME_TABLE) / sizeof(BUILTIN_NAME_TABLE[0]); i++) {
        if (strcmp(BUILTIN_NAME_TABLE[i].name, name) == 0) return BUILTIN_NAME_TABLE[i].id;
    }
    return BI_NONE;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder *sb, size_t initial_cap) {
    sb->cap = initial_cap ? initial_cap : 32;
    sb->len = 0;
    sb->data = (char *)cimple_malloc(sb->cap);
    sb->data[0] = '\0';
}

static void sb_reserve(StringBuilder *sb, size_t extra) {
    size_t needed = sb->len + extra + 1;
    if (needed <= sb->cap) return;
    while (sb->cap < needed) sb->cap *= 2;
    sb->data = (char *)cimple_realloc(sb->data, sb->cap);
}

static void sb_append_mem(StringBuilder *sb, const char *data, size_t len) {
    sb_reserve(sb, len);
    memcpy(sb->data + sb->len, data, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

static void sb_append_cstr(StringBuilder *sb, const char *s) {
    sb_append_mem(sb, s, strlen(s));
}

static char *sb_take(StringBuilder *sb) {
    char *out = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    return out;
}

static Value terminal_get_size_value(void) {
#ifdef __EMSCRIPTEN__
    return make_terminal_size_value(0, 0);
#elif defined(_WIN32)
    if (!terminal_is_tty()) return make_terminal_size_value(0, 0);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return make_terminal_size_value(0, 0);
    return make_terminal_size_value(
        (int)(info.srWindow.Right - info.srWindow.Left + 1),
        (int)(info.srWindow.Bottom - info.srWindow.Top + 1));
#else
    if (!terminal_is_tty()) return make_terminal_size_value(0, 0);
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
        return make_terminal_size_value(0, 0);
    return make_terminal_size_value((int)ws.ws_col, (int)ws.ws_row);
#endif
}

static void terminal_restore_raw_mode(void) {
#ifdef __EMSCRIPTEN__
    return;
#elif defined(_WIN32)
    if (!g_terminal_raw_state.enabled) return;
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_terminal_raw_state.in_mode);
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), g_terminal_raw_state.out_mode);
    g_terminal_raw_state.enabled = 0;
#else
    if (!g_terminal_raw_state.enabled) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_terminal_raw_state.saved);
    g_terminal_raw_state.enabled = 0;
#endif
}

static void terminal_enable_raw_mode(int line, int col) {
#ifdef __EMSCRIPTEN__
    error_runtime(line, col, "terminal API is not available on this platform");
#elif defined(_WIN32)
    terminal_require_available(line, col);
    if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return;
    if (g_terminal_raw_state.enabled) return;
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD in_mode = 0, out_mode = 0;
    if (!GetConsoleMode(hin, &in_mode) || !GetConsoleMode(hout, &out_mode)) {
        error_runtime(line, col, "terminal raw mode is not supported on this platform");
        return;
    }
    g_terminal_raw_state.in_mode = in_mode;
    g_terminal_raw_state.out_mode = out_mode;
    DWORD new_in_mode = in_mode;
    new_in_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    new_in_mode |= ENABLE_EXTENDED_FLAGS;
    DWORD new_out_mode = out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hin, new_in_mode) || !SetConsoleMode(hout, new_out_mode)) {
        error_runtime(line, col, "terminal raw mode is not supported on this platform");
        return;
    }
    g_terminal_raw_state.enabled = 1;
    atexit(terminal_restore_raw_mode);
#else
    terminal_require_available(line, col);
    if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return;
    if (g_terminal_raw_state.enabled) return;
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &g_terminal_raw_state.saved) != 0) {
        error_runtime(line, col, "terminal raw mode is not supported on this platform");
        return;
    }
    raw = g_terminal_raw_state.saved;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= (tcflag_t) (CS8);
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        error_runtime(line, col, "terminal raw mode is not supported on this platform");
        return;
    }
    signal(SIGWINCH, terminal_sigwinch_handler);
    g_terminal_raw_state.enabled = 1;
    atexit(terminal_restore_raw_mode);
#endif
}

static void terminal_disable_raw_mode(void) {
    terminal_restore_raw_mode();
}

static int terminal_style_color_valid(int color) {
    return color == -1 || (color >= 0 && color <= 7);
}

static void terminal_emit_style(int fg, int bg, int bold, int reverse, int underline) {
    printf("\033[0");
    if (bold) printf(";1");
    if (underline) printf(";4");
    if (reverse) printf(";7");
    if (fg == -1) printf(";39");
    else printf(";%d", 30 + fg);
    if (bg == -1) printf(";49");
    else printf(";%d", 40 + bg);
    printf("m");
}

#ifndef _WIN32
static Value terminal_read_key_posix(int timeout_ms) {
    if (g_terminal_resize_pending) {
        g_terminal_resize_pending = 0;
        return make_key_event_value(TERM_KEY_RESIZE, TERM_KEY_RESIZE, "", 0, 0, 0);
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }
    int sel = select(STDIN_FILENO + 1, &set, NULL, NULL, tv_ptr);
    if (sel <= 0) return make_key_event_value(TERM_KEY_NONE, 0, "", 0, 0, 0);

    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) != 1)
        return make_key_event_value(TERM_KEY_NONE, 0, "", 0, 0, 0);

    if (ch == '\r' || ch == '\n') return make_key_event_value(TERM_KEY_ENTER, TERM_KEY_ENTER, "", 0, 0, 0);
    if (ch == '\t') return make_key_event_value(TERM_KEY_TAB, TERM_KEY_TAB, "", 0, 0, 0);
    if (ch == 127 || ch == 8) return make_key_event_value(TERM_KEY_BACKSPACE, TERM_KEY_BACKSPACE, "", 0, 0, 0);
    if (ch == 27) {
        unsigned char seq[3] = {0, 0, 0};
        int have1 = read(STDIN_FILENO, &seq[0], 1) == 1;
        if (!have1) return make_key_event_value(TERM_KEY_ESC, TERM_KEY_ESC, "", 0, 0, 0);
        int have2 = read(STDIN_FILENO, &seq[1], 1) == 1;
        if (seq[0] == '[' && have2) {
            switch (seq[1]) {
            case 'A': return make_key_event_value(TERM_KEY_UP, TERM_KEY_UP, "", 0, 0, 0);
            case 'B': return make_key_event_value(TERM_KEY_DOWN, TERM_KEY_DOWN, "", 0, 0, 0);
            case 'C': return make_key_event_value(TERM_KEY_RIGHT, TERM_KEY_RIGHT, "", 0, 0, 0);
            case 'D': return make_key_event_value(TERM_KEY_LEFT, TERM_KEY_LEFT, "", 0, 0, 0);
            case 'H': return make_key_event_value(TERM_KEY_HOME, TERM_KEY_HOME, "", 0, 0, 0);
            case 'F': return make_key_event_value(TERM_KEY_END, TERM_KEY_END, "", 0, 0, 0);
            case '3':
                if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~')
                    return make_key_event_value(TERM_KEY_DELETE, TERM_KEY_DELETE, "", 0, 0, 0);
                break;
            case '5':
                if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~')
                    return make_key_event_value(TERM_KEY_PAGE_UP, TERM_KEY_PAGE_UP, "", 0, 0, 0);
                break;
            case '6':
                if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~')
                    return make_key_event_value(TERM_KEY_PAGE_DOWN, TERM_KEY_PAGE_DOWN, "", 0, 0, 0);
                break;
            default:
                break;
            }
        }
        return make_key_event_value(TERM_KEY_ESC, TERM_KEY_ESC, "", 0, 0, 0);
    }

    if (ch < 32) {
        char text[2] = { (char)(ch + '@'), '\0' };
        return make_key_event_value(TERM_KEY_CHAR, 0, text, 1, 0, 0);
    }

    {
        char text[2] = { (char)ch, '\0' };
        return make_key_event_value(TERM_KEY_CHAR, 0, text, 0, 0, 0);
    }
}
#endif

static Value terminal_read_key_value(int timeout_ms) {
#ifdef __EMSCRIPTEN__
    return make_key_event_value(TERM_KEY_NONE, 0, "", 0, 0, 0);
#elif defined(_WIN32)
    if (timeout_ms >= 0) {
        DWORD waited = 0;
        while (!_kbhit()) {
            if (waited >= (DWORD)timeout_ms)
                return make_key_event_value(TERM_KEY_NONE, 0, "", 0, 0, 0);
            Sleep(1);
            waited++;
        }
    } else {
        while (!_kbhit()) Sleep(1);
    }
    int ch = _getch();
    if (ch == 13) return make_key_event_value(TERM_KEY_ENTER, TERM_KEY_ENTER, "", 0, 0, 0);
    if (ch == 9) return make_key_event_value(TERM_KEY_TAB, TERM_KEY_TAB, "", 0, 0, 0);
    if (ch == 8) return make_key_event_value(TERM_KEY_BACKSPACE, TERM_KEY_BACKSPACE, "", 0, 0, 0);
    if (ch == 27) return make_key_event_value(TERM_KEY_ESC, TERM_KEY_ESC, "", 0, 0, 0);
    if (ch == 0 || ch == 224) {
        int code = _getch();
        switch (code) {
        case 72: return make_key_event_value(TERM_KEY_UP, TERM_KEY_UP, "", 0, 0, 0);
        case 80: return make_key_event_value(TERM_KEY_DOWN, TERM_KEY_DOWN, "", 0, 0, 0);
        case 75: return make_key_event_value(TERM_KEY_LEFT, TERM_KEY_LEFT, "", 0, 0, 0);
        case 77: return make_key_event_value(TERM_KEY_RIGHT, TERM_KEY_RIGHT, "", 0, 0, 0);
        case 71: return make_key_event_value(TERM_KEY_HOME, TERM_KEY_HOME, "", 0, 0, 0);
        case 79: return make_key_event_value(TERM_KEY_END, TERM_KEY_END, "", 0, 0, 0);
        case 73: return make_key_event_value(TERM_KEY_PAGE_UP, TERM_KEY_PAGE_UP, "", 0, 0, 0);
        case 81: return make_key_event_value(TERM_KEY_PAGE_DOWN, TERM_KEY_PAGE_DOWN, "", 0, 0, 0);
        case 83: return make_key_event_value(TERM_KEY_DELETE, TERM_KEY_DELETE, "", 0, 0, 0);
        default: return make_key_event_value(TERM_KEY_NONE, 0, "", 0, 0, 0);
        }
    }
    {
        char text[2] = { (char)ch, '\0' };
        return make_key_event_value(TERM_KEY_CHAR, 0, text, 0, 0, 0);
    }
#else
    return terminal_read_key_posix(timeout_ms);
#endif
}

/* -----------------------------------------------------------------------
 * NFC normalisation via utf8proc (JuliaStrings/utf8proc v2.9.0, MIT licence)
 * glyphLen and glyphAt normalise to NFC before counting / indexing, so that
 * e.g. NFD "e\u0301" (2 code points) is treated as NFC "é" (1 glyph).
 * ----------------------------------------------------------------------- */
#include "utf8proc.h"

/* Return a freshly-malloc'd NFC form of s (caller must free). */
static char *nfc_normalize(const char *s) {
    uint8_t *out = NULL;
    utf8proc_map((const utf8proc_uint8_t *)s, 0, &out,
                 UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_COMPOSE);
    if (out) return (char *)out;
    /* fallback: return a plain copy on error */
    size_t n = strlen(s) + 1;
    char *cp = (char *)cimple_malloc(n);
    memcpy(cp, s, n);
    return cp;
}

/* Count NFC code points in s. */
static int utf8_codepoint_count(const char *s) {
    char *n = nfc_normalize(s);
    utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(n);
    int count = 0;
    utf8proc_ssize_t pos = 0;
    utf8proc_int32_t cp;
    while (pos < len) {
        utf8proc_ssize_t r = utf8proc_iterate(
            (const utf8proc_uint8_t *)n + pos, len - pos, &cp);
        if (r <= 0) break;
        pos += r; count++;
    }
    free(n);
    return count;
}

/* Return the NFC code-point string at index (caller must free). */
static char *utf8_codepoint_at(const char *s, int index, int line, int col) {
    char *n = nfc_normalize(s);
    utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(n);
    int total = utf8_codepoint_count(n);   /* count on already-NFC string */
    if (index < 0 || index >= total) {
        free(n);
        error_runtime(line, col,
                      "glyphAt: index out of bounds (Index: %d   Glyph count: %d)",
                      index, total);
        return NULL;
    }
    /* advance to the requested code point */
    utf8proc_ssize_t pos = 0;
    for (int i = 0; i < index; i++) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t r = utf8proc_iterate(
            (const utf8proc_uint8_t *)n + pos, len - pos, &cp);
        if (r <= 0) break;
        pos += r;
    }
    /* measure the code point at pos */
    utf8proc_int32_t cp;
    utf8proc_ssize_t cplen = utf8proc_iterate(
        (const utf8proc_uint8_t *)n + pos, len - pos, &cp);
    if (cplen <= 0) cplen = 1;
    char *result = (char *)cimple_malloc((size_t)cplen + 1);
    memcpy(result, n + pos, (size_t)cplen);
    result[cplen] = '\0';
    free(n);
    return result;
}

/* -----------------------------------------------------------------------
 * File helpers
 * ----------------------------------------------------------------------- */
static char *read_file_str(const char *path, int line, int col) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        error_runtime(line, col, "Cannot read file: '%s'", path);
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        error_runtime(line, col, "Cannot read file: '%s': %s",
                      path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char *)cimple_malloc((size_t)size + 1);
    if ((long)fread(buf, 1, (size_t)size, f) != size) {
        fclose(f);
        free(buf);
        error_runtime(line, col, "Cannot read file: '%s'", path);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char *cwd_string(void) {
#ifdef _WIN32
    DWORD size = GetCurrentDirectoryA(0, NULL);
    char *buf = (char *)cimple_malloc((size_t)size + 1);
    GetCurrentDirectoryA(size, buf);
    return buf;
#else
    size_t cap = 256;
    for (;;) {
        char *buf = (char *)cimple_malloc(cap);
        if (getcwd(buf, cap)) return buf;
        free(buf);
        if (errno != ERANGE) return cimple_strdup("");
        cap *= 2;
    }
#endif
}

static char *temp_path_string(void) {
#ifdef _WIN32
    const char *base = getenv("TEMP");
    if (!base || !*base) base = getenv("TMP");
    if (!base || !*base) base = ".";
    int pid = (int)GetCurrentProcessId();
#else
    const char *base = getenv("TMPDIR");
    if (!base || !*base) base = "/tmp";
    int pid = (int)getpid();
#endif
    for (;;) {
        int n = ++g_temp_path_counter;
        size_t need = strlen(base) + 64;
        char *out = (char *)cimple_malloc(need);
        struct stat st;
        snprintf(out, need, "%s/cimple_%d_%d.tmp", base, pid, n);
        if (stat(out, &st) != 0) return out;
        free(out);
    }
}

static char *path_dirname_lexical(const char *path) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;
    if (len == 1 && path[0] == '/') return cimple_strdup("/");
    const char *slash = NULL;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') slash = path + i;
    }
    if (!slash) return cimple_strdup("");
    if (slash == path) return cimple_strdup("/");
    return cimple_strndup(path, (size_t)(slash - path));
}

static char *path_basename_lexical(const char *path) {
    size_t len = strlen(path);
    if (len == 0) return cimple_strdup("");
    if (len > 1 && path[len - 1] == '/') return cimple_strdup("");
    const char *slash = strrchr(path, '/');
    return cimple_strdup(slash ? slash + 1 : path);
}

static char *parent_dir_for_write(const char *path) {
    char *dir = path_dirname_lexical(path);
    if (dir[0] == '\0') {
        free(dir);
        return cimple_strdup(".");
    }
    return dir;
}

static void copy_file_contents(const char *src, const char *dst,
                               const char *op, int line, int col) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        error_runtime_hint(line, col, strerror(errno),
                           "Cannot %s: '%s' -> '%s'", op, src, dst);
        return;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        error_runtime_hint(line, col, strerror(errno),
                           "Cannot %s: '%s' -> '%s'", op, src, dst);
        return;
    }
    char buf[4096];
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n > 0 && fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot %s: '%s' -> '%s'", op, src, dst);
            return;
        }
        if (n < sizeof(buf)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                error_runtime_hint(line, col, strerror(errno),
                                   "Cannot %s: '%s' -> '%s'", op, src, dst);
                return;
            }
            break;
        }
    }
    fclose(in);
    fclose(out);
}

static Value read_file_bytes(const char *path, int line, int col) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        error_runtime(line, col, "Cannot read file: '%s'", path);
        return val_void();
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        error_runtime(line, col, "Cannot read file: '%s'", path);
        return val_void();
    }
    long size = ftell(f);
    rewind(f);
    Value out = val_array(TYPE_BYTE);
    out.u.arr->cap = (int)size;
    out.u.arr->data.bytes = size > 0 ? (unsigned char *)cimple_malloc((size_t)size) : NULL;
    out.u.arr->count = (int)size;
    if (size > 0 && fread(out.u.arr->data.bytes, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        value_free(&out);
        error_runtime(line, col, "Cannot read file: '%s'", path);
        return val_void();
    }
    fclose(f);
    return out;
}

static void write_file_bytes(const char *path, ArrayVal *arr, const char *mode, int line, int col) {
    FILE *f = fopen(path, mode);
    if (!f) {
        error_runtime(line, col, "Cannot write file: '%s'", path);
        return;
    }
    if (arr->count > 0 && fwrite(arr->data.bytes, 1, (size_t)arr->count, f) != (size_t)arr->count) {
        fclose(f);
        error_runtime(line, col, "Cannot write file: '%s'", path);
        return;
    }
    fclose(f);
}

static char *bytes_to_string_lossy(ArrayVal *arr) {
    StringBuilder sb;
    sb_init(&sb, (size_t)arr->count + 1);
    int i = 0;
    while (i < arr->count) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate((const utf8proc_uint8_t *)&arr->data.bytes[i], arr->count - i, &cp);
        if (n > 0) {
            sb_append_mem(&sb, (const char *)&arr->data.bytes[i], (size_t)n);
            i += (int)n;
        } else {
            sb_append_mem(&sb, "\xEF\xBF\xBD", 3);
            i++;
        }
    }
    return sb_take(&sb);
}

static int is_blank_ascii(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static char *trim_ascii_edges(const char *s, int trim_left, int trim_right) {
    size_t start = 0;
    size_t end = strlen(s);
    if (trim_left) {
        while (start < end && is_blank_ascii((unsigned char)s[start])) start++;
    }
    if (trim_right) {
        while (end > start && is_blank_ascii((unsigned char)s[end - 1])) end--;
    }
    return cimple_strndup(s + start, end - start);
}

static void sb_append_codepoint(StringBuilder *sb, utf8proc_int32_t cp) {
    utf8proc_uint8_t buf[4];
    utf8proc_ssize_t n = utf8proc_encode_char(cp, buf);
    if (n <= 0) return;
    sb_append_mem(sb, (const char *)buf, (size_t)n);
}

static char *transform_case_utf8(const char *s, int make_upper, int capitalize_first_only) {
    StringBuilder sb;
    sb_init(&sb, strlen(s) + 8);
    utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(s);
    utf8proc_ssize_t pos = 0;
    int first = 1;
    while (pos < len) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate((const utf8proc_uint8_t *)s + pos, len - pos, &cp);
        if (n <= 0) {
            sb_append_mem(&sb, s + pos, 1);
            pos++;
            continue;
        }
        pos += n;
        if (make_upper && cp == 0x00DF) {
            if (!capitalize_first_only || first) sb_append_cstr(&sb, "SS");
            else sb_append_mem(&sb, (const char *)"ß", 2);
        } else {
            utf8proc_int32_t mapped = cp;
            if (make_upper) {
                if (!capitalize_first_only || first) mapped = utf8proc_toupper(cp);
            } else {
                mapped = utf8proc_tolower(cp);
            }
            sb_append_codepoint(&sb, mapped);
        }
        first = 0;
    }
    return sb_take(&sb);
}

static char *pad_to_width(const char *s, int width, const char *pad, int pad_left, int line, int col, const char *name) {
    if (width < 0) {
        error_runtime(line, col, "%s: width must be non-negative", name);
        return NULL;
    }
    if (pad[0] == '\0') return cimple_strdup(s);
    int current = utf8_codepoint_count(s);
    if (current >= width) return cimple_strdup(s);
    int needed = width - current;
    int pad_glyphs = utf8_codepoint_count(pad);
    if (pad_glyphs <= 0) return cimple_strdup(s);

    char *pad_norm = nfc_normalize(pad);
    utf8proc_ssize_t pad_len = (utf8proc_ssize_t)strlen(pad_norm);
    utf8proc_ssize_t pos = 0;
    int produced = 0;
    StringBuilder fill;
    sb_init(&fill, (size_t)(needed * 4 + 1));
    while (produced < needed) {
        if (pos >= pad_len) pos = 0;
        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate((const utf8proc_uint8_t *)pad_norm + pos, pad_len - pos, &cp);
        if (n <= 0) break;
        pos += n;
        sb_append_codepoint(&fill, cp);
        produced++;
    }
    free(pad_norm);

    StringBuilder out;
    sb_init(&out, strlen(s) + fill.len + 1);
    if (pad_left) {
        sb_append_mem(&out, fill.data, fill.len);
        sb_append_cstr(&out, s);
    } else {
        sb_append_cstr(&out, s);
        sb_append_mem(&out, fill.data, fill.len);
    }
    free(fill.data);
    return sb_take(&out);
}

/* -----------------------------------------------------------------------
 * exec() — run external command
 * ----------------------------------------------------------------------- */
#ifndef __EMSCRIPTEN__
static Value do_exec(Value *args, int nargs, int line, int col) {
    if (nargs < 1 || !type_is_array(args[0].type)) {
        error_runtime(line, col, "exec: first argument must be string[]");
        return val_void();
    }
    ArrayVal *cmd_arr = args[0].u.arr;
    if (cmd_arr->count == 0) {
        error_runtime(line, col, "exec: command array must not be empty");
        return val_void();
    }

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
        for (int i = 0; i < ne; i++) {
            const char *entry = env_arr->data.strings[i];
            const char *eq = strchr(entry, '=');
            if (!eq || eq == entry) {
                error_runtime(line, col,
                              "exec: invalid environment entry: '%s'",
                              entry);
                free(argv);
                return val_void();
            }
        }
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
        free(cmdline);
        free(argv);
        return val_void();
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
    int exec_err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0)
        error_runtime(line, col, "exec: pipe() failed");
    if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
        free(argv);
        return val_void();
    }
    if (pipe(exec_err_pipe) != 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        error_runtime(line, col, "exec: pipe() failed");
        free(argv);
        return val_void();
    }
    (void)fcntl(exec_err_pipe[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        close(exec_err_pipe[0]); close(exec_err_pipe[1]);
        error_runtime(line, col, "exec: fork() failed");
        free(argv);
        return val_void();
    }

    if (pid == 0) {
        /* child */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        close(exec_err_pipe[0]);
        if (envp) execve(argv[0], argv, envp);
        else      execvp(argv[0], argv);
        {
            int exec_errno = errno;
            (void)write(exec_err_pipe[1], &exec_errno, sizeof(exec_errno));
        }
        close(exec_err_pipe[1]);
        _exit(127);
    }
    close(out_pipe[1]); close(err_pipe[1]);
    close(exec_err_pipe[1]);

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
    {
        int exec_errno = 0;
        ssize_t n = read(exec_err_pipe[0], &exec_errno, sizeof(exec_errno));
        close(exec_err_pipe[0]);
        if (n == (ssize_t)sizeof(exec_errno)) {
            free(out_str);
            free(err_str);
            if (exec_errno == ENOENT) {
                error_runtime(line, col, "exec: command not found: '%s'", argv[0]);
                if (envp) free(envp);
                free(argv);
                return val_void();
            }
            error_runtime(line, col, "exec: failed to start '%s': %s",
                          argv[0], strerror(exec_errno));
            if (envp) free(envp);
            free(argv);
            return val_void();
        }
    }
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

static int iso_weeks_in_year(int year) {
    int jan1 = (int)((days_from_civil(year, 1, 1) + 4) % 7);
    if (jan1 < 0) jan1 += 7;
    int leap = ((year % 4 == 0) && (year % 100 != 0 || year % 400 == 0));
    return (jan1 == 3 || (jan1 == 2 && leap)) ? 53 : 52;
}

static int iso_week_number(const struct tm *tm) {
    int wday = tm->tm_wday == 0 ? 7 : tm->tm_wday;
    int yday = tm->tm_yday;
    int week = (yday - wday + 10) / 7;
    int year = tm->tm_year + 1900;
    if (week < 1) return iso_weeks_in_year(year - 1);
    if (week > iso_weeks_in_year(year)) return 1;
    return week;
}

/* -----------------------------------------------------------------------
 * formatDate
 * ----------------------------------------------------------------------- */
static char *format_date(int64_t epochMs, const char *fmt) {
    struct tm tm;
    epoch_to_tm(epochMs, &tm);
    size_t len   = strlen(fmt);
    size_t buflen = len * 8 + 64;
    char  *out   = (char *)cimple_malloc(buflen);
    size_t oi    = 0;

    for (size_t i = 0; i < len && oi < buflen - 10; i++) {
        /* Backslash escape: \X copies X literally (PHP-compatible) */
        if (fmt[i] == '\\' && i + 1 < len) {
            out[oi++] = fmt[++i];
            continue;
        }
        /* Single-letter PHP-compatible format tokens */
        switch (fmt[i]) {
        case 'Y': /* 4-digit year, zero-padded */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%04d", tm.tm_year + 1900);
            break;
        case 'm': /* month 01-12 */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_mon + 1);
            break;
        case 'd': /* day of month 01-31 */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_mday);
            break;
        case 'H': /* hour 00-23 */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_hour);
            break;
        case 'i': /* minutes 00-59 */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_min);
            break;
        case 's': /* seconds 00-59 */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", tm.tm_sec);
            break;
        case 'w': /* weekday 0 (Sunday) – 6 (Saturday) */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%d", tm.tm_wday);
            break;
        case 'z': /* day of year, base 0, no padding */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%d", tm.tm_yday);
            break;
        case 'W': /* ISO 8601 week number, zero-padded */
            oi += (size_t)snprintf(out + oi, buflen - oi, "%02d", iso_week_number(&tm));
            break;
        case 'c': /* full UTC ISO 8601 date-time */ {
            int year = tm.tm_year + 1900;
            if (year < 0 || year > 9999) {
                strcpy(out + oi, "invalid");
                oi += 7;
            } else {
                oi += (size_t)snprintf(out + oi, buflen - oi,
                                       "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                       year, tm.tm_mon + 1, tm.tm_mday,
                                       tm.tm_hour, tm.tm_min, tm.tm_sec);
            }
            break;
        }
        default: /* unrecognised character: copy verbatim */
            out[oi++] = fmt[i];
            break;
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
    case TYPE_BYTE:
        snprintf(buf, sizeof(buf), "%u", (unsigned)v->u.i);
        return cimple_strdup(buf);
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
    BuiltinId id = builtin_lookup_id(name);
    if (id == BI_NONE) return not_a_builtin();


#define REQUIRE(n) do { if (nargs < (n)) \
    error_runtime(line, col, "Wrong number of arguments for '%s' (expected %d)", name, (n)); } while(0)
#define ARG_STR(idx)   args[(idx)].u.s
#define ARG_INT(idx)   args[(idx)].u.i
#define ARG_FLOAT(idx) args[(idx)].u.f
#define ARG_BOOL(idx)  args[(idx)].u.b
#define ARG_ARR(idx)   args[(idx)].u.arr

    /* ---- I/O ---- */
    if (id == BI_PRINT) {
        REQUIRE(1);
        fputs(args[0].u.s, stdout);
        fflush(stdout);
        return val_void();
    }

    if (id == BI_INPUT) {
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

    if (id == BI_TERM_IS_TTY) {
        REQUIRE(0);
        return val_bool(terminal_is_tty());
    }

    if (id == BI_TERM_GET_SIZE) {
        REQUIRE(0);
        return terminal_get_size_value();
    }

    if (id == BI_TERM_CLEAR) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputs("\033[2J\033[H", stdout);
        return val_void();
    }

    if (id == BI_TERM_CLEAR_LINE) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputs("\033[2K\r", stdout);
        return val_void();
    }

    if (id == BI_TERM_MOVE_CURSOR) {
        REQUIRE(2);
        if (ARG_INT(0) < 1 || ARG_INT(1) < 1)
            error_runtime(line, col, "terminal coordinates must be >= 1");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fprintf(stdout, "\033[%lld;%lldH", (long long)ARG_INT(0), (long long)ARG_INT(1));
        return val_void();
    }

    if (id == BI_TERM_WRITE_AT) {
        REQUIRE(3);
        if (ARG_INT(0) < 1 || ARG_INT(1) < 1)
            error_runtime(line, col, "terminal coordinates must be >= 1");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fprintf(stdout, "\033[%lld;%lldH%s", (long long)ARG_INT(0), (long long)ARG_INT(1), ARG_STR(2));
        return val_void();
    }

    if (id == BI_TERM_HIDE_CURSOR) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputs("\033[?25l", stdout);
        return val_void();
    }

    if (id == BI_TERM_SHOW_CURSOR) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputs("\033[?25h", stdout);
        return val_void();
    }

    if (id == BI_TERM_FLUSH) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fflush(stdout);
        return val_void();
    }

    if (id == BI_TERM_BEEP) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputc('\a', stdout);
        return val_void();
    }

    if (id == BI_TERM_SET_STYLE) {
        REQUIRE(5);
        if (!terminal_style_color_valid((int)ARG_INT(0)) ||
            !terminal_style_color_valid((int)ARG_INT(1))) {
            error_runtime(line, col, "terminal color must be TERM_COLOR_DEFAULT or in range 0..7");
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_emit_style((int)ARG_INT(0), (int)ARG_INT(1),
                            ARG_BOOL(2), ARG_BOOL(3), ARG_BOOL(4));
        return val_void();
    }

    if (id == BI_TERM_RESET_STYLE) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        fputs("\033[0m", stdout);
        return val_void();
    }

    if (id == BI_TERM_ENABLE_RAW_MODE) {
        REQUIRE(0);
        terminal_enable_raw_mode(line, col);
        return val_void();
    }

    if (id == BI_TERM_DISABLE_RAW_MODE) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_disable_raw_mode();
        return val_void();
    }

    if (id == BI_TERM_READ_KEY) {
        REQUIRE(0);
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return terminal_read_key_value(-1);
    }

    if (id == BI_TERM_POLL_KEY) {
        REQUIRE(1);
        if (ARG_INT(0) < 0)
            error_runtime(line, col, "termPollKey: timeout must be >= 0");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        terminal_require_available(line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return terminal_read_key_value((int)ARG_INT(0));
    }

    /* ---- String ---- */
    if (id == BI_LEN) {
        REQUIRE(1);
        return val_int((int64_t)strlen(ARG_STR(0)));
    }

    if (id == BI_GLYPH_LEN) {
        REQUIRE(1);
        return val_int(utf8_codepoint_count(ARG_STR(0)));
    }

    if (id == BI_GLYPH_AT) {
        REQUIRE(2);
        char *s = utf8_codepoint_at(ARG_STR(0), (int)ARG_INT(1), line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_string_own(s);
    }

    if (id == BI_BYTE_AT) {
        REQUIRE(2);
        const char *s = ARG_STR(0);
        int         slen = (int)strlen(s);
        int         idx  = (int)ARG_INT(1);
        if (idx < 0 || idx >= slen)
            error_runtime(line, col,
                          "byteAt: index out of bounds (Index: %d   String length: %d bytes)",
                          idx, slen);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_int((unsigned char)s[idx]);
    }

    if (id == BI_SUBSTR) {
        REQUIRE(3);
        const char *s    = ARG_STR(0);
        int         slen = (int)strlen(s);
        int         start = (int)ARG_INT(1);
        int         len2  = (int)ARG_INT(2);
        if (start < 0 || len2 < 0 || start > slen || len2 > slen - start)
            error_runtime(line, col,
                          "substr: range out of bounds (start: %d   length: %d   String length: %d bytes)",
                          start, len2, slen);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        char *out = cimple_strndup(s + start, (size_t)len2);
        return val_string_own(out);
    }

    if (id == BI_INDEX_OF) {
        REQUIRE(2);
        const char *found = strstr(ARG_STR(0), ARG_STR(1));
        return val_int(found ? (int64_t)(found - ARG_STR(0)) : -1);
    }

    if (id == BI_CONTAINS) {
        REQUIRE(2);
        return val_bool(strstr(ARG_STR(0), ARG_STR(1)) != NULL);
    }

    if (id == BI_STARTS_WITH) {
        REQUIRE(2);
        size_t plen = strlen(ARG_STR(1));
        return val_bool(strncmp(ARG_STR(0), ARG_STR(1), plen) == 0);
    }

    if (id == BI_ENDS_WITH) {
        REQUIRE(2);
        size_t slen = strlen(ARG_STR(0));
        size_t plen = strlen(ARG_STR(1));
        if (plen > slen) return val_bool(0);
        return val_bool(strcmp(ARG_STR(0) + slen - plen, ARG_STR(1)) == 0);
    }

    if (id == BI_REPLACE) {
        REQUIRE(3);
        const char *s   = ARG_STR(0);
        const char *old = ARG_STR(1);
        const char *repl= ARG_STR(2);
        if (old[0] == '\0')
            error_runtime(line, col, "replace: old argument cannot be empty");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        size_t slen = strlen(s);
        size_t oldlen = strlen(old);
        StringBuilder sb;
        sb_init(&sb, slen + 1);
        const char *p = s;
        while (*p) {
            const char *found = strstr(p, old);
            if (!found) {
                sb_append_cstr(&sb, p);
                break;
            }
            sb_append_mem(&sb, p, (size_t)(found - p));
            sb_append_cstr(&sb, repl);
            p = found + oldlen;
        }
        return val_string_own(sb_take(&sb));
    }

    if (id == BI_FORMAT) {
        REQUIRE(2);
        const char *tmpl = ARG_STR(0);
        ArrayVal   *arr  = ARG_ARR(1);
        int ph_count = 0;
        for (const char *p = tmpl; *p; p++)
            if (p[0] == '{' && p[1] == '}') ph_count++;
        if (ph_count != arr->count)
            error_runtime(line, col,
                          "format: marker count does not match argument count (Markers '{}': %d   Arguments provided: %d)",
                          ph_count, arr->count);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        StringBuilder sb;
        sb_init(&sb, strlen(tmpl) + 1);
        const char *p = tmpl;
        int ai = 0;
        while (*p) {
            if (p[0] == '{' && p[1] == '}') {
                sb_append_cstr(&sb, arr->data.strings[ai++]);
                p += 2;
            } else {
                sb_append_mem(&sb, p, 1);
                p++;
            }
        }
        return val_string_own(sb_take(&sb));
    }

    if (id == BI_JOIN) {
        REQUIRE(2);
        ArrayVal *arr = ARG_ARR(0);
        const char *sep = ARG_STR(1);
        size_t seplen = strlen(sep);
        if (arr->count == 0) return val_string("");
        size_t total = 1;
        for (int i = 0; i < arr->count; i++) total += strlen(arr->data.strings[i]);
        total += (size_t)(arr->count - 1) * seplen;
        StringBuilder sb;
        sb_init(&sb, total);
        for (int i = 0; i < arr->count; i++) {
            if (i > 0) sb_append_mem(&sb, sep, seplen);
            sb_append_cstr(&sb, arr->data.strings[i]);
        }
        return val_string_own(sb_take(&sb));
    }

    if (id == BI_SPLIT) {
        REQUIRE(2);
        const char *s   = ARG_STR(0);
        const char *sep = ARG_STR(1);
        if (sep[0] == '\0')
            error_runtime(line, col, "split: separator cannot be empty");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        Value result = val_array(TYPE_STRING);
        if (s[0] == '\0') {
            Value empty_s = val_string("");
            array_push_owned(result.u.arr, &empty_s);
            return result;
        }
        size_t seplen = strlen(sep);
        const char *p = s;
        while (1) {
            const char *found = strstr(p, sep);
            if (!found) {
                Value part = val_string(p);
                array_push_owned(result.u.arr, &part);
                break;
            }
            char *part_s = cimple_strndup(p, (size_t)(found - p));
            Value part = val_string_own(part_s);
            array_push_owned(result.u.arr, &part);
            p = found + seplen;
        }
        return result;
    }

    if (id == BI_CONCAT) {
        REQUIRE(1);
        ArrayVal *arr = ARG_ARR(0);
        size_t total = 1;
        for (int i = 0; i < arr->count; i++) total += strlen(arr->data.strings[i]);
        StringBuilder sb;
        sb_init(&sb, total);
        for (int i = 0; i < arr->count; i++) {
            sb_append_cstr(&sb, arr->data.strings[i]);
        }
        return val_string_own(sb_take(&sb));
    }

    if (id == BI_TRIM) {
        REQUIRE(1);
        return val_string_own(trim_ascii_edges(ARG_STR(0), 1, 1));
    }

    if (id == BI_TRIM_LEFT) {
        REQUIRE(1);
        return val_string_own(trim_ascii_edges(ARG_STR(0), 1, 0));
    }

    if (id == BI_TRIM_RIGHT) {
        REQUIRE(1);
        return val_string_own(trim_ascii_edges(ARG_STR(0), 0, 1));
    }

    if (id == BI_TO_UPPER) {
        REQUIRE(1);
        return val_string_own(transform_case_utf8(ARG_STR(0), 1, 0));
    }

    if (id == BI_TO_LOWER) {
        REQUIRE(1);
        return val_string_own(transform_case_utf8(ARG_STR(0), 0, 0));
    }

    if (id == BI_CAPITALIZE) {
        REQUIRE(1);
        return val_string_own(transform_case_utf8(ARG_STR(0), 1, 1));
    }

    if (id == BI_PAD_LEFT) {
        REQUIRE(3);
        char *out = pad_to_width(ARG_STR(0), (int)ARG_INT(1), ARG_STR(2), 1, line, col, "padLeft");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_string_own(out);
    }

    if (id == BI_PAD_RIGHT) {
        REQUIRE(3);
        char *out = pad_to_width(ARG_STR(0), (int)ARG_INT(1), ARG_STR(2), 0, line, col, "padRight");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_string_own(out);
    }

    if (id == BI_REPEAT) {
        REQUIRE(2);
        int64_t count = ARG_INT(1);
        if (count < 0) error_runtime(line, col, "repeat: count must be non-negative");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        if (count == 0 || ARG_STR(0)[0] == '\0') return val_string("");
        size_t slen = strlen(ARG_STR(0));
        StringBuilder sb;
        sb_init(&sb, slen * (size_t)count + 1);
        for (int64_t i = 0; i < count; i++) sb_append_mem(&sb, ARG_STR(0), slen);
        return val_string_own(sb_take(&sb));
    }

    if (id == BI_LAST_INDEX_OF) {
        REQUIRE(2);
        const char *s = ARG_STR(0);
        const char *needle = ARG_STR(1);
        size_t slen = strlen(s);
        size_t nlen = strlen(needle);
        if (nlen == 0) return val_int((int64_t)slen);
        if (nlen > slen) return val_int(-1);
        for (size_t i = slen - nlen + 1; i > 0; i--) {
            size_t pos = i - 1;
            if (memcmp(s + pos, needle, nlen) == 0) return val_int((int64_t)pos);
        }
        return val_int(-1);
    }

    if (id == BI_COUNT_OCCURRENCES) {
        REQUIRE(2);
        const char *s = ARG_STR(0);
        const char *needle = ARG_STR(1);
        size_t nlen = strlen(needle);
        if (nlen == 0) error_runtime(line, col, "countOccurrences: needle cannot be empty");
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        int64_t count = 0;
        const char *p = s;
        while ((p = strstr(p, needle)) != NULL) {
            count++;
            p += nlen;
        }
        return val_int(count);
    }

    if (id == BI_IS_BLANK) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        for (; *s; s++) {
            if (!is_blank_ascii((unsigned char)*s)) return val_bool(0);
        }
        return val_bool(1);
    }

    if (id == BI_REGEX_COMPILE) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        Value out = val_regexp(regex_compile_value(ARG_STR(0), ARG_STR(1), line, col));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_REGEX_PATTERN) {
        REQUIRE(1);
        return val_string(args[0].u.re ? args[0].u.re->pattern : "");
    }
    if (id == BI_REGEX_FLAGS) {
        REQUIRE(1);
        return val_string(args[0].u.re ? args[0].u.re->flags : "");
    }
    if (id == BI_REGEX_TEST) {
        REQUIRE(3);
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        Value out = val_bool(regex_test_value(args[0].u.re, ARG_STR(1), (int)ARG_INT(2), line, col));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_REGEX_FIND) {
        REQUIRE(3);
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        Value out = val_regexp_match(regex_find_value(args[0].u.re, ARG_STR(1), (int)ARG_INT(2), line, col));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_REGEX_FIND_ALL) {
        REQUIRE(4);
        int count = 0;
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        RegExpMatchVal **matches = regex_find_all_values(args[0].u.re, ARG_STR(1), (int)ARG_INT(2), (int)ARG_INT(3), &count, line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            free(matches);
            return val_void();
        }
        runtime_error_override_pop(saved);
        Value out = val_array(TYPE_REGEXP_MATCH);
        for (int i = 0; i < count; i++) {
            Value mv = val_regexp_match(matches[i]);
            array_push_owned(out.u.arr, &mv);
        }
        free(matches);
        return out;
    }
    if (id == BI_REGEX_REPLACE) {
        REQUIRE(4);
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        Value out = val_string_own(regex_replace_value(args[0].u.re, ARG_STR(1), ARG_STR(2), (int)ARG_INT(3), 0, 1, line, col));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_REGEX_REPLACE_ALL) {
        REQUIRE(5);
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        Value out = val_string_own(regex_replace_value(args[0].u.re, ARG_STR(1), ARG_STR(2), (int)ARG_INT(3), 1, (int)ARG_INT(4), line, col));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_REGEX_SPLIT) {
        REQUIRE(4);
        int count = 0;
        RuntimeErrorOverride saved = runtime_error_override_push("RegExpException", NULL);
        char **parts = regex_split_value(args[0].u.re, ARG_STR(1), (int)ARG_INT(2), (int)ARG_INT(3), &count, line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            free(parts);
            return val_void();
        }
        runtime_error_override_pop(saved);
        Value out = val_array(TYPE_STRING);
        for (int i = 0; i < count; i++) {
            Value sv = val_string_own(parts[i]);
            array_push_owned(out.u.arr, &sv);
        }
        free(parts);
        return out;
    }
    if (id == BI_REGEX_OK) {
        REQUIRE(1);
        return val_bool(args[0].u.rem && args[0].u.rem->ok);
    }
    if (id == BI_REGEX_START) {
        REQUIRE(1);
        return val_int(args[0].u.rem && args[0].u.rem->ok ? args[0].u.rem->start : 0);
    }
    if (id == BI_REGEX_END) {
        REQUIRE(1);
        return val_int(args[0].u.rem && args[0].u.rem->ok ? args[0].u.rem->end : 0);
    }
    if (id == BI_REGEX_GROUPS) {
        REQUIRE(1);
        Value out = val_array(TYPE_STRING);
        if (args[0].u.rem && args[0].u.rem->ok) {
            for (int i = 0; i < args[0].u.rem->group_count; i++) {
                Value sv = val_string(args[0].u.rem->groups[i]);
                array_push_owned(out.u.arr, &sv);
            }
        }
        return out;
    }

    if (id == BI_BYTE_TO_INT) {
        REQUIRE(1);
        return val_int(args[0].u.i);
    }
    if (id == BI_INT_TO_BYTE) {
        REQUIRE(1);
        int64_t v = args[0].u.i;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        return val_byte((unsigned char)v);
    }
    if (id == BI_STRING_TO_BYTES) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        size_t n = strlen(s);
        Value out = val_array(TYPE_BYTE);
        out.u.arr->cap = (int)n;
        out.u.arr->count = (int)n;
        out.u.arr->data.bytes = n > 0 ? (unsigned char *)cimple_malloc(n) : NULL;
        memcpy(out.u.arr->data.bytes, s, n);
        return out;
    }
    if (id == BI_BYTES_TO_STRING) {
        REQUIRE(1);
        return val_string_own(bytes_to_string_lossy(ARG_ARR(0)));
    }
    if (id == BI_INT_TO_BYTES) {
        REQUIRE(1);
        Value out = val_array(TYPE_BYTE);
        out.u.arr->cap = (int)sizeof(int64_t);
        out.u.arr->count = (int)sizeof(int64_t);
        out.u.arr->data.bytes = (unsigned char *)cimple_malloc(sizeof(int64_t));
        memcpy(out.u.arr->data.bytes, &args[0].u.i, sizeof(int64_t));
        return out;
    }
    if (id == BI_FLOAT_TO_BYTES) {
        REQUIRE(1);
        Value out = val_array(TYPE_BYTE);
        out.u.arr->cap = (int)sizeof(double);
        out.u.arr->count = (int)sizeof(double);
        out.u.arr->data.bytes = (unsigned char *)cimple_malloc(sizeof(double));
        memcpy(out.u.arr->data.bytes, &args[0].u.f, sizeof(double));
        return out;
    }
    if (id == BI_BYTES_TO_INT) {
        REQUIRE(1);
        if (ARG_ARR(0)->count != (int)sizeof(int64_t)) {
            error_runtime(line, col, "bytesToInt: expected INT_SIZE bytes, got %d", ARG_ARR(0)->count);
            return val_void();
        }
        int64_t value;
        memcpy(&value, ARG_ARR(0)->data.bytes, sizeof(int64_t));
        return val_int(value);
    }
    if (id == BI_BYTES_TO_FLOAT) {
        REQUIRE(1);
        if (ARG_ARR(0)->count != (int)sizeof(double)) {
            error_runtime(line, col, "bytesToFloat: expected FLOAT_SIZE bytes, got %d", ARG_ARR(0)->count);
            return val_void();
        }
        double value;
        memcpy(&value, ARG_ARR(0)->data.bytes, sizeof(double));
        return val_float(value);
    }

    /* ---- Conversions ---- */
    if (id == BI_TO_STRING) {
        REQUIRE(1);
        char *s = val_to_string(&args[0], line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_string_own(s);
    }

    if (id == BI_TO_INT) {
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
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_int(0);
    }

    if (id == BI_TO_FLOAT) {
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
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) return val_void();
        return val_float(0.0);
    }

    if (id == BI_TO_BOOL) {
        REQUIRE(1);
        const char *s = args[0].u.s;
        if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0) return val_bool(1);
        if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0) return val_bool(0);
        return val_bool(0);
    }

    /* ---- String validation ---- */
    if (id == BI_IS_INT_STRING) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        if (!s || !*s) return val_bool(0);
        char *end; strtoll(s, &end, 10);
        return val_bool(*end == '\0');
    }
    if (id == BI_IS_FLOAT_STRING) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        if (!s || !*s) return val_bool(0);
        char *end; strtod(s, &end);
        return val_bool(*end == '\0');
    }
    if (id == BI_IS_BOOL_STRING) {
        REQUIRE(1);
        const char *s = ARG_STR(0);
        return val_bool(strcmp(s,"true")==0 || strcmp(s,"false")==0 ||
                        strcmp(s,"1")==0    || strcmp(s,"0")==0);
    }

    /* ---- Float math ---- */
    if (id == BI_IS_NAN) { REQUIRE(1); return val_bool(isnan(ARG_FLOAT(0))); }
    if (id == BI_IS_INFINITE) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0))); }
    if (id == BI_IS_FINITE) { REQUIRE(1); return val_bool(isfinite(ARG_FLOAT(0))); }
    if (id == BI_IS_POSITIVE_INFINITY) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0)) && ARG_FLOAT(0) > 0); }
    if (id == BI_IS_NEGATIVE_INFINITY) { REQUIRE(1); return val_bool(isinf(ARG_FLOAT(0)) && ARG_FLOAT(0) < 0); }
    if (id == BI_ABS) { REQUIRE(1); return val_float(fabs(ARG_FLOAT(0))); }
    if (id == BI_MIN) { REQUIRE(2); return val_float(ARG_FLOAT(0) < ARG_FLOAT(1) ? ARG_FLOAT(0) : ARG_FLOAT(1)); }
    if (id == BI_MAX) { REQUIRE(2); return val_float(ARG_FLOAT(0) > ARG_FLOAT(1) ? ARG_FLOAT(0) : ARG_FLOAT(1)); }
    if (id == BI_CLAMP) {
        REQUIRE(3);
        double v = ARG_FLOAT(0), lo = ARG_FLOAT(1), hi = ARG_FLOAT(2);
        return val_float(v < lo ? lo : v > hi ? hi : v);
    }
    if (id == BI_FLOOR) { REQUIRE(1); return val_float(floor(ARG_FLOAT(0))); }
    if (id == BI_CEIL) { REQUIRE(1); return val_float(ceil(ARG_FLOAT(0))); }
    if (id == BI_ROUND) { REQUIRE(1); return val_float(round(ARG_FLOAT(0))); }
    if (id == BI_TRUNC) { REQUIRE(1); return val_float(trunc(ARG_FLOAT(0))); }
    if (id == BI_FMOD) { REQUIRE(2); return val_float(fmod(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (id == BI_SQRT) { REQUIRE(1); return val_float(sqrt(ARG_FLOAT(0))); }
    if (id == BI_POW) { REQUIRE(2); return val_float(pow(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (id == BI_APPROX_EQUAL) {
        REQUIRE(3);
        return val_bool(fabs(ARG_FLOAT(0) - ARG_FLOAT(1)) <= ARG_FLOAT(2));
    }
    if (id == BI_SIN) { REQUIRE(1); return val_float(sin(ARG_FLOAT(0))); }
    if (id == BI_COS) { REQUIRE(1); return val_float(cos(ARG_FLOAT(0))); }
    if (id == BI_TAN) { REQUIRE(1); return val_float(tan(ARG_FLOAT(0))); }
    if (id == BI_ASIN) { REQUIRE(1); return val_float(asin(ARG_FLOAT(0))); }
    if (id == BI_ACOS) { REQUIRE(1); return val_float(acos(ARG_FLOAT(0))); }
    if (id == BI_ATAN) { REQUIRE(1); return val_float(atan(ARG_FLOAT(0))); }
    if (id == BI_ATAN2) { REQUIRE(2); return val_float(atan2(ARG_FLOAT(0), ARG_FLOAT(1))); }
    if (id == BI_EXP) { REQUIRE(1); return val_float(exp(ARG_FLOAT(0))); }
    if (id == BI_LOG) { REQUIRE(1); return val_float(log(ARG_FLOAT(0))); }
    if (id == BI_LOG2) { REQUIRE(1); return val_float(log2(ARG_FLOAT(0))); }
    if (id == BI_LOG10) { REQUIRE(1); return val_float(log10(ARG_FLOAT(0))); }

    /* ---- Integer math ---- */
    if (id == BI_ABS_INT) { REQUIRE(1); int64_t v = ARG_INT(0); return val_int(v < 0 ? -v : v); }
    if (id == BI_MIN_INT) { REQUIRE(2); return val_int(ARG_INT(0) < ARG_INT(1) ? ARG_INT(0) : ARG_INT(1)); }
    if (id == BI_MAX_INT) { REQUIRE(2); return val_int(ARG_INT(0) > ARG_INT(1) ? ARG_INT(0) : ARG_INT(1)); }
    if (id == BI_CLAMP_INT) {
        REQUIRE(3);
        int64_t v = ARG_INT(0), lo = ARG_INT(1), hi = ARG_INT(2);
        return val_int(v < lo ? lo : v > hi ? hi : v);
    }
    if (id == BI_IS_EVEN) { REQUIRE(1); return val_bool(ARG_INT(0) % 2 == 0); }
    if (id == BI_IS_ODD) { REQUIRE(1); return val_bool(ARG_INT(0) % 2 != 0); }
    if (id == BI_SAFE_DIV_INT) {
        REQUIRE(3);
        if (ARG_INT(1) == 0) return val_int(ARG_INT(2));
        return val_int(ARG_INT(0) / ARG_INT(1));
    }
    if (id == BI_SAFE_MOD_INT) {
        REQUIRE(3);
        if (ARG_INT(1) == 0) return val_int(ARG_INT(2));
        return val_int(ARG_INT(0) % ARG_INT(1));
    }

    /* ---- Array intrinsics ---- */
    if (id == BI_COUNT) {
        REQUIRE(1);
        if (args[0].type == TYPE_MAP)
            return val_int(map_size(args[0].u.map));
        return val_int(ARG_ARR(0)->count);
    }
    if (id == BI_MAP_HAS) {
        REQUIRE(2);
        return val_bool(map_has(args[0].u.map, args[1]));
    }
    if (id == BI_MAP_REMOVE) {
        REQUIRE(2);
        map_remove(args[0].u.map, args[1]);
        return val_void();
    }
    if (id == BI_MAP_KEYS) {
        REQUIRE(1);
        Value out = val_void();
        out.type = type_make_array(args[0].u.map->key_type);
        out.u.arr = map_keys(args[0].u.map);
        return out;
    }
    if (id == BI_MAP_VALUES) {
        REQUIRE(1);
        Value out = val_void();
        out.type = args[0].u.map->val_type == TYPE_MAP ? TYPE_UNKNOWN : type_make_array(args[0].u.map->val_type);
        out.u.arr = map_values(args[0].u.map);
        if (args[0].u.map->val_type == TYPE_MAP) out.type = TYPE_UNKNOWN;
        return out;
    }
    if (id == BI_MAP_SIZE) {
        REQUIRE(1);
        return val_int(map_size(args[0].u.map));
    }
    if (id == BI_MAP_CLEAR) {
        REQUIRE(1);
        map_clear(args[0].u.map);
        return val_void();
    }
    if (id == BI_ARRAY_PUSH) {
        REQUIRE(2);
        array_push(ARG_ARR(0), args[1]);
        return val_void();
    }
    if (id == BI_ARRAY_POP) {
        REQUIRE(1);
        return array_pop(ARG_ARR(0), line, col);
    }
    if (id == BI_ARRAY_INSERT) {
        REQUIRE(3);
        array_insert(ARG_ARR(0), (int)ARG_INT(1), args[2], line, col);
        return val_void();
    }
    if (id == BI_ARRAY_REMOVE) {
        REQUIRE(2);
        array_remove(ARG_ARR(0), (int)ARG_INT(1), line, col);
        return val_void();
    }
    if (id == BI_ARRAY_GET) {
        REQUIRE(2);
        return array_get(ARG_ARR(0), (int)ARG_INT(1), line, col);
    }
    if (id == BI_ARRAY_SET) {
        REQUIRE(3);
        array_set(ARG_ARR(0), (int)ARG_INT(1), args[2], line, col);
        return val_void();
    }

    /* ---- File I/O ---- */
    if (id == BI_READ_FILE) {
        REQUIRE(1);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        char *content = read_file_str(ARG_STR(0), line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return val_string_own(content);
    }
    if (id == BI_WRITE_FILE) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        FILE *f = fopen(ARG_STR(0), "wb");
        if (!f) error_runtime(line, col, "Cannot write file: '%s'", ARG_STR(0));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        fputs(ARG_STR(1), f);
        fclose(f);
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_APPEND_FILE) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        FILE *f = fopen(ARG_STR(0), "ab");
        if (!f) error_runtime(line, col, "Cannot write file: '%s'", ARG_STR(0));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        fputs(ARG_STR(1), f);
        fclose(f);
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_READ_FILE_BYTES) {
        REQUIRE(1);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        Value out = read_file_bytes(ARG_STR(0), line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return out;
    }
    if (id == BI_WRITE_FILE_BYTES) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        write_file_bytes(ARG_STR(0), ARG_ARR(1), "wb", line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_APPEND_FILE_BYTES) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        write_file_bytes(ARG_STR(0), ARG_ARR(1), "ab", line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_FILE_EXISTS) {
        REQUIRE(1);
        struct stat st;
        if (stat(ARG_STR(0), &st) != 0) return val_bool(0);
        return val_bool(S_ISREG(st.st_mode) ? 1 : 0);
    }
    if (id == BI_TEMP_PATH) {
        REQUIRE(0);
        return val_string_own(temp_path_string());
    }
    if (id == BI_REMOVE) {
        REQUIRE(1);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        struct stat st;
        if (stat(ARG_STR(0), &st) != 0) {
            error_runtime_hint(line, col, "File does not exist.",
                               "Cannot remove file: '%s'", ARG_STR(0));
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (S_ISDIR(st.st_mode)) {
            error_runtime_hint(line, col, "Path is a directory, not a file.",
                               "Cannot remove file: '%s'", ARG_STR(0));
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (remove(ARG_STR(0)) != 0) {
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot remove file: '%s'", ARG_STR(0));
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_CHMOD) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
        error_runtime(line, col, "chmod: not supported on this platform");
        runtime_error_override_pop(saved);
        return val_void();
#else
        struct stat st;
        if (stat(ARG_STR(0), &st) != 0) {
            error_runtime_hint(line, col, "File or directory does not exist.",
                               "Cannot chmod: '%s'", ARG_STR(0));
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (chmod(ARG_STR(0), (mode_t)ARG_INT(1)) != 0) {
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot chmod: '%s'", ARG_STR(0));
        }
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        runtime_error_override_pop(saved);
        return val_void();
#endif
    }
    if (id == BI_CWD) {
        REQUIRE(0);
        return val_string_own(cwd_string());
    }
    if (id == BI_COPY) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        const char *src = ARG_STR(0);
        const char *dst = ARG_STR(1);
        struct stat src_st;
        if (stat(src, &src_st) != 0)
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot copy: source file not found: '%s'", src);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (S_ISDIR(src_st.st_mode))
            error_runtime_hint(line, col, "Source is a directory, not a file.",
                               "Cannot copy: '%s'", src);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        char *dst_parent = parent_dir_for_write(dst);
        struct stat pst;
        int parent_ok = (stat(dst_parent, &pst) == 0 && S_ISDIR(pst.st_mode));
        free(dst_parent);
        if (!parent_ok)
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot copy: destination directory not found: '%s'", dst);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        copy_file_contents(src, dst, "copy", line, col);
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_MOVE) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        const char *src = ARG_STR(0);
        const char *dst = ARG_STR(1);
        struct stat src_st;
        if (stat(src, &src_st) != 0)
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot move: source file not found: '%s'", src);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (S_ISDIR(src_st.st_mode))
            error_runtime_hint(line, col, "Source is a directory, not a file.",
                               "Cannot move: '%s'", src);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        char *dst_parent = parent_dir_for_write(dst);
        struct stat pst;
        int parent_ok = (stat(dst_parent, &pst) == 0 && S_ISDIR(pst.st_mode));
        free(dst_parent);
        if (!parent_ok)
            error_runtime_hint(line, col, strerror(errno),
                               "Cannot move: destination directory not found: '%s'", dst);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        if (rename(src, dst) != 0) {
            if (errno != EXDEV)
                error_runtime_hint(line, col, strerror(errno),
                                   "Cannot move: '%s' -> '%s'", src, dst);
            if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
                runtime_error_override_pop(saved);
                return val_void();
            }
            copy_file_contents(src, dst, "move", line, col);
            if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
                runtime_error_override_pop(saved);
                return val_void();
            }
            if (remove(src) != 0)
                error_runtime_hint(line, col, strerror(errno),
                                   "Cannot move: '%s' -> '%s'", src, dst);
        }
        runtime_error_override_pop(saved);
        return val_void();
    }
    if (id == BI_IS_READABLE) {
        REQUIRE(1);
        return val_bool(access(ARG_STR(0), R_OK) == 0);
    }
    if (id == BI_IS_EXECUTABLE) {
        REQUIRE(1);
        return val_bool(access(ARG_STR(0), X_OK) == 0);
    }
    if (id == BI_IS_DIRECTORY) {
        REQUIRE(1);
        struct stat st;
        return val_bool(stat(ARG_STR(0), &st) == 0 && S_ISDIR(st.st_mode));
    }
    if (id == BI_IS_WRITABLE) {
        REQUIRE(1);
        struct stat st;
        if (stat(ARG_STR(0), &st) == 0) {
            return val_bool(access(ARG_STR(0), W_OK) == 0);
        }
        char *parent = parent_dir_for_write(ARG_STR(0));
        int ok = access(parent, W_OK) == 0;
        free(parent);
        return val_bool(ok);
    }
    if (id == BI_DIRNAME) {
        REQUIRE(1);
        return val_string_own(path_dirname_lexical(ARG_STR(0)));
    }
    if (id == BI_BASENAME) {
        REQUIRE(1);
        return val_string_own(path_basename_lexical(ARG_STR(0)));
    }
    if (id == BI_FILENAME) {
        REQUIRE(1);
        char *base = path_basename_lexical(ARG_STR(0));
        char *dot = strrchr(base, '.');
        if (!dot || dot == base) return val_string_own(base);
        char *out = cimple_strndup(base, (size_t)(dot - base));
        free(base);
        return val_string_own(out);
    }
    if (id == BI_EXTENSION) {
        REQUIRE(1);
        char *base = path_basename_lexical(ARG_STR(0));
        char *dot = strrchr(base, '.');
        if (!dot || dot == base) {
            free(base);
            return val_string("");
        }
        char *out = cimple_strdup(dot + 1);
        free(base);
        return val_string_own(out);
    }
    if (id == BI_READ_LINES) {
        REQUIRE(1);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        char *content = read_file_str(ARG_STR(0), line, col);
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        Value result  = val_array(TYPE_STRING);
        char *p = content;
        char *start = p;
        while (*p) {
            if (*p == '\n' || (*p == '\r' && p[1] == '\n')) {
                char *end = p;
                char *line_s = cimple_strndup(start, (size_t)(end - start));
                Value lv = val_string_own(line_s);
                array_push_owned(result.u.arr, &lv);
                if (*p == '\r') p++;
                p++;
                start = p;
            } else if (*p == '\r') {
                char *line_s = cimple_strndup(start, (size_t)(p - start));
                Value lv = val_string_own(line_s);
                array_push_owned(result.u.arr, &lv);
                p++;
                start = p;
            } else {
                p++;
            }
        }
        if (start != p) {
            char *line_s = cimple_strndup(start, (size_t)(p - start));
            Value lv = val_string_own(line_s);
            array_push_owned(result.u.arr, &lv);
        }
        free(content);
        runtime_error_override_pop(saved);
        return result;
    }
    if (id == BI_WRITE_LINES) {
        REQUIRE(2);
        RuntimeErrorOverride saved = runtime_error_override_push("IoException", ARG_STR(0));
        FILE *f = fopen(ARG_STR(0), "wb");
        if (!f) error_runtime(line, col, "Cannot write file: '%s'", ARG_STR(0));
        if (g_current_interp && g_current_interp->signal == SIGNAL_THROW) {
            runtime_error_override_pop(saved);
            return val_void();
        }
        ArrayVal *arr = ARG_ARR(1);
        for (int i = 0; i < arr->count; i++) {
            fputs(arr->data.strings[i], f);
            fputc('\n', f);
        }
        fclose(f);
        runtime_error_override_pop(saved);
        return val_void();
    }

    /* ---- exec ---- */
    if (id == BI_EXEC) {
        REQUIRE(1);
        return do_exec(args, nargs, line, col);
    }
    if (id == BI_EXEC_STATUS) {
        REQUIRE(1);
        return val_int(args[0].u.exec.status);
    }
    if (id == BI_EXEC_STDOUT) {
        REQUIRE(1);
        return val_string(args[0].u.exec.out ? args[0].u.exec.out : "");
    }
    if (id == BI_EXEC_STDERR) {
        REQUIRE(1);
        return val_string(args[0].u.exec.err ? args[0].u.exec.err : "");
    }

    /* ---- Environment ---- */
    if (id == BI_GET_ENV) {
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
    if (id == BI_NOW) {
        return val_int(get_now_ms());
    }
    if (id == BI_EPOCH_TO_YEAR) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_year + 1900); }
    if (id == BI_EPOCH_TO_MONTH) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_mon + 1); }
    if (id == BI_EPOCH_TO_DAY) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_mday); }
    if (id == BI_EPOCH_TO_HOUR) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_hour); }
    if (id == BI_EPOCH_TO_MINUTE) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_min); }
    if (id == BI_EPOCH_TO_SECOND) { REQUIRE(1); struct tm t; epoch_to_tm(ARG_INT(0), &t); return val_int(t.tm_sec); }

    if (id == BI_MAKE_EPOCH) {
        REQUIRE(6);
        int64_t ts = make_epoch_utc_ms((int)ARG_INT(0), (int)ARG_INT(1), (int)ARG_INT(2),
                                       (int)ARG_INT(3), (int)ARG_INT(4), (int)ARG_INT(5));
        return val_int(ts);
    }

    if (id == BI_FORMAT_DATE) {
        REQUIRE(2);
        char *s = format_date(ARG_INT(0), ARG_STR(1));
        return val_string_own(s);
    }

    /* ---- Utility ---- */
    if (id == BI_ASSERT) {
        REQUIRE(1);
        if (!ARG_BOOL(0)) {
            interp_throw_builtin(g_current_interp, "RuntimeException",
                                 nargs >= 2 ? ARG_STR(1) : "Assertion failed", NULL);
        }
        return val_void();
    }

    if (id == BI_RAND_INT) {
        REQUIRE(2);
        int64_t lo = ARG_INT(0);
        int64_t hi = ARG_INT(1);
        if (lo > hi) error_runtime(line, col, "randInt: min > max");
        int64_t range = hi - lo + 1;
        return val_int(lo + (int64_t)((unsigned)rand() % (unsigned)range));
    }

    if (id == BI_RAND_FLOAT) {
        return val_float((double)rand() / ((double)RAND_MAX + 1.0));
    }

    if (id == BI_SLEEP) {
        REQUIRE(1);
        int64_t ms = ARG_INT(0);
#ifdef __EMSCRIPTEN__
        error_runtime(line, col, "sleep() is not available on WebAssembly");
#elif defined(_WIN32)
        Sleep((DWORD)ms);
#else
        usleep((useconds_t)(ms * 1000));
#endif
        return val_void();
    }

    return not_a_builtin();
}
