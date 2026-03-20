import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient, LanguageClientOptions, ServerOptions, TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    const serverModule = context.asAbsolutePath(path.join('out', 'server.js'));
    const serverOptions: ServerOptions = {
        run:   { module: serverModule, transport: TransportKind.ipc },
        debug: { module: serverModule, transport: TransportKind.ipc,
                 options: { execArgv: ['--nolazy', '--inspect=6009'] } }
    };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'cimple' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ci')
        }
    };
    client = new LanguageClient('cimple', 'Cimple Language Server', serverOptions, clientOptions);
    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
