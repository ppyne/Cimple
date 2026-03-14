# Cimple — spécification version 1.1

> **Version : 1.0**
> Tout ce qui est décrit dans ce document est **obligatoire et doit être implémenté intégralement**.

---

## 1. Objet du document

Ce document définit la spécification complète du langage **Cimple** et de son implémentation de référence.

Cimple est un **petit langage impératif de style C**, avec :

- une syntaxe familière et sobre ;
- un noyau minimal mais complet ;
- un **typage obligatoire** ;
- des **fonctions natives ordinaires** dans la bibliothèque standard ;
- un **point d'entrée obligatoire** ;
- l'absence volontaire des difficultés historiques du C.

L'objectif n'est pas de copier le C, mais de proposer un langage **simple, cohérent, accessible au plus grand nombre, et sans pièges inutiles**. Cimple est conçu pour être utilisable pour des programmes utilitaires courants et a vocation à être un langage généraliste minimaliste.

L'implémentation du parseur devra employer **Lemon de SQLite**.

---

## 2. Positionnement du langage

### 2.1 Vision

Cimple est un langage :

- impératif ;
- statiquement typé ;
- à syntaxe de type C ;
- conçu pour être facile à lire, à apprendre et à implémenter ;
- conçu pour éviter la complexité inutile, les ambiguïtés historiques et les comportements dangereux ;
- accessible à tout développeur ayant une expérience de base d'un langage C-like ;
- adapté à des programmes utilitaires, des scripts, ou des projets multipurpose.

### 2.2 Ce que Cimple cherche à offrir

- une prise en main immédiate pour les développeurs familiers de C-like languages ;
- une structure simple pour les programmes utilitaires courants ;
- une base saine pour une évolution future vers un langage généraliste complet ;
- un outillage de compilation/interprétation propre.

### 2.3 Ce que Cimple refuse explicitement

Cimple ne doit **pas** inclure :

- pointeurs bruts exposés à l'utilisateur ;
- arithmétique de pointeurs ;
- préprocesseur ;
- macros ;
- headers à la C ;
- conversions implicites dangereuses ou surprenantes ;
- déclarateurs complexes à la C ;

- `goto` ;
- comportement indéfini comme principe de performance ;
- surcharge de fonctions (ni utilisateur, ni bibliothèque standard) ;
- polymorphisme compliqué ;
- syntaxes concurrentes pour une même construction.

---

## 3. Objectifs d'implémentation

L'implémentation devra produire un projet clair, découpé, testable, avec :

1. un **lexer** ;
2. un **parseur généré par Lemon (SQLite)** ;
3. un **AST explicite** ;
4. une **analyse sémantique** ;
5. un **interpréteur direct de l'AST**.

L'objectif n'est **pas** de produire un compilateur natif, ni un backend LLVM, ni une VM sophistiquée. La priorité est :

- la justesse syntaxique ;
- la cohérence sémantique ;
- des erreurs claires ;
- une base de code propre et extensible ;
- la **portabilité multiplateforme** : l'outil doit fonctionner sans modification sur macOS, Linux/Unix, Windows et WebAssembly.

---

## 4. Architecture générale attendue

### 4.1 Pipeline

Le pipeline attendu est :

1. lecture du fichier source ;
2. analyse lexicale via **re2c** ;
3. analyse syntaxique via **Lemon** ;
4. construction d'un AST ;
5. validation sémantique ;
6. exécution via un interpréteur AST.

### 4.2 Technologies et contraintes

- L'implémentation doit être écrite en **C** (standard C99 ou C11). Aucun autre langage d'implémentation n'est autorisé.
- Le lexer doit être généré avec **re2c**.
- Le parseur doit être construit avec **Lemon de SQLite**.
- Les tokens produits par re2c doivent être envoyés explicitement au parseur Lemon.
- Le projet doit rester aussi simple que possible.
- L'arbre syntaxique abstrait doit être indépendant du parseur généré.
- La grammaire Lemon ne doit pas contenir de logique métier lourde dans ses actions.
- L'interpréteur ne doit pas contenir de logique de parsing cachée.
- L'implémentation doit être **portable** : elle doit compiler et s'exécuter sans modification sur macOS, Linux, Unix, Windows et WebAssembly.

### 4.3 Style d'implémentation

Le code doit être :

- lisible ;
- modulaire ;
- raisonnablement commenté ;
- testable ;
- organisé par responsabilités.

### 4.4 Portabilité multiplateforme

Cimple doit fonctionner sans modification sur les plateformes suivantes :

- **macOS** (Apple Silicon et Intel) ;
- **Linux** (distributions courantes : Ubuntu, Debian, Fedora, Arch) ;
- **Unix** (systèmes compatibles POSIX) ;
- **Windows** (via MSVC, MinGW/GCC ou Clang sous Windows) ;
- **WebAssembly** (via Emscripten ; cible `wasm32`) — voir règles spécifiques ci-dessous.

**Règles de code pour garantir la portabilité :**

- utiliser uniquement le **C standard (C99 ou C11)** ; aucune extension spécifique à GCC, Clang ou MSVC n'est autorisée sauf protection par `#ifdef` ;
- ne pas utiliser d'API POSIX non disponibles sur Windows (`fork`, `pipe`, `dirent`, etc.) directement sans abstraction ; encapsuler les appels système dans des fonctions portables ;
- pour `exec` (section 9.10) : utiliser `CreateProcess` sur Windows et `posix_spawn` ou `fork`/`execvp` sur POSIX ; encapsuler cette différence derrière une interface interne commune ;
- pour les fins de ligne : le runtime normalise `\r\n` → `\n` en lecture (voir section 9.9) ; en écriture, utiliser `\n` uniquement ; ne pas écrire `\r\n` même sur Windows ;
- ne pas supposer un séparateur de chemin fixe (`/` vs `\`) dans les messages d'erreur ; accepter les deux en entrée ;
- les types entiers doivent utiliser `int32_t`, `int64_t` etc. de `<stdint.h>` partout où la taille est significative, plutôt que `int` ou `long` dont la taille varie selon la plateforme ;
- ne pas utiliser `PATH_MAX`, `MAXPATHLEN` ou d'autres constantes dont la valeur varie ; allouer dynamiquement les buffers de chemin ;
- les chaînes sont traitées comme des séquences d'octets UTF-8 ; aucune conversion vers les API Unicode de Windows (`wchar_t`, `WCHAR`) n'est imposée au niveau du langage, mais l'implémentation Windows doit gérer correctement les chemins Unicode via les API `W` si nécessaire.

**Règles spécifiques à la cible WebAssembly :**

- la cible WebAssembly est compilée avec **Emscripten** (`emcc`) en C99/C11 standard ; aucune API native (POSIX ou Win32) ne doit être utilisée sans abstraction conditionnelle `#ifdef __EMSCRIPTEN__` ;
- `exec` n'est **pas disponible** sous WebAssembly ; un appel à `exec` dans un programme Cimple compilé pour wasm produit une **erreur runtime** avec le message : `exec: not supported on this platform` ;
- les fonctions de fichiers (`readFile`, `writeFile`, etc.) sont disponibles sous WebAssembly via le système de fichiers virtuel Emscripten (MEMFS ou NODEFS selon l'hôte) ; leur comportement observable est identique à celui des autres plateformes ;
- `now()` est disponible sous WebAssembly via `emscripten_get_now()` ou `clock_gettime` selon la disponibilité ;
- `getEnv` retourne toujours `fallback` sous WebAssembly (l'environnement du processus n'est pas accessible depuis wasm) ;
- la taille de `int` reste `int64_t` sous wasm32 ; `INT_SIZE` vaut `8` comme sur les autres plateformes ;
- le binaire wasm produit expose un point d'entrée compatible avec l'appel depuis JavaScript via la glue Emscripten.

**re2c et Lemon sont tous deux portables** sur les cinq plateformes cibles. re2c génère du C standard pur ; Lemon est un fichier C unique sans dépendance. Aucune configuration spécifique à la plateforme n'est nécessaire pour ces deux outils.

**Système de build recommandé :** utiliser **CMake** (version ≥ 3.15) pour garantir la portabilité du build sur les quatre plateformes natives. Pour la cible WebAssembly, utiliser le toolchain Emscripten avec le fichier `toolchain-emscripten.cmake` fourni dans le dépôt.

---

## 5. Modèle source

### 5.1 Fichier source et imports

Un programme Cimple est défini par un **fichier racine** (le fichier passé à `cimple run` ou `cimple check`). Ce fichier racine peut importer des fichiers `.ci` supplémentaires via la directive `import`. Les fichiers importés peuvent eux-mêmes importer d'autres fichiers. L'ensemble forme un **graphe acyclique dirigé de fichiers sources** ; l'exécution reste un programme unique sans notion de module ni d'espace de noms.

### 5.2 Directive `import`

#### Syntaxe

```c
import "utils.ci";
import "math/vectors.ci";
```

#### Règles normatives

- `import` est un **mot-clé du langage**, reconnu par le lexer et la grammaire ;
- une directive `import` doit apparaître **avant toute déclaration** de fonction, variable ou autre `import` dans le fichier courant — en d'autres termes, toutes les directives `import` d'un fichier doivent être groupées en tête, avant le premier `top_level_item` non-import ;
- le chemin est une **chaîne littérale** `"..."` ; les variables et expressions ne sont pas autorisées ;
- le chemin est résolu **relativement au répertoire du fichier qui importe** ; les chemins absolus sont autorisés mais déconseillés ;
- le chemin doit se terminer par l'extension `.ci` ; toute autre extension est une erreur sémantique ;
- les séparateurs de chemin `/` et `\` sont tous deux acceptés ; `..` et `.` sont résolus normalement ;
- `import` est suivi d'un `;` obligatoire ;
- un fichier ne peut être importé **qu'une seule fois** dans l'ensemble du programme (déduplication par chemin absolu résolu) ; les imports redondants sont silencieusement ignorés sans erreur ;
- les inclusions **circulaires** sont une **erreur sémantique** fatale ; exemple : `a.ci` importe `b.ci` qui importe `a.ci` ;
- la profondeur d'imbrication maximale est **32 niveaux** ; toute chaîne d'imports plus profonde est une erreur sémantique ;
- `import` n'est autorisé **qu'au niveau supérieur** (`top_level_item`) ; un `import` à l'intérieur d'une fonction ou d'un bloc est une erreur syntaxique.

#### Sémantique de fusion

Le compilateur résout les imports **avant l'analyse sémantique**, en produisant un AST fusionné unique. L'ordre de résolution est :

1. le fichier courant est lu et ses `import` sont collectés dans l'ordre d'apparition ;
2. chaque fichier importé est résolu récursivement selon le même algorithme ;
3. les déclarations des fichiers importés sont intégrées dans la portée globale **avant** les déclarations du fichier qui importe ;
4. si un fichier est référencé plusieurs fois dans le graphe, il n'est traité qu'une fois (déduplication par chemin absolu).

L'ordre effectif des déclarations dans l'AST fusionné est donc : imports résolus récursivement en profondeur d'abord, puis déclarations du fichier courant.

#### Portée et visibilité

- toutes les fonctions et variables globales déclarées dans un fichier importé sont **visibles dans l'ensemble du programme**, y compris dans le fichier racine et dans les autres fichiers importés ;
- il n'existe pas d'espace de noms : les noms sont globaux ; toute **redéfinition de fonction ou de variable globale** est une **erreur sémantique** (décision normative 2 étendue) ;
- les fichiers importés ne doivent **pas** déclarer `main` ; tout fichier importé contenant une définition de `main` produit une **erreur sémantique** ;
- `main` ne peut être défini que dans le fichier racine.

#### Exemple

```
project/
  main.ci
  utils/
    strings.ci
    math.ci
```

`main.ci` :
```c
import "utils/strings.ci";
import "utils/math.ci";

void main() {
    string result = repeat("abc", 3);
    float area = circleArea(5.0);
    print(result + "\n");
    print(toString(area) + "\n");
}
```

`utils/strings.ci` :
```c
string repeat(string s, int n) {
    string result = "";
    for (int i = 0; i < n; i++) {
        result = result + s;
    }
    return result;
}
```

`utils/math.ci` :
```c
float circleArea(float r) {
    return M_PI * r * r;
}
```

### 5.3 Point d'entrée obligatoire

Un programme exécutable doit définir une fonction spéciale `main`. Quatre formes sont autorisées :

```c
int main() {
    return 0;
}
```

```c
void main() {
    // pas de return nécessaire
}
```

```c
int main(string[] args) {
    return 0;
}
```

```c
void main(string[] args) {
    // pas de return nécessaire
}
```

Règles :

- `main` est obligatoire pour tout programme exécutable ;
- `main` peut être déclaré sans paramètre, ou avec un unique paramètre `string[] args` ; toute autre signature est une erreur sémantique ;
- `int main()` et `int main(string[] args)` doivent explicitement retourner un `int` ; l'absence de `return` est une erreur sémantique ;
- `void main()` et `void main(string[] args)` sont des formes valides ; aucun `return` n'est requis ;
- la valeur de retour de `int main(...)` est le code de sortie du programme ;
- en cas de `void main(...)`, le code de sortie du programme est `0` par convention ;
- lorsque `main` est déclaré avec `string[] args`, `args` contient l'ensemble des arguments passés à l'exécutable ou à l'interpréteur, sans filtrage, dans leur ordre d'apparition sur la ligne de commande ;
- si aucun argument n'est passé, `args` est un tableau vide (`count(args) == 0`) ;
- le nom du programme ou du fichier source n'est **pas** inclus dans `args` ; `args[0]` est le premier argument utilisateur s'il existe ;
- lorsque `main` est déclaré sans paramètre, les arguments éventuellement passés sur la ligne de commande sont silencieusement ignorés.

---

## 6. Système de types

### 6.1 Types primitifs

Cimple inclut les types scalaires suivants :

- `int`
- `float`
- `bool`
- `string`
- `byte`
- `void`

> **Note d'implémentation — `int` :** le type `int` de Cimple **doit** être implémenté en `int64_t` (C99 `<stdint.h>`). Cette contrainte est imposée par la fonction `now()` qui retourne une epoch Unix en **millisecondes** : la valeur courante (~1,7 × 10¹² ms) dépasse la capacité d'un `int32_t` (max ~2,1 × 10⁹), et un `int32_t` déborderait définitivement en janvier 2038 (problème Y2K38). Tout implémenteur qui choisirait `int32_t` obtiendrait un comportement incorrect pour `now()` et toutes les fonctions de date. En conséquence, `INT_SIZE` vaut `8` sur toutes les plateformes cibles de Cimple.

### 6.2 Types tableaux

Cimple inclut cinq types tableaux homogènes pour les types scalaires non-`void`, plus un type tableau par structure ou union définie par l'utilisateur :

- `int[]`
- `float[]`
- `bool[]`
- `string[]`
- `byte[]`
- `NomDeStructure[]` — tableau de structures (voir §6.8)
- `NomDeUnion[]` — tableau d'unions (voir §6.9)

Les variantes **2D** correspondantes sont également disponibles (`int[][]`, `float[][]`, `bool[][]`, `string[][]`, `byte[][]`, `NomDeStructure[][]`, `NomDeUnion[][]`) — voir §6.11.

Un type tableau est toujours associé à un type d'élément fixe. Il est interdit de mélanger les types d'éléments au sein d'un même tableau. `void[]` n'existe pas.

### 6.3 Type opaque natif : `ExecResult`

`ExecResult` est un type natif opaque fourni exclusivement par le runtime. Il représente le résultat d'une exécution de commande externe.

Propriétés :

- `ExecResult` est un type de première classe : une variable de type `ExecResult` peut être déclarée, affectée et passée en argument à une fonction ;
- `ExecResult` est **opaque** : son contenu interne n'est pas directement accessible ; il ne peut être manipulé qu'au travers des fonctions `execStatus`, `execStdout` et `execStderr` ;
- `ExecResult` ne peut être produit que par un appel à `exec` ; il n'existe pas de littéral `ExecResult` ;
- `ExecResult[]` n'existe pas ; on ne peut pas créer de tableau de `ExecResult` ;
- une variable `ExecResult` non initialisée (déclarée sans appel à `exec`) est une **erreur sémantique**.

- `int` : entier signé
- `float` : nombre à virgule flottante
- `bool` : `true` ou `false`
- `string` : chaîne de caractères
- `void` : absence de valeur de retour

### 6.4 Sémantique des tableaux

Un tableau est une séquence ordonnée, dynamique et homogène de valeurs d'un même type scalaire.

Propriétés fondamentales :

- un tableau est **dynamique** : sa taille peut varier au cours de l'exécution par ajout, insertion ou suppression d'éléments ;
- un tableau est **homogène** : tous ses éléments sont du même type, déterminé statiquement à la déclaration ;
- un tableau peut être vide (longueur nulle) ;
- les indices sont des entiers à partir de `0` ; le dernier élément est à l'indice `count(t) - 1` ;
- un accès à un indice hors bornes est une **erreur runtime** ; elle doit produire un message d'erreur clair indiquant l'indice demandé, la longueur du tableau, la ligne et la colonne dans le source ;
- un tableau peut être passé en argument à une fonction et retourné par une fonction ;
- les tableaux peuvent être déclarés comme variables locales ou globales ;
- les tableaux 2D (tableaux irréguliers) sont supportés (voir §6.11) ;
- `void[]` n'existe pas.

### 6.5 Sémantique des chaînes

`string` est un type natif de première classe avec les propriétés suivantes :

- une chaîne peut être déclarée, affectée, passée en argument et retournée par une fonction ;
- les littéraux de chaîne utilisent la syntaxe `"..."` ;
- les séquences d'échappement `\"`, `\\`, `\n`, `\t`, `\r`, `\b`, `\f` et `\uXXXX` sont supportées (voir section 11.1) ;
- les chaînes sont **immutables** au niveau du langage ;
- l'opérateur `+` est autorisé pour concaténer deux chaînes ;
- `s[i]` est une expression en **lecture** retournant une `string` d'un seul octet UTF-8 à la position `i` (base 0) ; c'est un alias de `byteAt` avec retour `string` plutôt qu'`int` ; un indice hors bornes est une erreur runtime ;
- `s[i] = ...` est **interdit** : les chaînes sont immutables ; toute tentative d'affectation par indexation sur une chaîne est une erreur sémantique ;
- les opérateurs `==` et `!=` comparent le **contenu octet par octet** des chaînes en UTF-8, et non une identité mémoire ; deux chaînes visuellement identiques mais de normalisation Unicode différente (NFC vs NFD) peuvent donc être inégales selon `==` ;
- aucune conversion implicite entre `string` et les types numériques ou booléens n'est autorisée ;
- `len(s)` retourne le **nombre d'octets UTF-8** de la chaîne, pas le nombre de caractères visibles ;
- `byteAt(s, i)` retourne la **valeur entière (0–255) de l'octet** à l'indice `i` (base 0, en octets) ;
- `glyphLen(s)` retourne le **nombre de points de code Unicode** après normalisation NFC de la chaîne (voir section 9.3) ;
- `glyphAt(s, i)` retourne le point de code à l'indice `i` sous sa **forme NFC** (voir section 9.3).

Exemples valides :

```c
string name = "Alice";
string msg = "Hello, " + name;
if (name == "Alice") {
    print("ok");
}
```

Exemple invalide sans conversion explicite :

```c
string s = "age=" + 42;
```

Il faudra écrire à la place :

```c
string s = "age=" + toString(42);
```

### 6.6 Typage

- Toute variable doit avoir un type explicite.
- Tout paramètre de fonction doit avoir un type explicite.
- Toute fonction doit déclarer explicitement son type de retour.
- Il n'existe pas de `var` ni d'inférence de type.

### 6.7 Conversions

Règles strictes :

- pas de conversion implicite entre `int` et `float` dans les affectations ;
- pas de conversion implicite entre `bool` et `int` ;
- pas de conversion implicite depuis `string` ;
- une expression binaire doit reposer sur des types compatibles.

Cimple fournit quatre **fonctions de conversion intrinsèques** à résolution statique : `toString`, `toInt`, `toFloat`, `toBool`. Ces fonctions acceptent plusieurs types sources, mais la résolution est effectuée **entièrement et sans ambiguïté à la compilation**, sur la base du type statique de l'argument. Ce mécanisme est exclusivement réservé à ces quatre fonctions intrinsèques ; le code utilisateur ne peut toujours pas déclarer deux fonctions de même nom.

Signatures complètes :

```
toString(int value)   → string
toString(float value) → string
toString(bool value)  → string

toInt(float value)    → int
toInt(string value)   → int

toFloat(int value)    → float
toFloat(string value) → float

toBool(string value)  → bool
```

Règles normatives :

- `toInt(float value)` effectue une **troncature vers zéro** : `toInt(3.9)` retourne `3`, `toInt(-2.7)` retourne `-2` ;
- `toInt(string value)` retourne `0` si la chaîne n'est pas un entier valide ;
- `toFloat(string value)` retourne `NaN` si la chaîne n'est pas un flottant valide ;
- `toBool(string value)` accepte au minimum `"true"`, `"false"`, `"1"`, `"0"` et retourne `false` si l'entrée n'est pas reconnue ;
- appeler une fonction de conversion avec un type non supporté (ex. `toInt(bool)`) est une **erreur sémantique** ;
- la résolution est effectuée statiquement ; il n'y a aucune dispatch dynamique.

---

### 6.6 Sémantique de `byte`

`byte` est un type scalaire représentant un entier **non signé sur 8 bits**, dont les valeurs valides sont comprises entre `0` et `255` inclus.

**Littéraux et affectation :**

- un littéral entier dans l'intervalle `[0, 255]` peut être affecté directement à une variable `byte` sans conversion explicite : `byte b = 0xFF;` est valide ; `byte c = 256;` est une **erreur sémantique** (littéral hors plage) ;
- l'affectation d'une variable ou expression de type `int` à un `byte` est une **erreur sémantique** ; la conversion explicite `intToByte()` est requise.

**Opérateurs arithmétiques `+ - * / %` :**

- une expression `byte op byte` ou `byte op int` ou `int op byte` produit un résultat de type **`int`** ;
- il n'y a pas d'overflow ni de wrapping : le résultat est un entier signé standard ;
- `byte(200) + byte(100)` → `int(300)` ; `intToByte(byte(200) + byte(100))` → `byte(255)` (clamp).

**Opérateurs bitwise `& | ^ ~` :**

- une expression `byte & byte`, `byte | byte`, `byte ^ byte` produit un résultat de type **`byte`** ; le résultat est toujours dans `[0, 255]` par nature ;
- `~byte` (complément à un) produit un `byte` : `~byte(0x0F)` → `byte(0xF0)` ;
- `byte & int`, `byte | int`, `byte ^ int` produit un **`int`**.

**Opérateurs de décalage `<< >>` :**

- `byte << int` et `byte >> int` produisent un **`int`** ; le résultat peut dépasser 255.

**Comparaisons `== != < <= > >=` :**

- comparaisons valides entre deux `byte`, ou entre `byte` et `int` (promotion implicite de `byte` vers `int` pour la comparaison) ; résultat `bool`.

**Opérateurs non autorisés sur `byte` :**

- `&&`, `||`, `!` — non définis sur `byte` ; erreur sémantique.

**Conversion :**

- `byteToInt(byte b)` → `int` : retourne la valeur entière de `b` (0–255) ;
- `intToByte(int n)` → `byte` : clamp — si `n < 0` retourne `byte(0)`, si `n > 255` retourne `byte(255)`.

**`byte[]` :**

- tableau dynamique et homogène de `byte`, passé par référence comme les autres tableaux ;
- `count(b[])` retourne le nombre d'éléments, comme pour tous les tableaux ;
- `b[i]` en lecture retourne un `byte` ; `b[i] = v` en écriture accepte un `byte` ou un littéral entier `[0,255]` ; affecter un `int` variable nécessite `intToByte()` ;
- les littéraux `byte[]` utilisent la même syntaxe que les autres tableaux : `byte[] px = [255, 0, 128, 255];` ; chaque élément doit être un littéral dans `[0,255]` ou une expression de type `byte`.

```c
byte r = 0xFF;
byte g = intToByte(128);
byte b = 0x00;
byte a = 255;

byte[] pixel = [r, g, b, a];

// Arithmétique : résultat int
int sum = r + g;              // int(383)
byte clamped = intToByte(r + g);  // byte(255)

// Bitwise : résultat byte
byte masked = r & 0x0F;       // byte(0x0F)
byte flipped = ~b;            // byte(0xFF)
byte combined = r | g;        // byte(0xFF)

// Comparaison
bool bright = r > 200;        // true

// Décalage : résultat int
int shifted = r << 2;         // int(1020)
```

---

### 6.8 Structures (Cimple 1.1)

Une **structure** est un type composite défini par l'utilisateur, regroupant des champs typés et des méthodes. Les structures constituent le seul mécanisme d'abstraction de données en Cimple. Elles sont **entièrement statiques** : leur taille est fixe et connue à la compilation.

#### 6.8.1 Déclaration

```
structure NomDeStructure {
    <type> <champ> = <valeur_par_défaut>;
    ...
    <type_retour> <méthode>(<paramètres>) { ... }
    ...
}
```

Règles normatives :

- le mot-clé est `structure` ;
- le nom d'une structure commence par une lettre majuscule ; un nom commençant par une minuscule est une **erreur sémantique** ;
- chaque champ peut avoir une **valeur par défaut** explicite ou implicite selon son type ; un champ de type structure doit avoir une valeur par défaut explicite `clone NomDeStructure` (voir tableau ci-dessous) ;
- valeurs par défaut implicites (applicables si le champ ne précise pas de valeur) :

  | Type | Valeur implicite |
  |---|---|
  | `int` | `0` |
  | `float` | `0.0` |
  | `bool` | `false` |
  | `string` | `""` |
  | `byte` | `0` |
  | `int[]`, `float[]`, `bool[]`, `string[]`, `byte[]`, `NomDeStructure[]` | `[]` |
  | `NomDeStructure` | **obligatoire** — `clone NomDeStructure` doit être déclaré explicitement |
- les champs et les méthodes peuvent apparaître dans n'importe quel ordre au sein de la structure ;
- une structure doit être déclarée **avant** toute utilisation dans le fichier (ordre textuel strict) ; référencer une structure non encore déclarée est une **erreur sémantique** ;

> **Note d'implémentation — pré-collecte des noms de structures :** avant le parse principal, l'implémentation doit effectuer une passe de pré-collecte qui parcourt le flux de tokens pour identifier tous les noms de structures déclarés (`structure IDENT`) et les enregistrer comme `TYPE_IDENT` connus. Cette passe est nécessaire pour que le parseur puisse distinguer un nom de structure d'un identifiant ordinaire dès la première occurrence, indépendamment de l'ordre de déclaration dans le source. Sans cette passe, une référence à une structure non encore vue par le parseur produit une erreur syntaxique (`Unknown type`) au lieu d'une erreur sémantique (`structure non déclarée avant utilisation`), ce qui est un écart à la spec. La passe de pré-collecte résout également la détection de récursion indirecte entre structures et la détection de cycles d'héritage.
- un champ récursif (champ dont le type est la structure elle-même, directement ou indirectement) est une **erreur sémantique** ;
- un champ peut être de type structure à condition que la structure référencée soit déjà déclarée et ne forme pas de cycle de dépendance ;
- `void[]` et `ExecResult[]` ne sont pas des types valides pour un champ.

```c
structure Point {
    float x = 0.0;
    float y = 0.0;

    void move(float dx, float dy) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }

    float distanceToOrigin() {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}
```

#### 6.8.2 Héritage

Une structure peut hériter d'une autre via la syntaxe `: NomDeBase` :

```
structure NomDérivé : NomDeBase {
    ...
}
```

Règles normatives :

- l'héritage est **simple** : une structure ne peut hériter que d'une seule base ;
- l'héritage en chaîne est autorisé : `A : B`, `B : C` est valide ; la profondeur n'est pas limitée ;
- une structure hérite de tous les champs et méthodes de sa base, dans l'ordre de déclaration de la base ;
- la base doit être déclarée avant la structure dérivée (ordre textuel) ;
- les cycles d'héritage (`A : B, B : A`) sont une **erreur sémantique**.

**Redéfinition de champ :** une sous-structure peut redéclarer un champ hérité à condition que le **type soit identique**. La valeur par défaut peut être différente de celle de la base — c'est précisément l'intérêt de la redéfinition. `self.champ` accède toujours au champ de la structure courante avec sa propre valeur par défaut.

**Redéfinition de méthode (override) :** une sous-structure peut redéfinir une méthode héritée à condition que la signature (nom, types des paramètres, type de retour) soit **identique**. Redéfinir avec une signature différente est une **erreur sémantique**.

```c
structure Animal {
    string name = "animal";
    void speak() {
        print("...\n");
    }
}

structure Dog : Animal {
    string name = "dog";   // redéfinition valide — même type, valeur par défaut différente autorisée
    void speak() {         // override valide — signature identique
        print("Woof!\n");
    }
}

Dog d = clone Dog;
print(d.name + "\n");  // → "dog"
d.speak();             // → "Woof!"
```

#### 6.8.3 `self`

À l'intérieur d'une méthode, `self` désigne l'instance courante de la structure. `self` est implicitement disponible dans toutes les méthodes.

Règles normatives :

- `self.champ` accède en lecture et en écriture au champ `champ` de l'instance courante ;
- `self.méthode(...)` appelle la méthode `méthode` sur l'instance courante ;
- `self` ne peut pas être réaffecté : `self = clone AutreStructure` est une **erreur sémantique** ;
- `self` ne peut pas être passé directement comme argument à une fonction globale attendant un type scalaire ; il peut en revanche être passé à une fonction attendant le type de la structure courante (passage par référence).

#### 6.8.4 `super`

À l'intérieur d'une méthode d'une sous-structure, `super` permet d'appeler explicitement la méthode de la base qui a été redéfinie.

Règles normatives :

- `super.méthode(...)` appelle la méthode `méthode` telle que définie dans la **base directe** de la structure courante ;
- `super` est uniquement disponible si la structure courante hérite d'une base et que la méthode appelée existe dans cette base ;
- `super.champ` est une **erreur sémantique** — l'accès aux champs de la base se fait toujours via `self` ;
- utiliser `super` dans une structure sans base est une **erreur sémantique**.

```c
structure MyBase {
    float f = 2.0;
    void compute() {
        self.f = self.f + 0.1;
    }
}

structure MyDerived : MyBase {
    void compute() {
        super.compute();       // appelle MyBase.compute() → f devient 2.1
        self.f = self.f + 0.1; // puis f devient 2.2
    }
}
```

#### 6.8.5 `clone`

`clone` est l'opérateur de construction d'une instance de structure. Il crée une nouvelle instance avec les valeurs par défaut de tous les champs.

Syntaxe :

```c
NomDeStructure s = clone NomDeStructure;
```

Règles normatives :

- l'opérande de `clone` est toujours un **nom de structure** (identifiant de type) ; `clone` sur une variable est une **erreur sémantique** ;
- `clone` initialise tous les champs à leur valeur par défaut — explicite si déclarée, implicite selon le type sinon (voir tableau section 6.8.1) ; les champs hérités utilisent la valeur par défaut de la structure courante si redéfinis, sinon celle de la base ;
- il n'existe pas de constructeur : `clone` applique uniquement les valeurs par défaut ;
- `clone` peut être utilisé comme valeur initiale d'un champ de structure : `AutreStructure inner = clone AutreStructure;` ;
- `clone` peut être utilisé dans `arrayPush` pour ajouter une instance à un tableau de structures.

```c
Point p = clone Point;          // p.x == 0.0, p.y == 0.0
p.move(3.0, 4.0);
print(toString(p.distanceToOrigin()) + "\n");  // → "5.0"

// Tableau de structures
Point[] points = [];
arrayPush(points, clone Point);
arrayPush(points, clone Point);
points[1].move(1.0, 1.0);
```

#### 6.8.6 Champs de type structure

Un champ peut être de type structure à condition que :

- la structure référencée soit **déjà déclarée** (ordre textuel) ;
- il n'y ait pas de cycle de dépendance entre structures (détecté à la compilation) ;
- la valeur par défaut du champ soit `clone NomDeStructure`.

```c
structure Color {
    float r = 1.0;
    float g = 0.0;
    float b = 0.0;
}

structure Pixel {
    float x = 0.0;
    float y = 0.0;
    Color color = clone Color;   // champ de type structure
}

Pixel px = clone Pixel;
px.color.r = 0.5;
```

#### 6.8.7 Structures et fonctions globales

Une structure peut être passée à une fonction globale et retournée par une fonction globale.

Règles normatives :

- le passage est **par référence** : les modifications apportées à l'intérieur de la fonction sont visibles à l'extérieur ;
- une fonction globale peut déclarer un paramètre de type structure : `void process(Point p)` ;
- une fonction globale peut retourner une structure : `Point makePoint(float x, float y)` ; la valeur retournée est une copie ;
- une fonction globale ne peut pas retourner un type structure si ce type n'est pas encore déclaré à l'endroit de l'appel.

```c
void resetPoint(Point p) {
    p.x = 0.0;
    p.y = 0.0;
}

Point p = clone Point;
p.move(5.0, 5.0);
resetPoint(p);
print(toString(p.x) + "\n");  // → "0.0"
```

#### 6.8.8 Tableaux de structures

Un tableau de structures se comporte comme tous les autres tableaux Cimple.

```c
Point[] pts = [];
arrayPush(pts, clone Point);
arrayPush(pts, clone Point);
pts[0].move(1.0, 2.0);
pts[1].move(3.0, 4.0);
int n = count(pts);   // → 2
```

Règles normatives :

- `NomDeStructure[]` est un type valide partout où un type tableau est attendu ;
- les fonctions `arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`, `arrayGet`, `arraySet`, `count` fonctionnent sur les tableaux de structures ;
- `arrayGet` retourne une **copie** de l'élément ; pour modifier l'élément en place, utiliser l'accès direct par index `arr[i].champ = ...`.

### 6.8.9 Polymorphisme et dispatch dynamique

Voir §6.9 pour la définition complète. Résumé applicable aux structures :

- les méthodes sont **virtuelles par défaut** : si une sous-structure redéfinit une méthode, c'est toujours la version de la sous-structure qui est appelée, quelle que soit la variable réceptrice ;
- un tableau déclaré `Animal[]` peut contenir des instances de `Animal` et de n'importe quelle sous-structure de `Animal` (**covariance**) ;
- l'affectation d'une instance de sous-structure à une variable du type de base est valide.

---

### 6.9 Unions discriminées

Une **union discriminée** est un type qui peut contenir, à un instant donné, une valeur de l'un de ses membres — exactement un. Le compilateur ajoute automatiquement un champ `.kind` (entier) qui identifie le membre actif. Des constantes symboliques `NomUnion.NOM_CHAMP` (en majuscules) sont générées pour chaque membre.

#### 6.9.1 Déclaration

```c
union Valeur {
    int    i;
    float  f;
    string s;
}
```

Cela déclare le type `Valeur` avec trois membres possibles : `i` (int), `f` (float), `s` (string).

Le compilateur génère implicitement :

| Constante | Valeur |
|-----------|--------|
| `Valeur.I` | 0 |
| `Valeur.F` | 1 |
| `Valeur.S` | 2 |

#### 6.9.2 Construction et assignation

L'initialisation se fait par assignation directe du champ souhaité. Le `.kind` est mis à jour automatiquement.

```c
Valeur v;
v.i = 42;         // v.kind == Valeur.I
v.s = "bonjour";  // v.kind == Valeur.S  (le précédent est écrasé)
```

#### 6.9.3 Lecture et branchement

La lecture d'un membre dont le kind ne correspond pas est une **erreur runtime**. L'idiome canonique est le `switch` sur `.kind` :

```c
switch (v.kind) {
    case Valeur.I: print("entier : "  + toString(v.i)); break;
    case Valeur.F: print("flottant : "+ toString(v.f)); break;
    case Valeur.S: print("chaîne : "  + v.s);           break;
    default: break;
}
```

Le compilateur émet un **avertissement sémantique** si un `switch` sur `.kind` ne couvre pas tous les cas de l'union.

#### 6.9.4 Tableaux d'unions

```c
Valeur[] config;

Valeur host; host.s = "localhost"; arrayPush(config, host);
Valeur port; port.i = 8080;        arrayPush(config, port);
Valeur tls;  tls.b  = true;        arrayPush(config, tls);

for (int k = 0; k < count(config); k++) {
    switch (config[k].kind) {
        case Valeur.S: print("host: " + config[k].s);           break;
        case Valeur.I: print("port: " + toString(config[k].i)); break;
        case Valeur.B: print("tls: "  + toString(config[k].b)); break;
    }
}
```

#### 6.9.5 Membres vides (`void`)

Un membre de type `void` représente un état sans donnée — équivalent d'un « null » ou d'un cas « rien ». Il se déclare avec le mot-clé `void` à la place du type :

```c
union Option {
    void none;
    int  some;
}
```

**Activation** — la syntaxe `x.none;` utilisée comme instruction (sans affectation ni lecture) active le membre void, mettant `.kind` à `Option.none` :

```c
Option x;
x.none;               // active le membre vide : x.kind == Option.none
x.some = 42;          // active le membre avec valeur : x.kind == Option.some
```

**Branchement** — le membre void participe normalement au `switch` :

```c
switch (x.kind) {
    case Option.none: print("absent\n"); break;
    case Option.some: print(toString(x.some) + "\n"); break;
}
```

#### 6.9.6 Règles normatives

- les membres d'une union peuvent être de n'importe quel type scalaire, tableau, ou `void` ; les structures comme membres d'union ne sont **pas** supportées dans cette version ;
- la déclaration d'une union est valide au niveau global uniquement (pas de `union` local) ;
- `NomUnion[]` est un type tableau valide ;
- `.kind` est un champ en lecture seule : l'affecter directement est une erreur sémantique ;
- les constantes de kind sont des entiers constants, utilisables dans tout `switch/case` ;
- lire un membre dont le `.kind` ne correspond pas est une erreur runtime ;
- un membre `void` s'active en écrivant `x.membreVoid;` comme instruction ;
- un `switch` sur `.kind` qui n'est pas exhaustif produit un avertissement sémantique (non bloquant).

---

### 6.10 Polymorphisme et dispatch dynamique

#### 6.10.1 Méthodes virtuelles

Toutes les méthodes de structure sont **virtuelles par défaut**. Lorsqu'une sous-structure redéfinit une méthode de sa base, c'est toujours la méthode de la sous-structure qui est appelée à l'exécution, quelle que soit le type déclaré de la variable réceptrice.

```c
structure Animal {
    string parle() { return "..."; }
}

structure Chien : Animal {
    string parle() { return "waf"; }
}

structure Chat : Animal {
    string parle() { return "miaou"; }
}
```

#### 6.10.2 Tableaux covariants

Un tableau déclaré avec un type de base (`Animal[]`) peut contenir des instances de cette base **et de toutes ses sous-structures**. Le dispatch se fait dynamiquement sur le type réel de chaque élément.

```c
Animal[] zoo = [clone Chien, clone Chat, clone Chien];

for (int i = 0; i < count(zoo); i++) {
    print(zoo[i].parle() + "\n");   // "waf", "miaou", "waf"
}
```

L'affectation d'une instance de sous-structure à une variable du type de base est également valide :

```c
Animal a = clone Chien;
print(a.parle() + "\n");   // "waf"
```

#### 6.10.3 Règles normatives

- toutes les méthodes de structure sont virtuelles ; il n'existe pas de modificateur `virtual` / `override` — la redéfinition est détectée par correspondance de signature ;
- un override doit avoir **exactement la même signature** (même type de retour, mêmes types de paramètres) ; tout écart est une erreur sémantique ;
- la covariance est valide à l'affectation et dans les littéraux de tableau ;
- `super.méthode()` appelle explicitement la méthode de la base directe, indépendamment du dispatch dynamique ;
- les champs ne sont **pas** virtuels : `a.x` sur une variable de type `Animal` accède toujours au champ `x` de `Animal`, même si l'instance est un `Chien` ;
- le dispatch dynamique s'applique aux appels de méthode sur variables, paramètres de fonctions, éléments de tableau, et valeurs retournées.

#### 6.10.4 Exemple complet

```c
structure Forme {
    float aire()    { return 0.0; }
    string nom()    { return "forme"; }
}

structure Cercle : Forme {
    float rayon;
    float aire()    { return 3.14159 * self.rayon * self.rayon; }
    string nom()    { return "cercle"; }
}

structure Rectangle : Forme {
    float largeur;
    float hauteur;
    float aire()    { return self.largeur * self.hauteur; }
    string nom()    { return "rectangle"; }
}

void afficherForme(Forme f) {
    print(f.nom() + " — aire : " + toString(f.aire()) + "\n");
}

void main() {
    Cercle    c = clone Cercle;    c.rayon   = 5.0;
    Rectangle r = clone Rectangle; r.largeur = 4.0; r.hauteur = 6.0;

    Forme[] formes = [c, r];
    for (int i = 0; i < count(formes); i++) {
        afficherForme(formes[i]);
        // cercle — aire : 78.53975
        // rectangle — aire : 24.0
    }
}
```

---

### 6.11 Tableaux bidimensionnels (2D)

Cimple supporte les **tableaux irréguliers** (*jagged arrays*) à deux dimensions. Chaque ligne est un tableau 1D indépendant ; les lignes peuvent avoir des tailles différentes.

#### 6.11.1 Types 2D

| Type       | Description                        |
|------------|------------------------------------|
| `int[][]`  | tableau de tableaux d'entiers      |
| `float[][]`| tableau de tableaux de flottants   |
| `bool[][]` | tableau de tableaux de booléens    |
| `string[][]`| tableau de tableaux de chaînes    |
| `byte[][]` | tableau de tableaux d'octets       |
| `T[][]`    | tableau de tableaux de structures/unions |

#### 6.11.2 Déclaration et initialisation

```c
// Déclaration sans initialisation (tableau vide)
int[][] matrix;

// Initialisation avec un littéral 2D
int[][] grid = [[1,2,3],[4,5,6],[7,8,9]];

// Ajout d'une ligne
int[] row = [10,11,12];
arrayPush(grid, row);
```

#### 6.11.3 Lecture et écriture

```c
int val = grid[1][2];   // lecture : 6
grid[0][1] = 99;        // écriture 2D
```

La syntaxe `a[i][j]` est un sucre syntaxique : `a[i]` retourne la ligne `i` (de type `int[]`), puis `[j]` indexe la colonne.

#### 6.11.4 Fonctions utilitaires

- `count(matrix)` retourne le nombre de lignes.
- `count(matrix[i])` retourne le nombre d'éléments de la ligne `i`.
- `arrayPush(matrix, row)` ajoute la ligne `row` en fin de tableau.
- `arrayPop(matrix)` retire et retourne la dernière ligne.

#### 6.11.5 Parcours

```c
for (int i = 0; i < count(matrix); i++) {
    for (int j = 0; j < count(matrix[i]); j++) {
        print(toString(matrix[i][j]) + " ");
    }
    print("\n");
}
```

#### 6.11.6 Règles normatives

- Un tableau 2D est toujours un tableau de tableaux du même type d'élément scalaire ou struct/union.
- `void[][]` n'existe pas.
- Les tableaux 3D et plus ne sont pas supportés.
- L'assignation entière d'une ligne (`matrix[i] = row`) utilise la syntaxe d'assignation simple (`=`), pas `[i][j]`.

---

## 7. Variables

### 7.1 Déclaration de variables scalaires

Syntaxe :

```c
int x;
int y = 10;
string name = "Alice";
```

### 7.2 Déclaration de tableaux

Syntaxe avec littéral :

```c
int[] scores = [10, 20, 30];
float[] temps = [0.1, 0.2, 0.3];
bool[] flags = [true, false, true];
string[] names = ["Alice", "Bob", "Charlie"];
```

Déclaration sans initialisation (tableau vide) :

```c
int[] values;
string[] words;
```

Un tableau déclaré sans initialisation est vide (longueur `0`). Il n'a pas de valeur par défaut d'élément puisqu'il ne contient aucun élément.

Règles :

- le type d'un tableau est déterminé par la déclaration et ne peut pas changer ;
- tous les éléments du littéral `[...]` doivent être du même type que le tableau déclaré ; tout type incompatible est une erreur sémantique ;
- un tableau ne peut pas être réaffecté en totalité par `t = [...]` après déclaration ; toute tentative est une **erreur sémantique** ; les modifications passent obligatoirement par les fonctions de manipulation (voir section 9.8) ;
- un tableau peut être passé à une fonction ou retourné par une fonction.

### 7.3 Indexation

Syntaxe d'accès en lecture :

```c
int x = scores[0];
string s = names[2];
```

L'indexation `array[expr]` est une expression qui retourne la valeur à l'indice donné. L'expression d'indice doit être de type `int`.

Syntaxe de modification d'un élément :

```c
scores[1] = 99;
names[0] = "Zoé";
```

La modification par indexation `array[expr] = value` est une **instruction autonome**, pas une expression. Elle doit vérifier la compatibilité de type entre la valeur assignée et le type d'élément du tableau.

Règles normatives :

- les indices commencent à `0` ;
- l'indice doit être de type `int` ; tout autre type est une erreur sémantique ;
- un indice négatif est une **erreur runtime** ;
- un indice supérieur ou égal à `count(t)` est une **erreur runtime** ;
- les erreurs runtime d'indexation doivent produire un message clair indiquant l'indice demandé, la longueur courante du tableau, la ligne et la colonne dans le source.

**Distinction tableau / chaîne :**

| Expression | Tableau | Chaîne |
|---|---|---|
| `x[i]` lecture | ✅ expression, type de l'élément | ✅ expression, `string` (1 octet) |
| `x[i] = v` écriture | ✅ instruction autonome | ❌ erreur sémantique (immutable) |

### 7.4 Règles des variables scalaires

- une variable doit être déclarée avant utilisation ;
- une variable locale appartient au bloc où elle est déclarée ;
- une affectation ultérieure est autorisée ;
- toute variable locale non initialisée reçoit une valeur par défaut ;
- les valeurs par défaut sont : `0` pour `int`, `0.0` pour `float`, `""` pour `string`, `false` pour `bool`.

### 7.5 Affectation scalaire

Syntaxe :

```c
x = 42;
name = "Bob";
```

L'affectation doit vérifier la compatibilité stricte des types.

### 7.6 Constantes prédéfinies

Cimple définit un ensemble de constantes prédéfinies fournies par le runtime. Ces constantes sont des identifiants en majuscules réservés ; elles ne peuvent pas être réaffectées, redéclarées ou utilisées comme noms de variables ou de fonctions.

**Constantes entières (`int`)**

| Constante | Type | Description |
|---|---|---|
| `INT_MAX` | `int` | La plus grande valeur entière supportée par cette version de Cimple. |
| `INT_MIN` | `int` | La plus petite valeur entière supportée par cette version de Cimple. |
| `INT_SIZE` | `int` | La taille d'un entier en octets dans cette version de Cimple. |
| `FLOAT_SIZE` | `int` | La taille d'une valeur flottante en octets dans cette version de Cimple. |
| `FLOAT_DIG` | `int` | Le nombre de chiffres décimaux significatifs pouvant être représentés sans perte de précision pour un flottant. |

**Constantes flottantes (`float`)**

| Constante | Type | Description |
|---|---|---|
| `FLOAT_EPSILON` | `float` | La plus petite valeur flottante positive telle que `1.0 + FLOAT_EPSILON != 1.0`. Utile pour les comparaisons approchées. |
| `FLOAT_MIN` | `float` | La plus petite valeur flottante positive normalisée supportée. Pour obtenir la plus petite valeur négative, utiliser `-FLOAT_MAX`. |
| `FLOAT_MAX` | `float` | La plus grande valeur flottante positive supportée. |

**Constantes mathématiques (`float`)**

| Constante | Valeur | Description |
|---|---|---|
| `M_PI` | `3.141592653589793` | π — rapport de la circonférence d'un cercle à son diamètre. |
| `M_E` | `2.718281828459045` | e — base du logarithme naturel. |
| `M_TAU` | `6.283185307179586` | τ = 2π — angle en radians d'un tour complet ; souvent plus pratique que π dans les calculs d'angles. |
| `M_SQRT2` | `1.4142135623730951` | √2 — racine carrée de 2. |
| `M_LN2` | `0.6931471805599453` | ln(2) — logarithme naturel de 2 ; utile pour les conversions entre bases logarithmiques. |
| `M_LN10` | `2.302585092994046` | ln(10) — logarithme naturel de 10 ; utile pour les conversions entre bases logarithmiques. |

**Règles normatives :**

- ces constantes sont des **expressions** de leur type respectif et peuvent apparaître partout où une expression de ce type est attendue ;
- leur valeur est déterminée à la compilation de l'implémentation Cimple et reflète les caractéristiques de la plateforme cible pour les constantes numériques (`INT_*`, `FLOAT_*`) ; les constantes mathématiques (`M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10`) ont des valeurs fixes indépendantes de la plateforme ;
- toute tentative de déclaration d'une variable portant le nom d'une constante prédéfinie (`INT_MAX`, `INT_MIN`, `INT_SIZE`, `FLOAT_SIZE`, `FLOAT_DIG`, `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX`, `M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10`) est une **erreur sémantique** ;
- toute tentative d'affectation à une constante prédéfinie est une **erreur sémantique** ;
- les constantes prédéfinies ne sont pas des mots-clés au sens du lexer ; ce sont des identifiants réservés reconnus par l'analyse sémantique.

**Valeurs typiques (implémentation 64 bits) :**

```
INT_MAX       →  9223372036854775807   (2^63 - 1, si int = int64_t)
INT_MIN       → -9223372036854775808   (-2^63,   si int = int64_t)
INT_SIZE      →  8                     (octets)
FLOAT_SIZE    →  8                     (octets, double IEEE 754)
FLOAT_DIG     →  15                    (chiffres décimaux significatifs)
FLOAT_EPSILON →  2.220446049250313e-16
FLOAT_MIN     →  2.2250738585072014e-308
FLOAT_MAX     →  1.7976931348623157e+308
M_PI          →  3.141592653589793
M_E           →  2.718281828459045
M_TAU         →  6.283185307179586
M_SQRT2       →  1.4142135623730951
M_LN2         →  0.6931471805599453
M_LN10        →  2.302585092994046
```

Les valeurs `INT_*` et `FLOAT_*` sont données à titre indicatif et dépendent de la plateforme et des types C utilisés dans l'implémentation (`int64_t` pour `int`, `double` pour `float`). Les valeurs `M_*` sont fixes et identiques sur toutes les plateformes.

**Exemples d'usage :**

```c
void main() {
    // Constantes numériques
    print("INT_MAX  = " + toString(INT_MAX)  + "\n");
    print("INT_MIN  = " + toString(INT_MIN)  + "\n");
    print("INT_SIZE = " + toString(INT_SIZE) + " octets\n");

    float x = 1.0 + FLOAT_EPSILON;
    if (x != 1.0) {
        print("FLOAT_EPSILON correctement détecté\n");
    }

    if (approxEqual(0.1 + 0.2, 0.3, FLOAT_EPSILON * 10.0)) {
        print("égalité approchée\n");
    }

    print("FLOAT_MAX = " + toString(FLOAT_MAX) + "\n");
    print("Plus petite valeur négative : " + toString(-FLOAT_MAX) + "\n");

    // Constantes mathématiques
    print("π        = " + toString(M_PI)   + "\n");
    print("e        = " + toString(M_E)    + "\n");
    print("τ = 2π   = " + toString(M_TAU)  + "\n");
    print("√2       = " + toString(M_SQRT2) + "\n");
    print("ln(2)    = " + toString(M_LN2)  + "\n");
    print("ln(10)   = " + toString(M_LN10) + "\n");

    // Calcul de la circonférence d'un cercle de rayon 5
    float rayon = 5.0;
    float circ = M_TAU * rayon;
    print("Circonférence (r=5) = " + toString(circ) + "\n");

    // Conversion log₂ via log et M_LN2
    float log2_val = log(val) / M_LN2;   // log₂(8) = 3.0
    print("log₂(8) = " + toString(log2_val) + "\n");
}
```

---

## 8. Fonctions

### 8.1 Déclaration de fonction

Syntaxe :

```c
int add(int a, int b) {
    return a + b;
}
```

### 8.2 Règles

- une fonction a un nom ;
- une liste ordonnée de paramètres ;
- un type de retour explicite, qui peut être un type scalaire, un type tableau, ou `void` ;
- un corps sous forme de bloc ;
- pas de paramètres par défaut ;
- **pas de surcharge utilisateur** : deux fonctions définies par l'utilisateur ne peuvent pas avoir le même nom, quelle que soit leur signature ; seules les fonctions de conversion intrinsèques (`toString`, `toInt`, `toFloat`, `toBool`) et les intrinsèques tableau bénéficient d'une résolution statique par type d'argument ;
- pas de variadique ;
- les paramètres de type tableau sont passés **par référence** : toute modification du tableau dans la fonction est visible à l'appelant ;
- les paramètres scalaires sont passés **par valeur**.

### 8.3 Fonctions comme paramètres (callbacks)

Une fonction peut être passée en argument à une autre fonction. La syntaxe du paramètre est celle d'une **déclaration de fonction sans corps** : type de retour, nom local, liste de types de paramètres.

```c
// Déclaration : bool cmp(int, int) est le type du paramètre
void trieBulles(int[] arr, bool cmp(int, int)) {
    for (int i = 0; i < count(arr) - 1; i++) {
        for (int j = 0; j < count(arr) - i - 1; j++) {
            if (!cmp(arr[j], arr[j+1])) {
                int tmp  = arr[j];
                arr[j]   = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }
}

bool croissant(int a, int b) { return a < b; }
bool décroissant(int a, int b) { return a > b; }

void main() {
    int[] scores = [5, 2, 8, 1, 9];
    trieBulles(scores, croissant);    // [1, 2, 5, 8, 9]
    trieBulles(scores, décroissant);  // [9, 8, 5, 2, 1]
}
```

Une référence de fonction peut également être stockée dans une variable, en utilisant la même syntaxe de déclaration avec initialisation :

```c
bool compare(int, int) = croissant;  // variable de type "fonction"
compare(3, 7);                        // appel identique à une fonction normale
```

Règles normatives :

- le type d'un paramètre fonction est entièrement défini par son type de retour et les types de ses paramètres (pas les noms) ;
- la vérification est **statique** : passer une fonction dont la signature ne correspond pas est une erreur sémantique ;
- les fonctions de la bibliothèque standard peuvent être passées comme callbacks lorsque leur signature correspond ;
- les fonctions anonymes (lambdas) ne sont **pas** supportées ; seules des fonctions nommées déclarées au niveau global peuvent être passées ;
- une variable de type fonction s'appelle comme une fonction ordinaire.

```c
// La stdlib peut être passée directement
void appliquer(string[] strs, void action(string)) {
    for (int i = 0; i < count(strs); i++) {
        action(strs[i]);
    }
}

string[] noms = ["Alice", "Bob", "Carol"];
appliquer(noms, print);   // print est void(string) : signature compatible
```

### 8.4 Retour

- une fonction non `void` doit retourner une valeur de son type déclaré ;
- une fonction `void` peut utiliser `return;` sans expression ;
- `return expr;` dans une fonction `void` doit être interdit ;
- une fonction `void` peut se terminer sans instruction `return;` explicite ;
- l'absence de `return` dans une fonction non `void` doit être une erreur sémantique détectée statiquement (voir section 20.6).

---

## 9. Bibliothèque standard

Les fonctions natives ne sont **pas** des mots-clés. Ce sont des fonctions connues du runtime.

**Règle de surcharge dans la bibliothèque standard.** Le code utilisateur ne peut pas déclarer deux fonctions de même nom. Les fonctions de la bibliothèque standard respectent également ce principe, à l'exception des quatre fonctions de conversion intrinsèques — `toString`, `toInt`, `toFloat`, `toBool` — qui acceptent plusieurs types sources résolus **statiquement à la compilation** par le type de l'argument. Ce mécanisme est documenté en section 6.6 et constitue la seule forme de résolution multiple autorisée dans Cimple.

### 9.1 Liste complète des fonctions

```c
void print(string value);
int len(string value);
int glyphLen(string value);
string glyphAt(string value, int index);
int byteAt(string value, int index);
string input();
string substr(string s, int start, int length);
int    indexOf(string s, string needle);
bool   contains(string s, string needle);
bool   startsWith(string s, string prefix);
bool   endsWith(string s, string suffix);
string replace(string s, string old, string new);
string format(string template, string[] args);
string join(string[] array, string separator);
string[] split(string value, string separator);
string concat(string[] array);
string trim(string s);
string trimLeft(string s);
string trimRight(string s);
string toUpper(string s);
string toLower(string s);
string capitalize(string s);
string padLeft(string s, int width, string pad);
string padRight(string s, int width, string pad);
string repeat(string s, int n);
int    lastIndexOf(string s, string needle);
int    countOccurrences(string s, string needle);
bool   isBlank(string s);

// Conversions intrinsèques — résolution statique par type de l'argument
string toString(int value);
string toString(float value);
string toString(bool value);
float  toFloat(int value);
float  toFloat(string value);
int    toInt(float value);
int    toInt(string value);
bool   toBool(string value);

bool isIntString(string value);
bool isFloatString(string value);
bool isBoolString(string value);
bool isNaN(float value);
bool isInfinite(float value);
bool isFinite(float value);
bool isPositiveInfinity(float value);
bool isNegativeInfinity(float value);
float abs(float x);
float min(float a, float b);
float max(float a, float b);
float floor(float x);
float ceil(float x);
float round(float x);
float trunc(float x);
float fmod(float x, float y);
float sqrt(float x);
float pow(float base, float exponent);
bool approxEqual(float a, float b, float epsilon);
// Trigonométrie (radians)
float sin(float x);
float cos(float x);
float tan(float x);
float asin(float x);
float acos(float x);
float atan(float x);
float atan2(float y, float x);
// Logarithmes et exponentielle
float exp(float x);
float log(float x);
float log2(float x);
float log10(float x);
int absInt(int x);
int minInt(int a, int b);
int maxInt(int a, int b);
int clampInt(int value, int minValue, int maxValue);
bool isEven(int value);
bool isOdd(int value);
int safeDivInt(int a, int b, int fallback);
int safeModInt(int a, int b, int fallback);

// Fonctions intrinsèques sur tableaux (polymorphes sur le type d'élément, gérées par le runtime)
int     count(T[] array);
void    arrayPush(T[] array, T value);
T       arrayPop(T[] array);
void    arrayInsert(T[] array, int index, T value);
void    arrayRemove(T[] array, int index);
T       arrayGet(T[] array, int index);
void    arraySet(T[] array, int index, T value);

// Entrées/sorties fichiers binaires
byte[]   readFileBytes(string path);
void     writeFileBytes(string path, byte[] data);
void     appendFileBytes(string path, byte[] data);

// Conversions byte
int      byteToInt(byte b);
byte     intToByte(int n);
byte[]   stringToBytes(string s);
string   bytesToString(byte[] data);
byte[]   intToBytes(int n);
byte[]   floatToBytes(float f);
int      bytesToInt(byte[] data);
float    bytesToFloat(byte[] data);

// Entrées/sorties fichiers
string   readFile(string path);
void     writeFile(string path, string content);
void     appendFile(string path, string content);
bool     fileExists(string path);
string[] readLines(string path);
void     writeLines(string path, string[] lines);
string   tempPath();
void     remove(string path);
void     chmod(string path, int mode);
string   cwd();
void     copy(string src, string dst);
void     move(string src, string dst);
bool     isReadable(string path);
bool     isWritable(string path);
bool     isExecutable(string path);
bool     isDirectory(string path);
string   dirname(string path);
string   basename(string path);
string   filename(string path);
string   extension(string path);

// Exécution de commandes externes
ExecResult exec(string[] command, string[] env);
int        execStatus(ExecResult result);
string     execStdout(ExecResult result);
string     execStderr(ExecResult result);

// Environnement du processus
string getEnv(string name, string fallback);

// Temps et dates (UTC)
int now();
int epochToYear(int epochMs);
int epochToMonth(int epochMs);
int epochToDay(int epochMs);
int epochToHour(int epochMs);
int epochToMinute(int epochMs);
int epochToSecond(int epochMs);
int makeEpoch(int year, int month, int day, int hour, int minute, int second);
string formatDate(int epochMs, string fmt);

// Utilitaires
void  assert(bool condition);
void  assert(bool condition, string message);
int   randInt(int min, int max);
float randFloat();
void  sleep(int ms);
```

La notation `T` dans les signatures des fonctions tableau indique un paramètre de type générique restreint aux quatre types tableau autorisés (`int`, `float`, `bool`, `string`). Ce polymorphisme est exclusivement réservé aux fonctions intrinsèques du runtime sur les tableaux ; il ne s'étend pas au code utilisateur et ne constitue pas une surcharge au sens de Cimple.

### 9.2 Affichage et saisie

- `print` n'accepte que `string` ;
- pour afficher un `int`, un `float` ou un `bool`, il faut passer par `toString(...)` qui résout statiquement la bonne conversion selon le type de l'argument.

#### `input`

```c
string input()
```

Lit une ligne depuis l'entrée standard (`stdin`) et retourne son contenu sous forme de `string`.

Règles normatives :

- la lecture s'arrête au prochain caractère de fin de ligne (`\n` ou `\r\n`) ; le caractère de fin de ligne n'est **pas** inclus dans la chaîne retournée ;
- si `stdin` est fermé (EOF) avant toute lecture, retourne `""` ;
- ne lève jamais d'erreur runtime ; en cas d'erreur de lecture, retourne `""` ;
- ne prend aucun argument ; tout argument est une erreur sémantique ;
- retourne un `string`.

```c
void main() {
    print("Entrez votre nom : ");
    string name = input();
    print("Bonjour, " + name + "\n");
}
```

### 9.3 Règles associées aux chaînes

- `len` retourne le **nombre d'octets UTF-8** de la chaîne ; pour une chaîne ASCII pure, `len` et `glyphLen` coïncident ; pour une chaîne contenant des caractères multi-octets, ils diffèrent ;
- `substr` retourne une sous-chaîne ;
- `indexOf` retourne la position de la première occurrence, ou `-1` si non trouvé ;
- `contains`, `startsWith`, `endsWith` retournent un `bool` ;
- `substr(s, start, length)` retourne au plus `length` **octets** à partir de la position `start` (en octets, base 0) ; les positions et longueurs sont en **octets** ;
- les indices négatifs sont interdits ;
- si la fin dépasse, la sous-chaîne est tronquée à la fin disponible ;
- les opérations sur chaînes sont sensibles à la casse.

#### `glyphLen`

```c
int glyphLen(string s)
```

Retourne le nombre de **points de code Unicode** de la chaîne `s` après normalisation NFC.

Règles normatives :

- la chaîne `s` est normalisée en **NFC** (forme canonique composée) avant comptage ;
- une séquence `e` + U+0301 (e + accent combinant) est comptée comme **1** point de code après NFC, car elle se compose en U+00E9 ;
- `glyphLen("")` retourne `0` ;
- pour une chaîne ASCII pure, `glyphLen(s) == len(s)` ;
- pour une chaîne contenant des caractères multi-octets, `glyphLen(s) <= len(s)`.

```c
string s = "élève";
print(toString(len(s)));       // → 7  (octets UTF-8)
print(toString(glyphLen(s)));  // → 5  (points de code NFC)
```

#### `glyphAt`

```c
string glyphAt(string s, int index)
```

Retourne le point de code Unicode à la position `index` (base 0) de la chaîne `s`, sous sa **forme NFC**, encodé en UTF-8 dans une `string` d'un seul point de code.

Règles normatives :

- la chaîne `s` est normalisée en **NFC** avant toute opération d'indexation ;
- `index` est un indice de point de code (base 0) dans la chaîne normalisée NFC, **pas** un indice d'octet ;
- si la chaîne source contient `e` + U+0301, `glyphAt(s, i)` retourne `"é"` (U+00E9) — la forme composée NFC ;
- deux appels sur des chaînes de représentations différentes mais sémantiquement identiques retournent toujours le même résultat NFC ;
- `index` doit être de type `int` ; tout autre type est une erreur sémantique ;
- un indice négatif est une **erreur runtime** ;
- un indice supérieur ou égal à `glyphLen(s)` est une **erreur runtime** ; le message d'erreur doit indiquer l'indice demandé, la longueur NFC de la chaîne, la ligne et la colonne.

```c
string s = "élève";
print(glyphAt(s, 0));  // → "é"  (U+00E9)
print(glyphAt(s, 1));  // → "l"
print(glyphAt(s, 2));  // → "è"  (U+00E8)
print(glyphAt(s, 3));  // → "v"
print(glyphAt(s, 4));  // → "e"
```

Démonstration de la normalisation NFC :

```c
void main() {
    string nfc = "\u00E9";          // é — forme composée (1 point de code)
    string nfd = "e\u0301";        // e + accent combinant (2 points de code)

    // glyphLen normalise les deux en NFC avant de compter
    print(toString(glyphLen(nfc))); // → 1
    print(toString(glyphLen(nfd))); // → 1

    // glyphAt retourne toujours la forme NFC
    print(glyphAt(nfc, 0));         // → "é"  (U+00E9)
    print(glyphAt(nfd, 0));         // → "é"  (U+00E9) — normalisé
}
```

#### `byteAt`

```c
int byteAt(string s, int index)
```

Retourne la valeur entière (0–255) de l'octet situé à la position `index` (base 0) dans la représentation UTF-8 de la chaîne.

```c
byteAt("A", 0)    // → 65    (0x41)
byteAt("abc", 2)  // → 99    (0x63, 'c')
byteAt("é", 0)    // → 195   (0xC3, premier octet UTF-8 de U+00E9)
byteAt("é", 1)    // → 169   (0xA9, second octet UTF-8 de U+00E9)
```

Règles normatives :

- `index` est un indice en **octets**, base 0 ; cohérent avec `len` et `substr` ;
- `index` doit être de type `int` ; tout autre type est une erreur sémantique ;
- `index` doit être compris entre `0` et `len(s) - 1` inclus ;
- un indice négatif est une **erreur runtime** avec message clair indiquant l'indice demandé, la longueur en octets et la position dans le source ;
- un indice supérieur ou égal à `len(s)` est une **erreur runtime** avec le même format ;
- la valeur retournée est toujours un entier dans l'intervalle `[0, 255]` ;
- `byteAt` opère sur les octets bruts UTF-8 sans aucune normalisation Unicode ; contrairement à `glyphAt`, il n'applique pas de NFC.

#### Indexation par crochet : `s[i]`

```c
s[i]   // expression, retourne string (1 octet)
```

`s[i]` est une expression de lecture retournant une `string` contenant l'unique octet UTF-8 situé à la position `i` (base 0). C'est l'équivalent fonctionnel de `byteAt` mais avec un type de retour `string` plutôt qu'`int`, cohérent avec l'absence de type `char` dans Cimple.

```c
string s = "hello";
string c = s[0];         // → "h"
print(s[1] + "\n");      // → "e"

// parcours caractère par caractère (octets)
for (int i = 0; i < len(s); i++) {
    print(s[i] + "\n");
}
```

Règles normatives :

- `s[i]` est une **expression** ; elle peut apparaître partout où une expression `string` est attendue ;
- `i` doit être de type `int` ; tout autre type est une erreur sémantique ;
- `i` doit être compris entre `0` et `len(s) - 1` inclus ; tout indice hors bornes est une **erreur runtime** avec message clair (indice demandé, longueur en octets, ligne et colonne) ;
- `s[i]` retourne toujours une `string` d'exactement un octet ; pour une chaîne ASCII, cet octet correspond au caractère visuel ; pour une chaîne UTF-8 multi-octets, `s[i]` peut retourner un octet partiel ne formant pas à lui seul un point de code Unicode valide ;
- `s[i] = ...` est **interdit** : les chaînes sont immutables ; toute tentative d'affectation par indexation sur une chaîne est une **erreur sémantique** ;
- `s[i]` et `byteAt(s, i)` sont sémantiquement équivalents en termes d'indice et de bornes ; seul le type retourné diffère (`string` vs `int`).

**Relation avec les autres fonctions de la famille chaîne :**

| Fonction / Expression | Unité | Normalisation | Type retourné |
|---|---|---|---|
| `len(s)` | octets | aucune | `int` |
| `byteAt(s, i)` | octets | aucune | `int` (0–255) |
| `s[i]` | octets | aucune | `string` (1 octet) |
| `glyphLen(s)` | points de code | NFC | `int` |
| `glyphAt(s, i)` | points de code | NFC | `string` |

#### `join`

```c
string join(string[] array, string separator)
```

Concatène tous les éléments de `array` en une seule chaîne, en insérant `separator` entre chaque élément consécutif.

Règles normatives :

- si `array` est vide, retourne `""` ;
- si `array` contient un seul élément, retourne cet élément sans séparateur ;
- si `separator` est `""`, les éléments sont concaténés sans séparation, ce qui est sémantiquement équivalent à `concat(array)` ;
- `array` doit être de type `string[]` ; tout autre type de tableau est une erreur sémantique.

```c
string[] words = ["Bonjour", "tout", "le", "monde"];
string s = join(words, " ");   // s vaut "Bonjour tout le monde"

string[] parts = ["a", "b", "c"];
string t = join(parts, "-");   // t vaut "a-b-c"

string[] empty = [];
string u = join(empty, ",");   // u vaut ""
```

#### `split`

```c
string[] split(string value, string separator)
```

Découpe `value` en un tableau de sous-chaînes en utilisant `separator` comme délimiteur.

Règles normatives :

- si `separator` n'est pas trouvé dans `value`, retourne un tableau contenant `value` comme unique élément ;
- si `value` est `""`, retourne un tableau contenant une seule chaîne vide `[""]` ;
- si `separator` est `""`, c'est une **erreur runtime** avec message clair : un séparateur vide est interdit ;
- les occurrences consécutives de `separator` produisent des chaînes vides dans le résultat ; ex. `split("a,,b", ",")` retourne `["a", "", "b"]` ;
- `separator` doit être de type `string` ; tout autre type est une erreur sémantique.

```c
string[] parts = split("a,b,c", ",");
// parts vaut ["a", "b", "c"]

string[] line = split("Bonjour monde", " ");
// line vaut ["Bonjour", "monde"]

string[] single = split("abc", "x");
// single vaut ["abc"]
```

#### `concat`

```c
string concat(string[] array)
```

Concatène tous les éléments de `array` en une seule chaîne, sans séparateur. Sémantiquement équivalent à `join(array, "")`.

Règles normatives :

- si `array` est vide, retourne `""` ;
- `array` doit être de type `string[]` ; tout autre type de tableau est une erreur sémantique.

```c
string[] parts = ["Bonjour", " ", "Alice"];
string s = concat(parts);   // s vaut "Bonjour Alice"

string[] tokens = ["x", "=", "42"];
string expr = concat(tokens);   // expr vaut "x=42"

string[] empty = [];
string nothing = concat(empty);   // nothing vaut ""
```

#### `replace`

```c
string replace(string s, string old, string new)
```

Retourne une nouvelle chaîne dans laquelle toutes les occurrences de `old` dans `s` ont été remplacées par `new`.

Règles normatives :

- toutes les occurrences sont remplacées, pas seulement la première ;
- si `old` n'apparaît pas dans `s`, retourne `s` inchangée ;
- si `old` est `""`, c'est une **erreur runtime** — remplacer une chaîne vide est indéfini ;
- si `new` est `""`, les occurrences de `old` sont supprimées ;
- les trois arguments doivent être de type `string` ; tout autre type est une erreur sémantique ;
- la chaîne source `s` n'est pas modifiée (les chaînes sont immutables).

```c
string s = "bonjour le monde";
string r = replace(s, "o", "0");      // → "b0nj0ur le m0nde"
string t = replace(s, "monde", "Cimple"); // → "bonjour le Cimple"
string u = replace(s, "xyz", "abc");  // → "bonjour le monde" (inchangé)
string v = replace(s, "o", "");       // → "bnjur le mnde"

// Enchaînement de remplacements
string result = replace(replace("a-b-c", "-", "_"), "a", "x");
// → "x_b_c"
```

#### `format`

```c
string format(string template, string[] args)
```

Retourne une nouvelle chaîne construite à partir de `template` en remplaçant chaque marqueur `{}` par l'argument correspondant dans `args`, dans l'ordre d'apparition.

Règles normatives :

- les marqueurs sont `{}` ; ils sont remplacés dans l'ordre par `args[0]`, `args[1]`, etc. ;
- le nombre de `{}` dans `template` doit être exactement égal à `count(args)` ; tout écart est une **erreur runtime** avec message indiquant le nombre de marqueurs attendus et le nombre d'arguments fournis ;
- les arguments doivent être de type `string` ; toute conversion doit être effectuée explicitement par l'utilisateur via `toString` avant l'appel ;
- `{}` littéral dans la sortie n'est pas possible directement — si le template doit contenir les caractères `{` ou `}`, utiliser `replace` après `format` ou éviter ces caractères dans le template ;
- `template` et `args` doivent respecter leurs types ; tout autre type est une erreur sémantique.

```c
string name = "Alice";
int age = 30;
float score = 98.5;

string msg = format("Bonjour {}, tu as {} ans.", [name, toString(age)]);
// → "Bonjour Alice, tu as 30 ans."

string line = format("Nom: {}, Score: {}", [name, toString(score)]);
// → "Nom: Alice, Score: 98.5"

// Formatage de plusieurs valeurs
string[] data = [toString(1), toString(2), toString(3)];
string row = format("{} + {} = {}", data);
// → "1 + 2 = 3"

// Erreur runtime : 2 marqueurs mais 1 argument
// string bad = format("x={}, y={}", [toString(x)]);  // erreur runtime
```

#### `trim`

```c
string trim(string s)
```

Retourne une copie de `s` dont les caractères d'espacement en début et en fin ont été supprimés. Les caractères supprimés sont : espace (`U+0020`), tabulation (`\t`), retour chariot (`\r`), saut de ligne (`\n`), tabulation verticale (`\v`), saut de page (`\f`).

Règles normatives :

- si `s` est entièrement composée de caractères d'espacement, retourne `""` ;
- si `s` ne contient aucun caractère d'espacement en début ni en fin, retourne `s` inchangée ;
- `s` doit être de type `string` ; tout autre type est une erreur sémantique.

```c
trim("  bonjour  ")      // → "bonjour"
trim("\t hello\n")       // → "hello"
trim("rien à faire")     // → "rien à faire"
trim("   ")              // → ""
```

#### `trimLeft`

```c
string trimLeft(string s)
```

Identique à `trim`, mais supprime uniquement les caractères d'espacement en **début** de chaîne.

```c
trimLeft("  bonjour  ")  // → "bonjour  "
trimLeft("\t hello\n")   // → "hello\n"
```

#### `trimRight`

```c
string trimRight(string s)
```

Identique à `trim`, mais supprime uniquement les caractères d'espacement en **fin** de chaîne.

```c
trimRight("  bonjour  ")  // → "  bonjour"
trimRight("\t hello\n")   // → "\t hello"
```

#### `toUpper`

```c
string toUpper(string s)
```

Retourne une copie de `s` dans laquelle chaque point de code Unicode en minuscule a été converti en sa forme majuscule, selon les mappings de conversion de casse Unicode standard, indépendamment de la locale du système.

Règles normatives :

- la conversion est **locale-agnostique** : les mappings Unicode standard sont utilisés sans tenir compte de la locale du système ;
- les caractères sans forme majuscule sont copiés inchangés ;
- la conversion peut modifier la longueur en octets de la chaîne : `toUpper("ß")` → `"SS"` (1 point de code → 2) ; le résultat peut donc avoir un `len` supérieur à celui de l'entrée ;
- `s` doit être de type `string` ; tout autre type est une erreur sémantique.

```c
toUpper("bonjour")    // → "BONJOUR"
toUpper("héllo")      // → "HÉLLO"
toUpper("straße")     // → "STRASSE"   (ß → SS)
toUpper("DÉJÀ")       // → "DÉJÀ"      (inchangé)
```

#### `toLower`

```c
string toLower(string s)
```

Retourne une copie de `s` dans laquelle chaque point de code Unicode en majuscule a été converti en sa forme minuscule, selon les mappings Unicode standard, indépendamment de la locale du système.

Règles normatives :

- la conversion est **locale-agnostique** ;
- les caractères sans forme minuscule sont copiés inchangés ;
- `s` doit être de type `string` ; tout autre type est une erreur sémantique.

```c
toLower("BONJOUR")    // → "bonjour"
toLower("HÉLLO")      // → "héllo"
toLower("déjà")       // → "déjà"      (inchangé)
```

#### `capitalize`

```c
string capitalize(string s)
```

Retourne une copie de `s` dont le premier point de code Unicode a été converti en majuscule selon les mappings Unicode standard. Le reste de la chaîne est copié **inchangé**.

Règles normatives :

- seul le **premier point de code** est affecté ; les suivants ne sont pas modifiés ;
- si `s` est vide, retourne `""` ;
- si le premier point de code n'a pas de forme majuscule, retourne `s` inchangée ;
- `s` doit être de type `string` ; tout autre type est une erreur sémantique.

```c
capitalize("bonjour")   // → "Bonjour"
capitalize("hELLO")     // → "HELLO"    (H majuscule, ELLO inchangé)
capitalize("été")       // → "Été"
capitalize("")          // → ""
```

#### `padLeft`

```c
string padLeft(string s, int width, string pad)
```

Retourne une chaîne de largeur `width` glyphes en ajoutant le caractère (ou la séquence) `pad` à **gauche** de `s` jusqu'à atteindre la largeur souhaitée. Si `glyphLen(s) >= width`, retourne `s` inchangée.

Règles normatives :

- `width` est mesuré en **glyphes** (points de code NFC, cohérent avec `glyphLen`) ;
- si `pad` est `""`, retourne `s` inchangée ;
- si `pad` contient plusieurs glyphes, il est répété puis tronqué à gauche pour tomber exactement à `width` glyphes (comportement PHP `str_pad`) ;
- si `glyphLen(s) >= width`, retourne `s` inchangée sans erreur ;
- `width` doit être ≥ 0 ; une valeur négative est une **erreur runtime** ;
- tous les arguments doivent respecter leurs types ; tout autre type est une erreur sémantique.

```c
padLeft("42", 6, " ")     // → "    42"
padLeft("hi", 6, "-")     // → "----hi"
padLeft("hello", 3, " ")  // → "hello"   (déjà plus long)
padLeft("x", 5, "ab")     // → "ababx"   (pad "ab" répété puis tronqué)
padLeft("x", 6, "ab")     // → "ababax"  (tronqué pour tomber pile)
```

#### `padRight`

```c
string padRight(string s, int width, string pad)
```

Identique à `padLeft`, mais le padding est ajouté à **droite** de `s`.

Règles normatives : identiques à `padLeft`, mutatis mutandis.

```c
padRight("hi", 6, "-")    // → "hi----"
padRight("hi", 6, "+-")   // → "hi+-+-"  (pad répété)
padRight("hi", 5, "+-")   // → "hi+-+"   (tronqué à droite)
padRight("hello", 3, " ") // → "hello"   (déjà plus long)
```

#### `repeat`

```c
string repeat(string s, int n)
```

Retourne une chaîne constituée de `n` répétitions de `s` bout à bout.

Règles normatives :

- si `n == 0`, retourne `""` ;
- si `n < 0`, c'est une **erreur runtime** ;
- si `s` est `""`, retourne `""` quel que soit `n` ;
- `s` doit être de type `string` et `n` de type `int` ; tout autre type est une erreur sémantique.

```c
repeat("ab", 3)    // → "ababab"
repeat("-", 5)     // → "-----"
repeat("", 10)     // → ""
repeat("x", 0)     // → ""
```

#### `lastIndexOf`

```c
int lastIndexOf(string s, string needle)
```

Retourne la position en octets de la **dernière** occurrence de `needle` dans `s`. Retourne `-1` si `needle` est absente.

Règles normatives :

- la position retournée est un index d'octet (base 0), cohérent avec `indexOf` ;
- si `needle` est `""`, retourne `len(s)` (cohérent avec `indexOf("", "")` → `0`) ;
- si `needle` n'apparaît pas dans `s`, retourne `-1` ;
- les deux arguments doivent être de type `string` ; tout autre type est une erreur sémantique.

```c
lastIndexOf("abcabc", "b")   // → 4
lastIndexOf("abcabc", "x")   // → -1
lastIndexOf("abcabc", "abc") // → 3
lastIndexOf("hello", "l")    // → 3
```

#### `countOccurrences`

```c
int countOccurrences(string s, string needle)
```

Retourne le nombre d'occurrences **non chevauchantes** de `needle` dans `s`.

Règles normatives :

- les occurrences sont comptées de gauche à droite sans chevauchement : après une correspondance, la recherche reprend après la fin de l'occurrence trouvée ;
- si `needle` est `""`, c'est une **erreur runtime** (cohérent avec `replace`) ;
- si `needle` n'apparaît pas, retourne `0` ;
- les deux arguments doivent être de type `string` ; tout autre type est une erreur sémantique.

```c
countOccurrences("abcabc", "abc")  // → 2
countOccurrences("aaaa", "aa")     // → 2   (non chevauchant : "aa|aa")
countOccurrences("hello", "x")     // → 0
countOccurrences("aaa", "a")       // → 3
```

#### `isBlank`

```c
bool isBlank(string s)
```

Retourne `true` si `s` est vide ou ne contient que des caractères d'espacement (espace, `\t`, `\r`, `\n`, `\v`, `\f`), `false` sinon. Équivalent à `trim(s) == ""`.

Règles normatives :

- `s` doit être de type `string` ; tout autre type est une erreur sémantique ;
- ne lève jamais d'erreur runtime.

```c
isBlank("")         // → true
isBlank("   ")      // → true
isBlank("\t\n")     // → true
isBlank("  x  ")   // → false
```

### 9.4 Conversions depuis `string`

- `toInt(string value)` retourne `0` si la chaîne n'est pas un entier valide ;
- `toFloat(string value)` retourne `NaN` si la chaîne n'est pas un flottant valide ;
- `toBool(string value)` accepte au minimum `"true"`, `"false"`, `"1"`, `"0"` ; retourne `false` si non reconnue ;
- `isIntString`, `isFloatString`, `isBoolString` permettent de contrôler les conversions avant appel.

### 9.5 Capacités booléennes

Le type `bool` repose d'abord sur le langage lui-même (`true`, `false`, `&&`, `||`, `!`).

La bibliothèque standard pour `bool` comprend :

- `toString(bool value) string`
- `toBool(string value) bool`
- `isBoolString(string value) bool`

### 9.6 Capacités numériques sur `float`

- `toFloat(int)` convertit explicitement un entier en flottant ;
- `toInt(float)` convertit explicitement un flottant en entier par **troncature vers zéro** ;
- `abs`, `min`, `max` fournissent les opérations numériques de base ;
- `floor`, `ceil`, `round`, `trunc` couvrent les besoins d'arrondi ;
- `fmod` fournit le modulo flottant ;
- `sqrt` et `pow` font partie du noyau mathématique ;
- `exp`, `log`, `log2`, `log10` couvrent les besoins logarithmiques et exponentiels ;
- `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` couvrent la trigonométrie ;
- `approxEqual` permet les comparaisons approchées entre flottants.

Philosophie numérique retenue pour `float` :

- les opérations sur `float` suivent IEEE ;
- `1.0 / 0.0` suit IEEE ;
- les valeurs spéciales `NaN`, `+Infinity` et `-Infinity` sont des résultats possibles des calculs flottants ;
- `log(0.0)` et `log(x)` pour `x < 0` retournent `-Infinity` ou `NaN` conformément à IEEE ;
- `asin(x)` et `acos(x)` pour `|x| > 1.0` retournent `NaN` conformément à IEEE ;
- les fonctions `isNaN`, `isInfinite`, `isFinite`, `isPositiveInfinity`, `isNegativeInfinity` permettent à l'utilisateur de contrôler explicitement ces cas.

Les comparaisons exactes `==` et `!=` sur `float` restent autorisées, mais doivent être documentées comme potentiellement piégeuses.

### 9.6.1 Trigonométrie

Toutes les fonctions trigonométriques opèrent en **radians**. Utiliser `M_PI` et `M_TAU` pour les conversions degrés ↔ radians.

```c
float sin(float x)    // sinus de x
float cos(float x)    // cosinus de x
float tan(float x)    // tangente de x
float asin(float x)   // arc sinus,    résultat dans [-π/2, π/2] ; NaN si |x| > 1
float acos(float x)   // arc cosinus,  résultat dans [0, π]     ; NaN si |x| > 1
float atan(float x)   // arc tangente, résultat dans [-π/2, π/2]
float atan2(float y, float x)  // arc tangente de y/x, tient compte du quadrant
                               // résultat dans [-π, π] ; défini pour (0, 0) → 0
```

Règles normatives :

- toutes reçoivent un ou deux arguments de type `float` ; tout autre type est une erreur sémantique ;
- `atan2(y, x)` : le premier argument est `y`, le second est `x` — convention C standard ;
- `tan(π/2)` peut produire une très grande valeur ou `Infinity` selon la précision flottante, conformément à IEEE.

```c
float angle_deg = 45.0;
float angle_rad = angle_deg * M_PI / 180.0;
float s = sin(angle_rad);   // → ~0.7071
float c = cos(angle_rad);   // → ~0.7071

// atan2 pour l'angle d'un vecteur (1, 1)
float theta = atan2(1.0, 1.0);  // → π/4 ≈ 0.7854
```

### 9.6.2 Logarithmes et exponentielle

```c
float exp(float x)     // eˣ — exponentielle naturelle
float log(float x)     // ln(x) — logarithme naturel ; NaN si x < 0 ; -Infinity si x == 0
float log2(float x)    // log₂(x) ; NaN si x < 0 ; -Infinity si x == 0
float log10(float x)   // log₁₀(x) ; NaN si x < 0 ; -Infinity si x == 0
```

Règles normatives :

- toutes reçoivent exactement un argument de type `float` ; tout autre type est une erreur sémantique ;
- comportement sur domaine invalide conforme à IEEE : `log(0.0)` → `-Infinity`, `log(-1.0)` → `NaN` ;
- `exp` ne lève pas d'erreur runtime sur dépassement ; retourne `+Infinity` conformément à IEEE.

```c
float x = exp(1.0);          // → e ≈ 2.71828
float y = log(M_E);          // → 1.0
float z = log2(1024.0);      // → 10.0
float w = log10(1000.0);     // → 3.0

// Conversion log₂ via log et M_LN2
float log2_manual = log(8.0) / M_LN2;   // → 3.0
```

### 9.6.3 Arrondi étendu et modulo flottant

```c
float trunc(float x)           // troncature vers zéro
float fmod(float x, float y)   // reste flottant de x / y, même signe que x
```

Règles normatives :

- `trunc` diffère de `floor` pour les négatifs : `floor(-1.5)` → `-2.0`, `trunc(-1.5)` → `-1.0` ;
- `fmod(x, 0.0)` retourne `NaN` conformément à IEEE ;
- `fmod` a le même signe que `x` (convention C `fmod`, pas Python `%`) ;
- les deux reçoivent des arguments de type `float` ; tout autre type est une erreur sémantique.

```c
trunc(2.9)      // → 2.0
trunc(-2.9)     // → -2.0   (≠ floor(-2.9) = -3.0)
fmod(7.5, 2.0)  // → 1.5
fmod(-7.5, 2.0) // → -1.5
fmod(360.0, M_TAU * (180.0 / M_PI))  // normalisation d'angle
```

### 9.7 Capacités entières sur `int`

```c
int absInt(int x);
int minInt(int a, int b);
int maxInt(int a, int b);
int clampInt(int value, int minValue, int maxValue);
bool isEven(int value);
bool isOdd(int value);
int safeDivInt(int a, int b, int fallback);
int safeModInt(int a, int b, int fallback);
```

Règles associées :

- `/` entre `int` effectue une **division entière par troncature vers zéro** : `7 / 2` → `3`, `-7 / 2` → `-3` (pas de floor division) ;
- `%` représente le reste entier, avec le même signe que le dividende : `-7 % 2` → `-1` ;
- les opérateurs bitwise sont disponibles sur `int` uniquement ;
- `safeDivInt(a, b, fallback)` retourne `fallback` si `b == 0` ;
- `safeModInt(a, b, fallback)` retourne `fallback` si `b == 0`.

La bibliothèque standard emploie des noms distincts selon les types pour éviter toute surcharge : `abs` pour `float` et `absInt` pour `int`, `min`/`minInt`, `max`/`maxInt`.

### 9.8 Fonctions de manipulation des tableaux

Ces fonctions sont des **intrinsèques du runtime**. Elles sont polymorphes sur les quatre types de tableaux (`int[]`, `float[]`, `bool[]`, `string[]`). Ce polymorphisme est strictement réservé à ces fonctions ; il est géré par le runtime et ne crée pas de surcharge au sens du langage.

#### `count`

```c
int count(T[] array)
```

Retourne le nombre d'éléments du tableau. Retourne `0` si le tableau est vide.

```c
int[] a = [10, 20, 30];
int n = count(a);   // n vaut 3
```

#### `arrayPush`

```c
void arrayPush(T[] array, T value)
```

Ajoute `value` à la fin du tableau. La longueur du tableau augmente de `1`.

```c
int[] a = [1, 2];
arrayPush(a, 3);    // a vaut [1, 2, 3]
```

Le type de `value` doit être identique au type d'élément du tableau ; tout type incompatible est une erreur sémantique.

#### `arrayPop`

```c
T arrayPop(T[] array)
```

Retire et retourne le dernier élément du tableau. La longueur du tableau diminue de `1`.

```c
int[] a = [1, 2, 3];
int last = arrayPop(a);   // last vaut 3, a vaut [1, 2]
```

Appeler `arrayPop` sur un tableau vide est une **erreur runtime** avec message clair indiquant la ligne et la colonne.

#### `arrayInsert`

```c
void arrayInsert(T[] array, int index, T value)
```

Insère `value` à la position `index`. L'élément précédemment à cet indice et tous ceux qui suivent sont décalés d'une position vers la droite. La longueur du tableau augmente de `1`.

```c
int[] a = [1, 3, 4];
arrayInsert(a, 1, 2);   // a vaut [1, 2, 3, 4]
```

Règles :

- `index` doit être de type `int` ; tout autre type est une erreur sémantique ;
- `index` doit être compris entre `0` et `count(array)` inclus ;
- insérer à l'indice `count(array)` est équivalent à `arrayPush` ;
- un indice négatif ou supérieur à `count(array)` est une **erreur runtime** avec message clair.

#### `arrayRemove`

```c
void arrayRemove(T[] array, int index)
```

Supprime l'élément à la position `index`. Les éléments suivants sont décalés d'une position vers la gauche. La longueur du tableau diminue de `1`.

```c
int[] a = [1, 2, 3, 4];
arrayRemove(a, 2);   // a vaut [1, 2, 4]
```

Règles :

- `index` doit être de type `int` ; tout autre type est une erreur sémantique ;
- `index` doit être compris entre `0` et `count(array) - 1` inclus ;
- appeler `arrayRemove` sur un tableau vide est une **erreur runtime** ;
- un indice négatif ou supérieur ou égal à `count(array)` est une **erreur runtime** avec message clair.

#### `arrayGet`

```c
T arrayGet(T[] array, int index)
```

Retourne la valeur à la position `index`. Équivalent à `array[index]` en lecture, fourni comme fonction explicite pour les contextes où une expression d'indexation n'est pas acceptée.

```c
int[] a = [10, 20, 30];
int x = arrayGet(a, 1);   // x vaut 20
```

Règles :

- `index` doit être de type `int` ;
- un indice hors bornes est une **erreur runtime** avec message clair.

#### `arraySet`

```c
void arraySet(T[] array, int index, T value)
```

Modifie la valeur à la position `index`. Équivalent à `array[index] = value` comme instruction autonome, fourni comme fonction explicite.

```c
int[] a = [10, 20, 30];
arraySet(a, 1, 99);   // a vaut [10, 99, 30]
```

Règles :

- `index` doit être de type `int` ;
- le type de `value` doit être identique au type d'élément du tableau ; tout type incompatible est une erreur sémantique ;
- un indice hors bornes est une **erreur runtime** avec message clair.

### 9.9 Entrées/sorties binaires et conversions `byte`

#### `readFileBytes`

```c
byte[] readFileBytes(string path)
```

Lit l'intégralité du fichier situé à `path` et retourne son contenu sous forme de `byte[]`. Aucune interprétation ni conversion : les octets bruts sont retournés tels quels.

Règles normatives :

- si le fichier est vide, retourne `[]` ;
- si `path` ne désigne pas un fichier lisible, c'est une **erreur runtime** ;
- le résultat est de type `byte[]` ; `count()` retourne le nombre d'octets lus.

```c
byte[] img = readFileBytes("photo.rgba");
print("Taille : " + toString(count(img)) + " octets\n");
```

#### `writeFileBytes`

```c
void writeFileBytes(string path, byte[] data)
```

Écrit le contenu de `data` dans le fichier situé à `path`. Si le fichier existe, son contenu est **entièrement remplacé**. Si le fichier n'existe pas, il est **créé**.

Règles normatives :

- les octets sont écrits tels quels, sans interprétation UTF-8 ni ajout de fin de ligne ;
- si le répertoire parent n'existe pas ou si les permissions sont insuffisantes, c'est une **erreur runtime**.

```c
byte[] rgba = [255, 0, 0, 255,   // pixel rouge opaque
               0, 255, 0, 255];  // pixel vert opaque
writeFileBytes("image.raw", rgba);
```

#### `appendFileBytes`

```c
void appendFileBytes(string path, byte[] data)
```

Ajoute le contenu de `data` à la fin du fichier situé à `path`. Si le fichier n'existe pas, il est **créé**.

Règles normatives :

- identiques à `writeFileBytes`, le contenu existant n'est pas modifié ;
- si les permissions sont insuffisantes, c'est une **erreur runtime**.

```c
appendFileBytes("stream.bin", [0xDE, 0xAD, 0xBE, 0xEF]);
```

#### `byteToInt`

```c
int byteToInt(byte b)
```

Retourne la valeur entière de `b` dans l'intervalle `[0, 255]`.

```c
byte b = 0xFF;
int n = byteToInt(b);   // → 255
```

#### `intToByte`

```c
byte intToByte(int n)
```

Convertit `n` en `byte` avec **clamp** : si `n < 0` retourne `byte(0)` ; si `n > 255` retourne `byte(255)`.

Règles normatives :

- ne lève jamais d'erreur runtime ;
- c'est le seul mécanisme de conversion explicite `int` → `byte`.

```c
intToByte(200)    // → byte(200)
intToByte(300)    // → byte(255)   (clamp)
intToByte(-5)     // → byte(0)     (clamp)
intToByte(0xFF)   // → byte(255)
```

#### `stringToBytes`

```c
byte[] stringToBytes(string s)
```

Retourne les octets UTF-8 bruts de `s` sous forme de `byte[]`. La longueur du résultat est `len(s)`.

Règles normatives :

- chaque octet de la représentation UTF-8 de `s` devient un élément du tableau résultant ;
- ne lève jamais d'erreur runtime ;
- `count(stringToBytes(s)) == len(s)` est toujours vrai.

```c
byte[] bytes = stringToBytes("ABC");
// → [65, 66, 67]

byte[] utf8 = stringToBytes("é");
// → [0xC3, 0xA9]   (2 octets UTF-8)
```

#### `bytesToString`

```c
string bytesToString(byte[] data)
```

Interprète `data` comme une séquence d'octets UTF-8 et retourne la chaîne correspondante.

Règles normatives :

- les séquences d'octets non valides en UTF-8 sont remplacées par le caractère de remplacement Unicode **U+FFFD** (`�`) ;
- ne lève jamais d'erreur runtime ;
- `bytesToString(stringToBytes(s)) == s` est toujours vrai pour toute chaîne `s` valide.

```c
string s = bytesToString([72, 101, 108, 108, 111]);   // → "Hello"
string t = bytesToString([0xC3, 0xA9]);               // → "é"
string u = bytesToString([0xFF]);                     // → "�"  (U+FFFD)
```

#### `intToBytes`

```c
byte[] intToBytes(int n)
```

Retourne un `byte[]` de **`INT_SIZE` octets** contenant la représentation mémoire brute de `n`, dans l'ordre natif de la machine hôte (endianness non définie par le langage).

Règles normatives :

- le résultat a toujours exactement `count == INT_SIZE` ;
- aucune transformation n'est appliquée : les `INT_SIZE` octets sont ceux que la machine utilise pour stocker `n` en mémoire (`memcpy` depuis `&n`) ;
- l'endianness du résultat dépend de la plateforme d'exécution ; l'utilisateur qui a besoin d'un ordre précis doit réorganiser les octets lui-même ;
- ne lève jamais d'erreur runtime.

```c
byte[] b = intToBytes(1);
// Sur une machine little-endian : [0x01, 0x00, 0x00, 0x00]
// Sur une machine big-endian    : [0x00, 0x00, 0x00, 0x01]

// Vérifier l'endianness de la machine courante :
bool isLittleEndian = intToBytes(1)[0] == 1;
```

#### `floatToBytes`

```c
byte[] floatToBytes(float f)
```

Retourne un `byte[]` de **`FLOAT_SIZE` octets** contenant la représentation mémoire brute de `f` (IEEE 754 double précision), dans l'ordre natif de la machine hôte.

Règles normatives :

- le résultat a toujours exactement `count == FLOAT_SIZE` ;
- aucune transformation n'est appliquée : les `FLOAT_SIZE` octets sont ceux que la machine utilise pour stocker `f` en mémoire (`memcpy` depuis `&f`) ;
- `NaN`, `+Infinity` et `-Infinity` produisent leurs représentations IEEE 754 brutes respectives, sans erreur runtime ;
- ne lève jamais d'erreur runtime.

```c
byte[] b = floatToBytes(1.0);
// 8 octets bruts de 1.0 en IEEE 754 double, ordre natif
```

#### `bytesToInt`

```c
int bytesToInt(byte[] data)
```

Recopie les `INT_SIZE` octets de `data` dans un `int` et retourne la valeur résultante. Opération symétrique de `intToBytes`.

Règles normatives :

- `data` doit avoir exactement `count(data) == INT_SIZE` ; tout autre nombre d'octets est une **erreur runtime** ;
- aucune interprétation n'est effectuée : les octets sont copiés tels quels dans la zone mémoire de l'entier résultat (`memcpy` vers `&result`) ;
- le résultat peut être négatif si le pattern de bits correspond à une valeur négative en représentation signée sur la machine hôte ; c'est le comportement attendu ;
- le round-trip `bytesToInt(intToBytes(n)) == n` est garanti sur la même machine.

```c
byte[] b = intToBytes(42);
int n = bytesToInt(b);    // → 42   (round-trip garanti)

// Mauvaise taille → erreur runtime
// int bad = bytesToInt([0x01, 0x02]);
```

#### `bytesToFloat`

```c
float bytesToFloat(byte[] data)
```

Recopie les `FLOAT_SIZE` octets de `data` dans un `float` et retourne la valeur résultante. Opération symétrique de `floatToBytes`.

Règles normatives :

- `data` doit avoir exactement `count(data) == FLOAT_SIZE` ; tout autre nombre d'octets est une **erreur runtime** ;
- aucune interprétation n'est effectuée : les octets sont copiés tels quels dans la zone mémoire du flottant résultat (`memcpy` vers `&result`) ;
- tout pattern de `FLOAT_SIZE` octets est un double IEEE 754 valide sur les plateformes cibles de Cimple ; le résultat peut donc être `NaN`, `+Infinity` ou `-Infinity` si les octets le représentent — c'est le comportement attendu ; l'utilisateur peut utiliser `isNaN`, `isInfinite` pour inspecter le résultat ;
- le round-trip `bytesToFloat(floatToBytes(f)) == f` est garanti sur la même machine (y compris pour `NaN` en termes de pattern de bits, bien que `NaN != NaN` soit vrai par définition IEEE 754) ;
- ne lève jamais d'erreur runtime si `count(data) == FLOAT_SIZE`.

```c
byte[] b = floatToBytes(3.14);
float f = bytesToFloat(b);    // → 3.14   (round-trip garanti)

// Inspecter le résultat si les octets viennent d'une source externe
float x = bytesToFloat(externalData);
if (isNaN(x) || isInfinite(x)) {
    print("valeur non finie\n");
}

// Mauvaise taille → erreur runtime
// float bad = bytesToFloat([0x01, 0x02, 0x03, 0x04]);
```

### 9.10 Entrées/sorties fichiers

Cimple fournit six fonctions natives pour lire et écrire des fichiers texte. Ces fonctions suivent les principes du reste du langage : pas de handle, pas de gestion de ressources manuelle, pas d'ouverture/fermeture explicite.

**Principes généraux :**

- tous les fichiers sont lus et écrits en **UTF-8** sans option d'encodage ;
- les chemins sont des chaînes UTF-8 ; les chemins relatifs sont résolus par rapport au répertoire de travail courant du processus ;
- toute erreur d'I/O (fichier introuvable, permission refusée, disque plein, chemin invalide) produit une **erreur runtime** avec message clair indiquant le chemin, la nature de l'erreur, la ligne et la colonne dans le source ;
- il n'existe pas de valeur sentinelle silencieuse : une erreur est toujours signalée explicitement ;
- `fileExists` permet à l'utilisateur de tester l'existence d'un fichier avant toute opération ;
- **portabilité** : le comportement des fonctions fichiers est identique sur macOS, Linux, Unix, Windows et WebAssembly (via MEMFS/NODEFS Emscripten) ; les séparateurs de chemin `/` et `\` sont tous deux acceptés en entrée sur Windows ; en écriture, Cimple utilise `\n` uniquement, indépendamment de la plateforme.

#### `readFile`

```c
string readFile(string path)
```

Lit l'intégralité du fichier situé à `path` et retourne son contenu sous forme de `string`.

Règles normatives :

- le fichier est lu en UTF-8 ;
- `readFile` retourne le contenu **brut** du fichier : les fins de ligne (`\n`, `\r\n`, `\r`) sont conservées telles quelles dans la chaîne retournée sans normalisation ; c'est le comportement voulu pour permettre une manipulation exacte du contenu octet par octet ; utiliser `readLines` si une normalisation des fins de ligne est souhaitée ;
- si le fichier est vide, retourne `""` ;
- si `path` ne désigne pas un fichier lisible, c'est une **erreur runtime**.

```c
string content = readFile("data.txt");
print(content);
```

#### `writeFile`

```c
void writeFile(string path, string content)
```

Écrit `content` dans le fichier situé à `path`. Si le fichier existe déjà, son contenu est **entièrement remplacé**. Si le fichier n'existe pas, il est **créé**. Les répertoires parents doivent exister ; `writeFile` ne crée pas de répertoires intermédiaires.

Règles normatives :

- le contenu est écrit en UTF-8 ;
- l'écriture est atomique du point de vue du contenu : soit le fichier est écrit intégralement, soit une erreur runtime est levée ;
- si le répertoire parent n'existe pas, c'est une **erreur runtime** ;
- si les permissions sont insuffisantes, c'est une **erreur runtime**.

```c
writeFile("output.txt", "Bonjour\n");
```

#### `appendFile`

```c
void appendFile(string path, string content)
```

Ajoute `content` à la fin du fichier situé à `path`. Si le fichier n'existe pas, il est **créé**. Le contenu existant n'est pas modifié.

Règles normatives :

- le contenu est ajouté en UTF-8 à la suite de l'existant ;
- aucun séparateur n'est ajouté automatiquement entre l'existant et le nouveau contenu ; si un saut de ligne est souhaité, il doit figurer dans `content` ;
- si les permissions sont insuffisantes, c'est une **erreur runtime**.

```c
appendFile("log.txt", "Nouvelle entrée\n");
```

#### `fileExists`

```c
bool fileExists(string path)
```

Retourne `true` si un fichier (et non un répertoire) existe et est accessible en lecture à `path`, `false` sinon.

Règles normatives :

- retourne `false` si `path` désigne un répertoire, un lien symbolique cassé, ou un fichier non lisible ;
- ne lève jamais d'erreur runtime ; toute condition d'inaccessibilité retourne `false` ;
- `fileExists` ne garantit pas qu'une lecture immédiatement suivante réussira (condition de course) ; son usage est indicatif.

```c
if (fileExists("config.txt")) {
    string cfg = readFile("config.txt");
    print(cfg);
} else {
    print("Fichier de configuration absent.\n");
}
```

#### `readLines`

```c
string[] readLines(string path)
```

Lit l'intégralité du fichier situé à `path` et retourne son contenu sous forme d'un tableau de lignes, une ligne par élément.

Règles normatives :

- les fins de ligne `\r\n`, `\n` et `\r` sont toutes reconnues et normalisées : elles ne font **pas** partie des chaînes retournées ;
- si le fichier est vide, retourne un tableau vide `[]` ;
- si le fichier se termine par une fin de ligne, cette dernière ne produit **pas** de chaîne vide supplémentaire ;
- si `path` ne désigne pas un fichier lisible, c'est une **erreur runtime**.

```c
string[] lines = readLines("data.csv");
for (int i = 0; i < count(lines); i++) {
    print(lines[i] + "\n");
}
```

#### `writeLines`

```c
void writeLines(string path, string[] lines)
```

Écrit le tableau `lines` dans le fichier situé à `path`, en séparant chaque ligne par `\n`. Si le fichier existe déjà, son contenu est **entièrement remplacé**. Si le fichier n'existe pas, il est **créé**.

Règles normatives :

- chaque élément de `lines` est écrit suivi de `\n` ; le fichier résultant se termine donc toujours par `\n` ;
- si `lines` est vide, le fichier est créé vide (ou vidé s'il existait) ;
- les répertoires parents doivent exister ; `writeLines` ne crée pas de répertoires intermédiaires ;
- si les permissions sont insuffisantes ou si le répertoire parent n'existe pas, c'est une **erreur runtime**.

```c
string[] lines = ["ligne 1", "ligne 2", "ligne 3"];
writeLines("output.txt", lines);
```

#### `tempPath`

```c
string tempPath()
```

Retourne un chemin de fichier temporaire **unique et inexistant** dans le répertoire temporaire du système.

Règles normatives :

- retourne une `string` contenant un chemin **valide** pour le système de fichiers courant ;
- le chemin retourné **ne désigne pas un fichier existant** au moment du retour ;
- la fonction **ne crée pas** le fichier ni aucune ressource sur le disque ;
- deux appels successifs à `tempPath()` **retournent deux chemins distincts** ; l'unicité est garantie pour la durée du processus Cimple courant ;
- ne prend aucun argument ; tout argument est une erreur sémantique ;
- ne lève jamais d'erreur runtime.

**Répertoire temporaire utilisé :**

| Plateforme | Répertoire |
|---|---|
| POSIX (macOS, Linux, Unix) | Valeur de `$TMPDIR` si définie et non vide ; sinon `/tmp` |
| Windows | Comportement défini par l'implémenteur |
| WebAssembly | Comportement défini par l'implémenteur |

**Propriétés assumées — limites du contrat :**

- aucune **réservation durable** n'est effectuée : entre le retour de `tempPath()` et l'utilisation effective du chemin, un autre processus peut créer un fichier au même emplacement ; c'est une race condition inhérente à ce type de fonction et Cimple n'y remédie pas ;
- aucune **suppression implicite** n'est effectuée : si l'appelant crée un fichier à ce chemin, il est responsable de sa suppression ; Cimple ne nettoie pas les fichiers temporaires en fin de programme ;
- `tempPath()` ne garantit pas la persistance du chemin au-delà du processus courant ; le répertoire temporaire peut être vidé par le système à tout moment.

```c
// Utilisation typique : fichier temporaire pour stocker un résultat intermédiaire
string tmp = tempPath();
writeFile(tmp, "données intermédiaires\n");

ExecResult r = exec(["traitement", tmp], []);
print(execStdout(r));

// Nettoyage à la charge de l'appelant
// (Cimple ne fournit pas de deleteFile dans cette version)

// Deux chemins distincts garantis
string a = tempPath();
string b = tempPath();
// a != b est toujours vrai
```

#### `remove`

```c
void remove(string path)
```

Supprime le fichier situé à `path`.

Règles normatives :

- si `path` désigne un fichier existant et accessible, il est supprimé ;
- si `path` désigne un fichier **inexistant**, c'est une **erreur runtime** ;
- si `path` désigne un **répertoire**, c'est une **erreur runtime** ; `remove` n'opère que sur les fichiers réguliers ;
- si les permissions sont insuffisantes pour supprimer le fichier, c'est une **erreur runtime** ;
- ne lève pas d'erreur runtime pour toute autre raison que celles listées ci-dessus.

```c
string tmp = tempPath();
writeFile(tmp, "temporaire");
// ... utilisation ...
remove(tmp);
```

#### `chmod`

```c
void chmod(string path, int mode)
```

Modifie les droits d'accès du fichier situé à `path`. `mode` est un entier dont les bits représentent les permissions Unix au format octal standard (ex. `0644`, `0755`).

Règles normatives :

- `path` doit désigner un fichier ou un répertoire existant et accessible ; si le chemin n'existe pas, c'est une **erreur runtime** ;
- `mode` est interprété comme un masque de permissions POSIX sur 12 bits (bits `setuid`, `setgid`, `sticky`, puis les 9 bits `rwxrwxrwx`) ; les valeurs typiques sont des littéraux octaux Cimple : `0644`, `0755`, `0600`, `0400` ;
- si les permissions courantes du processus sont insuffisantes pour modifier les droits du fichier, c'est une **erreur runtime** ;
- **portabilité :**
  - sur **macOS, Linux, Unix** : appel direct à `chmod(2)` avec `mode` ; comportement POSIX standard ;
  - sur **Windows** : `chmod` n'est pas disponible — lève une **erreur runtime** avec le message `chmod: not supported on this platform` ;
  - sur **WebAssembly** : `chmod` n'est pas disponible — lève une **erreur runtime** avec le message `chmod: not supported on this platform`.

```c
string path = "script.sh";
writeFile(path, "#!/bin/sh\necho hello\n");
chmod(path, 0755);   // rend le fichier exécutable

chmod("config.txt", 0600);   // lecture/écriture propriétaire seulement
chmod("public.txt", 0644);   // lecture/écriture propriétaire, lecture groupe et autres
```

#### `cwd`

```c
string cwd()
```

Retourne le chemin absolu du répertoire de travail courant du processus.

Règles normatives :

- ne prend aucun argument ; tout argument est une erreur sémantique ;
- retourne toujours un chemin absolu se terminant sans séparateur, sauf sur la racine `/` ;
- ne lève jamais d'erreur runtime.

```c
print(cwd() + "\n");   // ex. "/home/user/project"
```

#### `copy`

```c
void copy(string src, string dst)
```

Copie le fichier situé à `src` vers `dst`. Si `dst` existe déjà, son contenu est **écrasé silencieusement**. Si `dst` n'existe pas, il est créé.

Règles normatives :

- seuls les fichiers réguliers sont copiés ; si `src` désigne un répertoire, c'est une **erreur runtime** ;
- si `src` n'existe pas ou n'est pas lisible, c'est une **erreur runtime** ;
- si le répertoire parent de `dst` n'existe pas, c'est une **erreur runtime** ;
- si les permissions sont insuffisantes (lecture sur `src` ou écriture sur `dst`), c'est une **erreur runtime** ;
- le contenu est copié octet par octet ; les métadonnées (permissions, dates) ne sont pas copiées.

```c
copy("config.txt", "config.bak");
copy("template.html", "/tmp/output.html");
```

#### `move`

```c
void move(string src, string dst)
```

Déplace ou renomme le fichier situé à `src` vers `dst`. Si `dst` existe déjà, il est **écrasé silencieusement**. Si `dst` n'existe pas, il est créé.

Règles normatives :

- seuls les fichiers réguliers sont déplacés ; si `src` désigne un répertoire, c'est une **erreur runtime** ;
- si `src` n'existe pas, c'est une **erreur runtime** ;
- si le répertoire parent de `dst` n'existe pas, c'est une **erreur runtime** ;
- si `src` et `dst` sont sur le même système de fichiers, l'opération est effectuée par renommage atomique ; si les deux chemins sont sur des systèmes de fichiers différents, l'implémentation effectue une copie suivie d'une suppression de `src` de manière transparente pour le code Cimple ;
- si les permissions sont insuffisantes, c'est une **erreur runtime** ;
- après succès, `src` n'existe plus.

```c
move("draft.txt", "final.txt");
move("/tmp/result.csv", "data/result.csv");
```

#### `isReadable`

```c
bool isReadable(string path)
```

Retourne `true` si le chemin `path` désigne un fichier ou un répertoire existant et accessible en lecture par le processus courant, `false` sinon.

Règles normatives :

- retourne `false` si `path` n'existe pas, si les permissions sont insuffisantes, ou si le chemin est invalide ;
- ne lève jamais d'erreur runtime.

```c
if (isReadable("config.txt")) {
    string cfg = readFile("config.txt");
}
```

#### `isWritable`

```c
bool isWritable(string path)
```

Retourne `true` si le processus courant peut écrire à l'emplacement `path`, `false` sinon. Le comportement dépend de l'existence du fichier :

- si `path` désigne un **fichier existant** : teste si le fichier lui-même est accessible en écriture (`access(path, W_OK)`) ;
- si `path` désigne un chemin **inexistant** : teste si le **répertoire parent** est accessible en écriture, c'est-à-dire si un fichier pourrait être créé à cet emplacement ;
- si le répertoire parent de `path` n'existe pas : retourne `false`.

Règles normatives :

- ne lève jamais d'erreur runtime ;
- retourne `false` pour tout chemin invalide ou inaccessible ;
- si `path` désigne un **répertoire existant** : teste si le répertoire lui-même est accessible en écriture.

```c
if (isWritable("output.txt")) {
    writeFile("output.txt", result);
}

if (isWritable("/tmp/newfile.txt")) {
    // /tmp est accessible en écriture
}
```

#### `isExecutable`

```c
bool isExecutable(string path)
```

Retourne `true` si le chemin `path` désigne un fichier existant et exécutable par le processus courant, `false` sinon.

Règles normatives :

- retourne `false` si `path` n'existe pas, si les permissions sont insuffisantes, si le fichier n'est pas exécutable, ou si le chemin est invalide ;
- retourne `false` si `path` désigne un répertoire ;
- ne lève jamais d'erreur runtime.

```c
if (isExecutable("/usr/bin/git")) {
    ExecResult r = exec(["/usr/bin/git", "status"], []);
}
```

#### `isDirectory`

```c
bool isDirectory(string path)
```

Retourne `true` si `path` désigne un répertoire existant et accessible, `false` sinon.

Règles normatives :

- retourne `false` si `path` désigne un fichier régulier, un lien symbolique cassé, ou un chemin inexistant ;
- retourne `false` si le chemin est invalide ou inaccessible ;
- ne lève jamais d'erreur runtime.

```c
if (isDirectory("output")) {
    // le répertoire output existe
} else {
    print("Répertoire output absent.\n");
}
```

#### `dirname`

```c
string dirname(string path)
```

Retourne la partie répertoire du chemin `path`, c'est-à-dire tout ce qui précède le dernier séparateur de chemin.

Règles normatives (calcul purement lexical, sans accès au système de fichiers) :

- si `path` ne contient aucun séparateur, retourne `""` : `dirname("file.txt")` → `""` ;
- si `path` est la racine `"/"`, retourne `"/"` : `dirname("/")` → `"/"` ;
- si `path` est vide `""`, retourne `""` ;
- les séparateurs `/` et `\` sont tous deux reconnus ; le séparateur de la plateforme courante est utilisé dans le résultat ;
- le résultat ne contient pas de séparateur final, sauf pour la racine `"/"`.

```c
print(dirname("/path/to/my.file.txt") + "\n");  // "/path/to"
print(dirname("file.txt") + "\n");              // ""
print(dirname("/") + "\n");                     // "/"
print(dirname("a/b") + "\n");                   // "a"
```

#### `basename`

```c
string basename(string path)
```

Retourne le nom de fichier avec son extension, c'est-à-dire la dernière composante du chemin après le dernier séparateur.

Règles normatives (calcul purement lexical) :

- si `path` est vide `""`, retourne `""` ;
- si `path` se termine par un séparateur, le séparateur final est ignoré : `basename("/path/to/")` → `""` ;
- les séparateurs `/` et `\` sont tous deux reconnus.

```c
print(basename("/path/to/my.file.txt") + "\n");  // "my.file.txt"
print(basename("/path/to/") + "\n");             // ""
print(basename("file.txt") + "\n");              // "file.txt"
```

#### `filename`

```c
string filename(string path)
```

Retourne le nom de fichier sans sa dernière extension, c'est-à-dire `basename(path)` privé du dernier point et de ce qui suit.

Règles normatives (calcul purement lexical) :

- si `basename(path)` ne contient aucun point, retourne `basename(path)` intégralement : `filename("makefile")` → `"makefile"` ;
- si `basename(path)` commence par un point et ne contient pas d'autre point, le fichier est considéré comme un fichier caché sans extension : `filename(".gitignore")` → `".gitignore"` ;
- si `basename(path)` est vide, retourne `""`.

```c
print(filename("/path/to/my.file.txt") + "\n");  // "my.file"
print(filename("archive.tar.gz") + "\n");        // "archive.tar"
print(filename("makefile") + "\n");              // "makefile"
print(filename(".gitignore") + "\n");            // ".gitignore"
```

#### `extension`

```c
string extension(string path)
```

Retourne la dernière extension du fichier, sans le point. Retourne `""` si le fichier n'a pas d'extension.

Règles normatives (calcul purement lexical) :

- l'extension est la partie après le dernier point du `basename(path)` ;
- si `basename(path)` ne contient aucun point, retourne `""` : `extension("makefile")` → `""` ;
- si `basename(path)` commence par un point et ne contient pas d'autre point, retourne `""` : `extension(".gitignore")` → `""` ;
- si `basename(path)` est vide, retourne `""` ;
- le point lui-même n'est pas inclus dans le résultat.

```c
print(extension("/path/to/my.file.txt") + "\n");  // "txt"
print(extension("archive.tar.gz") + "\n");        // "gz"
print(extension("makefile") + "\n");              // ""
print(extension(".gitignore") + "\n");            // ""
```

### 9.11 Exécution de commandes externes

Cimple permet d'exécuter des commandes système via la fonction `exec`. Le résultat est encapsulé dans un `ExecResult` opaque, accessible via trois fonctions dédiées.

**Principes généraux :**

- la commande est exécutée comme un sous-processus du processus Cimple ;
- `exec` attend la fin de la commande avant de retourner ; il n'y a pas d'exécution asynchrone ;
- le sous-processus hérite de l'environnement du processus Cimple ; le paramètre `env` permet d'ajouter ou de surcharger des variables sans remplacer l'environnement entier ;
- stdout et stderr sont capturés séparément et accessibles via `execStdout` et `execStderr` ;
- `exec` ne lève jamais d'erreur runtime pour un code de retour non nul — un code non nul est un résultat valide, pas une erreur Cimple ;
- `exec` lève une **erreur runtime** uniquement si l'exécutable est introuvable, non exécutable, ou si le système ne peut pas créer le sous-processus ;
- **portabilité** : `exec` est disponible sur macOS, Linux, Unix et Windows ; `exec` n'est pas disponible sous WebAssembly (erreur runtime) ; l'implémentation utilise `CreateProcess` sur Windows et `posix_spawn` ou `fork`/`execvp` sur les systèmes POSIX ; cette différence est transparente pour le code Cimple ; le comportement observable (capture de stdout/stderr, code de retour, héritage d'environnement) est identique sur toutes les plateformes.

#### `exec`

```c
ExecResult exec(string[] command, string[] env)
```

Exécute la commande décrite par `command` et retourne un `ExecResult`.

Règles normatives :

- `command` doit contenir au moins un élément ; `command[0]` est le chemin ou le nom de l'exécutable ; les éléments suivants sont les arguments passés au processus, dans l'ordre ;
- `command` vide (`count(command) == 0`) est une **erreur runtime** ;
- `env` est un tableau de chaînes de la forme `"NOM=VALEUR"` ; chaque entrée ajoute ou surcharge la variable d'environnement `NOM` pour le sous-processus ; les autres variables héritées ne sont pas affectées ;
- `env` peut être `[]` pour n'ajouter aucune variable ;
- une entrée de `env` ne respectant pas le format `"NOM=VALEUR"` (pas de `=`, nom vide) est une **erreur runtime** ;
- `exec` attend la terminaison complète du sous-processus avant de retourner ;
- stdout et stderr sont capturés intégralement pendant l'exécution, y compris si le processus produit une sortie volumineuse ;
- si l'exécutable est introuvable ou non exécutable, c'est une **erreur runtime** avec message clair indiquant le nom de la commande.

```c
ExecResult r = exec(["git", "status", "--short"], []);
```

```c
ExecResult r = exec(["python3", "script.py"], ["DEBUG=1", "LANG=fr_FR.UTF-8"]);
```

#### `execStatus`

```c
int execStatus(ExecResult result)
```

Retourne le code de sortie du sous-processus. `0` indique conventionnellement un succès ; toute autre valeur indique une erreur ou un état particulier selon la convention de la commande exécutée.

```c
int code = execStatus(r);
if (code != 0) {
    print("Échec avec code " + toString(code) + "\n");
}
```

#### `execStdout`

```c
string execStdout(ExecResult result)
```

Retourne l'intégralité de la sortie standard (stdout) produite par le sous-processus, sous forme de `string` UTF-8. Si le sous-processus n'a rien écrit sur stdout, retourne `""`.

```c
string out = execStdout(r);
print(out);
```

#### `execStderr`

```c
string execStderr(ExecResult result)
```

Retourne l'intégralité de la sortie d'erreur (stderr) produite par le sous-processus, sous forme de `string` UTF-8. Si le sous-processus n'a rien écrit sur stderr, retourne `""`.

```c
string err = execStderr(r);
if (len(err) > 0) {
    print("Stderr : " + err);
}
```

### 9.12 Environnement du processus

#### `getEnv`

```c
string getEnv(string name, string fallback)
```

Lit la valeur de la variable d'environnement `name` dans l'environnement du processus Cimple courant. Si la variable n'existe pas, retourne `fallback`.

Règles normatives :

- si la variable `name` existe dans l'environnement et a une valeur (y compris vide `""`), retourne cette valeur telle quelle ;
- si la variable `name` n'existe pas dans l'environnement, retourne `fallback` ;
- une variable définie mais vide (`export FOO=`) retourne `""` et non `fallback` — la variable **existe**, elle est simplement vide ; si l'utilisateur doit traiter ce cas, il peut tester `len(getEnv("FOO", "")) == 0` ;
- `name` doit être de type `string` ; tout autre type est une erreur sémantique ;
- `fallback` doit être de type `string` ; tout autre type est une erreur sémantique ;
- `getEnv` ne lève jamais d'erreur runtime ; toute condition d'inaccessibilité retourne `fallback` ;
- `getEnv` lit l'environnement du processus Cimple au moment de l'appel ; il ne lit pas les variables ajoutées via le paramètre `env` de `exec` (celles-ci sont transmises uniquement au sous-processus).

```c
string home    = getEnv("HOME", "/tmp");
string debug   = getEnv("DEBUG", "0");
string port    = getEnv("PORT", "8080");
string lang    = getEnv("LANG", "en_US.UTF-8");

if (debug == "1") {
    print("Mode debug activé\n");
}

print("Port : " + port + "\n");
```

### 9.13 Temps et dates

Toutes les fonctions de cette section opèrent en **UTC**. Les fuseaux horaires locaux ne sont pas pris en charge ; si un offset est nécessaire, l'utilisateur l'applique manuellement en ajustant l'epoch.

#### `now`

```c
int now()
```

Retourne l'instant courant sous forme d'**epoch Unix en millisecondes** (nombre de millisecondes écoulées depuis le 1er janvier 1970 00:00:00 UTC). La valeur est toujours positive et tient dans un `int` (`int64_t`).

Règles normatives :

- ne prend aucun argument ; tout argument est une erreur sémantique ;
- retourne un `int` ; ne lève jamais d'erreur runtime ;
- la précision effective dépend de la plateforme (en général millisecondes ou mieux) ;
- pour obtenir l'epoch en secondes entières : `now() / 1000`.

```c
int t0 = now();
// ... traitement ...
int t1 = now();
int dureeMs = t1 - t0;
print("Durée : " + toString(dureeMs) + " ms\n");
```

#### `epochToYear` `epochToMonth` `epochToDay` `epochToHour` `epochToMinute` `epochToSecond`

```c
int epochToYear(int epochMs)
int epochToMonth(int epochMs)
int epochToDay(int epochMs)
int epochToHour(int epochMs)
int epochToMinute(int epochMs)
int epochToSecond(int epochMs)
```

Décomposent un epoch en millisecondes en ses composants de calendrier UTC.

| Fonction | Plage de valeurs retournées |
|---|---|
| `epochToYear` | année entière (ex. `2025`) |
| `epochToMonth` | `1` (janvier) – `12` (décembre) |
| `epochToDay` | `1` – `31` (jour du mois) |
| `epochToHour` | `0` – `23` |
| `epochToMinute` | `0` – `59` |
| `epochToSecond` | `0` – `59` |

Règles normatives :

- `epochMs` doit être de type `int` ; tout autre type est une erreur sémantique ;
- une valeur négative de `epochMs` est autorisée (dates antérieures au 1er janvier 1970) ;
- les millisecondes sont ignorées dans la décomposition ; `epochToSecond` retourne les secondes entières, pas les millisecondes résiduelles.

```c
int ts = now();
print(toString(epochToYear(ts))   + "\n");   // ex. "2025"
print(toString(epochToMonth(ts))  + "\n");   // ex. "3"
print(toString(epochToDay(ts))    + "\n");   // ex. "11"
print(toString(epochToHour(ts))   + "\n");   // ex. "14"
print(toString(epochToMinute(ts)) + "\n");   // ex. "32"
print(toString(epochToSecond(ts)) + "\n");   // ex. "7"

// Obtenir l'année et le mois courants
int annee = epochToYear(now());
int mois  = epochToMonth(now());
```

#### `makeEpoch`

```c
int makeEpoch(int year, int month, int day,
              int hour, int minute, int second)
```

Construit un epoch Unix en millisecondes à partir de composants de calendrier UTC. Les millisecondes sont toujours `0` en sortie (précision à la seconde).

Règles normatives :

- tous les arguments doivent être de type `int` ; tout autre type est une erreur sémantique ;
- les composants doivent être valides : `month` dans `[1, 12]`, `day` dans `[1, joursduMois]`, `hour` dans `[0, 23]`, `minute` dans `[0, 59]`, `second` dans `[0, 59]` ;
- si un composant est hors plage, `makeEpoch` retourne `-1` — valeur sentinelle indiquant un échec ; l'utilisateur doit tester le résultat avant usage ;
- `year` peut être négatif (dates avant l'an 0) ou supérieur à 9999 ; aucune limite calendaire n'est imposée au-delà de la plage de `int`.

```c
int ts = makeEpoch(2025, 3, 11, 0, 0, 0);
if (ts == -1) {
    print("Date invalide\n");
} else {
    print("Epoch : " + toString(ts) + "\n");
}

// Calcul d'un délai : dans 30 jours
int dans30j = makeEpoch(
    epochToYear(now()),
    epochToMonth(now()),
    epochToDay(now()) + 30,
    0, 0, 0
);
// Note : +30 jours peut dépasser le mois — makeEpoch retourne -1 dans ce cas
// Pour un calcul robuste, utiliser : now() + 30 * 24 * 3600 * 1000
```

#### `formatDate`

```c
string formatDate(int epochMs, string fmt)
```

Formate un epoch en millisecondes en chaîne de caractères selon le gabarit `fmt`, exprimé en UTC.
Format des lettres aligné sur PHP (`date()`). Un `\` devant une lettre la copie littéralement.

**Lettres reconnues dans `fmt` :**

| Lettre | Description | Plage / Format | Exemple |
|---|---|---|---|
| `Y` | Année sur 4 chiffres | `0000`–`9999` | `2025` |
| `m` | Mois sur 2 chiffres, zéro-paddé | `01`–`12` | `03` |
| `d` | Jour du mois sur 2 chiffres, zéro-paddé | `01`–`31` | `11` |
| `H` | Heure sur 2 chiffres (24 h), zéro-paddé | `00`–`23` | `14` |
| `i` | Minutes sur 2 chiffres, zéro-paddé | `00`–`59` | `32` |
| `s` | Secondes sur 2 chiffres, zéro-paddé | `00`–`59` | `07` |
| `w` | Jour de la semaine, numérique | `0` (dimanche) – `6` (samedi) | `2` |
| `z` | Jour de l'année, base 0, sans padding | `0`–`365` | `69` |
| `W` | Numéro de semaine ISO 8601, zéro-paddé | `01`–`53` | `11` |
| `c` | Date et heure complètes ISO 8601 en UTC | `Y-m-dTH:i:sZ` | `2025-03-11T14:32:07Z` |

Tout autre caractère dans `fmt` est copié tel quel. Un `\` suivi d'une lettre copie la lettre littéralement.

**Détail des lettres :**

- **`w`** — entier non paddé, convention POSIX : `0` = dimanche, `1` = lundi, …, `6` = samedi ;

- **`z`** — jour de l'année depuis `0` : 1er janvier = `0`, 31 décembre d'une année non bissextile = `364`, 31 décembre d'une année bissextile = `365` ; non paddé ;

- **`W`** — numéro de semaine ISO 8601, zéro-paddé sur 2 chiffres ; semaines commençant le lundi ; la semaine contenant le premier jeudi de l'année est la semaine `01` ; les premiers/derniers jours d'une année peuvent appartenir à la semaine de l'année précédente ou suivante ;

- **`c`** — représentation complète ISO 8601 en UTC `Y-m-dTH:i:sZ` ; retourne `"invalid"` si l'année est hors de `[0, 9999]`.

Règles normatives :

- `epochMs` doit être de type `int` ; `fmt` doit être de type `string` ; tout autre type est une erreur sémantique ;
- les lettres sont sensibles à la casse ;
- une lettre non reconnue est copiée telle quelle sans erreur ;
- `\X` copie `X` littéralement quelle que soit la lettre ;
- `fmt` peut être vide `""` : retourne `""` ;
- ne lève pas d'erreur runtime sur un epoch négatif ; formate la date correspondante, sauf `c` qui retourne `"invalid"` si l'année est hors de `[0, 9999]`.

```c
int ts = now();  // ex. 2025-03-11 14:32:07 UTC, mardi

string date  = formatDate(ts, "Y-m-d");              // "2025-03-11"
string heure = formatDate(ts, "H:i:s");              // "14:32:07"
string dt    = formatDate(ts, "Y-m-d H:i:s");        // "2025-03-11 14:32:07"
string slash = formatDate(ts, "d/m/Y");              // "11/03/2025"
string log   = formatDate(ts, "[Y-m-d H:i:s]");      // "[2025-03-11 14:32:07]"
string dow   = formatDate(ts, "w");                  // "2"  (mardi)
string doy   = formatDate(ts, "z");                  // "69" (70e jour, base 0)
string week  = formatDate(ts, "W");                  // "11" (semaine ISO 11)
string iso   = formatDate(ts, "c");                  // "2025-03-11T14:32:07Z"
string isowd = formatDate(ts, "Y-W-w");              // "2025-11-2" (date semaine ISO)
string lit   = formatDate(ts, "\Y\e\a\r: Y");        // "Year: 2025" (échappement)

// Horodatage de log
print(formatDate(now(), "[Y-m-d H:i:s] ") + "Démarrage\n");
```

---

### 9.x Utilitaires

#### `assert`

```c
void assert(bool condition)
void assert(bool condition, string message)
```

Arrête immédiatement l'exécution si `condition` est `false`. Affiche sur `stderr` :

```
[ASSERTION FAILED] <message> (line N)   // forme à 2 arguments
[ASSERTION FAILED] at line N            // forme à 1 argument
```

puis termine le processus avec le code de sortie `1`.

Règles normatives :

- `condition` doit être de type `bool` ;
- `message` (optionnel) doit être de type `string` ;
- l'appel avec 1 ou 2 arguments est valide ; tout autre nombre est une erreur sémantique ;
- `assert` n'est pas désactivable (pas de mode « release » sans assertions).

```c
assert(x > 0);
assert(count(items) > 0, "items ne peut pas être vide");
```

#### `randInt`

```c
int randInt(int min, int max)
```

Retourne un entier pseudo-aléatoire uniformément distribué dans l'intervalle **fermé** `[min, max]`.

Règles normatives :

- `min > max` est une **erreur runtime** ;
- `min == max` retourne `min` ;
- le générateur est initialisé une seule fois au démarrage du programme (via `srand(time(NULL))`).

```c
int d6    = randInt(1, 6);
int coin  = randInt(0, 1);
int fixed = randInt(7, 7);   // toujours 7
```

#### `randFloat`

```c
float randFloat()
```

Retourne un `float` pseudo-aléatoire uniformément distribué dans l'intervalle **semi-ouvert** `[0.0, 1.0)`.

```c
float r = randFloat();          // ex. 0.7341...
float s = 10.0 * randFloat();   // dans [0.0, 10.0)
```

#### `sleep`

```c
void sleep(int ms)
```

Suspend l'exécution du processus pendant `ms` millisecondes.

Règles normatives :

- `ms` doit être de type `int` ;
- `sleep` n'est **pas disponible sur les cibles WebAssembly** ; son appel produit une **erreur runtime** sur cette plateforme ;
- les implémentations POSIX utilisent `usleep(ms * 1000)` ; Windows utilise `Sleep(ms)`.

```c
sleep(500);    // attend 500 ms
sleep(1000);   // attend 1 s
```

---

### 9.14 Expressions régulières (`RegExp`)

#### 9.14.1 Types opaques

Cimple expose deux types opaques pour les expressions régulières, sur le modèle de `ExecResult` :

- `RegExp` — expression régulière compilée, produite uniquement par `regexCompile()`.
- `RegExpMatch` — résultat d'une recherche, produit uniquement par `regexFind()` ou `regexFindAll()`.

Règles normatives :

- `RegExp` et `RegExpMatch` ne peuvent pas être déclarés **sans valeur initiale** ; ils ne disposent pas de littéral.
- `RegExp[]` n'existe pas (erreur sémantique).
- `RegExpMatch[]` est un type valide **uniquement** comme type d'une variable initialisée par `regexFindAll()` ; il ne peut pas être déclaré vide ni construit par un littéral `[]`.

```c
RegExp re = regexCompile("\w+", "");              // OK
RegExpMatch m = regexFind(re, "hello", 0);        // OK
RegExpMatch[] ms = regexFindAll(re, "a b", 0, -1); // OK — seul usage autorisé de RegExpMatch[]

RegExp re2;                                        // ERREUR — pas de valeur initiale
RegExpMatch[] bad = [];                            // ERREUR — littéral vide interdit
```

#### 9.14.2 Compilation

```c
RegExp regexCompile(string pattern, string flags)
```

- Compile `pattern` avec `flags`.
- `flags` est une chaîne contenant zéro ou plusieurs des caractères suivants (ordre indifférent, doublons ignorés) :
  - `"i"` — insensible à la casse (ASCII A-Z ↔ a-z en V1)
  - `"m"` — multiline (`^` et `$` agissent par ligne)
  - `"s"` — dotall (`.` inclut `\n`)
- Tout autre flag est une **erreur runtime** `RegExpSyntax`.
- Toute erreur de syntaxe dans le motif est une **erreur runtime** `RegExpSyntax` avec message clair.

```c
string regexPattern(RegExp re)   // motif original
string regexFlags(RegExp re)     // flags canoniques, ex. "ims"
```

#### 9.14.3 Recherche

```c
bool          regexTest(RegExp re, string input, int start)
RegExpMatch   regexFind(RegExp re, string input, int start)
RegExpMatch[] regexFindAll(RegExp re, string input, int start, int max)
```

- `start` est un indice en **glyphes** dans `[0 .. glyphLen(input)]` ; hors borne → erreur runtime `RegExpRange`.
- `max` dans `regexFindAll` suit la règle unifiée : `-1` = illimité, `0` = retourne `[]`, `> 0` = limite ; `< -1` → erreur `RegExpRange`.
- Règle anti-boucle (`findAll`) : si un match a longueur nulle (`end == start`), la recherche suivante reprend à `end + 1` glyphe.

#### 9.14.4 Remplacement

```c
string regexReplace(RegExp re, string input, string replacement, int start)
string regexReplaceAll(RegExp re, string input, string replacement, int start, int max)
```

- `regexReplace` remplace la **première** occurrence à partir de `start` ; si aucun match, retourne `input` inchangée.
- `regexReplaceAll` remplace toutes les occurrences (non chevauchantes) ; `max` suit la règle unifiée ; `max == 0` → retourne `input`.
- Syntaxe de `replacement` :
  - `$0` : match complet
  - `$1` .. `$99` : groupes capturants (groupe inexistant → `""`)
  - `$$` → `$` littéral
  - `$X` où `X` n'est pas un chiffre → `$X` littéral

#### 9.14.5 Découpe

```c
string[] regexSplit(RegExp re, string input, int start, int maxParts)
```

- `maxParts` suit la règle unifiée : `-1` = illimité, `0` = `[]`, `1` = `[subString(input, start)]`, `> 1` = au plus `maxParts` éléments.
- Règle anti-boucle match vide : identique à `findAll`.

#### 9.14.6 Interrogation d'un `RegExpMatch`

```c
bool     regexOk(RegExpMatch m)
int      regexStart(RegExpMatch m)    // indice en glyphes (0 si ok=false)
int      regexEnd(RegExpMatch m)      // indice en glyphes (0 si ok=false)
string[] regexGroups(RegExpMatch m)   // [0]=match complet, [1..n]=groupes capturants
```

- `regexGroups(m)[0]` est toujours le match complet si `ok=true`.
- Un groupe capturant optionnel non matché produit `""`.

#### 9.14.7 Syntaxe du motif (V1)

| Catégorie | Syntaxe |
|---|---|
| Littéral | tout glyphe hors métacaractère |
| N'importe quel glyphe | `.` (sauf `\n` si flag `s` absent) |
| Ancre | `^` `$` (par ligne si flag `m`) |
| Classe | `[abc]` `[a-z]` `[^...]` |
| Groupe capturant | `(…)` |
| Groupe non capturant | `(?:…)` |
| Alternance | `A\|B\|C` |
| Quantificateurs greedy | `*` `+` `?` `{m}` `{m,}` `{m,n}` |
| Quantificateurs non-greedy | `*?` `+?` `??` `{m}?` `{m,}?` `{m,n}?` |
| Raccourcis de classes | `\d` `\D` `\w` `\W` `\s` `\S` |

Contraintes :
- `m` et `n` dans `{m,n}` sont des entiers décimaux, `0 ≤ m ≤ n` ; valeurs hors plage → `RegExpLimit`.
- Constructions **interdites** : backreferences `\1`, lookahead/lookbehind `(?=…)` `(?!…)` `(?<=…)` `(?<!…)`.
- Moteur **O(n)** garanti (Thompson NFA / DFA hybride) — pas d'explosion exponentielle.

#### 9.14.8 Erreurs runtime

| Catégorie | Déclencheur |
|---|---|
| `RegExpSyntax` | motif invalide, flag inconnu |
| `RegExpLimit` | quantificateur trop grand, profondeur de compilation dépassée |
| `RegExpRange` | `start < 0`, `start > glyphLen(input)`, `max < -1`, `maxParts < -1` |

#### 9.14.9 Exemples

```c
// Test simple — les raccourcis \d, \s s'écrivent sans double-backslash
RegExp digits = regexCompile("\d+", "");
bool found = regexTest(digits, "prix: 42", 0);    // true

// Groupes capturants
RegExp date = regexCompile("(\d{2})/(\d{2})/(\d{4})", "");
RegExpMatch m = regexFind(date, "Date: 31/12/2026", 0);
if (regexOk(m)) {
    string[] g = regexGroups(m);
    print(g[3] + "-" + g[2] + "-" + g[1] + "\n");  // 2026-12-31
}

// FindAll
RegExp word = regexCompile("\w+", "");
RegExpMatch[] all = regexFindAll(word, "un deux trois", 0, -1);
for (int i = 0; i < count(all); i++) {
    print(regexGroups(all[i])[0] + "\n");  // un / deux / trois
}

// Remplacement avec références
RegExp swap = regexCompile("(\w+)-(\w+)", "");
string result = regexReplaceAll(swap, "alpha-beta gamma-delta", "$2:$1", 0, -1);
print(result + "\n");  // beta:alpha delta:gamma

// Découpe
RegExp sep = regexCompile(",\s*", "");
string[] parts = regexSplit(sep, "a, b,  c", 0, -1);
// parts = ["a", "b", "c"]
```

---

## 10. Instructions prises en charge

### 10.1 Notion de statement

Un **statement** est une instruction exécutable unique. Il peut s'agir notamment de :

- un bloc ;
- une déclaration scalaire ou tableau ;
- une affectation scalaire ;
- une affectation par indexation de tableau (`array[i] = value`) ;
- un appel de fonction utilisé comme instruction ;
- une conditionnelle ;
- une boucle `while` ;
- une boucle `for` ;
- `break` ;
- `continue` ;
- `return`.

Règles générales :

- l'affectation scalaire et l'affectation par indexation sont des **instructions distinctes** et non des expressions générales ;
- les expressions sans effet utile ne doivent pas être acceptées comme instructions autonomes ;
- une déclaration de variable n'est pas autorisée comme corps simple d'un `if`, `else`, `while` ou `for` sans bloc.

### 10.2 Bloc

```c
{
    int x = 1;
    print("ok");
}
```

### 10.3 Instruction d'expression

```c
add(1, 2);
```

### 10.4 Déclaration

```c
int x = 10;
```

### 10.5 Affectation

```c
x = 20;
```

### 10.6 Conditionnelle

```c
if (x > 5) {
    print("ok");
} else {
    print("no");
}
```

Le corps d'un `if` ou d'un `else` peut être soit :

- un bloc `{ ... }` ;
- une instruction simple unique sans accolades.

Exemples valides :

```c
if (x > 5) print("ok");
if (x > 5) x = 2;
if (x > 5)
    print("ok");
else
    print("no");
```

Règles normatives :

- en l'absence de bloc, une seule instruction appartient au corps ;
- le `else` s'associe toujours au `if` non apparié le plus proche ;
- les blocs restent fortement recommandés dès que le corps n'est pas trivial ou en cas d'imbrication.

### 10.7 Boucle `while`

```c
while (x < 10) {
    x = x + 1;
}
```

Le corps peut être soit un bloc, soit une instruction simple unique sans accolades.

Exemples valides :

```c
while (x < 10) x = x + 1;
while (ready) print("go");
```

### 10.8 Boucle `for`

Syntaxe retenue :

```c
for (int i = 0; i < 10; i++) {
    print("ok");
}
```

Le corps peut être soit un bloc, soit une instruction simple unique sans accolades.

Exemples valides :

```c
for (int i = 0; i < 10; i++) print("ok");
for (int i = 0; i < 10; i++) x = x + i;
```

Règles normatives de la boucle `for` :

- la partie **initialisation** est obligatoire et doit être une déclaration de variable locale de type `int` avec initialisation (ex. `int i = 0`) ; tout autre type que `int` dans `for_init` est une **erreur sémantique** ;
- la variable déclarée dans l'initialisation n'est visible qu'à l'intérieur du `for` ; elle n'est pas accessible après la boucle ;
- la partie **condition** est obligatoire et doit être une expression de type `bool` ;
- la partie **mise à jour** est obligatoire et doit être une affectation simple (ex. `i = i + 1`) ou une instruction d'incrémentation/décrémentation (ex. `i++`, `i--`) ;
- les trois parties sont séparées par des `;` ;
- aucune des trois parties ne peut être omise.

### 10.9 Contrôle de boucle

```c
break;
continue;
```

Règles :

- `break` termine immédiatement la boucle courante (`while` ou `for`) ;
- `continue` passe immédiatement à l'itération suivante de la boucle courante ;
- `break` et `continue` sont valides uniquement à l'intérieur d'une boucle ;
- leur utilisation en dehors d'une boucle doit produire une erreur sémantique claire ;
- ils peuvent apparaître comme instruction simple unique dans un corps de boucle sans accolades ;
- `break` et `continue` s'appliquent toujours à la boucle la plus proche.

### 10.10 Retour

```c
return 0;
return;
```

---

## 11. Expressions

### 11.1 Littéraux

Cimple inclut les littéraux suivants :

- entiers : décimaux, hexadécimaux, binaires, octaux
- flottants : décimaux avec partie fractionnaire et/ou exposant
- booléens : `true`, `false`
- chaînes : séquences entre guillemets doubles avec séquences d'échappement
- tableaux : `[1, 2, 3]`, `[0.1, 0.2]`, `[true, false]`, `["a", "b"]`

#### Littéraux entiers

Quatre bases sont supportées :

| Base | Préfixe | Exemple | Valeur décimale |
|---|---|---|---|
| Décimale | aucun | `42` | 42 |
| Hexadécimale | `0x` ou `0X` | `0x2A` | 42 |
| Binaire | `0b` ou `0B` | `0b101010` | 42 |
| Octale | `0` suivi de chiffres octaux | `052` | 42 |

Règles normatives :

- les littéraux hexadécimaux acceptent les chiffres `0`–`9` et les lettres `a`–`f` / `A`–`F` ;
- les littéraux binaires acceptent uniquement `0` et `1` ;
- les littéraux octaux commencent par `0` suivi d'au moins un chiffre octal (`0`–`7`) ; le littéral `0` seul est un décimal valant zéro ;
- le signe `-` n'est **pas** inclus dans le littéral ; c'est l'opérateur unaire `-` appliqué à un littéral positif ;
- tout littéral entier représente une valeur positive ou nulle ; les valeurs négatives s'écrivent `-5`, `-(0x2A)` etc.

```c
int a = 10;       // décimal
int b = 0x2A;     // hexadécimal → 42
int c = 0b101010; // binaire → 42
int d = 052;      // octal → 42
int e = -5;       // unaire '-' appliqué au littéral 5
```

#### Littéraux flottants

Deux formes sont supportées :

- **décimale** : partie entière, point décimal, partie fractionnaire optionnelle — ex. `1.5`, `3.0`, `0.001` ;
- **scientifique** : décimale ou entière suivie d'un exposant `e` ou `E`, signe optionnel, partie entière — ex. `1e3`, `1.5e-2`, `2.0E+10`.

```c
float f1 = 1.5;
float f2 = 1e-3;    // 0.001
float f3 = 2.5e4;   // 25000.0
float f4 = 1.0E+2;  // 100.0
```

Règles normatives :

- un littéral flottant doit contenir soit un point décimal, soit un exposant, soit les deux ; un entier nu comme `1` est toujours de type `int`, jamais `float` ;
- la partie fractionnaire après le point peut être absente : `1.` est valide et vaut `1.0` ;
- l'exposant seul sans point est valide : `1e3` est un `float` ;
- le signe `-` n'est pas inclus dans le littéral ; c'est l'opérateur unaire.

#### Littéraux de chaîne

Un littéral de chaîne est une séquence de caractères entre guillemets doubles `"..."`.

Les séquences d'échappement suivantes sont supportées :

| Séquence | Caractère produit | Point de code |
|---|---|---|
| `\"` | guillemet double | U+0022 |
| `\\` | barre oblique inverse | U+005C |
| `\n` | saut de ligne | U+000A |
| `\t` | tabulation horizontale | U+0009 |
| `\r` | retour chariot | U+000D |
| `\b` | retour arrière | U+0008 |
| `\f` | saut de page | U+000C |
| `\uXXXX` | point de code Unicode | U+XXXX |

Règles normatives :

- `\uXXXX` requiert exactement 4 chiffres hexadécimaux (`0`–`9`, `a`–`f`, `A`–`F`) ; tout autre nombre de chiffres est une erreur lexicale ;
- tout `\X` où `X` n'est **pas** l'un des caractères ci-dessus (et n'est pas `u`) est **préservé littéralement** comme deux octets `\` + `X` ; cela permet d'écrire les raccourcis regex (`\s`, `\d`, `\w`, `\D`, `\W`, `\S`, `\b`, `\B`) directement dans les chaînes sans doubler le backslash ;
- un retour à la ligne littéral dans une chaîne (sans `\n`) est une **erreur lexicale** ;
- les chaînes sont encodées et manipulées en **UTF-8** par le runtime ;
- les points de code `\uXXXX` sont encodés en UTF-8 dans la représentation interne ;
- la représentation interne d'une chaîne est stockée **telle quelle**, sans normalisation Unicode automatique ; deux chaînes visuellement identiques mais de forme NFC et NFD différentes sont stockées différemment et comparées comme inégales par `==` ; `glyphLen` et `glyphAt` normalisent en NFC au moment de l'appel.

```c
string a = "Bonjour\nMonde";   // saut de ligne entre les deux mots
string b = "Chemin : C:\\tmp"; // barre oblique inverse littérale
string c = "Il a dit \"ok\"";  // guillemets dans la chaîne
string d = "\u00E9l\u00E8ve";  // "élève" via points de code Unicode
string e = "\t---\t";          // tabulations
```

#### Littéraux de tableau

Un littéral tableau `[...]` est une liste d'expressions séparées par des virgules, encadrée par `[` et `]`. Un littéral tableau vide `[]` est autorisé ; son type est inféré du type de la variable à laquelle il est assigné à la déclaration. Tous les éléments d'un littéral tableau doivent être du même type ; tout mélange de types est une erreur sémantique.

### 11.2 Identifiants

Les identifiants servent à désigner :

- variables ;
- fonctions.

### 11.3 Appel de fonction

```c
add(1, 2);
print("ok");
```

### 11.4 Parenthèses

```c
(a + b) * c
```

### 11.5 Opérateurs

#### Arithmétiques

- `+`
- `-`
- `*`
- `/`
- `%`

Règles particulières pour `string` :

- `+` est autorisé entre deux `string` pour la concaténation ;
- `+` n'est pas autorisé entre `string` et un autre type sans conversion explicite via `toString(int)`, `toString(float)` ou `toString(bool)`.

#### Comparaison

- `==`
- `!=`
- `<`
- `<=`
- `>`
- `>=`

Règle particulière pour `float` :

- les comparaisons exactes sur `float` sont autorisées ;
- pour les cas numériques sensibles, l'usage de `approxEqual(a, b, epsilon)` est recommandé.

#### Logiques

- `&&`
- `||`
- `!`

#### Bitwise sur `int`

Les opérateurs bitwise suivants sont autorisés sur `int` uniquement :

- `&`
- `|`
- `^`
- `~`
- `<<`
- `>>`

Règles associées :

- ces opérateurs ne s'appliquent qu'à des opérandes de type `int` ;
- ils ne s'appliquent ni à `float`, ni à `bool`, ni à `string` ;
- `~` est un opérateur unaire sur `int` ;
- `<<` et `>>` sont réservés aux entiers.

### 11.6 Restrictions

- `i++` et `i--` sont autorisés uniquement comme **instructions autonomes** (voir section 11.7) ; ils sont interdits comme sous-expressions ;
- pas d'opérateur virgule ;
- pas d'affectation scalaire comme expression ;
- l'affectation par indexation `array[i] = value` est une instruction autonome, pas une expression ;
- pas de type `char` ;
- pas de mutation caractère par caractère dans les chaînes.

### 11.7 Opérateurs d'incrémentation et de décrémentation

Cimple supporte `++` et `--` uniquement sous leur forme **postfixe**, et uniquement comme **instructions autonomes**.

Formes valides :

```c
i++;
i--;
```

Règles normatives :

- `i++` incrémente `i` de `1` ; équivalent à `i = i + 1` ;
- `i--` décrémente `i` de `1` ; équivalent à `i = i - 1` ;
- ces opérateurs ne s'appliquent qu'à des variables de type `int` ;
- les formes préfixes `++i` et `--i` sont **interdites** ; leur usage doit produire une erreur syntaxique ;
- `i++` et `i--` sont **interdits comme sous-expressions** : `x = i++`, `print(toString(i--))` ou toute utilisation dans une expression sont des erreurs sémantiques ;
- ils sont valides comme instruction simple unique dans le corps d'un `if`, `else`, `while` ou `for` sans bloc ;
- la partie **mise à jour** d'une boucle `for` peut utiliser `i++` ou `i--` en remplacement d'une affectation explicite.

Exemples valides :

```c
int i = 0;
i++;          // i vaut 1
i++;          // i vaut 2
i--;          // i vaut 1

for (int j = 0; j < 5; j++) {
    print("tick");
}

while (x > 0) x--;
```

Exemples invalides :

```c
++i;                     // interdit : forme préfixe
int a = i++;             // interdit : i++ utilisé comme expression
print(toString(i++));    // interdit : i++ utilisé comme expression
```

### 11.8 Opérateurs d'affectation composée

Cimple supporte les cinq opérateurs d'affectation composée suivants, applicables aux types `int` et `float` uniquement :

| Opérateur | Équivalent | Types |
|-----------|-----------|-------|
| `x += e`  | `x = x + e` | `int`, `float` |
| `x -= e`  | `x = x - e` | `int`, `float` |
| `x *= e`  | `x = x * e` | `int`, `float` |
| `x /= e`  | `x = x / e` | `int`, `float` |
| `x %= e`  | `x = x % e` | `int` uniquement |

Règles normatives :

- l'opérande gauche doit être un identifiant de variable de type `int` ou `float` ;
- le type de l'expression droite doit correspondre au type de la variable ;
- une division ou un modulo par zéro produit une **erreur runtime** ;
- ces opérateurs sont valides comme instruction autonome, dans le corps d'un `if`/`while`/`for` sans bloc, et dans la clause de mise à jour d'un `for` ;
- ils ne sont **pas** utilisables comme sous-expressions.

```c
int x = 10;
x += 5;    // 15
x -= 3;    // 12
x *= 2;    // 24
x /= 4;    // 6
x %= 4;    // 2

for (int i = 0; i < 100; i += 10) {
    print(toString(i) + "\n");
}
```

### 11.9 Opérateur ternaire `?:`

L'opérateur ternaire est une expression conditionnelle en ligne :

```
condition ? expression_si_vrai : expression_si_faux
```

Règles normatives :

- `condition` doit être de type `bool` ;
- les deux branches doivent être du **même type** ;
- l'évaluation est **paresseuse** (court-circuit) : seule la branche sélectionnée est évaluée ;
- l'opérateur est **associatif à droite**, permettant l'imbrication naturelle ;
- le type résultant est le type commun des deux branches.

```c
string s = (x > 0) ? "positif" : "non positif";
int abs  = (x >= 0) ? x : -x;

// Ternaire imbriqué
string signe = (n > 0) ? "positif" : (n < 0) ? "négatif" : "zéro";
```

### 11.10 Indexation de tableau en lecture

L'expression `array[expr]` retourne la valeur à l'indice `expr` dans le tableau `array`.

- `array` doit être un identifiant de type tableau ;
- `expr` doit être de type `int` ;
- le type de l'expression résultante est le type d'élément du tableau ;
- un indice hors bornes est une **erreur runtime** (voir section 7.3).

```c
int[] scores = [10, 20, 30];
int x = scores[1];           // x vaut 20
print(toString(scores[0]));
```

---

## 12. Priorités et associativité

L'implémentation devra respecter la hiérarchie d'opérateurs suivante.

Ordre du plus fort au plus faible :

1. appels de fonction / parenthèses
2. unaires `!`, unaire `-`, unaire `~`
3. `*`, `/`, `%`
4. `+`, `-`
5. `<<`, `>>`
6. `<`, `<=`, `>`, `>=`
7. `==`, `!=`
8. `&`
9. `^`
10. `|`
11. `&&`
12. `||`
13. `?:` (associatif à droite)
14. `=`, `+=`, `-=`, `*=`, `/=`, `%=` (associatifs à droite — instructions seulement)

Associativité :

- la plupart des opérateurs binaires sont associatifs à gauche ;
- `?:` et les opérateurs d'affectation sont associatifs à droite.

L'affectation et les affectations composées sont des instructions distinctes et non des expressions générales.

---

## 13. Portée et symboles

### 13.1 Portée lexicale

Cimple utilise une **portée lexicale par blocs**.

Exemple :

```c
int x = 1;
if (true) {
    int y = 2;
}
```

Ici `y` n'existe qu'à l'intérieur du bloc `if`.

### 13.2 Table des symboles

L'analyse sémantique doit au minimum vérifier :

- redéclaration interdite dans une même portée ;
- utilisation d'un identifiant non déclaré ;
- appel d'une fonction inexistante ;
- incohérence des types ;
- nombre d'arguments incorrect ;
- type d'argument incorrect.

---

## 14. Mots-clés

Mots-clés réservés :

- `int`
- `float`
- `bool`
- `string`
- `byte`
- `void`
- `ExecResult`
- `if`
- `else`
- `while`
- `for`
- `return`
- `break`
- `continue`
- `true`
- `false`
- `import`
- `structure`
- `clone`
- `self`
- `super`

`ExecResult` est un mot-clé réservé désignant le type opaque de résultat d'exécution (voir section 6.3). Il ne peut pas être utilisé comme nom de variable, de fonction ou de paramètre.

Les identifiants suivants sont des **constantes prédéfinies réservées** (voir section 7.6) : `INT_MAX`, `INT_MIN`, `INT_SIZE`, `FLOAT_SIZE`, `FLOAT_DIG`, `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX`, `M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10`. Ils ne peuvent pas être utilisés comme noms de variables, de fonctions ou de paramètres.

`print`, `input`, `len`, `byteAt`, `glyphLen`, `glyphAt`, `toString`, `toInt`, `toFloat`, `toBool`, `exec`, `execStatus`, `execStdout`, `execStderr`, `getEnv`, `readFile`, `writeFile`, `appendFile`, `fileExists`, `readLines`, `writeLines` et les autres fonctions natives ne sont **pas** des mots-clés : ce sont des fonctions connues du runtime.

---

## 15. Commentaires

Cimple supporte :

- commentaires de ligne : `// commentaire`
- commentaires bloc : `/* ... */`

---

## 16. Lexer re2c

### 16.1 Exigence explicite

Le lexer de Cimple doit être généré avec **re2c**. re2c compile les règles lexicales en un automate fini déterministe optimal directement en C, sans tables d'exécution ni backtracking. C'est l'outil utilisé par PHP, MariaDB et Ninja pour leurs lexers.

### 16.2 Tokens à reconnaître

Le lexer doit reconnaître et produire les tokens suivants :

**Identifiants et mots-clés**

- identifiants : séquence de lettres, chiffres et `_`, commençant par une lettre ou `_`
- mots-clés réservés (voir section 14) : `int`, `float`, `bool`, `string`, `byte`, `void`, `ExecResult`, `if`, `else`, `while`, `for`, `return`, `break`, `continue`, `true`, `false`, `import`, `structure`, `clone`, `self`, `super`
- les mots-clés sont reconnus dans le même scanner que les identifiants ; la distinction est faite dans l'action C associée par comparaison de la valeur lexicale

**Littéraux entiers**

- décimal : `[1-9][0-9]*` ou `0`
- hexadécimal : `0[xX][0-9a-fA-F]+`
- binaire : `0[bB][01]+`
- octal : `0[0-7]+`
- erreur lexicale : `0[xX]` sans chiffre, `0[bB]` sans chiffre

**Littéraux flottants**

- forme de base : `[0-9]+\.[0-9]*` (ex. `1.`, `1.5`)
- forme scientifique : `[0-9]+(\.[0-9]*)?[eE][+-]?[0-9]+` (ex. `1e3`, `1.5e-2`)
- un entier nu sans point ni exposant est toujours `INT_LITERAL`

**Littéraux chaînes**

- délimiteurs : `"`
- séquences d'échappement reconnues : `\"`, `\\`, `\n`, `\t`, `\r`, `\b`, `\f`, `\uXXXX`
- erreur lexicale : retour à la ligne littéral dans une chaîne, `\uXXXX` avec un nombre de chiffres hexadécimaux différent de 4
- `\X` inconnu (hors `\"`, `\\`, `\n`, `\t`, `\r`, `\b`, `\f`, `\uXXXX`) : **préservé littéralement** comme `\` + `X` (utile pour les motifs regex)

**Opérateurs**

`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `=`, `++`, `--`

**Ponctuation**

`(`, `)`, `{`, `}`, `[`, `]`, `,`, `;`

**Espaces et commentaires**

- espaces, tabulations, retours à la ligne : ignorés, non transmis à Lemon
- commentaires `//` jusqu'en fin de ligne : ignorés
- commentaires `/* ... */` (potentiellement multi-lignes) : ignorés

### 16.3 Intégration re2c / Lemon

L'interface entre re2c et Lemon suit le modèle push : le lexer produit un token, l'identifiant du token et sa valeur sont passés à `Parse()` de Lemon, puis le lexer recommence.

**Structure de la boucle d'intégration :**

```c
// Boucle principale dans le driver
while (1) {
    TokenValue val;
    int token_id = next_token(&lexer_state, &val);  // appel re2c
    if (token_id == TOKEN_EOF) {
        Parse(parser, 0, val, &ast);
        break;
    }
    Parse(parser, token_id, val, &ast);
}
```

**La valeur de token (`TokenValue`) doit porter :**

```c
typedef struct {
    const char *text;   // pointeur dans le buffer source (ou copie)
    int         len;    // longueur en octets
    int         line;   // numéro de ligne (base 1)
    int         col;    // colonne (base 1)
    const char *file;   // chemin du fichier source d'origine (pour les messages d'erreur multi-fichiers)
} TokenValue;
```

La position (`line`, `col`) doit être maintenue par le lexer re2c et propagée à chaque token pour permettre des messages d'erreur précis dans toutes les phases. Le champ `file` contient le chemin du fichier source tel que passé à `cimple run` pour le fichier racine, ou le chemin résolu du fichier importé pour les tokens issus d'un `import` ; il permet d'afficher dans les messages d'erreur le fichier d'origine réel plutôt que le fichier fusionné.

**Gestion du buffer source :**

re2c opère sur un buffer de caractères en mémoire. La stratégie recommandée pour Cimple est de charger l'intégralité du fichier source en mémoire avant de démarrer le lexer (`readFile` en C), puis de faire pointer re2c sur ce buffer. Cela simplifie la gestion de `YYFILL` (qui peut être laissé vide ou en mode `--no-generation-date --storable-state` désactivé).

**Déclarations re2c minimales :**

```c
/*!re2c
    re2c:define:YYCTYPE  = char;
    re2c:define:YYCURSOR = cursor;
    re2c:define:YYMARKER = marker;
    re2c:yyfill:enable   = 0;   // buffer complet en mémoire
*/
```

### 16.4 Gestion des erreurs lexicales

Toute séquence non reconnue est une erreur lexicale fatale. Le lexer doit :

- afficher un message indiquant la nature de l'erreur, la ligne et la colonne ;
- arrêter l'analyse immédiatement (pas de récupération sur erreur lexicale).

Cas d'erreurs lexicales normatives (voir aussi section 21.1) :

- `\` non suivi d'une séquence d'échappement reconnue dans une chaîne ;
- `\uXXXX` avec un nombre de chiffres hexadécimaux différent de 4 ;
- retour à la ligne littéral dans une chaîne ;
- `0x` ou `0b` sans chiffre suivant ;
- tout caractère non appartenant à la grammaire.

### 16.5 Gestion du suivi de position

re2c ne maintient pas automatiquement les numéros de ligne et de colonne. Le lexer doit les calculer manuellement dans les actions C :

```c
// À chaque token reconnu, enregistrer la position de début
// À chaque '\n' rencontré (y compris dans les commentaires) :
line++;
col = 1;
// Sinon :
col += (int)(cursor - token_start);
```

La position de début du token courant (`token_start`) est capturée au début de chaque itération de la boucle re2c, avant d'avancer `YYCURSOR`.

### 16.6 Règles de priorité re2c

re2c résout les ambiguïtés entre règles par ordre de déclaration (première règle gagnante). L'ordre recommandé dans le fichier `.re` est :

1. espaces et retours à la ligne
2. commentaires `//` et `/* */`
3. littéraux chaînes (règle complexe avec état interne)
4. littéraux numériques (flottants avant entiers, pour éviter que `1.5` soit lexé comme `1` puis `.` puis `5`)
5. mots-clés et identifiants (identifiants en dernier, après les mots-clés)
6. opérateurs multi-caractères avant mono-caractères (`==` avant `=`, `<=` avant `<`, `++` avant `+`)
7. ponctuation
8. règle catch-all pour les erreurs lexicales

---

## 17. Analyse syntaxique avec Lemon

### 17.1 Exigence explicite

Le parseur de Cimple devra être généré avec **Lemon de SQLite**.

### 17.2 Attendus

L'implémentation devra :

- écrire un fichier de grammaire Lemon ;
- définir proprement les tokens ;
- construire un AST dans les actions de grammaire ou via des fonctions utilitaires ;
- garder les actions de parsing lisibles ;
- éviter que le parseur contienne toute la logique sémantique.

### 17.3 Recommandations

- utiliser re2c pour le lexer (voir section 16) ;
- utiliser Lemon uniquement pour la syntaxe ;
- construire des nœuds AST explicites ;
- séparer grammaire, AST et sémantique ;
- ne pas mettre de logique sémantique dans les actions Lemon : les actions doivent uniquement allouer et assembler des nœuds AST.

### 17.4 Sortie de parsing

Le résultat du parseur doit être un nœud racine de programme :

- `ProgramNode`
- contenant une liste de fonctions et de déclarations globales

Les variables globales sont autorisées. Elles peuvent être déclarées et initialisées au niveau supérieur.

---

## 18. AST attendu

L'implémentation devra définir un AST explicite avec, au minimum, des nœuds du type :

### 18.1 Programme

- `Program`

### 18.2 Déclarations

- `FunctionDecl`
- `VarDecl` (scalaire ou tableau)
- `Parameter`

### 18.3 Instructions

- `BlockStmt`
- `IfStmt`
- `WhileStmt`
- `ForStmt`
- `ReturnStmt`
- `BreakStmt`
- `ContinueStmt`
- `ExprStmt`
- `AssignStmt` (affectation scalaire, l'affectation n'étant pas une expression)
- `IndexAssignStmt` (affectation par indexation `array[i] = value`)

### 18.4 Expressions

- `BinaryExpr`
- `UnaryExpr`
- `CallExpr`
- `IdentifierExpr`
- `IndexExpr` (lecture par indexation `array[i]`)
- `IntLiteral`
- `FloatLiteral`
- `BoolLiteral`
- `StringLiteral`
- `ArrayLiteral` (littéral tableau `[e1, e2, ...]`)
- `ExecResultExpr` (expression de type `ExecResult`, produite uniquement par un appel à `exec`)

---

## 19. Analyse sémantique attendue

L'analyse sémantique doit vérifier :

1. existence d'un `main` valide dont la signature est l'une des quatre formes autorisées : `int main()`, `void main()`, `int main(string[] args)`, `void main(string[] args)` ; toute autre signature de `main` est une erreur sémantique ;
2. unicité des noms de fonctions ;
3. unicité des variables dans une même portée ;
4. compatibilité des affectations ;
5. validité des retours ;
6. validité des expressions conditionnelles ;
7. validité des appels de fonctions ;
8. validité des opérateurs selon les types ;
9. usage valide de `break` et `continue` uniquement dans une boucle ;
10. que la variable d'initialisation d'une boucle `for` est de type `int` ; tout autre type dans `for_init` est une **erreur sémantique**.

### 19.1 Constantes prédéfinies

L'analyse sémantique doit vérifier :

- que `INT_MAX`, `INT_MIN`, `INT_SIZE`, `FLOAT_SIZE`, `FLOAT_DIG` sont utilisés dans un contexte attendant un `int` ;
- que `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX`, `M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10` sont utilisés dans un contexte attendant un `float` ;
- qu'aucune variable, paramètre ou fonction n'est déclaré avec l'un de ces noms ; toute tentative de déclaration est une **erreur sémantique** ;
- qu'aucune affectation n'est effectuée sur ces identifiants (`M_PI = ...` est une **erreur sémantique**).

### 19.2 Conditions

Les conditions de `if`, `while`, `for` doivent être de type `bool`.

Pas de vérité implicite à la C sur les `int`.

### 19.2c Vérifications spécifiques sur les structures

L'analyse sémantique doit vérifier :

**Déclaration :**
- que le nom d'une structure commence par une lettre majuscule ; un nom en minuscule est une **erreur sémantique** ;
- que les champs de type scalaire (`int`, `float`, `bool`, `string`, `byte`) et tableau sans valeur par défaut explicite reçoivent leur valeur implicite (`0`, `0.0`, `false`, `""`, `0`, `[]`) ; aucune erreur n'est levée dans ce cas ;
- que les champs de type structure ont une valeur par défaut explicite `clone NomDeStructure` ; l'absence de valeur par défaut pour un champ de type structure est une **erreur sémantique** ;
- qu'aucun champ n'est de type récursif **direct** (champ dont le type est la structure elle-même) ni **indirect** (cycle de dépendance : `A` contient un champ de type `B`, `B` contient un champ de type `A`) ; la détection de récursion indirecte requiert la passe de pré-collecte (section 6.8.1) ;
- qu'un champ de type structure référence une structure déjà déclarée (ordre textuel) ; toute violation est une **erreur sémantique** (et non syntaxique — cf. note d'implémentation section 6.8.1) ;
- que la valeur par défaut d'un champ de type structure est `clone NomDeStructure` ;
- qu'il n'y a pas deux champs de même nom dans une structure (héritage inclus, sauf redéfinition valide) ;
- qu'il n'y a pas deux méthodes avec la même signature dans une structure ;

**Héritage :**
- que la base déclarée dans `: NomDeBase` existe et est déclarée avant la sous-structure ;
- qu'il n'y a pas de cycle d'héritage (`A : B, B : A`) ; la détection requiert la passe de pré-collecte (section 6.8.1) ;
- qu'une redéfinition de champ a le **même type** que dans la base ; la valeur par défaut peut différer ; une redéfinition avec un type différent est une **erreur sémantique** ;
- qu'un override de méthode a une signature identique (nom, types des paramètres, type de retour) à celle de la base ; une signature différente est une **erreur sémantique**.

**`clone` :**
- que l'opérande de `clone` est un nom de structure (type) et non une variable ; `clone variable` est une **erreur sémantique** ;
- que le nom de structure passé à `clone` est déclaré.

**`self` :**
- que `self` n'est utilisé qu'à l'intérieur d'une méthode de structure ; utiliser `self` hors d'une méthode est une **erreur sémantique** ;
- que `self = ...` est une **erreur sémantique**.

**`super` :**
- que `super` n'est utilisé qu'à l'intérieur d'une méthode d'une structure qui hérite d'une base ;
- que `super.méthode(...)` référence une méthode existant dans la base directe ;
- que `super.champ` est une **erreur sémantique**.

**Accès membre :**
- que `instance.champ` référence un champ existant dans la structure (ou hérité) ;
- que `instance.méthode(...)` référence une méthode existante avec les bons types d'arguments ;
- que le type de `instance` est bien une structure déclarée.

**Tableaux de structures :**
- que `NomDeStructure[]` est résolu correctement comme type tableau ;
- que les fonctions de tableau (`arrayPush`, etc.) acceptent des éléments du bon type de structure.

### 19.2b Vérifications spécifiques sur `byte`

L'analyse sémantique doit vérifier :

- que les déclarations `byte b = <littéral>` utilisent un littéral entier dans `[0, 255]` ; un littéral hors plage est une **erreur sémantique** ;
- que l'affectation d'une expression ou variable de type `int` à un `byte` est une **erreur sémantique** sauf si c'est un littéral entier dans `[0, 255]` ; la conversion `intToByte()` est requise ;
- que les opérateurs `+`, `-`, `*`, `/`, `%` appliqués à deux `byte` ou à un `byte` et un `int` produisent un résultat de type `int` ;
- que les opérateurs `&`, `|`, `^` appliqués à deux `byte` produisent un résultat de type `byte` ; appliqués à `byte` et `int`, produisent `int` ;
- que `~` appliqué à un `byte` produit un `byte` ;
- que `<<` et `>>` appliqués à un `byte` produisent un `int` ;
- que les opérateurs `&&`, `||`, `!` ne sont pas autorisés sur des opérandes de type `byte` ;
- que `byteToInt` reçoit exactement un argument de type `byte` et retourne `int` ;
- que `intToByte` reçoit exactement un argument de type `int` et retourne `byte` ;
- que `stringToBytes` reçoit exactement un argument de type `string` et retourne `byte[]` ;
- que `bytesToString` reçoit exactement un argument de type `byte[]` et retourne `string` ;
- que `readFileBytes` reçoit un `string` et retourne `byte[]` ;
- que `writeFileBytes` et `appendFileBytes` reçoivent un `string` et un `byte[]` ;
- que `intToBytes` reçoit exactement un argument de type `int` et retourne `byte[]` ;
- que `floatToBytes` reçoit exactement un argument de type `float` et retourne `byte[]` ;
- que `bytesToInt` reçoit exactement un argument de type `byte[]` et retourne `int` ;
- que `bytesToFloat` reçoit exactement un argument de type `byte[]` et retourne `float` ;
- que dans un littéral `byte[]`, chaque élément est soit de type `byte`, soit un littéral entier dans `[0, 255]` ;
- que `b[i] = v` sur un `byte[]` accepte un `byte` ou un littéral entier `[0, 255]` ; affecter une variable `int` est une erreur sémantique.

### 19.3 Vérifications spécifiques sur les chaînes

L'analyse sémantique doit vérifier :

- que la concaténation par `+` entre chaînes n'est utilisée qu'entre deux opérandes de type `string` ;
- que les comparaisons `==` et `!=` entre chaînes s'appliquent à des opérandes compatibles ;
- que les appels à `substr`, `indexOf`, `contains`, `startsWith`, `endsWith` respectent les signatures attendues ;
- que `s[i]` en lecture sur une `string` produit un résultat de type `string` ; l'indice doit être de type `int` ;
- que `s[i] = ...` sur une `string` est rejeté comme **erreur sémantique** (chaîne immutable) ;
- que `byteAt` reçoit un premier argument de type `string` et un second argument de type `int` ; le résultat est de type `int` ; tout autre type est une erreur sémantique ;
- que `glyphAt` reçoit un `string` et un `int` ; tout autre type est une erreur sémantique ;
- que `glyphLen` reçoit un `string` ; tout autre type est une erreur sémantique ;
- que `replace` reçoit exactement trois arguments de type `string` ; tout autre type est une erreur sémantique ; `old == ""` est une erreur runtime et non sémantique ;
- que `format` reçoit exactement deux arguments : un `string` et un `string[]` ; tout autre type est une erreur sémantique ; la correspondance entre le nombre de `{}` et le nombre d'arguments est vérifiée à l'exécution et non à la compilation ;
- que `join` reçoit un `string[]` comme premier argument et un `string` comme second argument ; tout autre type est une erreur sémantique ;
- que `split` reçoit deux arguments de type `string` ; tout autre type est une erreur sémantique ;
- que `concat` reçoit un argument de type `string[]` ; tout autre type de tableau est une erreur sémantique ;
- que `trim`, `trimLeft`, `trimRight`, `isBlank` reçoivent exactement un argument de type `string` ; `trim`/`trimLeft`/`trimRight` retournent `string` ; `isBlank` retourne `bool` ;
- que `toUpper`, `toLower`, `capitalize` reçoivent exactement un argument de type `string` et retournent `string` ;
- que `padLeft` et `padRight` reçoivent exactement trois arguments : `string`, `int`, `string` ; retournent `string` ; `width < 0` est une erreur runtime et non sémantique ;
- que `repeat` reçoit exactement deux arguments : `string` et `int` ; retourne `string` ; `n < 0` est une erreur runtime et non sémantique ;
- que `lastIndexOf` reçoit exactement deux arguments de type `string` et retourne `int` ;
- que `countOccurrences` reçoit exactement deux arguments de type `string` et retourne `int` ; `needle == ""` est une erreur runtime et non sémantique.

### 19.4 Vérifications spécifiques sur les flottants

L'analyse sémantique doit vérifier :

- que les appels à `abs`, `min`, `max`, `floor`, `ceil`, `round`, `trunc`, `sqrt`, `pow`, `approxEqual`, `toFloat`, `toInt` respectent les signatures attendues ;
- que `fmod` reçoit exactement deux arguments de type `float` ;
- que `sin`, `cos`, `tan`, `asin`, `acos`, `atan` reçoivent exactement un argument de type `float` et retournent un `float` ;
- que `atan2` reçoit exactement deux arguments de type `float` (dans l'ordre `y`, `x`) et retourne un `float` ;
- que `exp`, `log`, `log2`, `log10` reçoivent exactement un argument de type `float` et retournent un `float` ;
- que les opérateurs arithmétiques sur `float` s'appliquent à des opérandes compatibles ;
- que les comparaisons entre `float` s'appliquent à des opérandes compatibles.

### 19.5 Vérifications spécifiques sur les entiers

L'analyse sémantique doit vérifier :

- que les appels à `absInt`, `minInt`, `maxInt`, `clampInt`, `isEven`, `isOdd`, `safeDivInt`, `safeModInt` respectent les signatures attendues ;
- que `%`, `&`, `|`, `^`, `~`, `<<`, `>>` s'appliquent uniquement à des opérandes de type `int` ;
- que les opérations de division entière utilisent des opérandes compatibles.

### 19.6 Vérifications spécifiques sur les conversions et l'affichage

L'analyse sémantique doit vérifier :

- que `print` reçoit toujours un `string` ;
- que `toString(int)`, `toString(float)`, `toString(bool)`, `toInt(float)`, `toInt(string)`, `toFloat(int)`, `toFloat(string)`, `toBool(string)` sont appelées avec des arguments de types conformes à leurs signatures ;
- que les corps simples sans bloc de `if`, `else`, `while`, `for` ne contiennent pas de déclaration de variable.

### 19.7 Vérifications spécifiques sur les tableaux

L'analyse sémantique doit vérifier :

- que le type d'un littéral tableau `[...]` est homogène : tous les éléments doivent avoir le même type ; tout mélange est une erreur sémantique ;
- que le type des éléments d'un littéral tableau correspond au type d'élément déclaré de la variable cible ;
- que l'expression d'indice dans `array[expr]` ou `array[expr] = value` est de type `int` ;
- que la valeur affectée dans `array[expr] = value` est du même type que le type d'élément du tableau ;
- que `x[i] = value` avec `x` de type `string` est rejeté comme **erreur sémantique** : les chaînes sont immutables ;
- que les appels à `count`, `arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`, `arrayGet`, `arraySet` reçoivent des arguments de types conformes aux signatures attendues ;
- que `arrayPush`, `arraySet` et `arrayInsert` reçoivent une valeur du type d'élément correct ;
- que `arrayGet` et `arrayPop` sont utilisés dans un contexte attendant le type d'élément correct ;
- qu'un tableau déclaré de type `int[]` ne reçoit pas d'éléments de type `float`, `bool` ou `string`, et ainsi de suite pour chaque type ;
- que `void[]` n'est jamais déclaré ;
- que les tableaux de tableaux ne sont pas déclarés ;
- que la réaffectation totale d'un tableau `t = [...]` après déclaration est rejetée comme **erreur sémantique**.

### 19.8 Vérifications spécifiques sur les entrées/sorties fichiers et l'environnement

L'analyse sémantique doit vérifier :

- que `readFile`, `writeFile`, `appendFile`, `fileExists`, `readLines` reçoivent exactement un argument de type `string` ;
- que `writeFile` et `appendFile` reçoivent exactement deux arguments : `string path` et `string content` ;
- que `writeLines` reçoit exactement deux arguments : `string path` et `string[]` ; tout autre type de tableau est une erreur sémantique ;
- que le résultat de `readFile` est utilisé dans un contexte attendant un `string` ;
- que le résultat de `readLines` est utilisé dans un contexte attendant un `string[]` ;
- que le résultat de `fileExists` est utilisé dans un contexte attendant un `bool` ;
- que `tempPath` est appelé sans argument ; tout argument est une erreur sémantique ; le résultat est de type `string` ;
- que `remove` reçoit exactement un argument de type `string` ; tout autre type est une erreur sémantique ; `remove` ne retourne pas de valeur utilisable (`void`) ;
- que `chmod` reçoit exactement deux arguments : un `string` (`path`) et un `int` (`mode`) ; tout autre type est une erreur sémantique ; `chmod` ne retourne pas de valeur utilisable (`void`) ;
- que `cwd` est appelé sans argument ; tout argument est une erreur sémantique ; le résultat est de type `string` ;
- que `copy` et `move` reçoivent exactement deux arguments de type `string` (`src` et `dst`) ; tout autre type est une erreur sémantique ; ces fonctions ne retournent pas de valeur utilisable (`void`) ;
- que `isReadable`, `isWritable`, `isExecutable`, `isDirectory` reçoivent exactement un argument de type `string` ; tout autre type est une erreur sémantique ; le résultat est de type `bool` ;
- que `dirname`, `basename`, `filename`, `extension` reçoivent exactement un argument de type `string` ; tout autre type est une erreur sémantique ; le résultat est de type `string` ;
- que `getEnv` reçoit exactement deux arguments de type `string` (`name` et `fallback`) ; tout autre type est une erreur sémantique ;
- que le résultat de `getEnv` est utilisé dans un contexte attendant un `string`.

### 19.9 Vérifications spécifiques sur l'exécution de commandes

L'analyse sémantique doit vérifier :

- que `exec` reçoit exactement deux arguments : `string[] command` et `string[] env` ; tout autre type est une erreur sémantique ;
- que le résultat de `exec` est affecté à une variable de type `ExecResult` ou passé directement à `execStatus`, `execStdout` ou `execStderr` ;
- que `execStatus`, `execStdout`, `execStderr` reçoivent exactement un argument de type `ExecResult` ;
- que `execStatus` est utilisé dans un contexte attendant un `int` ;
- que `execStdout` et `execStderr` sont utilisés dans un contexte attendant un `string` ;
- qu'une variable `ExecResult` n'est jamais déclarée sans initialisation par `exec` ;
- que `ExecResult[]` n'est jamais déclaré ;
- que `ExecResult` n'est pas utilisé comme type de retour de `main`.

### 19.10 Vérifications spécifiques sur le temps et les dates

L'analyse sémantique doit vérifier :

- que `now` est appelé sans argument ; tout argument est une erreur sémantique ;
- que `epochToYear`, `epochToMonth`, `epochToDay`, `epochToHour`, `epochToMinute`, `epochToSecond` reçoivent exactement un argument de type `int` et retournent un `int` ;
- que `makeEpoch` reçoit exactement six arguments de type `int` et retourne un `int` ;
- que `formatDate` reçoit exactement deux arguments : un `int` (`epochMs`) et un `string` (`fmt`) ; le résultat est de type `string` ;
- que les résultats de `now`, `epochTo*` et `makeEpoch` sont utilisés dans un contexte attendant un `int` ;
- que le résultat de `formatDate` est utilisé dans un contexte attendant un `string` ;
- la validité des composants passés à `makeEpoch` n'est pas vérifiable statiquement ; le contrôle est effectué à l'exécution.

### 19.11 Détection statique de l'absence de `return`

L'analyse sémantique doit détecter l'absence de `return` dans une fonction non `void`.

Règles de détection :

- si le corps d'une fonction non `void` ne contient **aucune instruction `return`** au niveau supérieur du bloc, c'est une erreur sémantique ;
- si tous les chemins évidents se terminent par un `return` de type correct, la vérification passe ;
- une vérification conservative est acceptable : si l'analyseur ne peut pas garantir statiquement qu'un `return` est atteint, il doit signaler une erreur ;
- en particulier, une fonction dont le seul `return` est à l'intérieur d'un `if` sans `else` doit être signalée comme potentiellement incomplète ;
- `int main()` et `int main(string[] args)` sont soumis à la même règle : l'absence de `return` est une erreur sémantique.

### 19.12 Vérifications spécifiques sur les imports

L'analyse sémantique doit vérifier, pour chaque directive `import` rencontrée dans le graphe de fichiers :

- que le chemin spécifié se termine par `.ci` ; toute autre extension est une **erreur sémantique** ;
- que le fichier désigné par le chemin résolu existe et est lisible ; dans le cas contraire, c'est une **erreur sémantique** avec le message `Cannot import file: '<path>'` ;
- qu'il n'existe pas de cycle dans le graphe d'imports ; un cycle est détecté en maintenant la pile des fichiers en cours de traitement ; la détection se fait lors de la résolution récursive, avant la fusion de l'AST ;
- que la profondeur d'imbrication n'excède pas **32 niveaux** ; la profondeur est comptée comme la longueur du chemin le plus long depuis le fichier racine jusqu'au fichier courant dans le graphe d'imports ;
- qu'aucun fichier importé ne définit `main` ; la présence de `main` dans un fichier importé est une **erreur sémantique** signalée avec le nom du fichier fautif ;
- que toutes les directives `import` sont en tête du fichier, avant toute déclaration ; le non-respect de cette contrainte doit avoir déjà été signalé par le parseur comme **erreur syntaxique** ;
- qu'aucune fonction ni variable globale n'est définie deux fois à travers le graphe d'imports ; une redéfinition est une **erreur sémantique** indiquant les deux fichiers en cause et les numéros de ligne respectifs.

**Déduplication :** un fichier importé plusieurs fois dans le graphe (via des chemins distincts menant au même fichier résolu) n'est traité qu'une seule fois. La deuxième occurrence et les suivantes sont silencieusement ignorées.

**Messages d'erreur multi-fichiers :** pour toute erreur détectée dans un fichier importé, le champ `<file>` du message doit indiquer le chemin du fichier importé (chemin relatif au fichier racine), et non le nom du fichier racine. Cela s'applique aux erreurs lexicales, syntaxiques, sémantiques et runtime.

---

## 20. Sémantique d'exécution

### 20.1 Modèle

Le runtime est un interpréteur direct de l'AST.

### 20.2 Environnement

L'interpréteur doit disposer :

- d'une table globale des fonctions ;
- d'une pile de portées locales ;
- d'un mécanisme de retour de fonction ;
- d'un support pour les fonctions natives.

### 20.3 Appels

Lors d'un appel de fonction :

- création d'une nouvelle portée locale ;
- association des arguments aux paramètres ;
- exécution du corps ;
- récupération de la valeur retournée.

### 20.4 Politique runtime sur les calculs numériques

La philosophie générale est la suivante :

- interrompre le moins possible le runtime ;
- privilégier des résultats calculables et des mécanismes de contrôle explicites ;
- exposer les cas numériques spéciaux plutôt que les masquer.

Règles générales :

- les calculs sur `float` suivent IEEE et peuvent produire `NaN`, `+Infinity` et `-Infinity` ;
- l'utilisateur peut tester ces cas via `isNaN`, `isInfinite`, `isFinite`, `isPositiveInfinity`, `isNegativeInfinity` ;
- pour `int`, les fonctions `safeDivInt` et `safeModInt` fournissent un contrôle explicite des divisions ou modulos par zéro.

---

## 21. Gestion des erreurs et messages de diagnostic

### 21.1 Principes généraux

Cimple distingue quatre niveaux d'erreur. Chaque erreur interrompt l'exécution ou la compilation selon les règles définies ci-dessous. Il n'existe pas de niveau « avertissement » : tout problème détecté est soit une erreur bloquante, soit silencieux.

**Langue des messages :** tous les messages d'erreur sont émis **en anglais**, quelle que soit la locale du système d'exploitation. La variable d'environnement `LANG`, `LC_ALL` ou toute autre variable de localisation n'a aucun effet sur les messages de Cimple.

**Sortie :** tous les messages d'erreur sont émis sur **`stderr`**. La sortie standard `stdout` n'est jamais utilisée pour les diagnostics.

**Pas de coloration ANSI :** les messages sont du texte brut sans codes d'échappement terminaux, quelle que soit la valeur de `TERM` ou `NO_COLOR`.

### 21.2 Format uniforme des messages

Tout message d'erreur respecte le format suivant :

```
[LEVEL]  <file>  line <N>, column <N>
  <description>
  -> <explanation or expectation>
```

- `[LEVEL]` est l'un de : `[LEXICAL ERROR]`, `[SYNTAX ERROR]`, `[SEMANTIC ERROR]`, `[RUNTIME ERROR]` ;
- `<file>` est le nom du fichier source tel que passé à la CLI ;
- `line` et `column` sont comptés à partir de `1` ;
- `<description>` est une phrase courte décrivant ce qui s'est passé ;
- la ligne `->` donne ce qui était attendu, ou une indication pour corriger ; elle peut être omise si aucune suggestion pertinente n'est possible ;
- une erreur peut comporter plusieurs lignes `->` si plusieurs informations complémentaires sont utiles.

Exemple :

```
[SEMANTIC ERROR]  hello.ci  line 12, column 9
  Type mismatch in assignment
  -> Variable 'score' has type 'int', but expression has type 'float'
  -> Use toInt() or toFloat() to convert explicitly.
```

```
[RUNTIME ERROR]  hello.ci  line 18, column 3
  Array index out of bounds
  -> Index: 5   Array size: 3
  -> Variable: 'scores'
```

### 21.3 Erreurs lexicales

Les erreurs lexicales sont **fatales et immédiates** : l'analyse s'arrête à la première erreur lexicale rencontrée. Le reste du fichier n'est pas analysé.

Catalogue des erreurs lexicales et messages associés :

| Situation | Description (`<description>`) | Indication (`->`) |
|---|---|---|
| Caractère non reconnu | `Unexpected character: '<c>'` | `Only printable ASCII and valid UTF-8 characters are allowed.` |
| Chaîne non fermée | `Unterminated string literal` | `Opening quote at line <N>, column <N> has no matching closing quote.` |
| Retour à la ligne dans une chaîne | `Newline inside string literal` | `Use '\n' to represent a newline inside a string.` |
| `\uXXXX` avec ≠ 4 chiffres hex | `Malformed Unicode escape` | `\uXXXX requires exactly 4 hex digits` |
| `\uXXXX` malformé | `Malformed Unicode escape: '\u<seq>'` | `Expected exactly 4 hexadecimal digits after \u.` |
| `0x` sans chiffres | `Invalid hexadecimal literal: '0x'` | `Expected at least one hexadecimal digit after '0x'.` |
| `0b` sans chiffres | `Invalid binary literal: '0b'` | `Expected at least one binary digit (0 or 1) after '0b'.` |

### 21.4 Erreurs syntaxiques

Les erreurs syntaxiques sont **fatales et immédiates** : l'AST étant invalide, aucune analyse sémantique n'est possible.

Catalogue des erreurs syntaxiques et messages associés :

| Situation | Description | Indication |
|---|---|---|
| Jeton inattendu | `Unexpected token: '<tok>'` | `Expected <what> at this position.` |
| `;` manquant | `Missing ';' after statement` | `A semicolon is required at the end of each statement.` |
| `)` manquant | `Missing ')' to close expression` | `Opening '(' at line <N>, column <N>.` |
| `}` manquant | `Missing '}' to close block` | `Opening '{' at line <N>, column <N>.` |
| `]` manquant | `Missing ']' to close array literal` | `Opening '[' at line <N>, column <N>.` |
| Type inconnu | `Unknown type: '<name>'` | `Valid types: int, float, bool, string, byte, void, ExecResult, or a declared structure name.` |
| Expression attendue | `Expected expression` | `Found '<tok>' which cannot start an expression.` |

### 21.5 Erreurs sémantiques

Les erreurs sémantiques sont **accumulées** : l'analyse sémantique continue après une erreur afin de rapporter autant de problèmes que possible en une seule passe. L'accumulation s'arrête après **10 erreurs** ; un message récapitulatif est alors émis et l'exécution est abandonnée.

```
10 semantic errors found. Further analysis aborted.
```

Catalogue des erreurs sémantiques et messages associés :

| Situation | Description | Indication |
|---|---|---|
| Variable non déclarée | `Undefined variable: '<name>'` | `'<name>' must be declared before use.` |
| Variable déjà déclarée | `Variable '<name>' already declared` | `First declaration at line <N>, column <N>.` |
| Fonction inconnue | `Unknown function: '<name>'` | `'<name>' is not defined in this scope or the standard library.` |
| Mauvais nombre d'arguments | `Wrong number of arguments for '<name>'` | `Expected <N>, got <M>.` |
| Incompatibilité de types | `Type mismatch in <context>` | `Expected '<T1>', got '<T2>'.` |
| Affectation de type incompatible | `Cannot assign '<T2>' to variable of type '<T1>'` | `Use toInt(), toFloat(), toString() or toBool() to convert.` |
| Opérateur sur mauvais type | `Operator '<op>' cannot be applied to type '<T>'` | `Operator '<op>' requires <expected types>.` |
| `return` absent | `Missing return in function '<name>'` | `Function returns '<T>' but not all paths return a value.` |
| `return` de mauvais type | `Wrong return type in function '<name>'` | `Expected '<T1>', got '<T2>'.` |
| Affectation à une constante | `Cannot assign to predefined constant '<name>'` | `'<name>' is read-only.` |
| Déclaration d'un identifiant réservé | `'<name>' is a reserved identifier` | `Choose a different name for your variable or function.` |
| `s[i] = ...` sur une chaîne | `Strings are immutable: index assignment is not allowed` | `Use replace() or build a new string with concatenation.` |
| `ExecResult` sans initialisation | `'ExecResult' variable must be initialized with exec()` | `Declaration without initialization is not allowed for ExecResult.` |
| `ExecResult[]` déclaré | `Array of ExecResult is not allowed` | `ExecResult can only be used as a scalar variable.` |
| Tableau de mauvais type d'élément | `Array element type mismatch` | `Expected '<T>', got '<T2>' at index <N>.` |
| Signature de `main` invalide | `Invalid signature for 'main'` | `Valid signatures: void main(), int main(), void main(string[] args), int main(string[] args).` |
| Littéral `byte` hors plage | `Byte literal out of range: <N>` | `Valid range: 0 to 255.` |
| Affectation `int` → `byte` sans conversion | `Cannot assign 'int' to 'byte'` | `Use intToByte() to convert.` |
| Opérateur `&&` / `\|\|` / `!` sur `byte` | `Operator '<op>' cannot be applied to type 'byte'` | `Logical operators require 'bool' operands.` |
| Champ de type structure sans `clone` | `Field '<n>' in structure '<S>' has no default value` | `Fields of structure type must be explicitly initialized with 'clone StructureName'.` |
| Champ récursif | `Recursive field '<n>' in structure '<S>'` | `Structures cannot contain fields of their own type (direct or indirect).` |
| `clone` sur variable | `Cannot clone a variable: '<n>'` | `Use 'clone StructureName' to create an instance.` |
| Override avec signature différente | `Method '<m>' in '<S>' overrides '<Base>.<m>' with a different signature` | `Signatures must be identical to override a method.` |
| `super` hors méthode / sans base | `'super' used outside of a derived structure method` | — |
| `super.champ` | `Cannot access field via 'super'` | `Use 'self.<field>' to access fields.` |
| `self` hors méthode | `'self' used outside of a structure method` | — |
| `self = ...` | `Cannot reassign 'self'` | — |
| Cycle d'héritage | `Inheritance cycle detected involving '<S>'` | — |
| Base inconnue | `Unknown base structure: '<n>'` | — |
| Champ redéfini avec type différent | `Field '<n>' in '<S>' redefines '<Base>.<n>' with a different type` | `Redefined fields must have the same type (default value may differ).` |
| Accès membre sur non-structure | `Member access requires a structure instance` | `'<n>' is of type '<T>', not a structure.` |
| Champ inconnu sur structure | `Unknown field '<n>' on structure '<S>'` | — |
| Méthode inconnue sur structure | `Unknown method '<n>' on structure '<S>'` | — |
| `main` dans un fichier importé | `'main' cannot be defined in an imported file` | `Define 'main' only in the root source file.` |
| Fichier importé introuvable | `Cannot import file: '<path>'` | `File not found or not readable.` |
| Extension import invalide | `Import path must end with '.ci'` | `Only .ci files can be imported.` |
| Import circulaire | `Circular import detected: '<path>'` | `Import chain: <file1> -> <file2> -> ... -> <path>` |
| Profondeur d'import dépassée | `Import depth limit exceeded (max 32)` | `Simplify your import graph.` |
| `import` hors tête de fichier | `'import' must appear before any declaration` | `Move all import directives to the top of the file.` |
| `import` dans un bloc ou fonction | `'import' is not allowed inside a function or block` | `Import directives are only valid at the top level.` |

### 21.6 Erreurs runtime

Les erreurs runtime sont **fatales et immédiates** : le programme s'arrête à la première erreur runtime et le message est émis sur `stderr`.

Catalogue des erreurs runtime et messages associés :

| Situation | Description | Indication |
|---|---|---|
| Accès tableau hors bornes | `Array index out of bounds` | `Index: <N>   Array size: <M>   Variable: '<name>'` |
| `arrayPop` sur tableau vide | `Cannot pop from empty array` | `Variable: '<name>'` |
| `arrayRemove` hors bornes | `arrayRemove: index out of bounds` | `Index: <N>   Array size: <M>   Variable: '<name>'` |
| `arrayInsert` hors bornes | `arrayInsert: index out of bounds` | `Index: <N>   Array size: <M>   Variable: '<name>'` |
| Indice chaîne hors bornes (`s[i]`) | `String index out of bounds` | `Index: <N>   String length: <M> bytes   Variable: '<name>'` |
| `byteAt` hors bornes | `byteAt: index out of bounds` | `Index: <N>   String length: <M> bytes` |
| `glyphAt` hors bornes | `glyphAt: index out of bounds` | `Index: <N>   Glyph count: <M>` |
| `substr` hors bornes | `substr: range out of bounds` | `start: <N>   length: <L>   String length: <M> bytes` |
| `split` séparateur vide | `split: separator cannot be empty` | — |
| `replace` ancien vide | `replace: old argument cannot be empty` | — |
| `format` marqueurs/args | `format: marker count does not match argument count` | `Markers '{}' in template: <N>   Arguments provided: <M>` |
| `repeat` n négatif | `repeat: count must be non-negative` | `Got: <N>` |
| `countOccurrences` needle vide | `countOccurrences: needle cannot be empty` | — |
| `padLeft` / `padRight` width négatif | `padLeft: width must be non-negative` / `padRight: width must be non-negative` | `Got: <N>` |
| Division entière par zéro | `Integer division by zero` | — |
| Modulo par zéro | `Integer modulo by zero` | — |
| `readFileBytes` fichier illisible | `Cannot read file: '<path>'` | `<system error message>` |
| `writeFileBytes` / `appendFileBytes` erreur écriture | `Cannot write file: '<path>'` | `<system error message>` |
| `bytesToInt` mauvaise taille | `bytesToInt: expected INT_SIZE bytes, got <N>` | — |
| `bytesToFloat` mauvaise taille | `bytesToFloat: expected FLOAT_SIZE bytes, got <N>` | — |
| `writeFile` / `appendFile` / `writeLines` | `Cannot write file: '<path>'` | `<system error message>` |
| `remove` fichier inexistant | `Cannot remove file: '<path>'` | `File does not exist.` |
| `remove` sur un répertoire | `Cannot remove file: '<path>'` | `Path is a directory, not a file.` |
| `remove` permission refusée | `Cannot remove file: '<path>'` | `<system error message>` |
| `chmod` chemin inexistant | `Cannot chmod: '<path>'` | `File or directory does not exist.` |
| `chmod` permission refusée | `Cannot chmod: '<path>'` | `<system error message>` |
| `chmod` sur Windows ou WebAssembly | `chmod: not supported on this platform` | — |
| `copy` src inexistant | `Cannot copy: source file not found: '<src>'` | `<system error message>` |
| `copy` src est un répertoire | `Cannot copy: '<src>'` | `Source is a directory, not a file.` |
| `copy` répertoire parent de dst inexistant | `Cannot copy: destination directory not found: '<dst>'` | `<system error message>` |
| `copy` permission insuffisante | `Cannot copy: '<src>' -> '<dst>'` | `<system error message>` |
| `move` src inexistant | `Cannot move: source file not found: '<src>'` | `<system error message>` |
| `move` src est un répertoire | `Cannot move: '<src>'` | `Source is a directory, not a file.` |
| `move` répertoire parent de dst inexistant | `Cannot move: destination directory not found: '<dst>'` | `<system error message>` |
| `move` permission insuffisante | `Cannot move: '<src>' -> '<dst>'` | `<system error message>` |
| `exec` commande vide | `exec: command array must not be empty` | — |
| `exec` exécutable introuvable | `exec: command not found: '<cmd>'` | `Check that the executable exists and is in PATH.` |
| `exec` entrée `env` invalide | `exec: invalid environment entry: '<entry>'` | `Expected format: 'NAME=VALUE'.` |

### 21.7 Codes de sortie

| Situation | Code de sortie |
|---|---|
| Programme exécuté avec succès | `0` (ou valeur retournée par `main` si `int main`) |
| Erreur lexicale, syntaxique ou sémantique | `1` — **écrase** tout code de retour utilisateur |
| Erreur runtime | `2` — **écrase** tout code de retour utilisateur |

Le code de sortie du programme utilisateur (valeur retournée par `int main()`) n'est utilisé que si aucune erreur Cimple ne survient. En présence d'une erreur lexicale, syntaxique, sémantique ou runtime, le code de sortie est toujours `1` ou `2` indépendamment de ce que le programme aurait retourné.

La commande `cimple check` retourne `0` si aucune erreur n'est trouvée, `1` sinon. Elle ne produit jamais le code `2` (pas d'exécution).

### 21.8 Comportement sur erreurs multiples sémantiques

```
[SEMANTIC ERROR]  hello.ci  line 5, column 3
  Undefined variable: 'scroe'
  -> 'scroe' must be declared before use.

[SEMANTIC ERROR]  hello.ci  line 9, column 7
  Type mismatch in assignment
  -> Expected 'int', got 'string'.

[SEMANTIC ERROR]  hello.ci  line 14, column 1
  Missing return in function 'compute'
  -> Function returns 'float' but not all paths return a value.

3 semantic error(s) found.
```

Si 10 erreurs sont atteintes :

```
10 semantic errors found. Further analysis aborted.
```

---

## 22. Grammaire EBNF

La grammaire suivante décrit formellement la syntaxe de Cimple.

Conventions :
- `|` : alternative
- `[ X ]` : X optionnel (0 ou 1 fois)
- `{ X }` : X répété (0 ou n fois)
- `( X )` : groupement
- les terminaux sont en `MAJUSCULES` ou entre guillemets `"..."` ;
- les non-terminaux sont en `minuscules`.

```ebnf
program
    = { import_decl } { top_level_item } ;

import_decl
    = "import" STRING_LITERAL ";" ;

top_level_item
    = function_decl
    | var_decl ";"
    | array_decl ";"
    | structure_decl ;

structure_decl
    = "structure" IDENT [ ":" IDENT ] "{" { structure_member } "}" ;

structure_member
    = field_decl ";"
    | method_decl ;

field_decl
    = type IDENT "=" expr
    | struct_type IDENT "=" clone_expr ;

method_decl
    = type IDENT "(" [ param_list ] ")" block ;

clone_expr
    = "clone" IDENT ;

struct_type
    = IDENT ;   (* nom d'une structure déclarée *)

function_decl
    = type IDENT "(" [ param_list ] ")" block
    | type "main" "(" ")" block
    | type "main" "(" "string" "[" "]" IDENT ")" block ;

param_list
    = param { "," param } ;

param
    = type IDENT
    | array_type IDENT
    | struct_type IDENT
    | struct_array_type IDENT ;

type
    = "int" | "float" | "bool" | "string" | "byte" | "void" | "ExecResult" | IDENT ;
    (* IDENT désigne un nom de structure déclaré *)

array_type
    = "int" "[" "]"
    | "float" "[" "]"
    | "bool" "[" "]"
    | "string" "[" "]"
    | "byte" "[" "]" ;

struct_array_type
    = IDENT "[" "]" ;   (* tableau de structures *)

block
    = "{" { statement } "}" ;

statement
    = block
    | var_decl ";"
    | array_decl ";"
    | assign_stmt ";"
    | index_assign_stmt ";"
    | incr_stmt ";"
    | expr_stmt ";"
    | if_stmt
    | while_stmt
    | for_stmt
    | return_stmt ";"
    | break_stmt ";"
    | continue_stmt ";" ;

var_decl
    = type IDENT [ "=" expr ] ;

array_decl
    = array_type IDENT [ "=" array_literal ]
    | array_type IDENT "=" "[" "]" ;

array_literal
    = "[" expr { "," expr } "]"
    | "[" "]" ;

assign_stmt
    = IDENT "=" expr ;

index_assign_stmt
    = IDENT "[" expr "]" "=" expr ;
    (* Note : uniquement valide si IDENT désigne un tableau ;
       si IDENT désigne une string, c'est une erreur sémantique — les chaînes sont immutables *)

expr_stmt
    = call_expr ;

if_stmt
    = "if" "(" expr ")" simple_or_block [ "else" simple_or_block ] ;

simple_or_block
    = block
    | simple_stmt ;

(* Une instruction simple exclut les déclarations de variables et les structures de contrôle *)
simple_stmt
    = assign_stmt ";"
    | index_assign_stmt ";"
    | incr_stmt ";"
    | expr_stmt ";"
    | return_stmt ";"
    | break_stmt ";"
    | continue_stmt ";" ;

while_stmt
    = "while" "(" expr ")" simple_or_block ;

for_stmt
    = "for" "(" for_init ";" expr ";" for_update ")" simple_or_block ;

for_init
    = "int" IDENT "=" expr ;

for_update
    = IDENT "=" expr
    | IDENT "++"
    | IDENT "--" ;

incr_stmt
    = IDENT "++"
    | IDENT "--" ;

return_stmt
    = "return" [ expr ] ;

break_stmt
    = "break" ;

continue_stmt
    = "continue" ;

expr
    = or_expr ;

or_expr
    = and_expr { "||" and_expr } ;

and_expr
    = bitor_expr { "&&" bitor_expr } ;

bitor_expr
    = bitxor_expr { "|" bitxor_expr } ;

bitxor_expr
    = bitand_expr { "^" bitand_expr } ;

bitand_expr
    = eq_expr { "&" eq_expr } ;

eq_expr
    = rel_expr { ( "==" | "!=" ) rel_expr } ;

rel_expr
    = shift_expr { ( "<" | "<=" | ">" | ">=" ) shift_expr } ;

shift_expr
    = add_expr { ( "<<" | ">>" ) add_expr } ;

add_expr
    = mul_expr { ( "+" | "-" ) mul_expr } ;

mul_expr
    = unary_expr { ( "*" | "/" | "%" ) unary_expr } ;

unary_expr
    = ( "!" | "-" | "~" ) unary_expr
    | primary_expr ;

primary_expr
    = INT_LITERAL
    | FLOAT_LITERAL
    | BOOL_LITERAL
    | STRING_LITERAL
    | array_literal
    | call_expr
    | index_expr
    | member_expr
    | clone_expr
    | "self"
    | PREDEFINED_CONST
    | IDENT
    | "(" expr ")" ;

(* PREDEFINED_CONST couvre les identifiants réservés :
   INT_MAX  INT_MIN  INT_SIZE  FLOAT_SIZE  FLOAT_DIG
   FLOAT_EPSILON  FLOAT_MIN  FLOAT_MAX
   M_PI  M_E  M_TAU  M_SQRT2  M_LN2  M_LN10
   Reconnus dans la phase sémantique, pas lexicale *)

member_expr
    = IDENT { "." ( IDENT | method_call ) }
    | "self" "." ( IDENT | method_call ) { "." ( IDENT | method_call ) }
    | "super" "." method_call ;

method_call
    = IDENT "(" [ arg_list ] ")" ;

index_expr
    = IDENT "[" expr "]"
    | member_expr "[" expr "]" ;

call_expr
    = IDENT "(" [ arg_list ] ")" ;

assign_stmt
    = IDENT "=" expr
    | index_assign_stmt
    | member_assign_stmt ;

member_assign_stmt
    = member_expr "=" expr ;

arg_list
    = expr { "," expr } ;

(* Terminaux *)
IDENT
    = LETTER { LETTER | DIGIT | "_" } ;

INT_LITERAL
    = DEC_LITERAL
    | HEX_LITERAL
    | BIN_LITERAL
    | OCT_LITERAL ;

DEC_LITERAL
    = DIGIT { DIGIT } ;

HEX_LITERAL
    = "0x" HEX_DIGIT { HEX_DIGIT }
    | "0X" HEX_DIGIT { HEX_DIGIT } ;

BIN_LITERAL
    = "0b" BIN_DIGIT { BIN_DIGIT }
    | "0B" BIN_DIGIT { BIN_DIGIT } ;

(* Un littéral octal commence par 0 suivi d'au moins un chiffre octal *)
OCT_LITERAL
    = "0" OCT_DIGIT { OCT_DIGIT } ;

FLOAT_LITERAL
    = DEC_LITERAL "." { DIGIT } [ EXP_PART ]
    | DEC_LITERAL EXP_PART ;

EXP_PART
    = ( "e" | "E" ) [ "+" | "-" ] DIGIT { DIGIT } ;

BOOL_LITERAL
    = "true" | "false" ;

STRING_LITERAL
    = '"' { str_char } '"' ;

str_char
    = any_char_except_backslash_or_quote_or_newline
    | escape_seq ;

escape_seq
    = "\\" '"'
    | "\\" "\\"
    | "\\" "n"
    | "\\" "t"
    | "\\" "r"
    | "\\" "b"
    | "\\" "f"
    | "\\" "u" HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT ;

LETTER
    = "a".."z" | "A".."Z" | "_" ;

DIGIT
    = "0".."9" ;

HEX_DIGIT
    = "0".."9" | "a".."f" | "A".."F" ;

BIN_DIGIT
    = "0" | "1" ;

OCT_DIGIT
    = "0".."7" ;
```

Notes normatives sur la grammaire :

- `import_decl` doit apparaître avant tout `top_level_item` ; le parseur doit rejeter un `import` apparaissant après une déclaration de fonction ou de variable comme une **erreur syntaxique** ; un `import` à l'intérieur d'un bloc ou d'une fonction est également une **erreur syntaxique** ;
- le chemin dans `import_decl` est un `STRING_LITERAL` ordinaire ; les séquences d'échappement y sont reconnues mais leur usage est déconseillé pour les chemins ; le chemin est résolu et validé lors de la phase sémantique, pas lors du parsing ;
- `simple_or_block` interdit explicitement une déclaration de variable scalaire ou tableau comme corps direct d'un `if`, `else`, `while` ou `for` sans bloc ; une telle déclaration doit produire une erreur syntaxique ou sémantique ;
- `function_decl` pour `main` est restreint à quatre signatures exactes : `int main()`, `void main()`, `int main(string[] args)`, `void main(string[] args)` ; le nom du paramètre dans les formes avec `string[] args` est libre mais doit être un identifiant valide ;
- `index_assign_stmt` est une instruction autonome et non une expression ; `array[i] = value` ne peut pas apparaître au sein d'une expression ; si l'identifiant désigne une `string`, c'est une **erreur sémantique** (chaînes immutables) ;
- `index_expr` (`x[i]`) est une expression en lecture valide à la fois sur les tableaux (retourne le type de l'élément) et sur les chaînes (retourne une `string` d'un octet) ; elle peut apparaître partout où une expression est attendue ;
- dans `for_init`, seule une déclaration `int` est autorisée ;
- dans `for_update`, seule une affectation simple ou une instruction d'incrémentation/décrémentation est autorisée ;
- le `else` se lie au `if` non apparié le plus proche (dangling-else résolu en faveur du `if` le plus proche) ;
- `call_expr` peut apparaître à la fois comme `primary_expr` (dans une expression) et comme `expr_stmt` (instruction autonome) ; cette ambiguïté est résolue par le contexte grammatical ;
- `array_type` est valide comme type de paramètre de fonction ; les tableaux sont passés par référence ;
- `OCT_LITERAL` : le littéral `0` seul est un décimal valant zéro, non un octal ;
- `FLOAT_LITERAL` : la partie fractionnaire après le point peut être absente (`1.` est valide) ; l'exposant seul sans point est valide (`1e3`) ; un entier nu sans point ni exposant est toujours `INT_LITERAL` ;
- `escape_seq` dans `STRING_LITERAL` : `\uXXXX` requiert exactement 4 chiffres hexadécimaux ; tout `\X` inconnu est préservé littéralement (`\` + `X`) ;
- `ExecResult` est un type valide dans `type` ; il peut être utilisé dans `var_decl` et comme type de paramètre ou de retour de fonction ; `ExecResult[]` est interdit ; une variable `ExecResult` doit être initialisée avec un appel à `exec` ; toute déclaration `ExecResult x;` sans initialisation est une erreur sémantique ;
- `PREDEFINED_CONST` est reconnu lexicalement comme un `IDENT` ordinaire ; c'est l'analyse sémantique qui identifie `INT_MAX`, `INT_MIN`, `INT_SIZE`, `FLOAT_SIZE`, `FLOAT_DIG`, `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX`, `M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10` comme des constantes prédéfinies et leur attribue leur type et valeur ; toute déclaration ou affectation sur ces identifiants est une erreur sémantique.

---

## 23. Programme exemple de référence

```c
int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 10;
    if (x > 5) {
        print("ok");
    }
    return 0;
}
```

---

## 24. Exemples de programmes

### 24.1 While

```c
int main() {
    int x = 0;
    while (x < 3) {
        print("loop");
        x = x + 1;
    }
    return 0;
}
```

Exemple valide sans bloc :

```c
int main() {
    int x = 0;
    while (x < 3) x = x + 1;
    return 0;
}
```

### 24.2 Fonction booléenne

```c
bool greater(int a, int b) {
    return a > b;
}

int main() {
    if (greater(5, 2)) {
        print("yes");
    } else {
        print("no");
    }
    return 0;
}
```

### 24.3 Bool et conversions

```c
int main() {
    string a = "true";
    string b = "0";

    if (toBool(a)) {
        print("a true");
    }

    print(toString(toBool(b)));
    return 0;
}
```

### 24.4 For

```c
int main() {
    for (int i = 0; i < 5; i++) {
        print("tick");
    }
    return 0;
}
```

Exemple valide sans bloc :

```c
int main() {
    for (int i = 0; i < 5; i++) print("tick");
    return 0;
}
```

### 24.5 Chaînes

```c
int main() {
    string name = "Alexandre";
    string msg = "Bonjour " + name;

    print(msg);

    if (startsWith(name, "Alex")) {
        print("prefix ok");
    }

    print(substr(name, 0, 4));

    return 0;
}
```

### 24.6 join, split, concat

```c
void main() {
    // split : découper une chaîne
    string[] words = split("Bonjour tout le monde", " ");
    // words vaut ["Bonjour", "tout", "le", "monde"]

    print(toString(count(words)));   // affiche 4
    print(words[0]);                 // affiche "Bonjour"

    // join : réassembler avec un séparateur
    string rejoined = join(words, "-");
    print(rejoined);   // affiche "Bonjour-tout-le-monde"

    // concat : assembler sans séparateur
    string[] parts = ["x", "=", "42"];
    string expr = concat(parts);
    print(expr);   // affiche "x=42"

    // join avec séparateur vide == concat
    string also = join(parts, "");
    print(also);   // affiche "x=42"
}
```

### 24.7 replace et format

```c
void main() {
    // replace : remplace toutes les occurrences
    string s = "le chat est un chat";
    string r = replace(s, "chat", "chien");
    print(r + "\n");   // → "le chien est un chien"

    // replace : suppression
    string clean = replace("  bonjour  monde  ", "  ", " ");
    print(clean + "\n");   // → " bonjour monde "

    // format : substitution positionnelle
    string name = "Alice";
    int age = 30;
    float score = 98.5;

    string msg = format("Nom: {}, Age: {}, Score: {}", [
        name,
        toString(age),
        toString(score)
    ]);
    print(msg + "\n");
    // → "Nom: Alice, Age: 30, Score: 98.5"

    // format : construction d'une ligne CSV
    string[] row = [toString(1), "Alice", toString(99.0)];
    string csv = format("{},{},{}", row);
    print(csv + "\n");   // → "1,Alice,99.0"

    // format : zéro argument
    string plain = format("aucun marqueur", []);
    print(plain + "\n");   // → "aucun marqueur"
}
```

```c
void main() {
    string s = "hello";

    // accès par crochet — retourne une string d'un octet
    print(s[0] + "\n");   // → h
    print(s[4] + "\n");   // → o

    // parcours caractère par caractère (octets)
    for (int i = 0; i < len(s); i++) {
        print(s[i]);
    }
    print("\n");           // → hello

    // comparaison d'un octet
    if (s[0] == "h") {
        print("commence par h\n");
    }
}
```

Différence avec une chaîne multi-octets :

```c
void main() {
    string s = "élan";

    // len = 5 octets (é = 2 octets, l+a+n = 3 octets)
    print(toString(len(s)) + "\n");      // → 5

    // s[0] et s[1] sont les deux octets de é — ne forment pas un glyphe valide seuls
    // glyphLen pour compter les glyphes
    print(toString(glyphLen(s)) + "\n"); // → 4

    // parcours par glyphe avec glyphAt
    for (int i = 0; i < glyphLen(s); i++) {
        print(glyphAt(s, i) + "\n");     // → é, l, a, n
    }
}
```

### 24.8 glyphLen et glyphAt — Unicode NFC

```c
void main() {
    string s = "élève";

    // len mesure les octets UTF-8
    print(toString(len(s)));        // → 7

    // glyphLen mesure les points de code après NFC
    print(toString(glyphLen(s)));   // → 5

    // glyphAt retourne le glyphe à l'indice donné (NFC)
    print(glyphAt(s, 0));           // → "é"  (U+00E9)
    print(glyphAt(s, 2));           // → "è"  (U+00E8)
    print(glyphAt(s, 4));           // → "e"

    // parcours de tous les glyphes
    int i = 0;
    while (i < glyphLen(s)) {
        print(glyphAt(s, i) + "\n");
        i++;
    }
}
```

Démonstration de la normalisation NFC :

```c
void main() {
    // Deux chaînes visuellement identiques, représentations différentes
    string nfc = "\u00E9";          // é comme point de code unique (NFC)
    string nfd = "e\u0301";        // e + combining accent aigu (NFD)

    // == compare les octets bruts : NFC != NFD
    if (nfc == nfd) {
        print("égales\n");          // jamais atteint
    } else {
        print("inégales\n");        // → "inégales" (représentations différentes)
    }

    // glyphLen normalise les deux en NFC avant de compter
    print(toString(glyphLen(nfc))); // → 1
    print(toString(glyphLen(nfd))); // → 1

    // glyphAt retourne toujours la forme NFC
    print(glyphAt(nfc, 0));         // → "é"  (U+00E9)
    print(glyphAt(nfd, 0));         // → "é"  (U+00E9) — normalisé

    // les résultats de glyphAt sont donc comparables entre eux
    if (glyphAt(nfc, 0) == glyphAt(nfd, 0)) {
        print("glyphes identiques\n"); // → "glyphes identiques"
    }
}
```

### 24.9 Flottants

```c
float average(float a, float b) {
    return (a + b) / 2.0;
}

int main() {
    float x = 3.2;
    float y = 4.8;
    float z = average(x, y);

    if (approxEqual(z, 4.0, 0.0001)) {
        print("ok");
    }

    print(toString(round(z)));
    print("\n");
    print(toString(trunc(-2.9)) + "\n");   // → -2.0
    print(toString(fmod(7.5, 2.0)) + "\n"); // → 1.5
    return 0;
}
```

### 24.10 Trigonométrie

```c
void main() {
    // Conversion degrés → radians
    float deg = 45.0;
    float rad = deg * M_PI / 180.0;

    print("sin(45°) = " + toString(sin(rad)) + "\n");  // → ~0.7071
    print("cos(45°) = " + toString(cos(rad)) + "\n");  // → ~0.7071
    print("tan(45°) = " + toString(tan(rad)) + "\n");  // → ~1.0

    // Angle d'un vecteur (3, 4)
    float angle = atan2(4.0, 3.0);
    print("angle = " + toString(angle) + " rad\n");

    // Vérification : sin²(x) + cos²(x) = 1
    float s = sin(rad);
    float c = cos(rad);
    if (approxEqual(s * s + c * c, 1.0, FLOAT_EPSILON * 10.0)) {
        print("identité trigonométrique vérifiée\n");
    }
}
```

### 24.11 Logarithmes et exponentielle

```c
void main() {
    print("exp(1)    = " + toString(exp(1.0))    + "\n"); // → e
    print("log(e)    = " + toString(log(M_E))    + "\n"); // → 1.0
    print("log2(8)   = " + toString(log2(8.0))   + "\n"); // → 3.0
    print("log10(100)= " + toString(log10(100.0)) + "\n"); // → 2.0

    // Croissance exponentielle : P(t) = P0 * e^(r*t)
    float p0 = 1000.0;
    float r  = 0.05;
    float t  = 10.0;
    float pt = p0 * exp(r * t);
    print("P(10) = " + toString(pt) + "\n");

    // Vérification domaine invalide
    float bad = log(-1.0);
    if (isNaN(bad)) {
        print("log(-1) est NaN\n");
    }
}

### 24.12 Entiers et bitwise

```c
int main() {
    int mask = 1 << 3;
    int value = y & mask;

    if (isOdd(y)) {
        print("odd");
    }

    print(toString(value));
    return 0;
}
```

### 24.13 void main

```c
void main() {
    print("hello");
}
```

### 24.14 Arguments de la ligne de commande

```c
void main(string[] args) {
    if (count(args) == 0) {
        print("Aucun argument.\n");
        return;
    }
    int i = 0;
    while (i < count(args)) {
        print("arg[" + toString(i) + "] = " + args[i] + "\n");
        i++;
    }
}
```

Invocation :

```bash
cimple run prog.ci bonjour monde 42
```

Produit :

```
arg[0] = bonjour
arg[1] = monde
arg[2] = 42
```

Forme avec code de retour :

```c
int main(string[] args) {
    if (count(args) < 2) {
        print("Usage : prog <nom>\n");
        return 1;
    }
    print("Bonjour " + args[0] + "\n");
    return 0;
}
```

### 24.15 Tableaux — opérations de base

```c
void main() {
    int[] scores = [10, 20, 30];

    // lecture par indexation
    print(toString(scores[0]));   // affiche 10

    // modification par indexation
    scores[1] = 99;
    print(toString(scores[1]));   // affiche 99

    // longueur
    int n = count(scores);
    print(toString(n));           // affiche 3

    // ajout en fin
    arrayPush(scores, 40);
    print(toString(count(scores))); // affiche 4

    // retrait du dernier
    int last = arrayPop(scores);
    print(toString(last));        // affiche 40

    // insertion à une position
    arrayInsert(scores, 1, 15);
    // scores vaut [10, 15, 99, 30]

    // suppression à une position
    arrayRemove(scores, 2);
    // scores vaut [10, 15, 30]
}
```

### 24.16 Tableaux — parcours avec for

```c
void main() {
    string[] names = ["Alice", "Bob", "Charlie"];

    for (int i = 0; i < count(names); i++) {
        print(names[i]);
    }
}
```

### 24.17 Tableaux — passage à une fonction

```c
int sumArray(int[] arr) {
    int total = 0;
    for (int i = 0; i < count(arr); i++) {
        total = total + arr[i];
    }
    return total;
}

int main() {
    int[] values = [1, 2, 3, 4, 5];
    int s = sumArray(values);
    print(toString(s));   // affiche 15
    return 0;
}
```

### 24.18 Tableaux — modification dans une fonction (passage par référence)

```c
void doubleAll(int[] arr) {
    for (int i = 0; i < count(arr); i++) {
        arr[i] = arr[i] * 2;
    }
}

void main() {
    int[] values = [1, 2, 3];
    doubleAll(values);
    // values vaut [2, 4, 6]
    print(toString(values[0])); // affiche 2
}
```

### 24.19 Fichiers — lecture et écriture

```c
void main() {
    // Écrire un fichier
    writeFile("hello.txt", "Bonjour Cimple\n");

    // Lire le fichier entier
    string content = readFile("hello.txt");
    print(content);   // affiche : Bonjour Cimple

    // Ajouter une ligne
    appendFile("hello.txt", "Deuxième ligne\n");

    // Vérifier l'existence
    if (fileExists("hello.txt")) {
        print("fichier présent\n");
    }
}
```

### 24.20 Fichiers — lecture ligne par ligne

```c
void main() {
    writeLines("noms.txt", ["Alice", "Bob", "Charlie"]);

    string[] lines = readLines("noms.txt");
    for (int i = 0; i < count(lines); i++) {
        print(toString(i + 1) + ". " + lines[i] + "\n");
    }
}
```

Produit :

```
1. Alice
2. Bob
3. Charlie
```

### 24.21 Fichiers — lecture conditionnelle avec fileExists

```c
void main(string[] args) {
    if (count(args) < 1) {
        print("Usage : prog <fichier>\n");
        return;
    }

    string path = args[0];

    if (!fileExists(path)) {
        print("Erreur : fichier introuvable : " + path + "\n");
        return;
    }

    string[] lines = readLines(path);
    print("Lu " + toString(count(lines)) + " ligne(s).\n");
}
```

### 24.22 Exécution de commandes — cas de base

```c
void main() {
    ExecResult r = exec(["git", "status", "--short"], []);

    if (execStatus(r) == 0) {
        string out = execStdout(r);
        if (len(out) == 0) {
            print("Dépôt propre.\n");
        } else {
            print(out);
        }
    } else {
        print("Erreur git :\n" + execStderr(r));
    }
}
```

### 24.23 Exécution de commandes — variables d'environnement

```c
void main() {
    // Passer des variables d'environnement au sous-processus
    ExecResult r = exec(
        ["python3", "-c", "import os; print(os.environ.get('MON_VAR', '?'))"],
        ["MON_VAR=bonjour", "LANG=fr_FR.UTF-8"]
    );

    print("Code : " + toString(execStatus(r)) + "\n");
    print("Stdout : " + execStdout(r));
}
```

### 24.24 Exécution de commandes — capture et traitement de la sortie

```c
void main() {
    ExecResult r = exec(["ls", "-1"], []);

    if (execStatus(r) != 0) {
        print("Erreur : " + execStderr(r));
        return;
    }

    // Découper la sortie ligne par ligne
    string[] files = split(execStdout(r), "\n");
    print(toString(count(files)) + " fichier(s) :\n");
    for (int i = 0; i < count(files); i++) {
        if (len(files[i]) > 0) {
            print("  " + files[i] + "\n");
        }
    }
}
```

### 24.25 Lecture de variables d'environnement

```c
void main() {
    string home  = getEnv("HOME", "/tmp");
    string port  = getEnv("PORT", "8080");
    string debug = getEnv("DEBUG", "0");

    print("Répertoire personnel : " + home + "\n");
    print("Port d'écoute       : " + port + "\n");

    if (debug == "1") {
        print("Mode debug activé\n");
    }
}
```

Combinaison avec `exec` — transmettre l'environnement courant enrichi :

```c
void main() {
    string env_path = getEnv("PATH", "");
    string tool = getEnv("MY_TOOL", "echo");

    ExecResult r = exec([tool, "bonjour"], ["PATH=" + env_path]);
    print(execStdout(r));
}
```

### 24.26 Temps et dates

```c
void main() {
    // Instant courant
    int ts = now();
    print("Epoch ms : " + toString(ts) + "\n");

    // Décomposition en composants UTC
    print("Année   : " + toString(epochToYear(ts))   + "\n");
    print("Mois    : " + toString(epochToMonth(ts))  + "\n");
    print("Jour    : " + toString(epochToDay(ts))    + "\n");
    print("Heure   : " + toString(epochToHour(ts))   + "\n");
    print("Minute  : " + toString(epochToMinute(ts)) + "\n");
    print("Seconde : " + toString(epochToSecond(ts)) + "\n");

    // Formatage
    print(formatDate(ts, "Y-m-d") + "\n");           // "2025-03-11"
    print(formatDate(ts, "H:i:s")   + "\n");          // "14:32:07"
    print(formatDate(ts, "Y-m-d H:i:s") + "\n");      // "2025-03-11 14:32:07"
    print(formatDate(ts, "d/m/Y") + "\n");             // "11/03/2025"

    // Horodatage de log
    string entree = formatDate(now(), "[Y-m-d H:i:s] ") + "Démarrage\n";
    print(entree);

    // Mesure de durée
    int t0 = now();
    int somme = 0;
    for (int i = 0; i < 1000000; i++) {
        somme = somme + i;
    }
    int t1 = now();
    print("Durée : " + toString(t1 - t0) + " ms\n");

    // Construction d'un epoch à partir de composants
    int debut = makeEpoch(2025, 1, 1, 0, 0, 0);
    if (debut == -1) {
        print("Date invalide\n");
    } else {
        print("Début 2025 : " + formatDate(debut, "Y-m-d") + "\n");
    }

    // Erreur : composants invalides
    int mauvais = makeEpoch(2025, 13, 1, 0, 0, 0);  // mois 13 → -1
    print(toString(mauvais) + "\n");   // → "-1"
}
```

---

## 25. Décisions normatives

Les décisions suivantes sont **normatives** et s'appliquent sans exception :

1. un programme Cimple est défini par un **fichier racine** unique passé à `cimple run` ou `cimple check` ; ce fichier racine peut importer d'autres fichiers `.ci` via la directive `import` (voir section 5.2) ; l'ensemble est fusionné en un AST unique avant l'analyse sémantique ; il n'existe pas de notion de module ni d'espace de noms ;
2. pas de modules ;
3. variables globales autorisées, y compris les tableaux globaux ;
4. tableaux dynamiques homogènes supportés pour les types `int[]`, `float[]`, `bool[]`, `string[]` ;
5. pas de tableaux de tableaux (tableaux 2D) ;
6. pas de `void[]` ;
7. les tableaux sont passés par référence aux fonctions ; les scalaires sont passés par valeur ;
8. l'indexation `array[i]` en lecture est une expression ; l'affectation `array[i] = value` est une instruction autonome ;
9. tout accès hors bornes est une erreur runtime avec message clair (indice, longueur, ligne, colonne) ;
10. pas de structures utilisateur ;
11. pas de fonctions imbriquées ;
12. pas d'objets ;
13. **pas de surcharge**, ni dans le code utilisateur, ni dans la bibliothèque standard ; les fonctions intrinsèques sur tableaux (`count`, `arrayPush`, etc.) sont polymorphes au niveau du runtime uniquement et ne constituent pas une surcharge au sens du langage ;
14. pas de cast implicite ;
15. chaînes immutables : aucune opération ne modifie une chaîne en place ;
16. pas de type `char` ; l'accès à un élément d'une chaîne par `s[i]` retourne une `string` d'un octet UTF-8 ;
17. `s[i]` en lecture est une expression `string` valide sur les chaînes ; `s[i] = ...` est une erreur sémantique (chaîne immutable) ; l'affectation par indexation `array[i] = value` est réservée aux tableaux ;
18. comparaisons exactes sur `float` autorisées, mais `approxEqual` fait partie de la bibliothèque standard ;
19. `toInt(float)` utilise une troncature vers zéro ;
20. opérateurs bitwise autorisés sur `int` uniquement ;
21. `float` suit IEEE et peut produire `NaN`, `+Infinity`, `-Infinity` ;
22. l'affectation scalaire n'est pas une expression ; `i++` et `i--` (forme postfixe uniquement) sont des instructions autonomes uniquement, jamais des sous-expressions ; les formes préfixes `++i` et `--i` sont interdites ;
23. `print` imprime uniquement des `string` ;
24. `void main()` est une forme valide du point d'entrée ; les quatre formes `int main()`, `void main()`, `int main(string[] args)`, `void main(string[] args)` sont toutes valides ; toute autre signature de `main` est une erreur sémantique ;
25. les entrées/sorties fichiers sont exclusivement texte UTF-8 ; toute erreur d'I/O est une erreur runtime avec message clair incluant le chemin et la nature de l'erreur ; `fileExists` ne lève jamais d'erreur runtime ; `remove` lève une erreur runtime si le fichier n'existe pas, s'il s'agit d'un répertoire, ou si les permissions sont insuffisantes ; `chmod` lève une erreur runtime si le chemin n'existe pas, si les permissions sont insuffisantes, ou si la plateforme est Windows ou WebAssembly ; `copy` et `move` écrasent silencieusement `dst` s'il existe ; `move` gère de manière transparente les déplacements inter-systèmes de fichiers (copie + suppression) ; `isReadable`, `isExecutable`, `isDirectory` retournent `false` sur chemin inexistant, sans erreur runtime ; `isWritable` teste le fichier lui-même s'il existe, le répertoire parent sinon, et retourne `false` si le parent n'existe pas ; `dirname`, `basename`, `filename`, `extension`, `cwd` sont des calculs purs ne levant jamais d'erreur runtime ;
26. `ExecResult` est le seul type composite de Cimple ; il est opaque, produit uniquement par `exec`, non tableautable (`ExecResult[]` interdit), non déclarable sans initialisation ; un code de retour non nul du sous-processus n'est pas une erreur runtime Cimple ;
27. la détection d'absence de `return` dans une fonction non `void` est conservative : tout chemin non garanti produit une erreur sémantique ;
28. toutes les fonctions de temps opèrent en **UTC** ; les fuseaux horaires locaux ne sont pas pris en charge ; `now()` retourne l'epoch Unix en millisecondes sous forme d'`int` (`int64_t`) ; `makeEpoch` retourne `-1` sur composants invalides et jamais d'erreur runtime ; `formatDate` ne lève pas d'erreur runtime ;
29. tous les messages d'erreur sont émis en **anglais**, quelle que soit la locale du système ; la sortie de diagnostic est exclusivement sur **`stderr`** ; aucune coloration ANSI n'est produite ;
30. les erreurs lexicales et syntaxiques sont **fatales et immédiates** (arrêt à la première erreur) ; les erreurs sémantiques sont **accumulées** jusqu'à 10 avant arrêt ; les erreurs runtime sont **fatales et immédiates** ;
31. les codes de sortie sont : `0` succès, `1` erreur lexicale/syntaxique/sémantique, `2` erreur runtime ; ces codes **écrasent** le code de retour du programme utilisateur en cas d'erreur ; il n'existe pas de niveau « avertissement » ;
32. l'implémentation doit être livrée avec un fichier `MANUAL.md` en anglais, auto-suffisant, couvrant les parties I à VIII définies en section 31 ; le manuel est normatif — tout écart entre le manuel et le comportement réel de l'implémentation est un bug ; le numéro de version du manuel doit correspondre à celui de la spec ;
33. l'implémentation est écrite en **C** (standard C99 ou C11) exclusivement ; aucun autre langage n'est autorisé ; re2c génère du C pur ; Lemon est un fichier C unique ; l'ensemble du projet est compilable avec GCC, Clang et MSVC sans modification du code source ;
34. les plateformes cibles sont : **macOS** (Apple Silicon et Intel), **Linux**, **Unix** (POSIX), **Windows** (MSVC/MinGW/Clang) et **WebAssembly** (Emscripten, cible `wasm32`) ; `exec` et `getEnv` ont un comportement dégradé défini sous WebAssembly (voir section 4.4) ;
35. la suite de tests est **normative** : tout bug corrigé doit être accompagné d'un test de non-régression ajouté avant la correction ; la suite est composée de scripts shell POSIX pilotés par CTest (`make test`) ; les tests positifs, négatifs, de non-régression, de parité WebAssembly et des exemples du manuel doivent tous passer avec code `0` avant toute publication ; la couverture minimale est ≥ 555 tests définie en sections 28.11 et 28.12 ; tout exemple présent dans `MANUAL.md` doit avoir un test correspondant dans `tests/manual/` ;
36. la directive `import` est le seul mécanisme d'inclusion de fichiers ; `import` est un mot-clé du langage reconnu par le lexer ; les imports sont résolus avant l'analyse sémantique par fusion récursive en profondeur d'abord ; les imports circulaires sont une erreur sémantique ; la profondeur maximale est 32 ; les fichiers importés ne peuvent pas définir `main` ; les redéfinitions de fonctions ou variables globales via import sont des erreurs sémantiques ; un fichier importé plusieurs fois est traité une seule fois (déduplication par chemin absolu résolu) ; les messages d'erreur indiquent toujours le fichier source d'origine ;
37. `byte` est un type scalaire entier non signé 8 bits (0–255) ; les opérateurs `+ - * / %` et `<< >>` sur `byte` produisent un `int` ; les opérateurs `& | ^ ~` sur deux `byte` produisent un `byte` ; les opérations mixtes `byte op int` produisent un `int` ; `intToByte(n)` applique un clamp (n<0→0, n>255→255) sans erreur runtime ; les littéraux entiers dans `[0,255]` sont affectables directement à un `byte` ; l'affectation d'une variable `int` à un `byte` sans `intToByte()` est une erreur sémantique ; `bytesToString` remplace les séquences UTF-8 invalides par U+FFFD sans erreur runtime ; `intToBytes` et `floatToBytes` sont des dumps mémoire bruts (`memcpy`) dans l'ordre natif de la machine hôte, sans interprétation ni transformation ; `bytesToInt` et `bytesToFloat` sont les opérations symétriques ; la taille du tableau est vérifiée (4 octets pour `int`, 8 pour `float`), toute autre taille est une erreur runtime ; NaN et ±Infinity sont valides comme résultat de `bytesToFloat` ; le round-trip est garanti sur la même machine.

38. les structures sont des types composites définis par l'utilisateur, entièrement statiques, de taille fixe connue à la compilation ; le mot-clé est `structure` ; les noms de structure sont des identifiants quelconques ; les champs de type scalaire ou tableau ont une valeur par défaut implicite (`0`, `0.0`, `false`, `""`, `0`, `[]`) si non précisée ; les champs de type structure doivent être explicitement initialisés avec `clone NomDeStructure` ; l'héritage est simple (une seule base), en chaîne autorisé, sans cycle ; la redéfinition de champ dans une sous-structure exige le même type mais autorise une valeur par défaut différente ; l'override de méthode exige une signature identique ; `super.méthode()` appelle la méthode de la base directe ; `super.champ` est interdit ; `clone NomDeStructure` est le seul opérateur de construction — `clone variable` est une erreur sémantique ; les champs récursifs sont interdits ; les structures sont passées par référence ; `NomDeStructure[]` est un type tableau valide.

39. les opérateurs d'affectation composée `+=`, `-=`, `*=`, `/=`, `%=` sont des instructions autonomes (pas des expressions) ; ils s'appliquent aux variables de type `int` ou `float` uniquement ; le type de l'expression droite doit correspondre au type de la variable ; une division ou un modulo par zéro produit une erreur runtime ; ils sont autorisés dans la clause de mise à jour d'un `for`.

40. l'opérateur ternaire `?:` est une expression ; la condition doit être de type `bool` ; les deux branches doivent être du même type ; l'évaluation est paresseuse (court-circuit) ; l'opérateur est associatif à droite ; sa priorité est inférieure à `||` et supérieure aux affectations.

41. `assert(condition)` et `assert(condition, message)` sont des fonctions intrinsèques ; si la condition est `false`, le programme se termine immédiatement avec le code 1 et affiche `[ASSERTION FAILED]` sur stderr ; les assertions ne sont pas désactivables.

42. `randInt(min, max)` retourne un entier pseudo-aléatoire dans `[min, max]` (intervalle fermé) ; `min > max` est une erreur runtime ; `randFloat()` retourne un flottant dans `[0.0, 1.0)` ; le générateur est initialisé par `srand(time(NULL))` au démarrage du programme.

43. `sleep(ms)` suspend l'exécution pendant `ms` millisecondes (POSIX : `usleep`, Windows : `Sleep`) ; non disponible sur WebAssembly (erreur runtime).

44. une fonction peut être passée comme argument à une autre fonction en utilisant la syntaxe de signature comme type de paramètre (`bool cmp(int, int)`) ; la vérification est statique ; seules les fonctions nommées déclarées au niveau global sont passables — les lambdas ne sont pas supportées ; une variable de type fonction se déclare et s'appelle avec la même syntaxe.

45. les unions discriminées (`union`) déclarent un type pouvant contenir exactement un membre actif à la fois ; le compilateur ajoute automatiquement un champ `.kind` (lecture seule) et des constantes symboliques `NomUnion.NOM_CHAMP` ; l'assignation d'un champ met à jour `.kind` automatiquement ; lire un champ dont le kind ne correspond pas est une erreur runtime ; un `switch` non exhaustif sur `.kind` produit un avertissement sémantique ; les membres peuvent être de tout type scalaire, tableau, ou `void` ; un membre `void` représente un état sans donnée (motif « Option/nullable ») et s'active en écrivant `x.membreVoid;` comme instruction.

46. toutes les méthodes de structure sont virtuelles par défaut ; la redéfinition d'une méthode dans une sous-structure est détectée par correspondance exacte de signature ; les tableaux sont covariants (`Base[]` peut contenir des instances de sous-structures) ; le dispatch dynamique s'applique aux appels de méthode sur tout récepteur dont le type déclaré est une structure ; les champs ne sont pas virtuels.
---

## 26. Livrables attendus

L'implémentation devra produire :

1. le code source du lexer (généré par re2c, en C) ;
2. le fichier de grammaire Lemon (en C) ;
3. le code AST (en C) ;
4. l'analyse sémantique (en C) ;
5. l'interpréteur AST (en C) ;
6. la bibliothèque native telle que définie dans cette spécification (en C) ;
7. un CLI (`cimple run` / `cimple check`) ;
8. un fichier `CMakeLists.txt` pour le build natif (CMake ≥ 3.15) ;
9. un fichier `toolchain-emscripten.cmake` pour la cible WebAssembly (Emscripten) ;
10. une suite de tests conforme aux sections 28.11 et 28.12 : scripts shell POSIX dans `tests/`, runner `run_tests.sh`, intégration CTest (`make test`), tests positifs, négatifs, de non-régression, de parité WebAssembly et des exemples du manuel (`tests/manual/`) ;
11. `MANUAL.md` — le manuel de référence utilisateur en anglais (section 31).

---

## 27. CLI attendue

Exemples de commandes :

```bash
cimple run hello.ci
cimple check hello.ci
```

Comportement :

- `run` : résout les imports, parse, vérifie, exécute ;
- `check` : résout les imports, parse et vérifie sans exécuter ; retourne `0` si aucune erreur dans le fichier racine **ni dans aucun fichier importé**, `1` sinon.

La CLI n'accepte qu'un seul fichier en argument (le fichier racine) ; les fichiers importés sont découverts automatiquement via les directives `import`. Il n'est pas possible de passer plusieurs fichiers sur la ligne de commande.

L'extension source officielle de Cimple est `.ci`.

Exemples :

- `main.ci`
- `hello.ci`
- `strings.ci`
- `math.ci`

Cette extension doit être utilisée de façon cohérente dans la CLI, les exemples, les tests et la documentation.

---

## 28. Infrastructure de tests

### 28.1 Principes généraux

La suite de tests est la **référence exécutable de la spécification**. Tout comportement décrit dans ce document doit être couvert par au moins un test. Un comportement non couvert par un test est considéré non garanti.

Les tests suivent le modèle des suites d'acceptation des langages de référence (Ruby, Python, SQLite) : chaque test est un programme source Cimple autonome, accompagné de la sortie attendue et/ou du code de sortie attendu. Les tests sont exécutés par des **scripts shell POSIX** (`/bin/sh`) pilotés par **`make test`** via **CMake/CTest**.

**Règles fondamentales :**

- tout bug corrigé doit être accompagné d'un test de non-régression ajouté dans la suite avant la correction ; ce test doit échouer avant la correction et passer après ;
- aucun test ne doit dépendre d'un autre ; chaque test est indépendant et reproductible dans n'importe quel ordre ;
- les tests doivent passer sur les cinq plateformes cibles : macOS, Linux, Unix, Windows et WebAssembly ;
- la suite doit pouvoir être lancée sans connexion réseau, sans variable d'environnement spécifique, et sans droits administrateur.

### 28.2 Structure des répertoires

```
tests/
  run_tests.sh              # runner principal POSIX
  lib/
    assert.sh               # fonctions d'assertion partagées
  positive/                 # programmes valides — vérification stdout + code 0
    lexer/
    syntax/
    types/
    variables/
    operators/
    strings/
    arrays/
    functions/
    control_flow/
    stdlib/
      math/
      strings/
      files/
      exec/
      time/
      env/
    constants/
    unicode/
    main_signatures/
    byte/                 # type byte, byte[], opérateurs, conversions
    structures/           # déclaration, héritage, clone, self, super, tableaux
    stdlib/
      binary/             # readFileBytes, writeFileBytes, appendFileBytes
    imports/                # programmes utilisant import (positifs)
  negative/                 # programmes invalides — vérification erreur + code 1 ou 2
    lexical/
    syntax/
    semantic/
      imports/              # erreurs sémantiques spécifiques aux imports
    runtime/
  regression/               # tests de non-régression (un sous-répertoire par bug corrigé)
    bug-001/
    bug-002/
    ...
  wasm/                     # tests de parité CLI ↔ WebAssembly
    run_wasm_tests.sh
CMakeLists.txt              # intègre CTest, cible `make test`
```

### 28.3 Format d'un test

Chaque test est un répertoire contenant trois fichiers :

```
tests/positive/strings/replace_basic/
  input.ci          # programme source Cimple
  expected_stdout   # sortie stdout attendue (peut être vide)
  expected_exit     # code de sortie attendu : "0", "1" ou "2"
```

Pour les tests négatifs, un quatrième fichier est obligatoire :

```
tests/negative/semantic/assign_to_constant/
  input.ci
  expected_stderr   # fragment de message d'erreur attendu (sous-chaîne)
  expected_exit     # "1" ou "2"
```

`expected_stderr` contient une **sous-chaîne** que le message sur `stderr` doit contenir. La vérification est une inclusion, pas une égalité exacte, pour rester robuste aux reformulations mineures du message tout en garantissant la présence des éléments clés (niveau d'erreur, nom de variable, ligne).

Exemple de `expected_stderr` :

```
[SEMANTIC ERROR]
Cannot assign to predefined constant 'M_PI'
```

Un test peut avoir un fichier optionnel `args` contenant les arguments CLI à passer au programme (une ligne = un argument) :

```
tests/positive/main_signatures/int_main_args/
  input.ci
  args              # "hello\nworld"  → cimple run input.ci hello world
  expected_stdout
  expected_exit
```

### 28.4 Script d'assertion partagé — `lib/assert.sh`

```sh
#!/bin/sh
# lib/assert.sh — fonctions d'assertion pour les scripts de test Cimple

PASS=0
FAIL=0
ERRORS=""

assert_exit() {
    expected="$1"
    actual="$2"
    test_name="$3"
    if [ "$actual" != "$expected" ]; then
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] exit: expected=$expected actual=$actual"
    else
        PASS=$((PASS + 1))
    fi
}

assert_stdout() {
    expected_file="$1"
    actual="$2"
    test_name="$3"
    expected=$(cat "$expected_file")
    if [ "$actual" != "$expected" ]; then
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] stdout mismatch"
    else
        PASS=$((PASS + 1))
    fi
}

assert_stderr_contains() {
    fragment="$1"
    actual_stderr="$2"
    test_name="$3"
    if ! printf '%s' "$actual_stderr" | grep -qF "$fragment"; then
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] stderr does not contain: $fragment"
    else
        PASS=$((PASS + 1))
    fi
}

print_summary() {
    total=$((PASS + FAIL))
    printf "\n%d/%d tests passed\n" "$PASS" "$total"
    if [ "$FAIL" -gt 0 ]; then
        printf "$ERRORS\n"
        exit 1
    fi
}
```

### 28.5 Runner principal — `run_tests.sh`

```sh
#!/bin/sh
# run_tests.sh — runner principal de la suite de tests Cimple
set -e

CIMPLE="${CIMPLE_BIN:-cimple}"
TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$TESTS_DIR/lib/assert.sh"

run_test() {
    dir="$1"
    name="$(basename "$dir")"

    src="$dir/input.ci"
    [ -f "$src" ] || return

    # Lecture des fichiers attendus
    expected_exit=$(cat "$dir/expected_exit" 2>/dev/null || echo "0")
    expected_stdout="$dir/expected_stdout"
    expected_stderr="$dir/expected_stderr"

    # Construction des arguments optionnels
    args=""
    if [ -f "$dir/args" ]; then
        args=$(cat "$dir/args")
    fi

    # Exécution
    actual_stdout=$(eval "$CIMPLE run $src $args" 2>/tmp/cimple_stderr_$$)
    actual_exit=$?
    actual_stderr=$(cat /tmp/cimple_stderr_$$ 2>/dev/null)
    rm -f /tmp/cimple_stderr_$$

    # Assertions
    assert_exit "$expected_exit" "$actual_exit" "$name"

    if [ -f "$expected_stdout" ]; then
        assert_stdout "$expected_stdout" "$actual_stdout" "$name"
    fi

    if [ -f "$expected_stderr" ]; then
        while IFS= read -r fragment; do
            [ -z "$fragment" ] && continue
            assert_stderr_contains "$fragment" "$actual_stderr" "$name"
        done < "$expected_stderr"
    fi
}

# Parcours de tous les tests
for category in positive negative regression; do
    find "$TESTS_DIR/$category" -name "input.ci" | sort | while read -r src; do
        run_test "$(dirname "$src")"
    done
done

print_summary
```

### 28.6 Intégration CMake / CTest

Dans `CMakeLists.txt` :

```cmake
enable_testing()

# Test runner principal
add_test(
    NAME cimple_test_suite
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(cimple_test_suite PROPERTIES
    ENVIRONMENT "CIMPLE_BIN=$<TARGET_FILE:cimple>"
    TIMEOUT 120
)

# Tests WebAssembly (optionnel, activé si Emscripten disponible)
if(EMSCRIPTEN)
    add_test(
        NAME cimple_wasm_test_suite
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/wasm/run_wasm_tests.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties(cimple_wasm_test_suite PROPERTIES
        TIMEOUT 180
    )
endif()
```

Invocation :

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure   # équivalent make test
```

Un alias `make test` peut être ajouté via une cible CMake custom :

```cmake
add_custom_target(test
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS cimple
)
```

### 28.7 Tests de non-régression

**Règle normative :** tout bug corrigé donne lieu à la création d'un répertoire dans `tests/regression/` avant que la correction ne soit appliquée. Le test doit :

1. échouer avec la version buggée du compilateur ;
2. passer avec la version corrigée ;
3. continuer à passer dans toutes les versions ultérieures.

**Convention de nommage :**

```
tests/regression/
  bug-001-array-oob-negative-index/
  bug-002-format-zero-args/
  bug-003-makeepoch-feb-29/
  ...
```

Chaque répertoire contient en plus des fichiers standard un fichier `description.txt` :

```
Bug: arrayGet with negative index did not produce a runtime error
Reported: 2025-03-15
Fixed in: v1.1
```

### 28.8 Tests positifs — catalogue de couverture

Chaque ligne ci-dessous correspond à au moins un répertoire de test dans `tests/positive/`.

**Lexer**
- littéraux entiers : décimal, hexadécimal (`0xFF`), binaire (`0b1010`), octal (`0o17`)
- littéraux flottants : décimal, scientifique (`1.5e3`, `1e-2`, `.5`)
- toutes les séquences d'échappement : `\"` `\\` `\n` `\t` `\r` `\b` `\f` `\uXXXX`
- commentaires sur une ligne (`//`) : ignorés, ne produisent pas de token

**Types et variables**
- déclaration et affectation de `int`, `float`, `bool`, `string`
- déclaration sans initialisation (valeur par défaut)
- variables globales scalaires et tableaux
- portée locale vs globale : la variable locale masque la globale

**Opérateurs**
- arithmétiques `+ - * / %` sur `int` et `float`
- comparaisons `== != < <= > >=` sur tous les types scalaires
- logiques `&& || !`
- bitwise `& | ^ ~ << >>` sur `int` uniquement
- concaténation `+` sur `string`
- `i++` et `i--` : post-seulement, instruction autonome
- précédence : vérification de `2 + 3 * 4 == 14`

**Chaînes**
- `len` sur chaîne ASCII, chaîne UTF-8 multi-octets
- `substr`, `indexOf`, `contains`, `startsWith`, `endsWith`
- `replace` : toutes occurrences, `new == ""` (suppression), `old` absent
- `format` : 0, 1, 3 marqueurs ; séparateurs conservés
- `join`, `split`, `concat`
- `s[i]` en lecture : ASCII, résultat de type `string`
- `byteAt` : valeur ASCII, valeur multi-octets
- `glyphLen` vs `len` sur chaîne multi-octets
- `glyphAt` : NFC sur entrée NFD
- immutabilité : `replace` retourne une nouvelle chaîne, `s` inchangé
- `trim` : espaces en début et fin, tabs, newlines, chaîne entièrement vide → `""`
- `trimLeft` / `trimRight` : asymétrie vérifiée (début seul, fin seule)
- `toUpper` : ASCII, accents (`é`→`É`), `ß`→`SS`, déjà en majuscule inchangé
- `toLower` : ASCII, accents (`É`→`é`), déjà en minuscule inchangé
- `capitalize` : première lettre uniquement, reste inchangé ; `hELLO` → `HELLO` ; `""` → `""`
- `padLeft` : padding simple, pad multi-char tronqué, `glyphLen(s) >= width` → inchangé, `pad=""` → inchangé
- `padRight` : idem côté droit, pad multi-char tronqué à droite
- `repeat` : `n=3`, `n=0` → `""`, `s=""` → `""`
- `lastIndexOf` : dernière occurrence, occurrence unique, absent → `-1`
- `countOccurrences` : non chevauchant `"aaaa"/"aa"` → 2, absent → 0
- `isBlank` : `""` → `true`, `"   "` → `true`, `"\t\n"` → `true`, `"  x"` → `false`

**Tableaux**
- déclaration avec littéral et sans initialisation
- `array[i]` en lecture et `array[i] = v` en écriture
- `count`, `arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`, `arrayGet`, `arraySet`
- passage par référence : modification dans une fonction visible à l'appelant
- tableaux des quatre types : `int[]`, `float[]`, `bool[]`, `string[]`
- tableau vide : `count == 0`, `arrayPush` puis `arrayPop` revient à vide

**Flot de contrôle**
- `if` seul, `if/else`, `if/else` imbriqués
- règle du dangling else : associé au `if` le plus proche
- `while` : cas nominal, boucle vide (condition fausse dès le départ)
- `for` avec `i++`, avec `break`, avec `continue`
- `break` : sort de la boucle immédiatement
- `continue` : passe à l'itération suivante

**Fonctions**
- déclaration et appel
- `return` de chaque type scalaire
- `void` : pas de valeur de retour
- récursion : factorielle, Fibonacci
- fonctions s'appelant mutuellement (déclarées avant usage)
- variable locale masquant un paramètre de même nom

**Signatures de `main`**
- `void main()` : code de sortie `0`
- `int main()` : code de sortie = valeur retournée
- `void main(string[] args)` : `count(args)` correct avec arguments
- `int main(string[] args)` : combinaison des deux

**Stdlib — maths**
- `abs`, `min`, `max`, `floor`, `ceil`, `round`, `trunc`, `fmod`, `sqrt`, `pow`, `approxEqual`
- `sin(0) == 0`, `cos(0) == 1`, `tan(π/4) ≈ 1`
- `asin(1) ≈ π/2`, `atan2(1,1) ≈ π/4`
- `exp(1) ≈ M_E`, `log(M_E) ≈ 1`, `log2(8) == 3`, `log10(100) == 2`
- `log(0) == -Infinity`, `log(-1) == NaN` (via `isNaN`)
- `absInt`, `minInt`, `maxInt`, `clampInt`, `isEven`, `isOdd`, `safeDivInt`, `safeModInt`
- `isNaN`, `isInfinite`, `isFinite`, `isPositiveInfinity`, `isNegativeInfinity`

**Constantes prédéfinies**
- `INT_MAX`, `INT_MIN`, `INT_SIZE` : type `int`, valeurs dans les plages attendues
- `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX` : type `float`
- `M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10` : valeurs connues à 10 décimales
- `1.0 + FLOAT_EPSILON != 1.0` : vrai
- `-FLOAT_MAX` : valeur négative, `isFinite(-FLOAT_MAX) == true`

**Stdlib — fichiers**
- `readFile` / `writeFile` : aller-retour sur contenu UTF-8
- `appendFile` : accumulation sur plusieurs appels
- `fileExists` : vrai, faux, faux sur répertoire
- `readLines` / `writeLines` : aller-retour, normalisation `\r\n` → `\n`
- fichier vide : `readLines` retourne `[]`, `readFile` retourne `""`
- `tempPath` : retourne une `string` non vide, chemin inexistant (`fileExists` → `false`), deux appels → chemins distincts
- `remove` : créer puis supprimer un fichier ; `fileExists` → `false` après suppression
- `chmod` : `chmod(path, 0644)` sur fichier existant sans erreur (POSIX uniquement)
- `cwd` : retourne une `string` non vide commençant par `/` (POSIX) ou lettre de lecteur (Windows)
- `copy` : copie d'un fichier, contenu identique, src toujours présent ; écrasement silencieux si dst existe
- `move` : déplacement d'un fichier, src absent après, contenu correct dans dst ; écrasement silencieux si dst existe
- `isReadable` : `true` sur fichier lisible, `false` sur chemin inexistant
- `isWritable` : `true` sur fichier existant accessible en écriture ; `true` sur chemin inexistant si parent accessible ; `false` si parent inexistant
- `isExecutable` : `true` après `chmod(path, 0755)`, `false` après `chmod(path, 0644)`
- `isDirectory` : `true` sur répertoire existant, `false` sur fichier, `false` sur inexistant
- `dirname` : `/path/to/file.txt` → `/path/to` ; `file.txt` → `""` ; `/` → `"/"` ; `a/b` → `"a"`
- `basename` : `/path/to/my.file.txt` → `my.file.txt` ; `file.txt` → `file.txt` ; `/path/to/` → `""`
- `filename` : `my.file.txt` → `my.file` ; `archive.tar.gz` → `archive.tar` ; `makefile` → `makefile` ; `.gitignore` → `.gitignore`
- `extension` : `my.file.txt` → `txt` ; `archive.tar.gz` → `gz` ; `makefile` → `""` ; `.gitignore` → `""`

**Stdlib — exec**
- commande retournant code 0, capture de stdout
- commande retournant code non nul : `execStatus` correct, pas d'erreur Cimple
- capture de stderr séparée
- passage de variables d'environnement via `env`
- `env` vide `[]` : héritage de l'environnement courant

**Stdlib — temps**
- `now()` : type `int`, positif, deux appels consécutifs non décroissants
- aller-retour `makeEpoch` → `epochTo*` sur date connue (`2025-03-11 14:32:07`)
- `formatDate` : toutes les lettres sur epoch connu — `Y`, `m`, `d`, `H`, `i`, `s`, `w`, `z`, `W`, `c`
- `formatDate` : séparateurs libres copiés tels quels, `fmt` vide → `""`
- `formatDate("w")` : dimanche → `"0"`, lundi → `"1"`, samedi → `"6"`
- `formatDate("z")` : 1er janvier → `"0"`, 31 décembre non-bissextile → `"364"`, 31 décembre bissextile → `"365"`
- `formatDate("W")` : semaine 1 → `"01"`, semaine 42 → `"42"`, semaine 53 sur année longue
- `formatDate("c")` : résultat de la forme `Y-m-dTH:i:sZ`
- `formatDate("Y-W-w")` : numéro de semaine ISO suivi du jour — ex. `"2025-11-2"` (pour ISO week 11, mardi)
- `formatDate("\\Y")` : retourne la lettre `Y` littérale (échappement backslash)
- `makeEpoch` : composants valides → epoch positif

**Stdlib — environnement**
- `getEnv` : variable existante, variable absente → fallback, variable vide → `""`

**Conversions**
- `toString(int)`, `toString(float)`, `toString(bool)`
- `toInt(float)` : troncature vers zéro sur positifs et négatifs
- `toInt(string)` : entier valide, non entier → `0`
- `toFloat(int)`, `toFloat(string)` : valide, non float → `NaN`
- `toBool(string)` : `"true"`, `"false"`, `"1"`, `"0"`, non reconnu → `false`
- `isIntString`, `isFloatString`, `isBoolString`

**Structures**
- déclaration simple : champs avec valeurs par défaut, méthodes, `self`
- `clone` : instance créée avec valeurs par défaut ; deux `clone` indépendants
- modification de champ via `self` : `self.f = self.f + 1.0`
- appel de méthode : `s.myMethod()`
- accès en lecture : `s.myField`
- héritage simple : sous-structure hérite des champs et méthodes de la base
- override : méthode redéfinie dans la sous-structure masque celle de la base
- `super.myMethod()` : appelle la méthode de la base depuis la sous-structure
- héritage en chaîne : `A : B : C`, méthodes et champs hérités correctement
- redéfinition de champ valide : même type, même valeur par défaut
- champ de type structure : `inner = clone InnerStruct`, accès `outer.inner.field`
- tableau de structures : `MyStruct[] arr = []`, `arrayPush(arr, clone MyStruct)`, `arr[0].field`
- passage par référence : modification d'un champ dans une fonction globale visible à l'extérieur
- exemple complet du résumé : `MyBasicStructure` / `MyStructure`, `s.myFunction()` → `s.f == 2.3`

**Type `byte` et `byte[]`**
- déclaration : `byte b = 0xFF;`, `byte b = 255;`, `byte b = intToByte(128);`
- arithmétique : `byte + byte` → `int`, `byte + int` → `int` ; valeur correcte sans overflow
- bitwise : `byte & byte` → `byte`, `byte | byte` → `byte`, `~byte` → `byte`
- décalage : `byte << int` → `int`
- `intToByte(300)` → `byte(255)` (clamp haut) ; `intToByte(-5)` → `byte(0)` (clamp bas)
- `byteToInt(0xFF)` → `int(255)`
- littéral `byte[]` : `[255, 0, 128, 255]` ; accès lecture et écriture par index
- `count(byte[])` : longueur correcte
- aller-retour `writeFileBytes` + `readFileBytes` : contenu identique octet par octet
- `stringToBytes("ABC")` → `[65, 66, 67]`
- `stringToBytes("é")` → `[0xC3, 0xA9]` (2 octets UTF-8)
- `bytesToString([72, 101, 108, 108, 111])` → `"Hello"`
- `bytesToString([0xFF])` → `"<U+FFFD>"` (séquence UTF-8 invalide → remplacement)
- aller-retour : `bytesToString(stringToBytes(s)) == s` pour toute chaîne valide
- `intToBytes(n)` : `count == INT_SIZE` garanti ; `intToBytes(1)[0] == 1` détecte little-endian
- `floatToBytes(f)` : `count == FLOAT_SIZE` garanti ; `NaN` et `±Infinity` produisent leurs octets IEEE 754
- `bytesToInt(intToBytes(n)) == n` : round-trip garanti pour tout `int`
- `bytesToFloat(floatToBytes(f)) == f` : round-trip garanti pour tout `float` fini
- `bytesToInt` sur tableau de `INT_SIZE` octets quelconques : résultat potentiellement négatif, pas d'erreur

**Imports**
- import d'un fichier contenant une fonction utilitaire, appelée depuis `main.ci`
- import de deux fichiers indépendants depuis `main.ci`
- import en chaîne : `a.ci` importe `b.ci` qui importe `c.ci`
- import dédupliqué : `a.ci` et `b.ci` importent tous deux `utils.ci` ; `utils.ci` n'est traité qu'une fois
- import d'un fichier définissant une variable globale
- import d'un fichier contenant des fonctions avec paramètres tableau (passage par référence)
- chemin relatif avec sous-répertoire : `import "utils/strings.ci";`
- `import` redondant silencieux : même chemin importé deux fois dans le même fichier

### 28.9 Tests négatifs — catalogue de couverture

Chaque test négatif vérifie : code de sortie `1` ou `2` selon le niveau, et présence dans `stderr` des éléments clés du message.

**Erreurs lexicales (code 1, arrêt immédiat)**

| Test | `expected_stderr` (fragment) |
|---|---|
| Caractère `@` inconnu | `[LEXICAL ERROR]` + `Unexpected character: '@'` |
| Chaîne non fermée | `[LEXICAL ERROR]` + `Unterminated string literal` |
| Séquence `\q` | chaîne contenant littéralement `\q` (deux octets : backslash + q) |
| `\uXXGG` malformé | `[LEXICAL ERROR]` + `Malformed Unicode escape` |
| `0x` sans chiffres | `[LEXICAL ERROR]` + `Invalid hexadecimal literal` |
| `0b` sans chiffres | `[LEXICAL ERROR]` + `Invalid binary literal` |
| Une seule erreur rapportée (deux caractères invalides) | une seule occurrence de `[LEXICAL ERROR]` |

**Erreurs syntaxiques (code 1, arrêt immédiat)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| `;` manquant après expression | `[SYNTAX ERROR]` + `Missing ';'` |
| `)` manquant | `[SYNTAX ERROR]` + `Missing ')'` |
| `}` manquant | `[SYNTAX ERROR]` + `Missing '}'` |
| Type inconnu `foo x;` | `[SYNTAX ERROR]` + `Unknown type: 'foo'` |
| Expression attendue mais `}` trouvé | `[SYNTAX ERROR]` + `Expected expression` |

**Erreurs sémantiques (code 1, accumulation jusqu'à 10)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| Variable non déclarée | `[SEMANTIC ERROR]` + `Undefined variable` |
| Variable déjà déclarée | `[SEMANTIC ERROR]` + `already declared` |
| Fonction inconnue | `[SEMANTIC ERROR]` + `Unknown function` |
| Mauvais nombre d'arguments | `[SEMANTIC ERROR]` + `Wrong number of arguments` |
| Type incompatible dans affectation | `[SEMANTIC ERROR]` + `Type mismatch` |
| `return` de mauvais type | `[SEMANTIC ERROR]` + `Wrong return type` |
| `return` absent | `[SEMANTIC ERROR]` + `Missing return` |
| Affectation à `M_PI` | `[SEMANTIC ERROR]` + `Cannot assign to predefined constant 'M_PI'` |
| Déclaration d'une variable nommée `INT_MAX` | `[SEMANTIC ERROR]` + `reserved identifier` |
| `s[i] = ...` sur une chaîne | `[SEMANTIC ERROR]` + `immutable` |
| `ExecResult` sans initialisation | `[SEMANTIC ERROR]` + `must be initialized` |
| `ExecResult[]` | `[SEMANTIC ERROR]` + `Array of ExecResult` |
| Signature `main` invalide | `[SEMANTIC ERROR]` + `Invalid signature for 'main'` |
| 11 erreurs : arrêt à 10 | `Further analysis aborted.` |
| Accumulation : message récapitulatif | `semantic error(s) found` |

**Erreurs sémantiques — structures (code 1)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| Champ sans valeur par défaut | `[SEMANTIC ERROR]` + `has no default value` |
| `clone s` (variable) | `[SEMANTIC ERROR]` + `Cannot clone a variable` |
| `self` hors méthode | `[SEMANTIC ERROR]` + `'self' used outside` |
| `self = clone MyStruct` | `[SEMANTIC ERROR]` + `Cannot reassign 'self'` |
| `super.myField` | `[SEMANTIC ERROR]` + `Cannot access field via 'super'` |
| Override signature différente | `[SEMANTIC ERROR]` + `overrides` + `different signature` |
| Cycle d'héritage `A : B, B : A` | `[SEMANTIC ERROR]` + `Inheritance cycle` |
| Champ récursif `structure A { A x = clone A; }` | `[SEMANTIC ERROR]` + `Recursive field` |
| Utilisation avant déclaration | `[SEMANTIC ERROR]` + `Unknown` |
| `structure A { B b = clone B; } structure B { }` | `[SEMANTIC ERROR]` + `Unknown` (pas `[SYNTAX ERROR]`) |
| Champ global `foo` et champ `foo` dans une structure | `[SEMANTIC ERROR]` + `conflict` |
| `s.unknownField` | `[SEMANTIC ERROR]` + `Unknown field` |
| `s.unknownMethod()` | `[SEMANTIC ERROR]` + `Unknown method` |
| `42.someField` | `[SEMANTIC ERROR]` + `Member access requires a structure instance` |

**Erreurs sémantiques — `byte` (code 1)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| `byte b = 256;` | `[SEMANTIC ERROR]` + `Byte literal out of range` |
| `byte b = -1;` | `[SEMANTIC ERROR]` + `Byte literal out of range` |
| `int n = 10; byte b = n;` | `[SEMANTIC ERROR]` + `Cannot assign 'int' to 'byte'` |
| `byte b = 0; bool x = b && true;` | `[SEMANTIC ERROR]` + `cannot be applied to type 'byte'` |

**Erreurs sémantiques — imports (code 1)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| Fichier importé introuvable | `[SEMANTIC ERROR]` + `Cannot import file` |
| Extension non `.ci` | `[SEMANTIC ERROR]` + `must end with '.ci'` |
| Import circulaire direct (`a.ci` → `a.ci`) | `[SEMANTIC ERROR]` + `Circular import` |
| Import circulaire indirect (`a` → `b` → `a`) | `[SEMANTIC ERROR]` + `Circular import` + chaîne complète |
| `main` défini dans un fichier importé | `[SEMANTIC ERROR]` + `cannot be defined in an imported file` |
| Redéfinition de fonction via import | `[SEMANTIC ERROR]` + `already declared` + les deux fichiers mentionnés |
| `import` après une déclaration | `[SYNTAX ERROR]` + `'import' must appear before any declaration` |
| `import` dans une fonction | `[SYNTAX ERROR]` + `not allowed inside a function` |
| Message d'erreur dans fichier importé : fichier correct | erreur dans `lib.ci` → `stderr` contient `lib.ci` (pas `main.ci`) |

**Erreurs runtime (code 2, arrêt immédiat)**

| Test | Fragment attendu dans `stderr` |
|---|---|
| `array[5]` avec taille 3 | `[RUNTIME ERROR]` + `Array index out of bounds` + `Index: 5` + `Array size: 3` |
| `array[-1]` | `[RUNTIME ERROR]` + `Array index out of bounds` |
| `arrayPop` sur tableau vide | `[RUNTIME ERROR]` + `Cannot pop from empty array` |
| `s[100]` sur chaîne courte | `[RUNTIME ERROR]` + `String index out of bounds` |
| `byteAt(s, 100)` | `[RUNTIME ERROR]` + `byteAt: index out of bounds` |
| `glyphAt(s, 100)` | `[RUNTIME ERROR]` + `glyphAt: index out of bounds` |
| Division entière par zéro | `[RUNTIME ERROR]` + `Integer division by zero` |
| Modulo par zéro | `[RUNTIME ERROR]` + `Integer modulo by zero` |
| `split(s, "")` | `[RUNTIME ERROR]` + `separator cannot be empty` |
| `replace(s, "", "x")` | `[RUNTIME ERROR]` + `old argument cannot be empty` |
| `format("{}", [])` | `[RUNTIME ERROR]` + `marker count does not match` |
| `repeat("x", -1)` | `[RUNTIME ERROR]` + `count must be non-negative` |
| `countOccurrences(s, "")` | `[RUNTIME ERROR]` + `needle cannot be empty` |
| `padLeft("x", -1, " ")` | `[RUNTIME ERROR]` + `width must be non-negative` |
| `bytesToInt([0x01, 0x02])` | `[RUNTIME ERROR]` + `expected INT_SIZE bytes` |
| `bytesToFloat([0x01, 0x02, 0x03])` | `[RUNTIME ERROR]` + `expected FLOAT_SIZE bytes` |
| `makeEpoch(2025, 13, 1, 0, 0, 0)` | résultat `-1`, pas d'erreur runtime |
| `readFile("nonexistent.txt")` | `[RUNTIME ERROR]` + `Cannot read file` |
| `writeFile("/no/such/dir/f.txt", "x")` | `[RUNTIME ERROR]` + `Cannot write file` |
| `remove` sur fichier inexistant | `[RUNTIME ERROR]` + `Cannot remove file` + `does not exist` |
| `remove` sur un répertoire | `[RUNTIME ERROR]` + `Cannot remove file` + `directory` |
| `chmod` sur chemin inexistant | `[RUNTIME ERROR]` + `Cannot chmod` + `does not exist` |
| `copy` src inexistant | `[RUNTIME ERROR]` + `Cannot copy` + `source file not found` |
| `copy` src est un répertoire | `[RUNTIME ERROR]` + `Cannot copy` + `directory` |
| `move` src inexistant | `[RUNTIME ERROR]` + `Cannot move` + `source file not found` |
| `move` src est un répertoire | `[RUNTIME ERROR]` + `Cannot move` + `directory` |
| `exec([], [])` | `[RUNTIME ERROR]` + `command array must not be empty` |
| `exec(["no_such_cmd_xyz"], [])` | `[RUNTIME ERROR]` + `command not found` |
| Code de sortie runtime = `2` | vérification `expected_exit = 2` |
| Code de sortie runtime écrase `int main` | programme `int main() { ... erreur runtime ... return 42; }` → exit `2` |

**Vérifications transversales des messages d'erreur**

| Test | Vérification |
|---|---|
| Pas de codes ANSI | `stderr` ne contient pas `\033[` |
| Messages en anglais | pas de mot français dans `stderr` |
| Ligne et colonne présentes | `stderr` contient `line` et `column` |
| Sortie sur `stderr` uniquement | `stdout` vide en cas d'erreur |

### 28.10 Tests de parité CLI ↔ WebAssembly

**Principe :** tout programme de la suite `positive/` doit produire exactement le même `stdout` et le même code de sortie lorsqu'il est exécuté via le CLI natif et via le runtime WebAssembly (Node.js + Emscripten).

**Runner wasm — `tests/wasm/run_wasm_tests.sh` :**

```sh
#!/bin/sh
# run_wasm_tests.sh — vérifie la parité CLI ↔ WebAssembly
set -e

CIMPLE="${CIMPLE_BIN:-cimple}"
CIMPLE_WASM="${CIMPLE_WASM:-build-wasm/cimple.js}"
TESTS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
. "$TESTS_DIR/lib/assert.sh"

node --version >/dev/null 2>&1 || { echo "Node.js required for wasm tests"; exit 1; }

for src in $(find "$TESTS_DIR/positive" -name "input.ci" | sort); do
    dir=$(dirname "$src")
    name=$(basename "$dir")

    # Exclusions connues (fonctionnalités non disponibles sous wasm)
    category=$(echo "$dir" | grep -oP 'positive/\K[^/]+')
    if [ "$category" = "exec" ]; then continue; fi

    expected_stdout="$dir/expected_stdout"
    [ -f "$expected_stdout" ] || continue

    # Exécution CLI natif
    stdout_native=$("$CIMPLE" run "$src" 2>/dev/null)
    exit_native=$?

    # Exécution via WebAssembly
    stdout_wasm=$(node "$CIMPLE_WASM" run "$src" 2>/dev/null)
    exit_wasm=$?

    # Parité stdout
    if [ "$stdout_native" != "$stdout_wasm" ]; then
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [wasm:$name] stdout differs"
    else
        PASS=$((PASS + 1))
    fi

    # Parité code de sortie
    assert_exit "$exit_native" "$exit_wasm" "wasm:$name:exit"
done

# Cas spécifique wasm : exec → erreur runtime "not supported on this platform"
wasm_exec_test="$TESTS_DIR/wasm/exec_not_supported"
if [ -d "$wasm_exec_test" ]; then
    actual=$(node "$CIMPLE_WASM" run "$wasm_exec_test/input.ci" 2>&1)
    assert_stderr_contains "not supported on this platform" "$actual" "wasm:exec_not_supported"
fi

# Cas spécifique wasm : getEnv → toujours fallback
wasm_env_test="$TESTS_DIR/wasm/getenv_fallback"
if [ -d "$wasm_env_test" ]; then
    actual_stdout=$(node "$CIMPLE_WASM" run "$wasm_env_test/input.ci" 2>/dev/null)
    assert_stdout "$wasm_env_test/expected_stdout" "$actual_stdout" "wasm:getenv_fallback"
fi

print_summary
```

**Cas WebAssembly spécifiques :**

| Test | Comportement attendu |
|---|---|
| `exec` appelé sous wasm | `[RUNTIME ERROR]` + `not supported on this platform`, code `2` |
| `getEnv("HOME", "fallback")` sous wasm | retourne `"fallback"` |
| `now()` sous wasm | retourne un `int` positif non nul |
| Fichier I/O sous wasm | identique au CLI via MEMFS |

### 28.11 Couverture minimale requise

La suite est considérée suffisante si elle couvre :

| Catégorie | Nombre minimum de tests |
|---|---|
| Lexer (positifs + négatifs) | 30 |
| Syntaxe (négatifs) | 15 |
| Sémantique (négatifs) | 30 |
| Types et variables | 20 |
| Opérateurs | 20 |
| Flot de contrôle | 20 |
| Fonctions et récursion | 15 |
| Tableaux | 25 |
| Stdlib — chaînes | 40 |
| Stdlib — maths | 30 |
| Stdlib — fichiers | 15 |
| Stdlib — exec | 10 |
| Stdlib — temps | 15 |
| Constantes prédéfinies | 20 |
| Runtime (négatifs) | 25 |
| Messages d'erreur (format) | 10 |
| **Imports** | **20** |
| WebAssembly (parité) | ≥ 80 % des tests positifs |
| Non-régression | 1 par bug corrigé (minimum) |
| Tests du manuel (`tests/manual/`) | ≥ 165 (section 28.12) |
| **Total** | **≥ 555** |

### 28.12 Tests des exemples du manuel

**Règle normative :** tout exemple de code présent dans `MANUAL.md` doit avoir un test correspondant dans la suite. Un exemple de manuel sans test est un défaut de l'implémentation.

**Principe :** le manuel est la première chose que lit un utilisateur. Si un exemple du manuel ne compile pas ou produit une sortie incorrecte, la confiance dans le langage entier est détruite. Les tests du manuel sont donc aussi critiques que les tests du compilateur lui-même.

**Structure des répertoires :**

```
tests/
  manual/                       # tests des exemples du MANUAL.md
    part1_getting_started/
      hello_world/
      ...
    part2_basics/
      types_int/
      types_float/
      operators_precedence/
      ...
    part3_functions/
    part4_arrays/
    part5_stdlib/
      string_replace/
      math_sin_cos/
      time_formatdate/
      ...
    part6_types_in_depth/
    part7_errors/
    appendix_g/
      g1_hello_world/
      g2_fizzbuzz/
      g3_fibonacci_iterative/
      g3_fibonacci_recursive/
      g4_bubble_sort/
      g5_word_count/
      g6_csv_reader/
      g7_calculator/
      g8_stopwatch/
      g9_timestamped_report/
      g10_system_command/
```

**Conventions :**

- chaque exemple extrait du manuel devient un répertoire dans `tests/manual/` avec les fichiers standard `input.ci`, `expected_stdout`, `expected_exit` ;
- le fichier `description.txt` est **obligatoire** pour tous les tests du manuel et indique la section exacte d'origine :

```
Source: MANUAL.md, Part II, section 8.6 — Operator precedence table
Tests that 2 + 3 * 4 evaluates to 14, not 20.
```

- si un exemple dans le manuel est présenté comme un extrait partiel (fragment non exécutable seul), il doit être complété dans `input.ci` avec le minimum nécessaire pour le rendre exécutable, sans altérer la partie illustrée ;
- les exemples de l'Appendix G sont testés intégralement — leur sortie complète est dans `expected_stdout` ;
- les exemples montrant des erreurs (sections 33–36 du manuel) sont des tests négatifs avec `expected_stderr` et `expected_exit = 1` ou `2`.

**Intégration dans le runner :**

`run_tests.sh` inclut le répertoire `manual/` dans son parcours au même titre que `positive/` et `negative/`. Aucun runner séparé n'est nécessaire.

**Cohérence avec la section 31.12 :** la section 31.12 impose que tous les exemples du manuel compilent et produisent la sortie indiquée. La présente section est le mécanisme qui rend cette exigence **vérifiable automatiquement**.

**Couverture minimale des tests du manuel :**

| Source dans le manuel | Nombre minimum de tests |
|---|---|
| Partie I — Getting Started | 5 |
| Partie II — Language Basics | 30 |
| Partie III — Functions | 10 |
| Partie IV — Arrays | 15 |
| Partie V — Standard Library | 60 (un par exemple de fonction en Appendix B) |
| Partie VI — Types in Depth | 15 |
| Partie VII — Error Reference | 20 (exemples négatifs) |
| Appendix G — Worked Examples | 10 (un programme complet par exemple) |
| **Total** | **≥ 165** |

Ces 165 tests s'ajoutent aux ≥ 390 tests de la section 28.11, portant le total de la suite à **≥ 555 tests**.

### 28.13 Convention `description.txt` pour tous les tests

Chaque répertoire de test peut contenir un fichier `description.txt` facultatif documentant l'intention du test :

```
Tests that replace() replaces all occurrences of 'old', not just the first.
See spec section 9.3 (replace).
```

Pour les tests de régression, ce fichier est **obligatoire** (voir section 28.7).

### 28.14 Exécution partielle

CTest permet d'exécuter une sous-catégorie de tests via des labels :

```cmake
set_tests_properties(cimple_test_suite PROPERTIES LABELS "unit")
set_tests_properties(cimple_wasm_test_suite PROPERTIES LABELS "wasm")
```

```bash
ctest --test-dir build -L unit          # tests natifs uniquement
ctest --test-dir build -L wasm          # tests wasm uniquement
ctest --test-dir build --output-on-failure -j4   # parallèle
```

### 28.15 Intégration continue

La spec ne prescrit pas d'outil de CI spécifique, mais impose que la commande suivante réussisse avec code `0` sur toute plateforme cible avant toute publication d'une version :

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Pour la cible WebAssembly :

```bash
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm
ctest --test-dir build-wasm --output-on-failure -L wasm
```

---

## 29. Consignes de réalisation

L'implémentation ne doit pas ajouter de fonctionnalités non décrites dans ce document. En particulier :

- ne pas ajouter de pointeurs ;
- ne pas ajouter de macros ;
- ne pas ajouter un système de types avancé ;
- ne pas ajouter des conversions implicites à la C ;
- ne pas transformer le projet en langage complexe.

L'implémentation doit utiliser **re2c** pour le lexer et **Lemon** pour le parseur, conformément aux sections 16 et 17. Aucun autre générateur de lexer ou de parseur n'est autorisé.

L'implémentation doit être écrite en **C** (C99/C11) et **portable** sur macOS, Linux, Unix, Windows et WebAssembly, conformément aux règles de la section 4.4. Le build natif doit utiliser **CMake** (version ≥ 3.15) ; la cible WebAssembly utilise le toolchain Emscripten.

L'objectif est un langage **complet selon cette spec, exact, lisible, stable et multiplateforme**.

---

## 30. Résumé normatif

Cimple est :

- un langage impératif statiquement typé ;
- à syntaxe de style C ;
- avec types obligatoires ;
- avec `main` obligatoire, acceptant `int main()`, `void main()`, `int main(string[] args)` ou `void main(string[] args)` ;
- avec lexer généré par **re2c** et parseur généré par **Lemon de SQLite** ;
- **implémenté en C** (C99/C11) exclusivement, compilable avec GCC, Clang et MSVC sans modification ;
- **multiplateforme** : fonctionne sans modification sur macOS, Linux, Unix, Windows et WebAssembly (Emscripten, cible `wasm32`) ; build natif via CMake ≥ 3.15 ; `exec` et `getEnv` ont un comportement dégradé défini sous WebAssembly ;
- avec fonctions, variables globales, conditions, boucles, `break`, `continue`, retours, appels ;
- avec **constantes prédéfinies** numériques (`INT_MAX`, `INT_MIN`, `INT_SIZE`, `FLOAT_SIZE`, `FLOAT_DIG`, `FLOAT_EPSILON`, `FLOAT_MIN`, `FLOAT_MAX`) et mathématiques (`M_PI`, `M_E`, `M_TAU`, `M_SQRT2`, `M_LN2`, `M_LN10`) ; pour `int[]`, `float[]`, `bool[]`, `string[]`, indexés à partir de `0`, passés par référence aux fonctions ;
- avec fonctions intrinsèques de manipulation de tableaux : `count`, `arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`, `arrayGet`, `arraySet` ;
- avec fonctions de manipulation de chaînes incluant `len` (octets UTF-8), `glyphLen` (points de code NFC), `glyphAt` (accès par glyphe NFC), `join`, `split`, `concat`, `replace` (remplacement toutes occurrences) et `format` (substitution positionnelle `{}` avec `string[]`) ;
- avec fonctions de temps et dates UTC : `now()` (epoch ms), `epochToYear/Month/Day/Hour/Minute/Second`, `makeEpoch` (composants → epoch, `-1` sur invalide), `formatDate` (tokens `YYYY MM DD HH mm ss w yday WW ISO`) ;
- avec fonctions d'entrées/sorties fichiers texte UTF-8 : `readFile`, `writeFile`, `appendFile`, `fileExists`, `readLines`, `writeLines` ; toute erreur d'I/O est une erreur runtime explicite ;
- avec exécution de commandes externes via `exec` retournant un `ExecResult` opaque natif ; stdout et stderr capturés séparément via `execStdout` et `execStderr` ; code de retour via `execStatus` ;
- avec fonctions mathématiques complètes : noyau (`sqrt`, `pow`, `abs`, `min`, `max`, `floor`, `ceil`, `round`, `trunc`, `fmod`, `approxEqual`), trigonométrie (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`), logarithmes et exponentielle (`exp`, `log`, `log2`, `log10`) ;
- avec directive **`import "fichier.ci";`** permettant d'inclure des fichiers sources `.ci` ; les imports sont résolus récursivement avant l'analyse sémantique ; l'ordre est profondeur d'abord ; les imports circulaires sont une erreur sémantique ; la profondeur maximale est 32 ; les fichiers importés ne peuvent pas définir `main` ; les redéfinitions entre fichiers sont des erreurs sémantiques ; un fichier importé plusieurs fois n'est traité qu'une fois ;
- sans pointeurs, sans macros, sans préprocesseur, sans pièges historiques du C ;
- avec `if`, `else`, `while` et `for` acceptant soit un bloc, soit une instruction simple unique ;
- avec le `else` associé au `if` non apparié le plus proche ;
- avec `float` suivant IEEE et pouvant produire `NaN`, `+Infinity`, `-Infinity` ;
- **sans surcharge utilisateur** ; les intrinsèques tableau sont polymorphes au niveau du runtime uniquement ;
- avec détection conservative de l'absence de `return` dans les fonctions non `void` ;
- avec erreurs runtime claires sur tout accès tableau hors bornes ;
- avec **système de diagnostic uniforme** : quatre niveaux `[LEXICAL ERROR]`, `[SYNTAX ERROR]`, `[SEMANTIC ERROR]`, `[RUNTIME ERROR]` ; messages toujours en anglais ; format `[LEVEL]  file  line N, column N / description / -> indication` ; sortie sur `stderr` uniquement ; pas de coloration ANSI ; erreurs lexicales et syntaxiques fatales immédiates ; erreurs sémantiques accumulées jusqu'à 10 ; erreurs runtime fatales immédiates ; codes de sortie `0` / `1` / `2` écrasant le code retour utilisateur en cas d'erreur ;
- livré avec **`MANUAL.md`** — manuel de référence en anglais, auto-suffisant, normatif, en 8 parties (Getting Started → Appendices), couvrant tous les concepts du débutant absolu à l'expert, avec un exemple complet par fonction, un catalogue exhaustif des erreurs, 10 programmes annotés en Appendix G, et un changelog versionné ;
- livré avec une **suite de tests normative** : scripts shell POSIX, pilotée par CMake/CTest (`make test`), ≥ 555 tests couvrant positifs, négatifs, non-régression, parité CLI ↔ WebAssembly, et **tous les exemples du manuel** (`tests/manual/`) ; tout bug corrigé accompagné d'un test de non-régression obligatoire ; tout exemple de `MANUAL.md` sans test correspondant est un défaut.

---

## 31. User Manual

### 31.1 Normative requirement

The implementation must be delivered with a file named `MANUAL.md`, written in **English**, located at the root of the repository alongside the compiler. The manual is **normative**: any discrepancy between the manual and the actual behaviour of the implementation is a bug in the implementation, not in the manual. The manual must be updated at every revision of this specification.

The manual is **self-contained**: a user who has never programmed must be able to learn Cimple using the manual alone, without any external resource. No prior knowledge of programming, C, or any other language is assumed.

### 31.2 Format and delivery

- Single Markdown file: `MANUAL.md`
- Language: English exclusively
- Encoding: UTF-8
- No external dependencies: the manual must be readable as plain text, in a terminal, or rendered as HTML from the Markdown source without any build tool
- A table of contents with anchor links appears at the top of the document
- The manual must be searchable with a standard text search (`Ctrl+F` in a browser, `grep` in a terminal)
- No ANSI colour codes in code examples shown as terminal output

### 31.3 Conventions used throughout the manual

The following conventions are defined at the very beginning of `MANUAL.md` and applied consistently:

```
[!]  Important — a rule that must not be overlooked
[>]  Example — a complete, runnable program or expression
[?]  Tip — advice for beginners or common pitfalls
[*]  Advanced — content intended for experienced users
```

Every code example includes:
- a short comment describing what the program does
- the complete source code, runnable as-is with `cimple run`
- the expected output shown as a comment block at the end

```c
// Compute and print the sum of 1 to 10
void main() {
    int sum = 0;
    for (int i = 1; i <= 10; i++) {
        sum = sum + i;
    }
    print(toString(sum) + "\n");
}
// Output:
// 55
```

### 31.4 Structure — Part I: Getting Started

*Target audience: absolute beginners, first contact with Cimple.*

```
1. What is Cimple?
   1.1 Design goals
   1.2 What Cimple is not
   1.3 A note on simplicity

2. Installing Cimple
   2.1 Requirements
   2.2 Building from source
   2.3 Verifying the installation

3. Your first program: Hello, World
   3.1 Writing the source file
   3.2 Running the program
   3.3 What each line means

4. The Cimple command line
   4.1 cimple run <file>
   4.2 cimple check <file>
   4.3 Exit codes: 0, 1, 2
   4.4 Passing arguments to the program

5. Understanding error messages
   5.1 The four error levels
   5.2 Reading a Lexical Error
   5.3 Reading a Syntax Error
   5.4 Reading a Semantic Error
   5.5 Reading a Runtime Error
   5.6 Fixing your first errors (worked examples)
```

### 31.5 Structure — Part II: The Language Basics

*Target audience: beginners who have completed Part I.*

```
6. Types
   6.1 int — whole numbers
   6.2 float — decimal numbers
   6.3 bool — true and false
   6.4 string — text
   6.5 void — the absence of a value
   6.6 Type rules: no implicit conversion

7. Variables
   7.1 Declaring a variable
   7.2 Assigning a value
   7.3 Declaration with initialisation
   7.4 Variable naming rules
   7.5 Predefined constants (INT_MAX, M_PI, FLOAT_EPSILON…)

8. Operators and expressions
   8.1 Arithmetic operators: + - * / %
   8.2 Comparison operators: == != < <= > >=
   8.3 Logical operators: && || !
   8.4 Bitwise operators: & | ^ ~ << >>  (int only)
   8.5 String concatenation with +
   8.6 Operator precedence table
   8.7 i++ and i-- (post-increment, statement only)

9. String operations
   9.1 Concatenation
   9.2 String length: len()
   9.3 Substrings: substr()
   9.4 Searching: indexOf(), contains(), startsWith(), endsWith()
   9.5 Replacing: replace()
   9.6 Formatting: format()
   9.7 Strings are immutable

10. Printing and reading
    10.1 print() — output to stdout
    10.2 input() — reading a line from stdin
    10.3 Converting values before printing: toString()

11. Conditional statements
    11.1 if
    11.2 if / else
    11.3 Nested conditions
    11.4 The dangling else rule

12. Loops
    12.1 while
    12.2 for
    12.3 break
    12.4 continue
    12.5 Infinite loops and how to avoid them
```

### 31.6 Structure — Part III: Functions

*Target audience: beginners comfortable with Part II.*

```
13. Defining functions
    13.1 Syntax
    13.2 Return types and the return statement
    13.3 void functions
    13.4 Calling a function
    13.5 Functions must be declared before use

14. Parameters and arguments
    14.1 Passing values (scalars are passed by value)
    14.2 No default parameter values

15. Scope
    15.1 Local variables
    15.2 Global variables
    15.3 Shadowing rules

16. Recursion
    16.1 How recursion works
    16.2 Base case and recursive case
    16.3 Example: factorial
    16.4 Example: Fibonacci
    16.5 Stack depth considerations
```

### 31.7 Structure — Part IV: Arrays

*Target audience: intermediate users.*

```
17. Array basics
    17.1 Declaring an array
    17.2 Array literals
    17.3 Empty arrays
    17.4 Accessing elements: array[i]
    17.5 Modifying elements: array[i] = value
    17.6 Index out of bounds: the runtime error

18. Array functions
    18.1 count()
    18.2 arrayPush() and arrayPop()
    18.3 arrayInsert() and arrayRemove()
    18.4 arrayGet() and arraySet()

19. Arrays and functions
    19.1 Arrays are passed by reference
    19.2 Modifying an array inside a function
    19.3 Returning an array from a function

20. Iterating over arrays
    20.1 for loop with index
    20.2 Building arrays dynamically with arrayPush
    20.3 Common patterns: filter, map, accumulate
```

### 31.8 Structure — Part V: The Standard Library

*Target audience: intermediate to advanced users. Each subsection is a self-contained reference.*

```
21. String library
    21.1 len, glyphLen, byteAt, glyphAt
    21.2 substr, indexOf, contains, startsWith, endsWith
    21.3 replace
    21.4 format
    21.5 join, split, concat
    21.6 toString, toInt, toFloat, toBool
    21.7 isIntString, isFloatString, isBoolString

22. Math library
    22.1 Basic: abs, min, max, floor, ceil, round, trunc, fmod
    22.2 Power and root: sqrt, pow
    22.3 Trigonometry: sin, cos, tan, asin, acos, atan, atan2
    22.4 Exponential and logarithms: exp, log, log2, log10
    22.5 Comparison: approxEqual
    22.6 Float predicates: isNaN, isInfinite, isFinite,
                           isPositiveInfinity, isNegativeInfinity
    22.7 Integer utilities: absInt, minInt, maxInt, clampInt,
                            isEven, isOdd, safeDivInt, safeModInt

23. Predefined constants
    23.1 Integer constants: INT_MAX, INT_MIN, INT_SIZE
    23.2 Float numeric constants: FLOAT_SIZE, FLOAT_DIG,
                                  FLOAT_EPSILON, FLOAT_MIN, FLOAT_MAX
    23.3 Mathematical constants: M_PI, M_E, M_TAU, M_SQRT2, M_LN2, M_LN10

24. File I/O
    24.1 readFile, writeFile, appendFile
    24.2 fileExists
    24.3 readLines, writeLines
    24.4 Error handling in file operations
    24.5 UTF-8 and line endings

25. Running external commands
    25.1 exec: syntax and behaviour
    25.2 The ExecResult type
    25.3 execStatus, execStdout, execStderr
    25.4 Passing environment variables
    25.5 Error cases

26. Environment variables
    26.1 getEnv

27. Date and time
    27.1 now() — current epoch in milliseconds
    27.2 epochToYear, epochToMonth, epochToDay
    27.3 epochToHour, epochToMinute, epochToSecond
    27.4 makeEpoch
    27.5 formatDate and format tokens
    27.6 Working with durations
    27.7 UTC and the absence of time zones
```

### 31.9 Structure — Part VI: Types in Depth

*Target audience: advanced users who need precise, expert-level understanding.*

```
28. Integers in depth
    28.1 Internal representation: int64_t
    28.2 INT_MAX, INT_MIN, INT_SIZE
    28.3 Integer overflow behaviour
    28.4 Integer division and modulo: truncation toward zero
    28.5 Bitwise operators in detail: &, |, ^, ~, <<, >>
    28.6 safeDivInt and safeModInt

29. Floats in depth
    29.1 IEEE 754 double precision
    29.2 FLOAT_SIZE, FLOAT_DIG, FLOAT_EPSILON, FLOAT_MIN, FLOAT_MAX
    29.3 NaN: what it is, how it propagates, how to test for it
    29.4 Infinity: positive, negative, how it arises
    29.5 Precision and rounding: why 0.1 + 0.2 != 0.3
    29.6 approxEqual and FLOAT_EPSILON: the correct way to compare floats
    29.7 toInt(float): truncation toward zero

30. Strings in depth
    30.1 Internal encoding: UTF-8
    30.2 Bytes vs glyphs: len vs glyphLen
    30.3 byteAt: raw byte access (0–255)
    30.4 s[i]: single-byte access as string
    30.5 glyphAt: NFC-normalised glyph access
    30.6 Why strings are immutable
    30.7 Unicode normalisation: NFC and NFD

31. The ExecResult type
    31.1 What ExecResult is and is not
    31.2 Why it is opaque
    31.3 Rules: no array, no declaration without init, no return from main
```

### 31.10 Structure — Part VII: Error Reference

*Target audience: all users. A lookup reference, not a tutorial.*

```
32. Error message format
    32.1 The four levels: LEXICAL, SYNTAX, SEMANTIC, RUNTIME
    32.2 Reading the location: file, line, column
    32.3 The description line
    32.4 The -> indication line

33. Lexical errors — complete reference
    (one entry per error, with cause and fix)

34. Syntax errors — complete reference
    (one entry per error, with cause and fix)

35. Semantic errors — complete reference
    (one entry per error, with cause and fix)

36. Runtime errors — complete reference
    (one entry per error, with cause and fix)
```

### 31.11 Structure — Part VIII: Appendices

*Reference material, no prose explanation required.*

```
Appendix A — Complete language grammar (EBNF)
Appendix B — Complete standard library reference
             (every function: signature, description, return value,
              error conditions, one example)
Appendix C — Reserved keywords and predefined identifiers
Appendix D — Operator precedence table (high to low)
Appendix E — Escape sequences
Appendix F — formatDate token reference
Appendix G — Worked examples
             G.1  Hello, World
             G.2  FizzBuzz
             G.3  Fibonacci (iterative and recursive)
             G.4  Bubble sort on an integer array
             G.5  Word count in a string
             G.6  Reading and processing a simple CSV file
             G.7  Command-line calculator using args
             G.8  Stopwatch with now()
             G.9  Generating a timestamped text report
             G.10 Running a system command and processing its output
```

### 31.12 Quality requirements for the manual

The following requirements apply to every section of `MANUAL.md` :

- every function documented in Appendix B must include: signature, plain-English description, description of each parameter, return value, error conditions (both semantic and runtime), and at least one complete runnable example with expected output ;
- every error listed in sections 33–36 must include: the exact error message as produced by the compiler, the cause, a minimal code example that triggers it, and the fix ;
- no section may refer to a concept without either defining it or providing a cross-reference to the section where it is defined ;
- all code examples must be valid Cimple programs that compile without error under `cimple check` and produce the stated output under `cimple run` ; **each example must have a corresponding test in `tests/manual/`** (see section 28.12) — an example without a test is a defect ;
- the manual must cover every operator, every keyword, every predefined constant, every stdlib function, and every error condition defined in this specification ; any omission is a defect.

### 31.13 Synchronisation with the specification

The manual version number must match the specification version number. A changelog section at the end of `MANUAL.md` lists, for each version, the changes made to the language and their effect on the manual. The changelog uses the format:

```
## Changelog

### v42
- Added: note d'implémentation obligatoire — passe de pré-collecte des noms de structures avant le parse principal, nécessaire pour produire des erreurs sémantiques (et non syntaxiques) sur les références forward, la récursion indirecte et les cycles d'héritage (section 6.8.1)
- Fixed: section 19.2c — distinction récursion directe / indirecte explicitée ; référence à la passe de pré-collecte ; détection de cycle d'héritage liée à la passe de pré-collecte
- Fixed: section 19.2c — vérification des conflits noms de champs/méthodes vs globaux formalisée avec note de désambiguïsation contextuelle
- Added: 3 messages de diagnostic dans le catalogue 21.5 : `Member access requires a structure instance`, `Unknown field '<n>' on structure '<S>'`, `Unknown method '<n>' on structure '<S>'`
- Added: 5 nouveaux cas de tests négatifs en section 28.9 (forward reference → erreur sémantique, conflit global/champ, accès membre invalide)

### v41
- Fixed: redéfinition de champ dans une sous-structure — même type obligatoire, **valeur par défaut peut différer** (sections 6.8.2, 19.2c, 21.5, décision 38)
- Fixed: valeurs par défaut des champs — implicites pour les types scalaires (`0`, `0.0`, `false`, `""`, `0`) et tableaux (`[]`) ; seuls les champs de type structure restent obligatoirement initialisés avec `clone` (sections 6.8.1, 6.8.5, 19.2c, 21.5, décision 38)
- Fixed (errata) : `formatDate` — format strings des exemples mis à jour vers les nouvelles lettres

### v40 — Cimple 1.1
- **Version du langage : 1.0 → 1.1**
- Added: type composite `structure` — déclaration, champs avec valeurs par défaut, méthodes, `self` (section 6.8)
- Added: héritage simple en chaîne, redéfinition de champ (même type/valeur), override de méthode (même signature) (section 6.8.2)
- Added: `super.méthode()` — appel de la méthode de la base directe depuis une sous-structure (section 6.8.4)
- Added: opérateur `clone NomDeStructure` — construction d'instance avec valeurs par défaut ; `clone variable` interdit (section 6.8.5)
- Added: champs de type structure initialisés via `clone` (section 6.8.6)
- Added: passage des structures par référence, retour par copie (section 6.8.7)
- Added: `NomDeStructure[]` — tableaux de structures (section 6.8.8)
- Added: mots-clés `structure`, `clone`, `self`, `super` (sections 14, 16.2)
- Added: règles sémantiques structures (section 19.2c)
- Added: 15 nouvelles erreurs sémantiques dans le catalogue 21.5
- Updated: grammaire EBNF — `structure_decl`, `member_expr`, `clone_expr`, `super`, `struct_array_type` (section 22)
- Updated: section 6.2 — `NomDeStructure[]` comme type tableau valide
- Updated: section 6.3 — retrait de "seul type composite"
- Added: décision normative 38
- Updated: catalogues de tests 28.8 et 28.9

### v39
- Added: note d'implémentation en section 6.1 — `int` doit être `int64_t` ; justification : `now()` retourne une epoch Unix en millisecondes (~1,7 × 10¹² ms), incompatible avec `int32_t` ; `INT_SIZE = 8` sur toutes les plateformes cibles

### v38
- Fixed: `intToBytes`, `bytesToInt` — taille exprimée en `INT_SIZE` et non en valeur absolue 4 (section 9.9, 21.6, 28.8, 28.9)
- Fixed: `floatToBytes`, `bytesToFloat` — taille exprimée en `FLOAT_SIZE` et non en valeur absolue 8 (section 9.9, 21.6, 28.8, 28.9)
- Rationale: `INT_SIZE` et `FLOAT_SIZE` sont des constantes prédéfinies reflétant les choix de l'implémentation ; fixer les tailles en dur contredisait ce principe

### v37
- Added: `intToBytes(int n)` → `byte[INT_SIZE]` — dump mémoire brut de l'entier, ordre natif (section 9.9)
- Added: `floatToBytes(float f)` → `byte[FLOAT_SIZE]` — dump mémoire brut du flottant IEEE 754, ordre natif (section 9.9)
- Added: `bytesToInt(byte[] data)` → `int` — reconstruction symétrique ; `count != INT_SIZE` erreur runtime (section 9.9)
- Added: `bytesToFloat(byte[] data)` → `float` — reconstruction symétrique ; `count != FLOAT_SIZE` erreur runtime ; NaN/Inf valides (section 9.9)
- Updated: vérifications sémantiques 19.2b
- Updated: catalogue erreurs runtime 21.6 — 2 nouvelles entrées
- Updated: catalogues de tests 28.8 et 28.9
- Updated: décision normative 37

### v36
- Added: type scalaire `byte` — entier non signé 8 bits (section 6.1, 6.6)
- Added: type tableau `byte[]` (section 6.2)
- Added: `byte` comme mot-clé réservé (section 14, 16.2)
- Added: règles arithmétiques complètes : `+ - * / % << >>` → `int` ; `& | ^ ~` sur deux `byte` → `byte` ; mixte `byte op int` → `int` (section 6.6)
- Added: `intToByte(n)` — clamp silencieux [0,255] (section 9.9)
- Added: `byteToInt(b)` — valeur entière 0–255 (section 9.9)
- Added: `readFileBytes`, `writeFileBytes`, `appendFileBytes` — I/O binaires (section 9.9)
- Added: `stringToBytes`, `bytesToString` — conversion chaîne ↔ octets ; U+FFFD sur UTF-8 invalide (section 9.9)
- Added: exception littérale — littéraux entiers `[0,255]` affectables directement à `byte` ; variables `int` → `intToByte()` obligatoire (section 6.6, 19.2b)
- Added: section 19.2b — vérifications sémantiques `byte`
- Added: 3 nouvelles erreurs sémantiques dans le catalogue 21.5
- Added: 2 nouvelles erreurs runtime dans le catalogue 21.6
- Updated: grammaire EBNF — `type` et `array_type` étendus (section 22)
- Updated: catalogue des erreurs syntaxiques 21.4 — `byte` dans la liste des types valides
- Updated: structure de tests 28.2, catalogues 28.8 et 28.9
- Added: décision normative 37

### v35
- Added: `trim`, `trimLeft`, `trimRight` — suppression des espaces/tabs/newlines en début et/ou fin (section 9.3)
- Added: `toUpper`, `toLower` — conversion de casse Unicode complète, locale-agnostique ; `ß`→`SS` pour `toUpper` (section 9.3)
- Added: `capitalize` — première lettre en majuscule, reste inchangé (section 9.3)
- Added: `padLeft`, `padRight` — padding jusqu'à `width` glyphes ; `pad=""` → inchangé ; pad multi-char tronqué pour tomber pile à `width` ; `width<0` erreur runtime (section 9.3)
- Added: `repeat(string s, int n)` — `n=0` → `""` ; `n<0` erreur runtime (section 9.3)
- Added: `lastIndexOf(string s, string needle)` — dernière occurrence en octets, `-1` si absente (section 9.3)
- Added: `countOccurrences(string s, string needle)` — occurrences non chevauchantes ; `needle=""` erreur runtime (section 9.3)
- Added: `isBlank(string s)` — `true` si vide ou uniquement espaces/tabs/newlines (section 9.3)
- Added: 4 nouvelles entrées dans le catalogue des erreurs runtime 21.6
- Updated: vérifications sémantiques 19.3
- Updated: catalogues de tests 28.8 et 28.9

### v34
- Added: `string cwd()` — répertoire de travail courant, jamais d'erreur runtime (section 9.9)
- Added: `void copy(string src, string dst)` — copie de fichier, écrasement silencieux si dst existe (section 9.9)
- Added: `void move(string src, string dst)` — déplacement/renommage, transparent inter-systèmes de fichiers, écrasement silencieux si dst existe (section 9.9)
- Added: `bool isReadable(string path)`, `bool isWritable(string path)`, `bool isExecutable(string path)`, `bool isDirectory(string path)` — tests de propriétés, jamais d'erreur runtime ; `isWritable` teste le fichier lui-même s'il existe, le répertoire parent sinon (section 9.9)
- Added: `string dirname(string path)`, `string basename(string path)`, `string filename(string path)`, `string extension(string path)` — décomposition lexicale de chemin ; `dirname("file.txt")` → `""` ; `dirname("/")` → `"/"` ; `extension(".gitignore")` → `""` (section 9.9)
- Added: 8 nouvelles entrées dans le catalogue des erreurs runtime 21.6 (`copy`, `move`)
- Updated: vérifications sémantiques 19.8 — toutes les nouvelles fonctions
- Updated: décision normative 25
- Updated: catalogues de tests 28.8 et 28.9

### v33
- Added: `void remove(string path)` — supprime un fichier ; erreur runtime si inexistant, si répertoire, ou si permissions insuffisantes (section 9.9)
- Added: `void chmod(string path, int mode)` — modifie les droits POSIX ; `mode` est un entier octal (ex. `0644`) ; erreur runtime si chemin inexistant, permissions insuffisantes, ou plateforme Windows/WebAssembly (`chmod: not supported on this platform`) (section 9.9)
- Added: signatures `remove` et `chmod` dans la liste stdlib 9.1
- Added: 5 nouvelles entrées dans le catalogue des erreurs runtime 21.6
- Updated: vérifications sémantiques 19.8 — `remove`, `chmod`, `tempPath`
- Updated: décision normative 25 — comportement de `remove` et `chmod`
- Updated: catalogues de tests 28.8 (positifs) et 28.9 (négatifs)

### v32
- Added: `string tempPath()` — chemin temporaire unique et inexistant (section 9.9) ; ne crée pas le fichier ; deux appels successifs retournent deux chemins distincts ; répertoire POSIX : `$TMPDIR` sinon `/tmp` ; Windows et WebAssembly : comportement défini par l'implémenteur ; race condition externe non protégée, pas de suppression implicite
- Added: signature `tempPath()` dans la liste stdlib 9.1

### v31
- Added: `formatDate` — 4 nouveaux tokens : `w` (jour de la semaine, 0=dim), `yday` (jour de l'année base 0, sans padding), `WW` (semaine ISO 8601, zéro-paddé), `ISO` (date complète ISO 8601 UTC : `YYYY-MM-DDTHH:mm:ssZ`, retourne `"invalid"` hors plage 0–9999) (section 9.12)
- Updated: règle de substitution précisée — correspondance du préfixe le plus long en premier
- Updated: section 28.8 — catalogue de tests `formatDate` étendu
- Updated: résumé section 30

### v30
- Added: `import` directive — directive de premier niveau permettant d'inclure des fichiers `.ci` (sections 5.1–5.2, 14, 16.2, 19.12, 21.5, 22, 25, 27, 28)
- Added: `import` comme mot-clé réservé (section 14)
- Added: `TokenValue.file` pour les messages d'erreur multi-fichiers (section 16.3)
- Added: section 19.12 — vérifications sémantiques des imports (cycles, profondeur, `main` interdit, redéfinitions)
- Added: 8 nouvelles erreurs sémantiques dans le catalogue 21.5
- Updated: grammaire EBNF — `program` distingue `import_decl` et `top_level_item` (section 22)
- Updated: décision normative 1 reformulée ; décision 36 ajoutée (section 25)
- Updated: CLI section 27 — un seul fichier racine, imports résolus automatiquement
- Updated: structure de tests — `tests/positive/imports/` et `tests/negative/semantic/imports/` (section 28)
- Updated: couverture minimale 535 → 555 tests (sections 28.11, 28.12, 30, 35)
- Updated: résumé normatif section 30 — mention de `import`

### v29
- Fixed: `makeEpoch` retiré du catalogue des erreurs runtime (retourne `-1`, jamais d'erreur)
- Fixed: doublon du bloc d'exemple `getEnv` supprimé
- Fixed: commentaire erroné "log() n'existe pas en Cimple" supprimé
- Added: section `input()` complète (lecture stdin)
- Fixed: `readFile` clarifié — contenu brut, pas de normalisation ; `readLines` normalise
- Fixed: `substr` — "caractères" → "octets"
- Fixed: commentaire PREDEFINED_CONST dans la grammaire complété avec les constantes `M_*`
- Fixed: artefacts `toString(...)` × 3 répétés (sections 11.5, 19.6, 30)
- Added: règle de division entière par troncature vers zéro avec exemples
- Added: `for_init` type non-`int` qualifié comme erreur sémantique
- Added: réaffectation totale de tableau `t = [...]` qualifiée comme erreur sémantique

### v28
- Added: suite de tests normative complète (section 28) — structure, format, couverture ≥ 535
- Added: tests du manuel (section 28.12) — ≥ 165 tests dans `tests/manual/`
- Added: tests de parité CLI ↔ WebAssembly (section 28.10)
- Added: MANUAL.md requirement (section 31) — 8 parties, Appendix G (10 programmes annotés)

### v25
- Added: time and date functions (now, epochTo*, makeEpoch, formatDate)
- Added: replace and format string functions
- Added: mathematical constants M_PI, M_E, M_TAU, M_SQRT2, M_LN2, M_LN10
- Added: trigonometry and logarithm functions
- Added: predefined numeric constants INT_MAX … FLOAT_MAX
- Added: error message format specification

---
