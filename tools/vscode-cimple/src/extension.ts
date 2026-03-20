import * as vscode from 'vscode';
import * as path from 'path';

// ---------------------------------------------------------------------------
// Built-in function signatures
// ---------------------------------------------------------------------------

interface BuiltinFunction {
  name: string;
  signature: string;
  description: string;
  snippet: string;
}

interface BuiltinConstant {
  name: string;
  type: string;
  description: string;
}

interface BuiltinClass {
  name: string;
  description: string;
  fields?: Array<{ name: string; type: string }>;
  methods?: Array<{ name: string; signature: string }>;
}

const BUILTIN_FUNCTIONS: BuiltinFunction[] = [
  // I/O
  { name: 'print',       signature: 'void print(string value)',         description: 'Print a value to standard output.',                 snippet: 'print(${1:value})' },
  { name: 'printErr',    signature: 'void printErr(string value)',      description: 'Print a value to standard error.',                  snippet: 'printErr(${1:value})' },
  { name: 'exit',        signature: 'void exit(int code)',              description: 'Exit the process with the given exit code.',        snippet: 'exit(${1:0})' },

  // String
  { name: 'format',      signature: 'string format(string fmt, ...)',   description: 'Format a string using printf-style format specifiers.', snippet: 'format(${1:fmt}${2:, args})' },
  { name: 'toString',    signature: 'string toString(value)',           description: 'Convert a value to its string representation.',     snippet: 'toString(${1:value})' },
  { name: 'toInt',       signature: 'int toInt(value)',                 description: 'Convert a value to int.',                          snippet: 'toInt(${1:value})' },
  { name: 'toFloat',     signature: 'float toFloat(value)',             description: 'Convert a value to float.',                        snippet: 'toFloat(${1:value})' },
  { name: 'toBool',      signature: 'bool toBool(value)',               description: 'Convert a value to bool.',                         snippet: 'toBool(${1:value})' },
  { name: 'length',      signature: 'int length(string s)',             description: 'Return the byte-length of a string.',              snippet: 'length(${1:s})' },
  { name: 'substring',   signature: 'string substring(string s, int start, int end)', description: 'Extract a substring [start, end).', snippet: 'substring(${1:s}, ${2:start}, ${3:end})' },
  { name: 'charAt',      signature: 'string charAt(string s, int index)', description: 'Return the character at index.',                 snippet: 'charAt(${1:s}, ${2:index})' },
  { name: 'upper',       signature: 'string upper(string s)',           description: 'Convert string to uppercase.',                     snippet: 'upper(${1:s})' },
  { name: 'lower',       signature: 'string lower(string s)',           description: 'Convert string to lowercase.',                     snippet: 'lower(${1:s})' },
  { name: 'trim',        signature: 'string trim(string s)',            description: 'Trim whitespace from both ends.',                  snippet: 'trim(${1:s})' },
  { name: 'trimStart',   signature: 'string trimStart(string s)',       description: 'Trim whitespace from the start.',                  snippet: 'trimStart(${1:s})' },
  { name: 'trimEnd',     signature: 'string trimEnd(string s)',         description: 'Trim whitespace from the end.',                    snippet: 'trimEnd(${1:s})' },
  { name: 'startsWith',  signature: 'bool startsWith(string s, string prefix)', description: 'Check if string starts with prefix.',     snippet: 'startsWith(${1:s}, ${2:prefix})' },
  { name: 'endsWith',    signature: 'bool endsWith(string s, string suffix)', description: 'Check if string ends with suffix.',         snippet: 'endsWith(${1:s}, ${2:suffix})' },
  { name: 'contains',    signature: 'bool contains(string s, string sub)', description: 'Check if string contains substring.',          snippet: 'contains(${1:s}, ${2:sub})' },
  { name: 'indexOf',     signature: 'int indexOf(string s, string sub)', description: 'Return first index of sub, or -1.',              snippet: 'indexOf(${1:s}, ${2:sub})' },
  { name: 'lastIndexOf', signature: 'int lastIndexOf(string s, string sub)', description: 'Return last index of sub, or -1.',           snippet: 'lastIndexOf(${1:s}, ${2:sub})' },
  { name: 'replace',     signature: 'string replace(string s, string old, string new)', description: 'Replace first occurrence.',        snippet: 'replace(${1:s}, ${2:old}, ${3:new})' },
  { name: 'replaceAll',  signature: 'string replaceAll(string s, string old, string new)', description: 'Replace all occurrences.',      snippet: 'replaceAll(${1:s}, ${2:old}, ${3:new})' },
  { name: 'repeat',      signature: 'string repeat(string s, int n)',   description: 'Repeat string n times.',                          snippet: 'repeat(${1:s}, ${2:n})' },
  { name: 'padLeft',     signature: 'string padLeft(string s, int width, string pad)', description: 'Pad string on the left.',           snippet: 'padLeft(${1:s}, ${2:width}, ${3:" "})' },
  { name: 'padRight',    signature: 'string padRight(string s, int width, string pad)', description: 'Pad string on the right.',         snippet: 'padRight(${1:s}, ${2:width}, ${3:" "})' },
  { name: 'split',       signature: 'string[] split(string s, string delim)', description: 'Split string by delimiter.',                snippet: 'split(${1:s}, ${2:delim})' },
  { name: 'join',        signature: 'string join(string[] parts, string sep)', description: 'Join array of strings with separator.',    snippet: 'join(${1:parts}, ${2:sep})' },
  { name: 'byteAt',      signature: 'int byteAt(string s, int index)',  description: 'Return byte value at index.',                     snippet: 'byteAt(${1:s}, ${2:index})' },
  { name: 'glyph',       signature: 'string glyph(string s, int index)', description: 'Return Unicode glyph at logical index.',         snippet: 'glyph(${1:s}, ${2:index})' },
  { name: 'glyphCount',  signature: 'int glyphCount(string s)',         description: 'Return number of Unicode glyphs.',                snippet: 'glyphCount(${1:s})' },

  // Array
  { name: 'count',   signature: 'int count(array a)',                   description: 'Return number of elements in array.',             snippet: 'count(${1:a})' },
  { name: 'append',  signature: 'void append(array a, value)',          description: 'Append a value to the end of array.',             snippet: 'append(${1:a}, ${2:value})' },
  { name: 'pop',     signature: 'value pop(array a)',                   description: 'Remove and return last element.',                  snippet: 'pop(${1:a})' },
  { name: 'insert',  signature: 'void insert(array a, int index, value)', description: 'Insert value at index.',                       snippet: 'insert(${1:a}, ${2:index}, ${3:value})' },
  { name: 'remove',  signature: 'void remove(array a, int index)',      description: 'Remove element at index.',                        snippet: 'remove(${1:a}, ${2:index})' },
  { name: 'concat',  signature: 'array concat(array a, array b)',       description: 'Concatenate two arrays.',                         snippet: 'concat(${1:a}, ${2:b})' },
  { name: 'sort',    signature: 'void sort(array a)',                   description: 'Sort array in place.',                            snippet: 'sort(${1:a})' },
  { name: 'reverse', signature: 'void reverse(array a)',                description: 'Reverse array in place.',                         snippet: 'reverse(${1:a})' },
  { name: 'slice',   signature: 'array slice(array a, int start, int end)', description: 'Return sub-array [start, end).',              snippet: 'slice(${1:a}, ${2:start}, ${3:end})' },
  { name: 'unique',  signature: 'array unique(array a)',                description: 'Return array with duplicates removed.',           snippet: 'unique(${1:a})' },

  // Math
  { name: 'sqrt',             signature: 'float sqrt(float x)',                  description: 'Square root.',                              snippet: 'sqrt(${1:x})' },
  { name: 'pow',              signature: 'float pow(float base, float exp)',      description: 'Power: base^exp.',                          snippet: 'pow(${1:base}, ${2:exp})' },
  { name: 'abs',              signature: 'float abs(float x)',                   description: 'Absolute value.',                           snippet: 'abs(${1:x})' },
  { name: 'floor',            signature: 'float floor(float x)',                 description: 'Floor (round down).',                       snippet: 'floor(${1:x})' },
  { name: 'ceil',             signature: 'float ceil(float x)',                  description: 'Ceiling (round up).',                       snippet: 'ceil(${1:x})' },
  { name: 'round',            signature: 'float round(float x)',                 description: 'Round to nearest integer.',                 snippet: 'round(${1:x})' },
  { name: 'trunc',            signature: 'float trunc(float x)',                 description: 'Truncate to integer part.',                 snippet: 'trunc(${1:x})' },
  { name: 'log',              signature: 'float log(float x)',                   description: 'Natural logarithm.',                        snippet: 'log(${1:x})' },
  { name: 'exp',              signature: 'float exp(float x)',                   description: 'e^x.',                                      snippet: 'exp(${1:x})' },
  { name: 'log2',             signature: 'float log2(float x)',                  description: 'Logarithm base 2.',                         snippet: 'log2(${1:x})' },
  { name: 'log10',            signature: 'float log10(float x)',                 description: 'Logarithm base 10.',                        snippet: 'log10(${1:x})' },
  { name: 'sin',              signature: 'float sin(float x)',                   description: 'Sine (radians).',                           snippet: 'sin(${1:x})' },
  { name: 'cos',              signature: 'float cos(float x)',                   description: 'Cosine (radians).',                         snippet: 'cos(${1:x})' },
  { name: 'tan',              signature: 'float tan(float x)',                   description: 'Tangent (radians).',                        snippet: 'tan(${1:x})' },
  { name: 'asin',             signature: 'float asin(float x)',                  description: 'Arc sine.',                                 snippet: 'asin(${1:x})' },
  { name: 'acos',             signature: 'float acos(float x)',                  description: 'Arc cosine.',                               snippet: 'acos(${1:x})' },
  { name: 'atan',             signature: 'float atan(float x)',                  description: 'Arc tangent.',                              snippet: 'atan(${1:x})' },
  { name: 'atan2',            signature: 'float atan2(float y, float x)',         description: 'Arc tangent of y/x.',                       snippet: 'atan2(${1:y}, ${2:x})' },
  { name: 'min',              signature: 'value min(value a, value b)',           description: 'Return smaller of two values.',             snippet: 'min(${1:a}, ${2:b})' },
  { name: 'max',              signature: 'value max(value a, value b)',           description: 'Return larger of two values.',              snippet: 'max(${1:a}, ${2:b})' },
  { name: 'clamp',            signature: 'value clamp(value x, value lo, value hi)', description: 'Clamp x to [lo, hi].',                  snippet: 'clamp(${1:x}, ${2:lo}, ${3:hi})' },
  { name: 'approxEqual',      signature: 'bool approxEqual(float a, float b)',   description: 'Check approximate floating-point equality.', snippet: 'approxEqual(${1:a}, ${2:b})' },
  { name: 'isFinite',         signature: 'bool isFinite(float x)',               description: 'Check if value is finite.',                 snippet: 'isFinite(${1:x})' },
  { name: 'isInfinite',       signature: 'bool isInfinite(float x)',             description: 'Check if value is infinite.',               snippet: 'isInfinite(${1:x})' },
  { name: 'isNaN',            signature: 'bool isNaN(float x)',                  description: 'Check if value is NaN.',                    snippet: 'isNaN(${1:x})' },
  { name: 'isPositiveInfinity', signature: 'bool isPositiveInfinity(float x)',  description: 'Check if value is +Inf.',                   snippet: 'isPositiveInfinity(${1:x})' },
  { name: 'isNegativeInfinity', signature: 'bool isNegativeInfinity(float x)',  description: 'Check if value is -Inf.',                   snippet: 'isNegativeInfinity(${1:x})' },
  { name: 'safeDiv',          signature: 'float safeDiv(float a, float b)',      description: 'Division returning 0 on divide-by-zero.',   snippet: 'safeDiv(${1:a}, ${2:b})' },
  { name: 'safeMod',          signature: 'float safeMod(float a, float b)',      description: 'Modulo returning 0 on divide-by-zero.',     snippet: 'safeMod(${1:a}, ${2:b})' },

  // File I/O
  { name: 'readFile',    signature: 'string readFile(string path)',               description: 'Read entire file as string.',               snippet: 'readFile(${1:path})' },
  { name: 'writeFile',   signature: 'void writeFile(string path, string data)',   description: 'Write string to file (overwrite).',         snippet: 'writeFile(${1:path}, ${2:data})' },
  { name: 'appendFile',  signature: 'void appendFile(string path, string data)',  description: 'Append string to file.',                    snippet: 'appendFile(${1:path}, ${2:data})' },
  { name: 'fileExists',  signature: 'bool fileExists(string path)',               description: 'Check if file exists.',                     snippet: 'fileExists(${1:path})' },
  { name: 'deleteFile',  signature: 'void deleteFile(string path)',               description: 'Delete a file.',                            snippet: 'deleteFile(${1:path})' },
  { name: 'copyFile',    signature: 'void copyFile(string src, string dst)',      description: 'Copy a file.',                              snippet: 'copyFile(${1:src}, ${2:dst})' },
  { name: 'moveFile',    signature: 'void moveFile(string src, string dst)',      description: 'Move/rename a file.',                       snippet: 'moveFile(${1:src}, ${2:dst})' },
  { name: 'listDir',     signature: 'string[] listDir(string path)',              description: 'List directory entries.',                   snippet: 'listDir(${1:path})' },
  { name: 'makeDir',     signature: 'void makeDir(string path)',                  description: 'Create a directory.',                       snippet: 'makeDir(${1:path})' },
  { name: 'makeDirAll',  signature: 'void makeDirAll(string path)',               description: 'Create directory and all parents.',         snippet: 'makeDirAll(${1:path})' },
  { name: 'tempPath',    signature: 'string tempPath()',                          description: 'Return a temporary file path.',             snippet: 'tempPath()' },
  { name: 'pathJoin',    signature: 'string pathJoin(string a, string b)',        description: 'Join two path segments.',                   snippet: 'pathJoin(${1:a}, ${2:b})' },
  { name: 'pathDir',     signature: 'string pathDir(string path)',                description: 'Return directory component of path.',       snippet: 'pathDir(${1:path})' },
  { name: 'pathBase',    signature: 'string pathBase(string path)',               description: 'Return filename component of path.',        snippet: 'pathBase(${1:path})' },
  { name: 'pathExt',     signature: 'string pathExt(string path)',                description: 'Return file extension of path.',            snippet: 'pathExt(${1:path})' },
  { name: 'pathIsAbs',   signature: 'bool pathIsAbs(string path)',                description: 'Check if path is absolute.',                snippet: 'pathIsAbs(${1:path})' },

  // Exec / Env
  { name: 'exec',        signature: 'ExecResult exec(string cmd, string[] args)', description: 'Execute an external process.',              snippet: 'exec(${1:cmd}, ${2:args})' },
  { name: 'env',         signature: 'string env(string name)',                    description: 'Get environment variable (throws if missing).', snippet: 'env(${1:name})' },
  { name: 'envDefault',  signature: 'string envDefault(string name, string def)', description: 'Get environment variable with default.',    snippet: 'envDefault(${1:name}, ${2:default})' },

  // Time
  { name: 'now',          signature: 'int now()',                                 description: 'Return current Unix timestamp (seconds).',   snippet: 'now()' },
  { name: 'sleep',        signature: 'void sleep(int ms)',                        description: 'Sleep for ms milliseconds.',                  snippet: 'sleep(${1:ms})' },
  { name: 'formatDate',   signature: 'string formatDate(int epoch, string fmt)',  description: 'Format epoch time as string.',                snippet: 'formatDate(${1:epoch}, ${2:fmt})' },
  { name: 'parseDate',    signature: 'int parseDate(string date, string fmt)',    description: 'Parse date string to epoch.',                 snippet: 'parseDate(${1:date}, ${2:fmt})' },
  { name: 'makeEpoch',    signature: 'int makeEpoch(int y, int mo, int d, int h, int mi, int s)', description: 'Build epoch from components.', snippet: 'makeEpoch(${1:year}, ${2:month}, ${3:day}, ${4:hour}, ${5:min}, ${6:sec})' },
  { name: 'epochYear',    signature: 'int epochYear(int epoch)',                  description: 'Extract year from epoch.',                    snippet: 'epochYear(${1:epoch})' },
  { name: 'epochMonth',   signature: 'int epochMonth(int epoch)',                 description: 'Extract month from epoch (1-12).',            snippet: 'epochMonth(${1:epoch})' },
  { name: 'epochDay',     signature: 'int epochDay(int epoch)',                   description: 'Extract day of month from epoch.',            snippet: 'epochDay(${1:epoch})' },
  { name: 'epochHour',    signature: 'int epochHour(int epoch)',                  description: 'Extract hour from epoch.',                    snippet: 'epochHour(${1:epoch})' },
  { name: 'epochMinute',  signature: 'int epochMinute(int epoch)',                description: 'Extract minute from epoch.',                  snippet: 'epochMinute(${1:epoch})' },
  { name: 'epochSecond',  signature: 'int epochSecond(int epoch)',                description: 'Extract second from epoch.',                  snippet: 'epochSecond(${1:epoch})' },

  // Rand
  { name: 'rand',    signature: 'float rand()',                                   description: 'Return random float in [0, 1).',              snippet: 'rand()' },
  { name: 'randInt', signature: 'int randInt(int lo, int hi)',                    description: 'Return random int in [lo, hi].',              snippet: 'randInt(${1:lo}, ${2:hi})' },

  // Assert
  { name: 'assert',  signature: 'void assert(bool condition, string message)',    description: 'Throw RuntimeException if condition is false.', snippet: 'assert(${1:condition}, ${2:"message"})' },

  // Binary I/O
  { name: 'readBytes',   signature: 'byte[] readBytes(string path)',              description: 'Read file as byte array.',                    snippet: 'readBytes(${1:path})' },
  { name: 'writeBytes',  signature: 'void writeBytes(string path, byte[] data)', description: 'Write byte array to file.',                   snippet: 'writeBytes(${1:path}, ${2:data})' },
  { name: 'appendBytes', signature: 'void appendBytes(string path, byte[] data)', description: 'Append byte array to file.',                  snippet: 'appendBytes(${1:path}, ${2:data})' },

  // Regex
  { name: 'regexMatch',      signature: 'RegExpMatch regexMatch(RegExp re, string s)',       description: 'Match regex against string, return first match.', snippet: 'regexMatch(${1:re}, ${2:s})' },
  { name: 'regexTest',       signature: 'bool regexTest(RegExp re, string s)',               description: 'Test if regex matches anywhere in string.',       snippet: 'regexTest(${1:re}, ${2:s})' },
  { name: 'regexFindAll',    signature: 'RegExpMatch[] regexFindAll(RegExp re, string s)',   description: 'Find all matches of regex in string.',            snippet: 'regexFindAll(${1:re}, ${2:s})' },
  { name: 'regexReplace',    signature: 'string regexReplace(RegExp re, string s, string r)', description: 'Replace first match.',                          snippet: 'regexReplace(${1:re}, ${2:s}, ${3:replacement})' },
  { name: 'regexReplaceAll', signature: 'string regexReplaceAll(RegExp re, string s, string r)', description: 'Replace all matches.',                      snippet: 'regexReplaceAll(${1:re}, ${2:s}, ${3:replacement})' },

  // Terminal
  { name: 'termInit',      signature: 'void termInit()',                          description: 'Initialize raw terminal mode.',               snippet: 'termInit()' },
  { name: 'termRestore',   signature: 'void termRestore()',                       description: 'Restore terminal to previous mode.',          snippet: 'termRestore()' },
  { name: 'termClear',     signature: 'void termClear()',                         description: 'Clear the terminal screen.',                  snippet: 'termClear()' },
  { name: 'termClearLine', signature: 'void termClearLine()',                     description: 'Clear current terminal line.',                snippet: 'termClearLine()' },
  { name: 'termMoveTo',    signature: 'void termMoveTo(int row, int col)',        description: 'Move cursor to (row, col).',                  snippet: 'termMoveTo(${1:row}, ${2:col})' },
  { name: 'termSetColor',  signature: 'void termSetColor(int fg, int bg)',        description: 'Set foreground and background colors.',       snippet: 'termSetColor(${1:fg}, ${2:bg})' },
  { name: 'termResetColor', signature: 'void termResetColor()',                   description: 'Reset terminal color to default.',            snippet: 'termResetColor()' },
  { name: 'termBold',      signature: 'void termBold()',                          description: 'Enable bold text.',                          snippet: 'termBold()' },
  { name: 'termDim',       signature: 'void termDim()',                           description: 'Enable dim text.',                           snippet: 'termDim()' },
  { name: 'termUnderline', signature: 'void termUnderline()',                     description: 'Enable underline text.',                     snippet: 'termUnderline()' },
  { name: 'termReset',     signature: 'void termReset()',                         description: 'Reset all terminal attributes.',             snippet: 'termReset()' },
  { name: 'termGetSize',   signature: 'TerminalSize termGetSize()',                description: 'Get terminal dimensions.',                   snippet: 'termGetSize()' },
  { name: 'termReadKey',   signature: 'KeyEvent termReadKey()',                   description: 'Read a single key event from terminal.',     snippet: 'termReadKey()' },
  { name: 'shellInputBox', signature: 'string shellInputBox(string prompt)',      description: 'Show a shell-level input prompt box.',       snippet: 'shellInputBox(${1:prompt})' },

  // Type checks
  { name: 'isBlank',  signature: 'bool isBlank(string s)',                        description: 'Check if string is empty or whitespace-only.', snippet: 'isBlank(${1:s})' },
  { name: 'isInt',    signature: 'bool isInt(string s)',                          description: 'Check if string is a valid integer.',          snippet: 'isInt(${1:s})' },
  { name: 'isFloat',  signature: 'bool isFloat(string s)',                        description: 'Check if string is a valid float.',            snippet: 'isFloat(${1:s})' },
  { name: 'isBool',   signature: 'bool isBool(string s)',                         description: 'Check if string is "true" or "false".',        snippet: 'isBool(${1:s})' },

  // Map
  { name: 'mapKeys',     signature: 'string[] mapKeys(map m)',                    description: 'Return all keys of a map.',                   snippet: 'mapKeys(${1:m})' },
  { name: 'mapValues',   signature: 'array mapValues(map m)',                     description: 'Return all values of a map.',                 snippet: 'mapValues(${1:m})' },
  { name: 'mapContains', signature: 'bool mapContains(map m, string key)',         description: 'Check if map contains a key.',               snippet: 'mapContains(${1:m}, ${2:key})' },
  { name: 'mapRemove',   signature: 'void mapRemove(map m, string key)',           description: 'Remove a key from a map.',                   snippet: 'mapRemove(${1:m}, ${2:key})' },
  { name: 'mapCount',    signature: 'int mapCount(map m)',                         description: 'Return number of entries in a map.',         snippet: 'mapCount(${1:m})' },

  // Integer
  { name: 'intMax',   signature: 'int intMax(int a, int b)',                       description: 'Return larger of two integers.',             snippet: 'intMax(${1:a}, ${2:b})' },
  { name: 'intMin',   signature: 'int intMin(int a, int b)',                       description: 'Return smaller of two integers.',            snippet: 'intMin(${1:a}, ${2:b})' },
  { name: 'intAbs',   signature: 'int intAbs(int x)',                              description: 'Absolute value of integer.',                 snippet: 'intAbs(${1:x})' },
  { name: 'intClamp', signature: 'int intClamp(int x, int lo, int hi)',            description: 'Clamp integer to [lo, hi].',                 snippet: 'intClamp(${1:x}, ${2:lo}, ${3:hi})' },
];

const BUILTIN_CONSTANTS: BuiltinConstant[] = [
  { name: 'EOL',                   type: 'string', description: 'Platform end-of-line string.' },
  { name: 'INT_MAX',               type: 'int',    description: 'Maximum int value.' },
  { name: 'INT_MIN',               type: 'int',    description: 'Minimum int value.' },
  { name: 'INT_SIZE',              type: 'int',    description: 'Size of int in bits.' },
  { name: 'FLOAT_SIZE',            type: 'int',    description: 'Size of float in bits.' },
  { name: 'FLOAT_DIG',             type: 'int',    description: 'Decimal digits of float precision.' },
  { name: 'FLOAT_EPSILON',         type: 'float',  description: 'Smallest float difference from 1.0.' },
  { name: 'FLOAT_MIN',             type: 'float',  description: 'Minimum positive float.' },
  { name: 'FLOAT_MAX',             type: 'float',  description: 'Maximum float value.' },
  { name: 'M_PI',                  type: 'float',  description: 'Mathematical constant π.' },
  { name: 'M_E',                   type: 'float',  description: 'Mathematical constant e.' },
  { name: 'M_TAU',                 type: 'float',  description: 'Mathematical constant τ (2π).' },
  { name: 'M_SQRT2',               type: 'float',  description: 'Square root of 2.' },
  { name: 'M_LN2',                 type: 'float',  description: 'Natural log of 2.' },
  { name: 'M_LN10',                type: 'float',  description: 'Natural log of 10.' },
  { name: 'KEY_NONE',              type: 'int',    description: 'Key event: no key.' },
  { name: 'KEY_CHAR',              type: 'int',    description: 'Key event: printable character.' },
  { name: 'KEY_ENTER',             type: 'int',    description: 'Key event: Enter.' },
  { name: 'KEY_ESC',               type: 'int',    description: 'Key event: Escape.' },
  { name: 'KEY_TAB',               type: 'int',    description: 'Key event: Tab.' },
  { name: 'KEY_BACKSPACE',         type: 'int',    description: 'Key event: Backspace.' },
  { name: 'KEY_DELETE',            type: 'int',    description: 'Key event: Delete.' },
  { name: 'KEY_UP',                type: 'int',    description: 'Key event: Up arrow.' },
  { name: 'KEY_DOWN',              type: 'int',    description: 'Key event: Down arrow.' },
  { name: 'KEY_LEFT',              type: 'int',    description: 'Key event: Left arrow.' },
  { name: 'KEY_RIGHT',             type: 'int',    description: 'Key event: Right arrow.' },
  { name: 'KEY_HOME',              type: 'int',    description: 'Key event: Home.' },
  { name: 'KEY_END',               type: 'int',    description: 'Key event: End.' },
  { name: 'KEY_PAGE_UP',           type: 'int',    description: 'Key event: Page Up.' },
  { name: 'KEY_PAGE_DOWN',         type: 'int',    description: 'Key event: Page Down.' },
  { name: 'KEY_RESIZE',            type: 'int',    description: 'Key event: terminal resize.' },
  { name: 'TERM_COLOR_DEFAULT',    type: 'int',    description: 'Terminal color: default.' },
  { name: 'TERM_COLOR_BLACK',      type: 'int',    description: 'Terminal color: black.' },
  { name: 'TERM_COLOR_RED',        type: 'int',    description: 'Terminal color: red.' },
  { name: 'TERM_COLOR_GREEN',      type: 'int',    description: 'Terminal color: green.' },
  { name: 'TERM_COLOR_YELLOW',     type: 'int',    description: 'Terminal color: yellow.' },
  { name: 'TERM_COLOR_BLUE',       type: 'int',    description: 'Terminal color: blue.' },
  { name: 'TERM_COLOR_MAGENTA',    type: 'int',    description: 'Terminal color: magenta.' },
  { name: 'TERM_COLOR_CYAN',       type: 'int',    description: 'Terminal color: cyan.' },
  { name: 'TERM_COLOR_WHITE',      type: 'int',    description: 'Terminal color: white.' },
];

const BUILTIN_CLASSES: BuiltinClass[] = [
  {
    name: 'Exception',
    description: 'Base exception type.',
    fields: [
      { name: 'message', type: 'string' },
    ],
  },
  {
    name: 'RuntimeException',
    description: 'General runtime exception.',
    fields: [
      { name: 'message', type: 'string' },
    ],
  },
  {
    name: 'IoException',
    description: 'I/O exception (file, network, etc.).',
    fields: [
      { name: 'message', type: 'string' },
      { name: 'path', type: 'string' },
    ],
  },
  {
    name: 'RegExpException',
    description: 'Regular expression compilation or match exception.',
    fields: [
      { name: 'message', type: 'string' },
    ],
  },
  {
    name: 'ExecResult',
    description: 'Result of exec() call.',
    fields: [
      { name: 'stdout', type: 'string' },
      { name: 'stderr', type: 'string' },
      { name: 'exitCode', type: 'int' },
    ],
  },
  {
    name: 'RegExp',
    description: 'Compiled regular expression object.',
  },
  {
    name: 'RegExpMatch',
    description: 'A regex match result.',
    fields: [
      { name: 'match', type: 'string' },
      { name: 'groups', type: 'string[]' },
      { name: 'index', type: 'int' },
    ],
  },
  {
    name: 'TerminalSize',
    description: 'Terminal dimensions.',
    fields: [
      { name: 'rows', type: 'int' },
      { name: 'cols', type: 'int' },
    ],
  },
  {
    name: 'KeyEvent',
    description: 'A keyboard event from termReadKey().',
    fields: [
      { name: 'kind', type: 'int' },
      { name: 'ch', type: 'string' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Parsed workspace symbol types
// ---------------------------------------------------------------------------

interface ParsedField {
  name: string;
  type: string;
}

interface ParsedMethod {
  name: string;
  returnType: string;
  params: string;
  isConstructor: boolean;
}

interface ParsedStructure {
  name: string;
  baseClass?: string;
  fields: ParsedField[];
  methods: ParsedMethod[];
  filePath: string;
  line: number;
}

interface ParsedUnion {
  name: string;
  fields: ParsedField[];
  filePath: string;
  line: number;
}

interface ParsedFunction {
  name: string;
  returnType: string;
  params: string;
  filePath: string;
  line: number;
}

interface ParsedFile {
  structures: ParsedStructure[];
  unions: ParsedUnion[];
  functions: ParsedFunction[];
}

// ---------------------------------------------------------------------------
// File parser
// ---------------------------------------------------------------------------

function parseFile(content: string, filePath: string): ParsedFile {
  const result: ParsedFile = { structures: [], unions: [], functions: [] };
  const lines = content.split('\n');

  let insideStructure: ParsedStructure | null = null;
  let insideUnion: ParsedUnion | null = null;
  let braceDepth = 0;
  let structureStartDepth = -1;

  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const stripped = stripLineComment(raw);

    // Track brace depth
    const openCount = (stripped.match(/\{/g) || []).length;
    const closeCount = (stripped.match(/\}/g) || []).length;

    // Check for structure declaration
    const structMatch = stripped.match(/^(structure)\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s*:\s*([A-Za-z_][A-Za-z0-9_]*))?/);
    if (structMatch && braceDepth === 0) {
      insideStructure = {
        name: structMatch[2],
        baseClass: structMatch[3],
        fields: [],
        methods: [],
        filePath,
        line: i,
      };
      structureStartDepth = braceDepth;
    }

    // Check for union declaration
    const unionMatch = stripped.match(/^(union)\s+([A-Za-z_][A-Za-z0-9_]*)/);
    if (unionMatch && braceDepth === 0) {
      insideUnion = {
        name: unionMatch[2],
        fields: [],
        filePath,
        line: i,
      };
    }

    braceDepth += openCount - closeCount;

    // End of structure/union block
    if (insideStructure && structureStartDepth >= 0 && braceDepth <= structureStartDepth) {
      result.structures.push(insideStructure);
      insideStructure = null;
      structureStartDepth = -1;
    }

    if (insideUnion && braceDepth === 0 && closeCount > 0) {
      result.unions.push(insideUnion);
      insideUnion = null;
    }

    // Parse members inside structures (depth 1)
    if (insideStructure && braceDepth === 1) {
      // Method: returnType name(...)
      const methodMatch = stripped.match(/^\s+([A-Za-z_][A-Za-z0-9_]*(?:\[\])?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)/);
      if (methodMatch) {
        insideStructure.methods.push({
          name: methodMatch[2],
          returnType: methodMatch[1],
          params: methodMatch[3],
          isConstructor: methodMatch[2] === '_init',
        });
        continue;
      }
      // Field: type name (optional = default);
      const fieldMatch = stripped.match(/^\s+([A-Za-z_][A-Za-z0-9_]*(?:\[\])?)\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s*=.*)?;/);
      if (fieldMatch) {
        insideStructure.fields.push({ name: fieldMatch[2], type: fieldMatch[1] });
      }
    }

    // Parse union members (depth 1)
    if (insideUnion && braceDepth === 1) {
      const fieldMatch = stripped.match(/^\s+([A-Za-z_][A-Za-z0-9_]*(?:\[\])?)\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s*=.*)?;/);
      if (fieldMatch) {
        insideUnion.fields.push({ name: fieldMatch[2], type: fieldMatch[1] });
      }
    }

    // Top-level functions (depth 0, before we processed open braces above)
    if (!insideStructure && !insideUnion && braceDepth === (openCount > 0 ? 1 : 0)) {
      const funcMatch = stripped.match(/^([A-Za-z_][A-Za-z0-9_]*(?:\[\])?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*\{?/);
      if (funcMatch && openCount > 0) {
        const name = funcMatch[2];
        // Exclude keywords that look like identifiers
        const keywords = new Set(['if', 'else', 'while', 'for', 'return', 'break', 'continue',
          'try', 'catch', 'finally', 'throw', 'structure', 'union', 'import', 'clone', 'in']);
        if (!keywords.has(name) && !keywords.has(funcMatch[1])) {
          result.functions.push({
            name,
            returnType: funcMatch[1],
            params: funcMatch[3],
            filePath,
            line: i,
          });
        }
      }
    }
  }

  return result;
}

function stripLineComment(line: string): string {
  const idx = line.indexOf('//');
  return idx >= 0 ? line.substring(0, idx) : line;
}

// ---------------------------------------------------------------------------
// Symbol cache
// ---------------------------------------------------------------------------

class SymbolCache {
  private cache = new Map<string, ParsedFile>();

  update(uri: vscode.Uri, content: string): void {
    const parsed = parseFile(content, uri.fsPath);
    this.cache.set(uri.fsPath, parsed);
  }

  remove(uri: vscode.Uri): void {
    this.cache.delete(uri.fsPath);
  }

  get(uri: vscode.Uri): ParsedFile | undefined {
    return this.cache.get(uri.fsPath);
  }

  allStructures(): ParsedStructure[] {
    const result: ParsedStructure[] = [];
    for (const f of this.cache.values()) {
      result.push(...f.structures);
    }
    return result;
  }

  allUnions(): ParsedUnion[] {
    const result: ParsedUnion[] = [];
    for (const f of this.cache.values()) {
      result.push(...f.unions);
    }
    return result;
  }

  allFunctions(): ParsedFunction[] {
    const result: ParsedFunction[] = [];
    for (const f of this.cache.values()) {
      result.push(...f.functions);
    }
    return result;
  }
}

// ---------------------------------------------------------------------------
// Completion item builders
// ---------------------------------------------------------------------------

function makeBuiltinFunctionItem(fn: BuiltinFunction): vscode.CompletionItem {
  const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
  item.detail = fn.signature;
  item.documentation = new vscode.MarkdownString(fn.description);
  item.insertText = new vscode.SnippetString(fn.snippet);
  return item;
}

function makeConstantItem(c: BuiltinConstant): vscode.CompletionItem {
  const item = new vscode.CompletionItem(c.name, vscode.CompletionItemKind.Constant);
  item.detail = `(${c.type}) ${c.name}`;
  item.documentation = new vscode.MarkdownString(c.description);
  return item;
}

function makeBuiltinClassItem(cls: BuiltinClass): vscode.CompletionItem {
  const item = new vscode.CompletionItem(cls.name, vscode.CompletionItemKind.Class);
  item.detail = cls.name;
  item.documentation = new vscode.MarkdownString(cls.description);
  return item;
}

function makeStructureItem(s: ParsedStructure): vscode.CompletionItem {
  const item = new vscode.CompletionItem(s.name, vscode.CompletionItemKind.Class);
  item.detail = s.baseClass ? `structure ${s.name} : ${s.baseClass}` : `structure ${s.name}`;
  item.documentation = new vscode.MarkdownString(`User-defined structure from \`${path.basename(s.filePath)}\``);
  return item;
}

function makeCloneSnippetItem(s: ParsedStructure): vscode.CompletionItem {
  const item = new vscode.CompletionItem(`clone ${s.name}`, vscode.CompletionItemKind.Constructor);
  item.insertText = new vscode.SnippetString(`clone ${s.name}(\${1})`);
  item.detail = `Clone ${s.name} instance`;
  item.documentation = new vscode.MarkdownString(`Create a new instance of \`${s.name}\`.`);
  return item;
}

function makeUnionItem(u: ParsedUnion): vscode.CompletionItem {
  const item = new vscode.CompletionItem(u.name, vscode.CompletionItemKind.Struct);
  item.detail = `union ${u.name}`;
  item.documentation = new vscode.MarkdownString(`User-defined union from \`${path.basename(u.filePath)}\``);
  return item;
}

function makeFunctionItem(fn: ParsedFunction): vscode.CompletionItem {
  const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
  item.detail = `${fn.returnType} ${fn.name}(${fn.params})`;
  item.documentation = new vscode.MarkdownString(`Defined in \`${path.basename(fn.filePath)}\``);
  return item;
}

function makeMemberItem(name: string, type: string, kind: vscode.CompletionItemKind): vscode.CompletionItem {
  const item = new vscode.CompletionItem(name, kind);
  item.detail = type ? `${type} ${name}` : name;
  return item;
}

// ---------------------------------------------------------------------------
// Resolve member type from current document
// ---------------------------------------------------------------------------

function resolveVariableType(
  varName: string,
  documentText: string,
  cache: SymbolCache
): ParsedStructure | BuiltinClass | null {
  // Look for variable declaration: type name or type name = ...
  const declPattern = new RegExp(
    `\\b([A-Za-z_][A-Za-z0-9_]*)(?:\\[\\])?\\s+${escapeRegex(varName)}\\s*(?:=|;|,|\\))`,
    'g'
  );
  let match: RegExpExecArray | null;
  let typeName: string | undefined;
  while ((match = declPattern.exec(documentText)) !== null) {
    typeName = match[1];
  }
  if (!typeName) return null;

  // Check user structures
  const struct = cache.allStructures().find(s => s.name === typeName);
  if (struct) return struct;

  // Check builtin classes
  const builtin = BUILTIN_CLASSES.find(c => c.name === typeName);
  if (builtin) return builtin;

  return null;
}

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// ---------------------------------------------------------------------------
// Completion provider
// ---------------------------------------------------------------------------

class CimpleCompletionProvider implements vscode.CompletionItemProvider {
  constructor(private cache: SymbolCache) {}

  provideCompletionItems(
    document: vscode.TextDocument,
    position: vscode.Position,
    _token: vscode.CancellationToken,
    _context: vscode.CompletionContext
  ): vscode.ProviderResult<vscode.CompletionItem[]> {
    const linePrefix = document.lineAt(position).text.substring(0, position.character);

    // --- Dot completion ---
    const dotMatch = linePrefix.match(/\b([A-Za-z_][A-Za-z0-9_]*)\.\s*$/);
    if (dotMatch) {
      const varName = dotMatch[1];
      const items: vscode.CompletionItem[] = [];

      const resolved = resolveVariableType(varName, document.getText(), this.cache);
      if (resolved) {
        if ('fields' in resolved && resolved.fields) {
          for (const field of resolved.fields) {
            items.push(makeMemberItem(field.name, field.type, vscode.CompletionItemKind.Field));
          }
        }
        if ('methods' in resolved && (resolved as ParsedStructure).methods) {
          for (const method of (resolved as ParsedStructure).methods) {
            const item = makeMemberItem(method.name, method.returnType, vscode.CompletionItemKind.Method);
            item.detail = `${method.returnType} ${method.name}(${method.params})`;
            items.push(item);
          }
        }
      }

      // Always show built-in class members for known prefix names
      const builtinClass = BUILTIN_CLASSES.find(c => c.name === varName);
      if (builtinClass) {
        for (const field of (builtinClass.fields ?? [])) {
          items.push(makeMemberItem(field.name, field.type, vscode.CompletionItemKind.Field));
        }
        for (const method of (builtinClass.methods ?? [])) {
          items.push(makeMemberItem(method.name, method.signature, vscode.CompletionItemKind.Method));
        }
      }

      return items.length > 0 ? items : undefined;
    }

    // --- Global completions ---
    const items: vscode.CompletionItem[] = [];

    // Built-in functions
    for (const fn of BUILTIN_FUNCTIONS) {
      items.push(makeBuiltinFunctionItem(fn));
    }

    // Built-in constants
    for (const c of BUILTIN_CONSTANTS) {
      items.push(makeConstantItem(c));
    }

    // Built-in classes
    for (const cls of BUILTIN_CLASSES) {
      items.push(makeBuiltinClassItem(cls));
    }

    // User-defined structures
    for (const s of this.cache.allStructures()) {
      items.push(makeStructureItem(s));
      items.push(makeCloneSnippetItem(s));
    }

    // User-defined unions
    for (const u of this.cache.allUnions()) {
      items.push(makeUnionItem(u));
    }

    // User-defined functions
    for (const fn of this.cache.allFunctions()) {
      items.push(makeFunctionItem(fn));
    }

    return items;
  }
}

// ---------------------------------------------------------------------------
// Hover provider
// ---------------------------------------------------------------------------

class CimpleHoverProvider implements vscode.HoverProvider {
  provideHover(
    document: vscode.TextDocument,
    position: vscode.Position,
    _token: vscode.CancellationToken
  ): vscode.ProviderResult<vscode.Hover> {
    const wordRange = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
    if (!wordRange) return undefined;
    const word = document.getText(wordRange);

    // Check built-in functions
    const fn = BUILTIN_FUNCTIONS.find(f => f.name === word);
    if (fn) {
      const md = new vscode.MarkdownString();
      md.appendCodeblock(fn.signature, 'cimple');
      md.appendMarkdown(`\n\n${fn.description}`);
      return new vscode.Hover(md, wordRange);
    }

    // Check built-in constants
    const constant = BUILTIN_CONSTANTS.find(c => c.name === word);
    if (constant) {
      const md = new vscode.MarkdownString();
      md.appendCodeblock(`${constant.type} ${constant.name}`, 'cimple');
      md.appendMarkdown(`\n\n${constant.description}`);
      return new vscode.Hover(md, wordRange);
    }

    // Check built-in classes
    const cls = BUILTIN_CLASSES.find(c => c.name === word);
    if (cls) {
      const md = new vscode.MarkdownString();
      md.appendCodeblock(`structure ${cls.name}`, 'cimple');
      md.appendMarkdown(`\n\n${cls.description}`);
      if (cls.fields && cls.fields.length > 0) {
        md.appendMarkdown('\n\n**Fields:**\n');
        for (const f of cls.fields) {
          md.appendMarkdown(`- \`${f.type} ${f.name}\`\n`);
        }
      }
      return new vscode.Hover(md, wordRange);
    }

    return undefined;
  }
}

// ---------------------------------------------------------------------------
// Document symbol provider
// ---------------------------------------------------------------------------

class CimpleDocumentSymbolProvider implements vscode.DocumentSymbolProvider {
  constructor(private cache: SymbolCache) {}

  provideDocumentSymbols(
    document: vscode.TextDocument,
    _token: vscode.CancellationToken
  ): vscode.ProviderResult<vscode.DocumentSymbol[]> {
    const parsed = this.cache.get(document.uri);
    if (!parsed) return [];

    const symbols: vscode.DocumentSymbol[] = [];

    // Structures
    for (const s of parsed.structures) {
      const range = new vscode.Range(s.line, 0, s.line, 200);
      const sym = new vscode.DocumentSymbol(
        s.name,
        s.baseClass ? `: ${s.baseClass}` : '',
        vscode.SymbolKind.Class,
        range,
        range
      );

      // Fields
      for (const f of s.fields) {
        const fieldSym = new vscode.DocumentSymbol(
          f.name,
          f.type,
          vscode.SymbolKind.Field,
          range,
          range
        );
        sym.children.push(fieldSym);
      }

      // Methods
      for (const m of s.methods) {
        const kind = m.isConstructor ? vscode.SymbolKind.Constructor : vscode.SymbolKind.Method;
        const methodSym = new vscode.DocumentSymbol(
          m.name,
          `${m.returnType}(${m.params})`,
          kind,
          range,
          range
        );
        sym.children.push(methodSym);
      }

      symbols.push(sym);
    }

    // Unions
    for (const u of parsed.unions) {
      const range = new vscode.Range(u.line, 0, u.line, 200);
      const sym = new vscode.DocumentSymbol(
        u.name,
        'union',
        vscode.SymbolKind.Struct,
        range,
        range
      );
      for (const f of u.fields) {
        const fieldSym = new vscode.DocumentSymbol(
          f.name,
          f.type,
          vscode.SymbolKind.Field,
          range,
          range
        );
        sym.children.push(fieldSym);
      }
      symbols.push(sym);
    }

    // Functions
    for (const fn of parsed.functions) {
      const range = new vscode.Range(fn.line, 0, fn.line, 200);
      const sym = new vscode.DocumentSymbol(
        fn.name,
        `${fn.returnType}(${fn.params})`,
        vscode.SymbolKind.Function,
        range,
        range
      );
      symbols.push(sym);
    }

    return symbols;
  }
}

// ---------------------------------------------------------------------------
// Extension activation
// ---------------------------------------------------------------------------

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  const cache = new SymbolCache();

  // Load all workspace .ci files
  const uris = await vscode.workspace.findFiles('**/*.ci', '**/node_modules/**');
  for (const uri of uris) {
    try {
      const bytes = await vscode.workspace.fs.readFile(uri);
      const text = Buffer.from(bytes).toString('utf8');
      cache.update(uri, text);
    } catch (_) {
      // Ignore unreadable files
    }
  }

  // Watch for file changes
  const watcher = vscode.workspace.createFileSystemWatcher('**/*.ci');

  watcher.onDidChange(async uri => {
    try {
      const bytes = await vscode.workspace.fs.readFile(uri);
      cache.update(uri, Buffer.from(bytes).toString('utf8'));
    } catch (_) { /* ignore */ }
  });

  watcher.onDidCreate(async uri => {
    try {
      const bytes = await vscode.workspace.fs.readFile(uri);
      cache.update(uri, Buffer.from(bytes).toString('utf8'));
    } catch (_) { /* ignore */ }
  });

  watcher.onDidDelete(uri => {
    cache.remove(uri);
  });

  context.subscriptions.push(watcher);

  // Also update cache on document edits (for the active document)
  context.subscriptions.push(
    vscode.workspace.onDidChangeTextDocument(event => {
      if (event.document.languageId === 'cimple') {
        cache.update(event.document.uri, event.document.getText());
      }
    })
  );

  // Register completion provider (trigger on '.' and '(')
  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      { language: 'cimple', scheme: 'file' },
      new CimpleCompletionProvider(cache),
      '.', '('
    )
  );

  // Register hover provider
  context.subscriptions.push(
    vscode.languages.registerHoverProvider(
      { language: 'cimple', scheme: 'file' },
      new CimpleHoverProvider()
    )
  );

  // Register document symbol provider
  context.subscriptions.push(
    vscode.languages.registerDocumentSymbolProvider(
      { language: 'cimple', scheme: 'file' },
      new CimpleDocumentSymbolProvider(cache)
    )
  );
}

export function deactivate(): void {
  // Nothing to clean up beyond what context.subscriptions handles
}
