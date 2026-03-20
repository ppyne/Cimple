import {
    createConnection,
    TextDocuments,
    ProposedFeatures,
    InitializeParams,
    InitializeResult,
    TextDocumentSyncKind,
    CompletionItem,
    CompletionItemKind,
    Diagnostic,
    DiagnosticSeverity,
    Range,
    Hover,
    Location,
    DocumentSymbol,
    SymbolKind,
    SymbolInformation,
    SignatureHelp,
    SignatureInformation,
    ParameterInformation,
    TextDocumentPositionParams,
    WorkspaceSymbolParams,
    DefinitionParams,
    ReferenceParams,
    DocumentSymbolParams,
    SignatureHelpParams,
    CompletionParams,
    MarkupKind,
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { URI } from 'vscode-uri';
import * as cp from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import { BUILTIN_FUNCTIONS, BUILTIN_CONSTANTS, BUILTIN_TYPES } from './builtins';

// ---------------------------------------------------------------------------
// Connection and document manager
// ---------------------------------------------------------------------------

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// ---------------------------------------------------------------------------
// Symbol index
// ---------------------------------------------------------------------------

interface CimpleSymbol {
    name: string;
    kind: 'function' | 'structure' | 'union' | 'method' | 'field' | 'variable';
    returnType: string;
    params: { name: string; type: string }[];
    uri: string;
    line: number;       // 0-based
    col: number;        // 0-based
    endLine: number;
    structName?: string; // for methods/fields
    baseType?: string;   // for structures (inheritance)
    docComment?: string;
}

interface FileSymbols {
    uri: string;
    symbols: CimpleSymbol[];
    version: number;
}

const symbolIndex = new Map<string, FileSymbols>();
let workspaceFolders: string[] = [];

// ---------------------------------------------------------------------------
// Debounce map for diagnostics
// ---------------------------------------------------------------------------

const debounceTimers = new Map<string, ReturnType<typeof setTimeout>>();

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

let configuredBinPath = '';

// ---------------------------------------------------------------------------
// File parsing
// ---------------------------------------------------------------------------

function parseParams(paramStr: string): { name: string; type: string }[] {
    const result: { name: string; type: string }[] = [];
    if (!paramStr.trim()) return result;
    const parts = paramStr.split(',');
    for (const part of parts) {
        const trimmed = part.trim();
        if (!trimmed) continue;
        // Match: type[] name  or  type[type] name  or  type name
        const m = trimmed.match(/^([\w]+(?:\[\w*\]|\[\])*)\s+(\w+)$/);
        if (m) {
            result.push({ name: m[2], type: m[1] });
        } else {
            // Fallback: treat the whole thing as a name-less type
            result.push({ name: trimmed, type: trimmed });
        }
    }
    return result;
}

function parseFile(uri: string, content: string): FileSymbols {
    const symbols: CimpleSymbol[] = [];
    const lines = content.split('\n');

    // Track brace depth for identifying top-level vs nested
    // We do a line-based scan with brace counting

    // First, collect doc comments per line
    const docComments = new Map<number, string>();
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trimEnd();
        if (line.trimStart().startsWith('//')) {
            docComments.set(i + 1, line.trimStart().replace(/^\/\/\s*/, ''));
        }
    }

    // Top-level function pattern: must start at column 0 (no leading whitespace)
    // Captures: returnType  funcName  (params)  {
    const topFuncRe = /^((?:[\w]+)(?:\[\]|\[[\w]+\])?)\s+(\w+)\s*\(([^)]*)\)\s*\{/gm;

    // Structure pattern
    const structRe = /^structure\s+(\w+)(?:\s*:\s*(\w+))?\s*\{/gm;

    // Union pattern
    const unionRe = /^union\s+(\w+)\s*\{/gm;

    // We need line numbers, so scan line by line for top-level constructs
    let braceDepth = 0;
    let inStruct: CimpleSymbol | null = null;
    let structBraceDepth = 0;

    for (let lineIdx = 0; lineIdx < lines.length; lineIdx++) {
        const line = lines[lineIdx];
        const rawLine = line;

        // Count brace changes on this line (simple heuristic, ignores strings/comments)
        const openBraces = (rawLine.match(/\{/g) || []).length;
        const closeBraces = (rawLine.match(/\}/g) || []).length;

        // Check if we're at top level (braceDepth == 0 before processing)
        const isTopLevel = braceDepth === 0;

        if (isTopLevel) {
            // Check for structure
            const structMatch = /^structure\s+(\w+)(?:\s*:\s*(\w+))?\s*\{/.exec(rawLine);
            if (structMatch) {
                const sym: CimpleSymbol = {
                    name: structMatch[1],
                    kind: 'structure',
                    returnType: structMatch[1],
                    params: [],
                    uri,
                    line: lineIdx,
                    col: 0,
                    endLine: lineIdx,
                    baseType: structMatch[2],
                    docComment: docComments.get(lineIdx),
                };
                symbols.push(sym);
                inStruct = sym;
                structBraceDepth = braceDepth + openBraces - closeBraces;
                braceDepth += openBraces - closeBraces;
                continue;
            }

            // Check for union
            const unionMatch = /^union\s+(\w+)\s*\{/.exec(rawLine);
            if (unionMatch) {
                symbols.push({
                    name: unionMatch[1],
                    kind: 'union',
                    returnType: unionMatch[1],
                    params: [],
                    uri,
                    line: lineIdx,
                    col: 0,
                    endLine: lineIdx,
                    docComment: docComments.get(lineIdx),
                });
                braceDepth += openBraces - closeBraces;
                continue;
            }

            // Check for top-level function
            const funcMatch = /^([\w]+(?:\[\]|\[[\w]+\])?)\s+(\w+)\s*\(([^)]*)\)\s*\{/.exec(rawLine);
            if (funcMatch && funcMatch[2] !== 'structure' && funcMatch[2] !== 'union') {
                symbols.push({
                    name: funcMatch[2],
                    kind: 'function',
                    returnType: funcMatch[1],
                    params: parseParams(funcMatch[3]),
                    uri,
                    line: lineIdx,
                    col: 0,
                    endLine: lineIdx,
                    docComment: docComments.get(lineIdx),
                });
                braceDepth += openBraces - closeBraces;
                continue;
            }
        } else if (inStruct && braceDepth === structBraceDepth) {
            // Inside structure body at one level deep
            const trimmed = rawLine.trimStart();

            // Method inside struct: "  returnType methodName(params) {"
            const methodMatch = /^\s+([\w]+(?:\[\]|\[[\w]+\])?)\s+(\w+)\s*\(([^)]*)\)\s*(?:\{|;)/.exec(rawLine);
            if (methodMatch) {
                symbols.push({
                    name: methodMatch[2],
                    kind: 'method',
                    returnType: methodMatch[1],
                    params: parseParams(methodMatch[3]),
                    uri,
                    line: lineIdx,
                    col: rawLine.indexOf(methodMatch[2]),
                    endLine: lineIdx,
                    structName: inStruct.name,
                    docComment: docComments.get(lineIdx),
                });
            } else {
                // Field inside struct: "  type name;" or "  type name = expr;"
                const fieldMatch = /^\s+([\w]+(?:\[\]|\[[\w]+\])?)\s+(\w+)(?:\s*=\s*[^;]+)?;/.exec(rawLine);
                if (fieldMatch && !trimmed.startsWith('//')) {
                    symbols.push({
                        name: fieldMatch[2],
                        kind: 'field',
                        returnType: fieldMatch[1],
                        params: [],
                        uri,
                        line: lineIdx,
                        col: rawLine.indexOf(fieldMatch[2]),
                        endLine: lineIdx,
                        structName: inStruct.name,
                    });
                }
            }
        }

        braceDepth += openBraces - closeBraces;
        if (braceDepth < 0) braceDepth = 0;

        // Check if struct ended
        if (inStruct && braceDepth < structBraceDepth) {
            inStruct.endLine = lineIdx;
            inStruct = null;
        }
    }

    return { uri, symbols, version: Date.now() };
}

// ---------------------------------------------------------------------------
// Workspace indexing
// ---------------------------------------------------------------------------

async function indexWorkspace(): Promise<void> {
    for (const folder of workspaceFolders) {
        await indexDirectory(folder);
    }
}

async function indexDirectory(dir: string): Promise<void> {
    let entries: fs.Dirent[];
    try {
        entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
        return;
    }

    for (const entry of entries) {
        if (entry.name.startsWith('.') || entry.name === 'node_modules' || entry.name === 'build') {
            continue;
        }
        const fullPath = path.join(dir, entry.name);
        if (entry.isDirectory()) {
            await indexDirectory(fullPath);
        } else if (entry.isFile() && entry.name.endsWith('.ci')) {
            try {
                const content = fs.readFileSync(fullPath, 'utf-8');
                const uri = URI.file(fullPath).toString();
                symbolIndex.set(uri, parseFile(uri, content));
            } catch {
                // ignore unreadable files
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Find cimple binary
// ---------------------------------------------------------------------------

function findCimpleBin(): string | null {
    // 1. Configuration setting
    if (configuredBinPath && fs.existsSync(configuredBinPath)) {
        return configuredBinPath;
    }
    // 2. CIMPLE_BIN environment variable
    if (process.env.CIMPLE_BIN && fs.existsSync(process.env.CIMPLE_BIN)) {
        return process.env.CIMPLE_BIN;
    }
    // 3. Search workspace roots for build/cimple or build/cimple.exe
    for (const ws of workspaceFolders) {
        const candidates = [
            path.join(ws, 'build', 'cimple'),
            path.join(ws, 'build', 'cimple.exe'),
            path.join(ws, 'build', 'Debug', 'cimple.exe'),
            path.join(ws, 'build', 'Release', 'cimple.exe'),
        ];
        for (const c of candidates) {
            if (fs.existsSync(c)) return c;
        }
    }
    // 4. PATH
    try {
        cp.execSync('cimple --version', { stdio: 'ignore', timeout: 1000 });
        return 'cimple';
    } catch {
        return null;
    }
}

// ---------------------------------------------------------------------------
// Diagnostic parsing
// ---------------------------------------------------------------------------

function parseDiagnostics(stderr: string, filePath: string): Diagnostic[] {
    const diags: Diagnostic[] = [];
    const fileName = path.basename(filePath);

    // Pattern: [TYPE ERROR/WARNING]  filename  line N, column M\n  message\n  -> hint (optional)
    const blockRe = /\[(LEXICAL|SYNTAX|SEMANTIC|RUNTIME)\s+(ERROR|WARNING)\]\s+(\S+)\s+line\s+(\d+),\s+column\s+(\d+)\n\s+(.*?)(?:\n\s+->\s+(.*?))?(?=\n\[|\n\d+\s+semantic|\s*$)/gs;

    let m: RegExpExecArray | null;
    while ((m = blockRe.exec(stderr)) !== null) {
        const [, errType, severity, errFile, lineStr, colStr, message, hint] = m;
        // Only include if this file matches
        if (path.basename(errFile) !== fileName) continue;

        const line = Math.max(0, parseInt(lineStr) - 1);
        const col = Math.max(0, parseInt(colStr) - 1);
        const trimmedMsg = message.trim();
        const msg = hint ? `${trimmedMsg}\n${hint.trim()}` : trimmedMsg;

        diags.push({
            range: Range.create(line, col, line, col + 999),
            severity: severity === 'WARNING' ? DiagnosticSeverity.Warning : DiagnosticSeverity.Error,
            message: msg,
            source: `cimple ${errType.toLowerCase()}`,
        });
    }
    return diags;
}

// ---------------------------------------------------------------------------
// Run cimple check
// ---------------------------------------------------------------------------

async function runCheck(filePath: string): Promise<Diagnostic[]> {
    return new Promise(resolve => {
        const cimpleBin = findCimpleBin();
        if (!cimpleBin) {
            connection.console.warn('Cimple binary not found. Set cimple.executablePath or build the project.');
            resolve([]);
            return;
        }

        const proc = cp.spawn(cimpleBin, ['check', filePath], { timeout: 5000 });
        let stderr = '';
        proc.stderr.on('data', (d: Buffer) => { stderr += d.toString(); });
        proc.on('close', () => resolve(parseDiagnostics(stderr, filePath)));
        proc.on('error', () => resolve([]));
    });
}

async function validateDocument(document: TextDocument): Promise<void> {
    const filePath = URI.parse(document.uri).fsPath;
    const diags = await runCheck(filePath);
    connection.sendDiagnostics({ uri: document.uri, diagnostics: diags });
}

function scheduleValidation(document: TextDocument, delayMs: number): void {
    const uri = document.uri;
    const existing = debounceTimers.get(uri);
    if (existing) clearTimeout(existing);
    const timer = setTimeout(() => {
        debounceTimers.delete(uri);
        validateDocument(document);
    }, delayMs);
    debounceTimers.set(uri, timer);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function getWordAtPosition(text: string, lines: string[], position: { line: number; character: number }): string {
    const lineText = lines[position.line] || '';
    const char = position.character;
    let start = char;
    let end = char;
    while (start > 0 && /\w/.test(lineText[start - 1])) start--;
    while (end < lineText.length && /\w/.test(lineText[end])) end++;
    return lineText.slice(start, end);
}

function getWordRangeAtPosition(lines: string[], position: { line: number; character: number }): { word: string; start: number; end: number } {
    const lineText = lines[position.line] || '';
    const char = position.character;
    let start = char;
    let end = char;
    while (start > 0 && /\w/.test(lineText[start - 1])) start--;
    while (end < lineText.length && /\w/.test(lineText[end])) end++;
    return { word: lineText.slice(start, end), start, end };
}


function symbolKindForCimple(kind: CimpleSymbol['kind']): SymbolKind {
    switch (kind) {
        case 'function': return SymbolKind.Function;
        case 'structure': return SymbolKind.Class;
        case 'union': return SymbolKind.Struct;
        case 'method': return SymbolKind.Method;
        case 'field': return SymbolKind.Field;
        case 'variable': return SymbolKind.Variable;
    }
}

function makeParamsSnippet(params: { name: string; type: string }[]): string {
    if (params.length === 0) return '()';
    const parts = params.map((p, i) => `\${${i + 1}:${p.name}}`);
    return `(${parts.join(', ')})`;
}

function buildSignatureLabel(sym: CimpleSymbol): string {
    const paramStr = sym.params.map(p => `${p.type} ${p.name}`).join(', ');
    return `${sym.returnType} ${sym.name}(${paramStr})`;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

connection.onInitialize(async (params: InitializeParams): Promise<InitializeResult> => {
    workspaceFolders = (params.workspaceFolders ?? []).map(f => URI.parse(f.uri).fsPath);
    // Kick off workspace indexing asynchronously
    indexWorkspace().catch(e => connection.console.error(`Index error: ${e}`));

    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: false,
                triggerCharacters: ['.', '('],
            },
            hoverProvider: true,
            definitionProvider: true,
            referencesProvider: true,
            documentSymbolProvider: true,
            workspaceSymbolProvider: true,
            signatureHelpProvider: {
                triggerCharacters: ['(', ','],
            },
        },
    };
});

connection.onInitialized(async () => {
    // Read configuration
    try {
        const config = await connection.workspace.getConfiguration('cimple');
        if (config && config.executablePath) {
            configuredBinPath = config.executablePath;
        }
    } catch {
        // configuration not available
    }
});

// ---------------------------------------------------------------------------
// Document lifecycle
// ---------------------------------------------------------------------------

documents.onDidOpen(e => {
    // Index the opened document
    const doc = e.document;
    symbolIndex.set(doc.uri, parseFile(doc.uri, doc.getText()));
    validateDocument(doc);
});

documents.onDidChangeContent(e => {
    const doc = e.document;
    // Update index immediately
    symbolIndex.set(doc.uri, parseFile(doc.uri, doc.getText()));
    // Debounce diagnostics
    scheduleValidation(doc, 500);
});

documents.onDidSave(e => {
    validateDocument(e.document);
});

documents.onDidClose(e => {
    connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] });
    symbolIndex.delete(e.document.uri);
});

// ---------------------------------------------------------------------------
// Watched files
// ---------------------------------------------------------------------------

connection.onDidChangeWatchedFiles(async params => {
    for (const change of params.changes) {
        const uri = change.uri;
        const filePath = URI.parse(uri).fsPath;
        if (change.type === 3 /* Deleted */) {
            symbolIndex.delete(uri);
        } else {
            try {
                const content = fs.readFileSync(filePath, 'utf-8');
                symbolIndex.set(uri, parseFile(uri, content));
            } catch {
                // ignore
            }
        }
    }
});

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------

const CIMPLE_KEYWORDS = [
    'if', 'else', 'while', 'for', 'return', 'break', 'continue',
    'structure', 'union', 'clone', 'true', 'false', 'null',
    'int', 'float', 'bool', 'string', 'void', 'byte',
    'try', 'catch', 'throw', 'finally', 'in',
    'import', 'export', 'const', 'let',
];

connection.onCompletion((params: CompletionParams): CompletionItem[] => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return [];

    const text = doc.getText();
    const lines = text.split('\n');
    const position = params.position;
    const lineText = lines[position.line] || '';
    const charBefore = lineText.slice(0, position.character);

    // Detect if triggered after '.'
    const dotMatch = /(\w+)\.\s*$/.exec(charBefore);
    if (dotMatch) {
        const varName = dotMatch[1];
        const items: CompletionItem[] = [];

        // Try to find the type of varName by looking at local declarations
        // Simple heuristic: search current file for type declarations
        const fileSyms = symbolIndex.get(params.textDocument.uri);
        if (fileSyms) {
            // Look for struct fields/methods matching varName's inferred type
            // We do a simple scan: find the first struct with matching name usage
            for (const sym of fileSyms.symbols) {
                if (sym.structName) {
                    // This is a field/method of some struct — include it
                    items.push({
                        label: sym.name,
                        kind: sym.kind === 'method' ? CompletionItemKind.Method : CompletionItemKind.Field,
                        detail: `${sym.returnType} (${sym.structName}.${sym.name})`,
                    });
                }
            }
        }

        // Also add builtin type fields/methods
        for (const bt of BUILTIN_TYPES) {
            for (const f of bt.fields) {
                items.push({
                    label: f.name,
                    kind: CompletionItemKind.Field,
                    detail: `${f.type} (${bt.name}.${f.name})`,
                });
            }
        }

        return items;
    }

    const items: CompletionItem[] = [];

    // Built-in functions
    for (const fn of BUILTIN_FUNCTIONS) {
        const snippet = makeParamsSnippet(fn.params);
        items.push({
            label: fn.name,
            kind: CompletionItemKind.Function,
            detail: fn.signature,
            documentation: { kind: MarkupKind.Markdown, value: fn.doc },
            insertText: `${fn.name}${snippet}`,
            insertTextFormat: 2, // Snippet
        });
    }

    // Built-in constants
    for (const c of BUILTIN_CONSTANTS) {
        items.push({
            label: c.name,
            kind: CompletionItemKind.Constant,
            detail: `${c.type} = ${c.value}`,
            documentation: { kind: MarkupKind.Markdown, value: c.doc },
        });
    }

    // Built-in types
    for (const t of BUILTIN_TYPES) {
        items.push({
            label: t.name,
            kind: CompletionItemKind.Class,
            detail: `structure ${t.name}`,
            insertText: `clone ${t.name}(\${1})`,
            insertTextFormat: 2, // Snippet
        });
    }

    // User-defined symbols from all workspace files
    for (const fileSyms of symbolIndex.values()) {
        for (const sym of fileSyms.symbols) {
            if (sym.kind === 'function') {
                const snippet = makeParamsSnippet(sym.params);
                items.push({
                    label: sym.name,
                    kind: CompletionItemKind.Function,
                    detail: buildSignatureLabel(sym),
                    insertText: `${sym.name}${snippet}`,
                    insertTextFormat: 2,
                });
            } else if (sym.kind === 'structure') {
                items.push({
                    label: sym.name,
                    kind: CompletionItemKind.Class,
                    detail: `structure ${sym.name}`,
                    insertText: `clone ${sym.name}(\${1})`,
                    insertTextFormat: 2,
                });
            } else if (sym.kind === 'union') {
                items.push({
                    label: sym.name,
                    kind: CompletionItemKind.Struct,
                    detail: `union ${sym.name}`,
                });
            }
        }
    }

    // Keywords
    for (const kw of CIMPLE_KEYWORDS) {
        items.push({
            label: kw,
            kind: CompletionItemKind.Keyword,
        });
    }

    return items;
});

// ---------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------

connection.onHover((params: TextDocumentPositionParams): Hover | null => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const text = doc.getText();
    const lines = text.split('\n');
    const { word } = getWordRangeAtPosition(lines, params.position);
    if (!word) return null;

    // Check built-in functions
    const builtinFn = BUILTIN_FUNCTIONS.find(f => f.name === word);
    if (builtinFn) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `\`\`\`cimple\n${builtinFn.signature}\n\`\`\`\n\n${builtinFn.doc}`,
            },
        };
    }

    // Check built-in constants
    const builtinConst = BUILTIN_CONSTANTS.find(c => c.name === word);
    if (builtinConst) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `\`\`\`cimple\nconst ${builtinConst.name}: ${builtinConst.type} = ${builtinConst.value}\n\`\`\`\n\n${builtinConst.doc}`,
            },
        };
    }

    // Check built-in types
    const builtinType = BUILTIN_TYPES.find(t => t.name === word);
    if (builtinType) {
        const fieldLines = builtinType.fields.map(f => `  ${f.type} ${f.name};`).join('\n');
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `\`\`\`cimple\nstructure ${builtinType.name} {\n${fieldLines}\n}\`\`\``,
            },
        };
    }

    // Check user-defined symbols
    for (const fileSyms of symbolIndex.values()) {
        for (const sym of fileSyms.symbols) {
            if (sym.name === word) {
                const sig = buildSignatureLabel(sym);
                const docStr = sym.docComment ? `\n\n${sym.docComment}` : '';
                return {
                    contents: {
                        kind: MarkupKind.Markdown,
                        value: `\`\`\`cimple\n${sig}\n\`\`\`${docStr}`,
                    },
                };
            }
        }
    }

    return null;
});

// ---------------------------------------------------------------------------
// Definition
// ---------------------------------------------------------------------------

connection.onDefinition((params: DefinitionParams): Location | null => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const lines = doc.getText().split('\n');
    const { word } = getWordRangeAtPosition(lines, params.position);
    if (!word) return null;

    for (const fileSyms of symbolIndex.values()) {
        for (const sym of fileSyms.symbols) {
            if (sym.name === word) {
                return Location.create(
                    sym.uri,
                    Range.create(sym.line, sym.col, sym.endLine, sym.col + sym.name.length)
                );
            }
        }
    }

    return null;
});

// ---------------------------------------------------------------------------
// References
// ---------------------------------------------------------------------------

connection.onReferences((params: ReferenceParams): Location[] => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return [];

    const lines = doc.getText().split('\n');
    const { word } = getWordRangeAtPosition(lines, params.position);
    if (!word) return [];

    const locations: Location[] = [];
    const wordRe = new RegExp(`\\b${escapeRegex(word)}\\b`, 'g');

    for (const fileSyms of symbolIndex.values()) {
        const filePath = URI.parse(fileSyms.uri).fsPath;
        let content: string;
        // Try to get content from open documents first
        const openDoc = documents.get(fileSyms.uri);
        if (openDoc) {
            content = openDoc.getText();
        } else {
            try {
                content = fs.readFileSync(filePath, 'utf-8');
            } catch {
                continue;
            }
        }

        const fileLines = content.split('\n');
        for (let li = 0; li < fileLines.length; li++) {
            let m: RegExpExecArray | null;
            wordRe.lastIndex = 0;
            while ((m = wordRe.exec(fileLines[li])) !== null) {
                locations.push(Location.create(
                    fileSyms.uri,
                    Range.create(li, m.index, li, m.index + word.length)
                ));
            }
        }
    }

    return locations;
});

function escapeRegex(s: string): string {
    return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// ---------------------------------------------------------------------------
// Document symbols
// ---------------------------------------------------------------------------

connection.onDocumentSymbol((params: DocumentSymbolParams): DocumentSymbol[] => {
    const fileSyms = symbolIndex.get(params.textDocument.uri);
    if (!fileSyms) return [];

    const result: DocumentSymbol[] = [];
    const structMap = new Map<string, DocumentSymbol>();

    // First pass: top-level (functions, structures, unions)
    for (const sym of fileSyms.symbols) {
        if (sym.structName) continue; // handle in second pass

        const range = Range.create(sym.line, sym.col, sym.endLine, sym.col + sym.name.length);
        const ds = DocumentSymbol.create(
            sym.name,
            sym.docComment,
            symbolKindForCimple(sym.kind),
            range,
            range,
            []
        );
        result.push(ds);
        if (sym.kind === 'structure' || sym.kind === 'union') {
            structMap.set(sym.name, ds);
        }
    }

    // Second pass: methods and fields inside structs
    for (const sym of fileSyms.symbols) {
        if (!sym.structName) continue;
        const parent = structMap.get(sym.structName);
        if (!parent) continue;

        const range = Range.create(sym.line, sym.col, sym.endLine, sym.col + sym.name.length);
        const ds = DocumentSymbol.create(
            sym.name,
            sym.docComment,
            symbolKindForCimple(sym.kind),
            range,
            range,
            []
        );
        (parent.children as DocumentSymbol[]).push(ds);
    }

    return result;
});

// ---------------------------------------------------------------------------
// Workspace symbols
// ---------------------------------------------------------------------------

connection.onWorkspaceSymbol((params: WorkspaceSymbolParams): SymbolInformation[] => {
    const query = params.query.toLowerCase();
    const results: SymbolInformation[] = [];

    for (const fileSyms of symbolIndex.values()) {
        for (const sym of fileSyms.symbols) {
            if (!query || sym.name.toLowerCase().includes(query)) {
                const range = Range.create(sym.line, sym.col, sym.endLine, sym.col + sym.name.length);
                results.push(SymbolInformation.create(
                    sym.name,
                    symbolKindForCimple(sym.kind),
                    range,
                    sym.uri,
                    sym.structName
                ));
            }
        }
    }

    return results;
});

// ---------------------------------------------------------------------------
// Signature help
// ---------------------------------------------------------------------------

connection.onSignatureHelp((params: SignatureHelpParams): SignatureHelp | null => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const text = doc.getText();
    const lines = text.split('\n');
    const lineText = lines[params.position.line] || '';
    const charBefore = lineText.slice(0, params.position.character);

    // Find the last unclosed '('
    let depth = 0;
    let openParen = -1;
    for (let i = charBefore.length - 1; i >= 0; i--) {
        const ch = charBefore[i];
        if (ch === ')') depth++;
        else if (ch === '(') {
            if (depth === 0) { openParen = i; break; }
            depth--;
        }
    }
    if (openParen < 0) return null;

    // Extract function name before '('
    const beforeParen = charBefore.slice(0, openParen);
    const fnNameMatch = /(\w+)\s*$/.exec(beforeParen);
    if (!fnNameMatch) return null;
    const fnName = fnNameMatch[1];

    // Count active parameter (commas between openParen and cursor)
    const argsStr = charBefore.slice(openParen + 1);
    let activeParam = 0;
    let argDepth = 0;
    for (const ch of argsStr) {
        if (ch === '(' || ch === '[') argDepth++;
        else if (ch === ')' || ch === ']') argDepth--;
        else if (ch === ',' && argDepth === 0) activeParam++;
    }

    // Look up in built-ins
    const builtinFn = BUILTIN_FUNCTIONS.find(f => f.name === fnName);
    if (builtinFn) {
        const paramInfos: ParameterInformation[] = builtinFn.params.map(p =>
            ParameterInformation.create(`${p.type} ${p.name}`)
        );
        const sig = SignatureInformation.create(builtinFn.signature, builtinFn.doc, ...paramInfos);
        return {
            signatures: [sig],
            activeSignature: 0,
            activeParameter: Math.min(activeParam, builtinFn.params.length - 1),
        };
    }

    // Look up in user symbols
    for (const fileSyms of symbolIndex.values()) {
        for (const sym of fileSyms.symbols) {
            if ((sym.kind === 'function' || sym.kind === 'method') && sym.name === fnName) {
                const paramInfos: ParameterInformation[] = sym.params.map(p =>
                    ParameterInformation.create(`${p.type} ${p.name}`)
                );
                const sig = SignatureInformation.create(buildSignatureLabel(sym), sym.docComment, ...paramInfos);
                return {
                    signatures: [sig],
                    activeSignature: 0,
                    activeParameter: Math.min(activeParam, sym.params.length - 1),
                };
            }
        }
    }

    return null;
});

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

documents.listen(connection);
connection.listen();
