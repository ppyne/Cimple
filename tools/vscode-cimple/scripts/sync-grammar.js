#!/usr/bin/env node
/**
 * sync-grammar.js
 *
 * Calls `cimple dump-language`, reads the JSON output, and regenerates
 * the relevant parts of syntaxes/cimple.tmLanguage.json so that the
 * grammar is always in sync with the installed binary.
 *
 * Usage:
 *   node scripts/sync-grammar.js [path/to/cimple]
 *   npm run sync-grammar
 *
 * The binary is looked up in this order:
 *   1. First CLI argument
 *   2. CIMPLE_BIN environment variable
 *   3. ../../build/cimple  (relative to this script)
 *   4. cimple  (PATH)
 */

const { execSync } = require('child_process');
const fs   = require('fs');
const path = require('path');

/* ---- locate the cimple binary ---- */
function findBinary() {
    if (process.argv[2]) return process.argv[2];
    if (process.env.CIMPLE_BIN) return process.env.CIMPLE_BIN;
    const rel = path.resolve(__dirname, '..', '..', '..', 'build', 'cimple');
    if (fs.existsSync(rel)) return rel;
    const relExe = rel + '.exe';
    if (fs.existsSync(relExe)) return relExe;
    return 'cimple';
}

/* ---- call cimple dump-language ---- */
function dumpLanguage(bin) {
    try {
        const out = execSync(`"${bin}" dump-language`, { encoding: 'utf8' });
        return JSON.parse(out);
    } catch (e) {
        console.error(`Error running '${bin} dump-language': ${e.message}`);
        process.exit(1);
    }
}

/* ---- build a regex alternation from an array of strings ---- */
function alt(names) {
    return names.join('|');
}

/* ---- regenerate the grammar ---- */
function syncGrammar(lang) {
    const grammarPath = path.resolve(__dirname, '..', 'syntaxes', 'cimple.tmLanguage.json');
    const grammar = JSON.parse(fs.readFileSync(grammarPath, 'utf8'));
    const repo    = grammar.repository;

    /* keywords_control */
    repo.keywords_control.patterns[0].match =
        `\\b(${alt(lang.keywords.control)})\\b`;

    /* keywords_declaration */
    repo.keywords_declaration.patterns[0].match =
        `\\b(${alt(lang.keywords.declaration)})\\b`;

    /* keywords_exception */
    repo.keywords_exception.patterns[0].match =
        `\\b(${alt(lang.keywords.exception)})\\b`;

    /* keywords_other */
    repo.keywords_other.patterns[0].match =
        `\\b(${alt(lang.keywords.other)})\\b`;

    /* storage_types */
    const typeAlt = alt(lang.types);
    repo.storage_types.patterns[0].match =
        `\\b(${typeAlt})(?=\\s*(?:\\[\\]|\\[\\w+\\]|\\s))`;
    repo.storage_types.patterns[1].match =
        `\\b(${typeAlt})\\b`;

    /* support_classes (builtin structs) */
    repo.support_classes.patterns[0].match =
        `\\b(${alt(lang.builtin_structs)})\\b`;

    /* support_constants (all constants flattened) */
    const allConsts = [
        ...lang.constants.string,
        ...lang.constants.int,
        ...lang.constants.float
    ];
    repo.support_constants.patterns[0].match =
        `\\b(${alt(allConsts)})\\b`;

    /* support_functions */
    repo.support_functions.patterns[0].match =
        `\\b(${alt(lang.functions)})\\b(?=\\s*\\()`;

    fs.writeFileSync(grammarPath, JSON.stringify(grammar, null, 2) + '\n', 'utf8');
    console.log(`✓  Grammar synced from ${lang.functions.length} functions, ` +
                `${allConsts.length} constants, ` +
                `${lang.builtin_structs.length} builtin types.`);
}

/* ---- main ---- */
const bin  = findBinary();
console.log(`Using binary: ${bin}`);
const lang = dumpLanguage(bin);
syncGrammar(lang);
