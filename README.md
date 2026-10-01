# IFJ26 - Vývojový plán

Projekt do předmětu IFJ, psaný v jazyce **C** (standard C11).

## Struktura projektu

```
.
├── src/            # zdrojové soubory (.c, .h)
├── tests/
│   ├── utest/      # jednotkové testy (cmocka) a setup (.h)
│   ├── system/     # jednotlivé systémové testy
│   └── data/       # data pro systémové testování
├── zadani/         # zadání projektu (2 soubory, PDF)
│   ├── Specifikace_IFJ26_a_IFJcode26.pdf
│   └── Zadání_projektu_IFJ_a_IAL_2026_2027.pdf
├── Makefile        # sestavení, testy a formátování
└── rozdeleni       # rozdělení práce mezi členy týmu
```

## Plán projektu

### Rozšíření (minimálně 6 b)

| Rozšíření  | Body |
| ---------- | ---- |
| FUN EXP    | 1,5  |
| BOOL THEN  | 1    |
| CONTROL    | 1,5  |
| VALMEM     | 0,5  |
| LIST       | 2    |
| PARAM LIST | 0,5  |

**Celkem: 7 bodů při perfektní implementaci.**

### Rozdělení práce

- **VJ**: –
- **OG**: –
- **PH**: –
- **MB**: –

(za zkratky budou doplněna celá jména na schůzce)

Zkratky: **PH** = xhanzlp00 (Petr Hanzlík), **MB** = xberanm00, **OG** = xgleton00,
**VJ** = xjurokv00.

### Rozdělení úkolů

| Úkol                    | Kdo   |
| ----------------------- | ----- |
| Makefile                | PH    |
| Lexikální analýza       | VJ    |
| Dynamické struktury     | VJ    |
| Systémové testy         | VJ,OG |
| Syntaktická analýza     | OG    |
| Implementace AST        | OG    |
| Sémantická analýza      | PH    |
| Tabulka symbolů         | PH    |
| Implementace stromu     | PH    |
| Implementace zásobníku  | MB    |
| Generátor cílového kódu | MB    |

- **UNIT TESTY: VŠICHNI SVOJE**

**CODEREVIEW:**
| KDO | KOHO |
| --- | ---- |
| OG  | VJ   |
| PH  | OG   |
| MB  | PH   |
| VJ  | MB   |

### Plán

Vývojáři **VJ** a **OG** by měli hned od začátku počítat s rozšířením, aby ho
správně propojili. Vývojáři **PH** a **MB** by měli nejprve pořešit základní
zadání a potom přidávat rozšíření. Cíl je mít přeložitelný program do
**19. 11.**, který bude pokrývat základní zadání.

### Postup

Lexikální analýza se dělá jako první, to ale neznamená, že ostatní nemůžou
pracovat. Implementace stromu a zásobníku je nezávislá na ostatních věcech a
kódy pro systémové testy taky nemusí potřebovat samotný kompilátor. Po dokončení
bude daná část rovnou testována.

```text
     1. iterace                                  2. iterace                           3. iterace                    4. iterace
VJ lexikální analýza (dynamické struktury) -|-> systémové testy -------------> diagram FSM (dokumentace) --> dokumentace
OG LL gramatika a tabluka (dokumentace) ------> syntaktická analýza (AST)--|-> systemové testy ------------> pomoc generování cílového kódu
PH implementace BVS --------------------------> tabulka symbolů -------------> sémantická analýza ---|-----> prezentace 
MB pomoc při vytvareni FSM -------------------> implementace zásobníku ------> blokové schéma -------------> generování cílového kódu
```

### Deadline

- **1. it.** – do 11. 10. kód, opravené code review do 15. 10. 
- **2. it.** – do 22. 10. kód, opravené code review do 25. 10.
- **3. it.** – do 1. 11. kód, opravené code review do 5. 11.   (blokové schema nemusí být dokončené)
- **4. it.** – do 15. 11. kód, opravené code review do 18. 11. (dokumentace a prezentace se může zpozdit)
- **19. 11. 2026, 23:59:59** – pokusné odevzdání (nepovinné).
- **do 26. 11.** – snažit se dodělat rozšíření a najít chyby, pokud se nějaké
  ukážou. Pokud to bude už 19. 11., ještě lepší.
- **27. 11. 2026, 11:59:59** – **hlavní odevzdání** (pátek, před polednem) do
  StudISu.
- **26. 11. – 1. 12. 2026, 8:00** – odevzdat vytištěnou dokumentaci na fakultu.
- **12.–13. týden semestru** – osobní obhajoby (předem odevzdat prezentaci PDF).

## Povinné metody implementace

Pro implementaci jednotlivých částí překladače **povinně** využijte následující
metody. Všechny budou probírány na přednáškách předmětu IFJ. **Nedodržení těchto
metod bude penalizováno značnou ztrátou bodů!**

1. **Lexikální analýza** – implementujte (v principu) jako **konečný automat**.
   Drobné odchylky od standardní funkce tohoto modelu jsou povoleny.
2. **Syntaktická analýza založená na LL gramatice** (vše kromě výrazů) – využijte
   buď metodu **rekurzivního sestupu** (doporučeno), nebo **prediktivní analýzu
   řízenou LL tabulkou**.
3. **Výrazy** (včetně pravdivostních) – zpracujte pouze pomocí **precedenční
   syntaktické analýzy**, která odpovídá výkladu v přednáškách (využívá
   precedenční tabulku).

Při řešení projektu je **zakázáno** využívat generátory lexerů či parserů (např.
**lex, flex, yacc, Bison** atp.).

## Vstup, výstup a návratové kódy

Překladač je konzolová aplikace (tzv. filtr) bez grafického rozhraní:

- zdrojový kód v jazyce **IFJ26** se načítá ze **standardního vstupu** (`stdin`),
- výsledný mezikód **IFJcode26** se generuje na **standardní výstup** (`stdout`),
- všechna chybová hlášení, varování a ladicí výpisy jdou na **standardní chybový
  výstup** (`stderr`).

Návratová hodnota:

- při úspěšném překladu **0**,
- při chybě příslušný **chybový návratový kód** dle specifikace jazyka (přehled
  chybových návratových kódů).

Do `stdout` nesmí uniknout žádný znak navíc – vygenerovaný mezikód nesmí
vypisovat nic, co nepředepisuje přeložený program. Testy porovnávají výstup
pomocí `diff`, takže i jediný neočekávaný znak (včetně konce řádku) znamená
nevyhovující hodnocení.

## Tabulka symbolů a vyhledávací struktura

Registrovaná varianta je **vv-BVS** – **výškově vyvážený binární vyhledávací
strom**.

- Povinně vytvořte modul `symtable.c` + `symtable.h`, který poskytuje tabulku
  symbolů jako **abstraktní datový typ** (interní reprezentace je před uživatelem
  skrytá).
- Zbytek překladače pracuje s TS pouze přes operace z modulu `symtable`.
- Konkrétní typ vyváženého stromu a operace si volíme sami dle látky IAL; odchylky
  od probíraných algoritmů se odůvodní v dokumentaci.
- **Pro kód vyhledávací struktury je použití AI zakázáno** (viz níže).

## Sestavení a spuštění

```sh
make          # výchozí cíl `build` – přeloží projekt do binárky `ifj26`
make test     # přeloží a spustí testy (cmocka), poté je pustí i pod Valgrindem
make format   # naformátuje kód pomocí clang-format (styl LLVM)
make clean    # smaže všechny vygenerované soubory
make zip      # vytvoří odevzdávaný archiv xhanzlp00.zip
```

Výchozí cíl je `build` (spustí se pouhým `make`), `all` je jeho alias. Projekt
musí jít přeložit pomocí **`gcc` ve verzích 14/15/16** nebo **`clang` ve verzích
19/23** na architektuře **x86_64** (referenční server Merlin). Používáme
přepínače `-std=c11 -Wall -Wextra -MMD -MP -g -Isrc`.

## Code review

Každá změna jde přes *pull request* do větve `main`. Před sloučením musí projít
CI pipeline (`.github/workflows/ci.yml`), která řeší:

- **Build (Make)** – projekt se přeloží bez chyb.
- **Code Style (clang-format)** – všechny `.c` a `.h` soubory splňují styl **LLVM**
  (`clang-format --style=llvm --dry-run --Werror`).
- **Run Tests** – testy projdou a jsou čisté i bez úniků paměti (Valgrind).

Autoři by si měli před odesláním PR spustit `make format` (a ideálně i `make test`).

### Co clang-format neuhlídá (musí zkontrolovat recenzent)

Základní kontrola stylem nestačí na následující pravidla, proto je při code review
nutné ověřit ručně:

1. **Nový řádek na konci souboru** – každý soubor musí končit znakem nového řádku
   (prázdný řádek po posledním řádku kódu).
2. **Dělení `#include`** – systémové hlavičky se vkládají přes `<...>` a patří do
   jedné skupiny; vlastní hlavičky se vkládají přes `"..."` a patří do samostatné
   skupiny. Skupiny se oddělují prázdným řádkem. Příklad:

   ```c
   #include <stdio.h>
   #include <stdlib.h>

   #include "parser.h"
   #include "scanner.h"
   ```

3. **`static` u interních funkcí** – každá funkce, která není deklarovaná v hlavičkovém
   souboru (není součástí veřejného rozhraní), musí být označena jako `static`.
4. **Závorky u `sizeof`** – u `sizeof` platí tato pravidla pro zápis:
   - `sizeof(type)` – s typem se **vždy** píší závorky.
   - `sizeof var` – s proměnnou se závorky **nikdy** nepíší.

   ```c
   size_t a = sizeof(int);   // správně
   size_t b = sizeof(x);     // špatně
   int x;
   size_t c = sizeof x;      // správně
   ```

5. **Návratové hodnoty API funkcí** – funkce deklarované v hlavičkových souborech
   (veřejné API) vrací výhradně návratový typ `IFJ_ERR`.

6. **Kontrola `malloc`** – každé volání `malloc` (a obdobné alokace) musí být
   zkontrolováno, že proběhlo úspěšně, a to ještě před použitím paměti. Neúspěšnou
   alokaci je potřeba ošetřit a uvolnit už dříve alokované prostředky. Pro úklid
   (i po neúspěšném `malloc`) je vhodné použít `goto` na společné úklidové
   návěstí, které uvolní všechny alokované zdroje.

   ```c
   char *data = malloc(n);
   if (data == NULL) {
     return IFJ_ERR;
   }
   // ...
   ```

7. **Existence testů** – ke každé nové či upravené hlavičce (`src/*.h`) musí
   existovat odpovídající jednotkový test `tests/utest/test_<modul>.c`. Recenzent
   ověří, že testy pokrývají nově přidané funkce i jejich chybové a okrajové
   stavy (white-box). Bez testů PR nesloučíme.

8. **Pojmenování identifikátorů** – `clang-format` názvy nehlídá, proto je
   kontroluje recenzent. Funkce, proměnné a typy se píší **snake_case** (standard
   C), např. `sym_table`, `bst_insert`, `ifj_err`. Makra a konstanty (včetně hodnot
   výčtových typů) se píší **SCREAMING_SNAKE_CASE**, např. `LOG_ERR`, `IFJ_SUCCESS`.

## Git a commity

### Sloučení do `main`

- **Všechny commity se tvoří přes pull request** – jiná možnost není.
- **Force push do `main` je zakázaný** (blokováno GitHubem).
- Historie `main` musí zůstat **lineární** (taky hlídáno GitHubem). PR se merguje
  přes **rebase**, takže by s tím neměl být problém.
- Sloučení do `main` probíhá přes **rebase** (mělo by být nastaveno v GitHubu,
  ale pro jistotu to dodržujte).
- Pokud se ve větvi objevil **bordel** (např. `clang-format` vám přeformátoval
  všechny commitované soubory), proveďte **rebase**, ať se do historie `main`
  nedostanou zbytečné změny.

### Formát commit message

Commit messages se píší **anglicky**. Formát:

```
<thing you are working on> CREATE <additional info>
<thing you are working on> UPDATE <additional info>
...
```

Examples:

```
binary tree CREATE rotation function
scanner UPDATE fix number lexeme
```

## Styl formátování (LLVM)

Formátování je definováno v `Makefile` (`clang-format --style=llvm`) a vynuceno
CI. Základní pravidla stylu **LLVM**, se kterými se v kódu setkáte:

- odsazení **2 mezery**, používají se mezery (žádné taby);
- maximální šířka řádku **80 znaků**;
- otevírací složená závorka zůstává na stejném řádku (K&R/„attach“);
- `#include` se automaticky třídí (case-sensitive);
- ukazatele se píší k názvu proměnné: `char *ptr`;
- krátké jednoduché funkce mohou být na jednom řádku;
- závorky se nechávají na konci předchozího výrazu, bloky se neočíslovávají.

Formátování si nikdy neupravujte ručně „potichu“ – spusťte `make format`
a zkontrolujte výsledek v diffu.

## Komentování kódu (Doxygen)

Komentáře se píší **česky**. Používají se řádkové komentáře `//`; dokumentační
komentáře pro **Doxygen** se píší `///` (Doxygen samotné `//` nerozpozná).

- **Úvod všech zdrojových textů** musí obsahovat zakomentovaný název projektu,
  přihlašovací jména a jména studujících, kteří se na daném souboru skutečně
  autorsky podíleli.

  ```c
  /// @file parser.c
  /// @brief Projekt IFJ26 - syntaktická analýza
  ///
  /// @author xhanzlp00 Petr Hanzlík
  /// @author xberanm00 ...
  ```

- **Funkce** mají `@brief`, `@param` a `@return`. Návratových hodnot (`@return`)
  může být i více, pokud to dává smysl.

  ```c
  /// @brief Rotace uzlu binárního stromu doleva.
  ///
  /// @param tree Strom, ve kterém se rotuje.
  /// @param node Uzel, nad kterým se rotace provádí.
  /// @return IFJ_ERR Úspěch či chybový stav.
  ```

- **Struktury** se deklarují přes `typedef` (`typedef struct`) a mají `@brief`;
  u jednotlivých položek se uvede jejich význam.

  ```c
  /// @brief Reprezentace uzlu binárního stromu.
  typedef struct node {
    int value;          // Hodnota uložená v uzlu.
    struct node *left;  // Levý potomek.
    struct node *right; // Pravý potomek.
  } node_t;
  ```

- Uvnitř kódu používejte komentáře pro jednodušší code review i snazší práci
  ostatních.

## Využití AI nástrojů

Použití AI nástrojů je povolené jen v mezích zadání a musí být **transparentní**.
Projekt musí vznikat vlastní činností – AI je pomocník, ne řešitel.

**Povolené použití:**

- dovysvětlení nejasných částí zadání,
- diskuze o návrhu architektury (na základě vlastních znalostí z přednášek),
- vysvětlení použití nástrojů a konkrétních funkcí,
- hledání chyb v námi napsaném kódu (AI v procesu code review, ale vždy s
  člověkem),
- implementace konkrétních, ohraničených a přesně specifikovaných částí kódu,
- tvorba testů na základě přesného popisu testované situace.

**Zakázané použití:**

- generování či úprava textu závěrečné dokumentace,
- faktická příprava gramatiky, LL tabulky, diagramu konečného automatu,
  precedenční tabulky a jiných podkladových formalismů,
- generování kódu vyhledávací datové struktury (vv-BVS).

**Povinnosti:**

- v dokumentaci uvést, které konkrétní AI nástroje a modely jsme použili, a
  popsat typický postup práce s nimi i problémy, na které jsme narazili,
- **každý segment kódu vytvořený AI označit komentářem** a uvést v něm použitý
  prompt.

Nedovolené a nepřiznané použití AI je disciplinární přestupek.

## Testování

Testování probíhá na dvou úrovních: **bílé skříňky (white-box)** a **černé
skříňky (black-box)**.

### Bílé skříňky – jednotkové testy (autoři modulů)

Jednotkové testy používají knihovnu [cmocka](https://cmocka.org/) a píše je
autor daného modulu, protože zná jeho vnitřnosti (white-box testing).

- **Každá hlavička (`src/*.h`) má svůj vlastní testovací soubor**
  `tests/utest/test_<modul>.c` (např. `scanner.h` → `tests/utest/test_scanner.c`).
  Tím je zaručeno, že každá část programu má pokryté chování.
- **Pokrývají se všechny varianty a větve** dané funkce, ne jen „šťastná cesta“:
  - platné vstupy i ty neplatné/okrajové (`NULL`, prázdný vstup, přetečení,
    mezní hodnoty);
  - všechny chybové stavy a návratové hodnoty `IFJ_ERR`;
  - každá větev `if`/`else`, `switch` a cyklus alespoň jednou.
- Testy musí být čisté i z hlediska paměti – `make test` je po spuštění pouští
  znovu pod [Valgrindem](https://valgrind.org/) a jakýkoli únik paměti test
  shodí.

Nový test se přidává jako `tests/utest/test_*.c`; cíl `utest_*` se v Makefile
vygeneruje automaticky. Hlavička `tests/utest/setup_test.h` obsahuje společné
zahrnuté hlavičky pro testy.

### Černé skříňky – testy celého programu (interní testeři)

Testeři nezávislí na implementaci ověřují hotový program jako celek
(black-box testing) a vnitřnosti modulů neznají ani je nepoužívají.

- **Vstupní programy** v jazyce IFJ26 (`*.ifj`) se ukládají do `tests/data/`.
- **Jednotlivé systémové testy** (skripty/spouštěcí logika, které program
  přeloží a porovnají výstup) patří do `tests/system/`.
- Program se přeloží naším kompilátorem `ifj26` a zkontroluje se, že se chová
  přesně tak, jak bylo zadáno – tedy že výstup/interpretace odpovídá očekávání.
- Testuje se i to, co autor modulu nemusel vidět: správná chybová hlášení a
  návratové kódy pro nevalidní vstupy.

### Spuštění testů

```sh
make          # sestaví kompilátor ifj26 pro black-box testy
make test     # přeloží a spustí všechny jednotkové testy (cmocka) a Valgrind
```
