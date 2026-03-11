#!/usr/bin/env node
const fs = require('fs');
const path = require('path');
const NodeModule = require('module');

if (process.argv.length < 5) {
  console.error('Usage: node run_cimple_wasm.js <cimple.js> <run|check> <source.ci> [args...]');
  process.exit(1);
}

const wasmEntry = path.resolve(process.argv[2]);
const cmd = process.argv[3];
const hostSource = path.resolve(process.argv[4]);
const extraArgs = process.argv.slice(5);
const virtualSource = '/tmp/input.ci';

process.argv = [process.argv[0], wasmEntry, cmd, virtualSource, ...extraArgs];

const prefix = `var Module = {\n  preRun: [function() {\n    var fs = require('fs');\n    var src = fs.readFileSync(${JSON.stringify(hostSource)}, 'utf8');\n    FS.mkdirTree('/tmp');\n    FS.writeFile(${JSON.stringify(virtualSource)}, src);\n  }]\n};\n`;

const mod = new NodeModule(wasmEntry, module);
mod.filename = wasmEntry;
mod.paths = NodeModule._nodeModulePaths(path.dirname(wasmEntry));
mod._compile(prefix + fs.readFileSync(wasmEntry, 'utf8'), wasmEntry);
