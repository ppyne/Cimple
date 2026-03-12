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
const hostRoot = path.dirname(hostSource);
const virtualRoot = '/tmp/src';
const virtualSource = path.posix.join(virtualRoot, path.basename(hostSource));

process.argv = [process.argv[0], wasmEntry, cmd, virtualSource, ...extraArgs];

const prefix = `var Module = {\n  preRun: [function() {\n    var fs = require('fs');\n    var path = require('path');\n    function copyTree(hostDir, virtualDir) {\n      FS.mkdirTree(virtualDir);\n      for (var entry of fs.readdirSync(hostDir, { withFileTypes: true })) {\n        var hostPath = path.join(hostDir, entry.name);\n        var virtualPath = virtualDir + '/' + entry.name;\n        if (entry.isDirectory()) {\n          copyTree(hostPath, virtualPath);\n        } else if (entry.isFile()) {\n          FS.writeFile(virtualPath, fs.readFileSync(hostPath));\n        }\n      }\n    }\n    copyTree(${JSON.stringify(hostRoot)}, ${JSON.stringify(virtualRoot)});\n  }]\n};\n`;

const mod = new NodeModule(wasmEntry, module);
mod.filename = wasmEntry;
mod.paths = NodeModule._nodeModulePaths(path.dirname(wasmEntry));
mod._compile(prefix + fs.readFileSync(wasmEntry, 'utf8'), wasmEntry);
