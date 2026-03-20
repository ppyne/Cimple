# Extension Cimple pour VS Code

Cette extension ajoute à VS Code le support complet du langage Cimple (fichiers `.ci`) :
coloration syntaxique, auto-complétion, signatures, erreurs en temps réel, aller à la définition, etc.

---

## Prérequis

Avant de commencer, assurez-vous d'avoir installé :

- **[VS Code](https://code.visualstudio.com/)** (version 1.85 ou plus récente)
- **[Node.js](https://nodejs.org/)** (version 18 ou plus récente) — vérifiez avec `node --version`
- **npm** — fourni avec Node.js, vérifiez avec `npm --version`
- Le binaire **`cimple`** compilé (voir la section [Compiler Cimple](#compiler-cimple))

---

## Étape 1 — Compiler Cimple

Si ce n'est pas encore fait, compilez d'abord le compilateur Cimple. Depuis la racine du dépôt :

```sh
cd tools/
./fetch_lemon.sh

cd ..
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Le binaire `cimple` (ou `cimple.exe` sur Windows) se trouve ensuite dans `build/`.

---

## Étape 2 — Compiler l'extension

Depuis la racine du dépôt, allez dans le dossier de l'extension et installez les dépendances :

```sh
cd tools/vscode-cimple
# si besoin: rm -rf node_modules out
npm install
npm run compile
```

Vous devriez voir `> cimple@0.1.0 compile` suivi de rien d'autre. Si c'est le cas, la compilation a réussi.

---

## Étape 3 — Installer l'extension dans VS Code

Vous avez deux options :

### Option A — Mode développement (recommandé pour débuter)

1. Ouvrez VS Code
2. Choisissez **Fichier → Ouvrir le dossier…** et sélectionnez `tools/vscode-cimple/`
3. Appuyez sur **F5**

Une nouvelle fenêtre VS Code s'ouvre : c'est l'**Extension Development Host**. L'extension Cimple est active dans cette fenêtre. Ouvrez-y un fichier `.ci` pour la tester.

> Cette méthode est idéale pour tester l'extension. À chaque modification du code de l'extension, relancez la compilation (`npm run compile`) puis rechargez la fenêtre de développement avec **Ctrl+Shift+P** → `Developer: Reload Window`.

---

### Option B — Installation permanente (via VSIX)

Cette méthode installe l'extension définitivement dans votre VS Code, sans avoir besoin d'ouvrir le dossier de l'extension à chaque fois.

**1. Installez `vsce`** (outil de packaging d'extensions VS Code) :

```sh
npm install -g @vscode/vsce
```

**2. Créez le fichier `.vsix`** depuis le dossier de l'extension :

```sh
cd tools/vscode-cimple
vsce package --allow-missing-repository
```

Un fichier `cimple-0.1.0.vsix` est créé dans le dossier courant.

**3. Installez le `.vsix` dans VS Code** :

- Ouvrez VS Code
- Allez dans l'onglet Extensions (**Ctrl+Shift+X**)
- Cliquez sur le bouton `…` (trois points) en haut à droite du panneau Extensions
- Choisissez **Installer depuis VSIX…**
- Sélectionnez le fichier `cimple-0.1.0.vsix`
- Redémarrez VS Code si demandé

L'extension est maintenant installée de façon permanente.

---

## Étape 4 — Configurer le chemin vers `cimple`

L'extension lance automatiquement `cimple check` pour détecter les erreurs dans vos fichiers. Elle cherche le binaire dans cet ordre :

1. Le paramètre **`cimple.executablePath`** dans les settings VS Code
2. La variable d'environnement **`CIMPLE_BIN`**
3. Le dossier **`build/cimple`** à la racine du workspace ouvert
4. Le **PATH** système

Dans la grande majorité des cas (si vous avez ouvert le dépôt Cimple comme dossier racine dans VS Code), le binaire est trouvé automatiquement et aucune configuration n'est nécessaire.

Si besoin, configurez manuellement :

1. Ouvrez les settings VS Code (**Ctrl+,**)
2. Recherchez `cimple`
3. Renseignez le champ **Cimple: Executable Path** avec le chemin absolu vers le binaire, par exemple :
   - macOS/Linux : `/Users/vous/dev/Cimple/build/cimple`
   - Windows : `C:\dev\Cimple\build\Release\cimple.exe`

---

## Ce que fait l'extension

| Fonctionnalité | Description |
|---|---|
| **Coloration syntaxique** | Mots-clés, types, fonctions et constantes natives, strings, nombres, opérateurs |
| **Erreurs en temps réel** | Soulignements rouges/jaunes basés sur la vraie sortie de `cimple check` |
| **Auto-complétion** | Toutes les fonctions natives avec leurs signatures + vos propres fonctions/structures/unions |
| **Aide à la signature** | Lors d'un appel de fonction, affiche les paramètres attendus et surligne celui en cours |
| **Hover** | Survolez un nom de fonction pour voir sa signature complète et sa documentation |
| **Aller à la définition** | **F12** sur un nom de fonction ou de structure → saute à la déclaration |
| **Trouver les références** | **Maj+F12** → liste toutes les utilisations dans le workspace |
| **Plan du document** | La vue Outline liste toutes les fonctions, structures et méthodes du fichier courant |
| **Recherche de symboles** | **Ctrl+T** → cherche dans tous les symboles de tous les fichiers `.ci` |
| **Snippets** | `main`, `fn`, `structure`, `for`, `forin`, `try`, `throw`, `clone`, `_init`, etc. |

---

## Snippets disponibles

Tapez le préfixe et appuyez sur **Tab** :

| Préfixe | Résultat |
|---|---|
| `main` | `void main() { }` |
| `fn` | Déclaration de fonction |
| `structure` | Déclaration de structure |
| `structext` | Structure avec héritage |
| `_init` | Méthode d'initialisation |
| `for` | Boucle for C-style |
| `forin` | Boucle for-in sur tableau |
| `forinmap` | Boucle for-in sur map (clé, valeur) |
| `while` | Boucle while |
| `if` / `ifelse` | Conditions |
| `try` | Try-catch |
| `trycf` | Try-catch-finally |
| `clone` | `clone StructureName` |
| `cloneargs` | `clone StructureName(args)` |
| `throw` | `throw clone Exception("message");` |
| `print` | `print("...\n");` |
| `assert` | `assert(condition, "message");` |

---

## Dépannage

**L'extension ne signale aucune erreur**
→ Le binaire `cimple` n'a pas été trouvé. Vérifiez que `build/cimple` existe et est exécutable, ou configurez `cimple.executablePath` dans les settings.

**`npm run compile` échoue**
→ Vérifiez que Node.js est bien installé (`node --version`). Si la version est inférieure à 18, mettez-la à jour.

**F5 ne fonctionne pas**
→ Assurez-vous d'avoir ouvert le dossier `tools/vscode-cimple/` directement dans VS Code (pas un dossier parent).

**Les erreurs apparaissent au mauvais endroit**
→ Sauvegardez le fichier (**Ctrl+S**) : le check est immédiat à la sauvegarde.
