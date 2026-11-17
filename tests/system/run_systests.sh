#!/usr/bin/env bash
#
# @file run_systests.sh
# @brief IFJ26 - spouštění systémových testů překladače
#
# @author xjurokv00 Vojtěch Jurok
#
# Testy vygenerovala AI (Claude, model Opus 5.5) podle tohoto zadání:
#
# "Udělej mi systémové testy pro náš překladač IFJ26. Zaměř se hlavně na
# programy, které obsahují chyby, ať jsou pokryté všechny chybové kódy ze
# zadání (1-7, 10 a běhová 40) a ideálně aspoň 5 testů na jednu chybu.
# Máme rozšíření FUNEXP, BOOLTHEN, CONTROL, VALMEM, LISTS a PARAMLISTS, tak
# ať tam nejsou věci, které s nimi chyba nejsou. Přidej i pár správných
# programů a skript, co to celé pustí a porovná návratový kód."
#
# Použití (z kořene repozitáře, po `make`):
#   bash tests/system/run_systests.sh
#
# Testovací programy jsou v tests/data/, očekávaný návratový kód je číslo
# na začátku názvu složky (např. tests/data/3_undef -> 3).
# Složky 0_ok a 40_runtime se musí přeložit bez chyby; pokud je dostupný
# interpret ic26int, spustí se i vygenerovaný kód a ověří se jeho návratový
# kód (0 / 40) a případně výstup podle souboru .out.
#
# Každý test se navíc přeloží i pod Valgrindem (pokud je nainstalovaný)
# a únik paměti nebo jiná chyba paměti test shodí. Valgrind se dá vypnout
# proměnnou VALGRIND=0.
#
# Proměnné prostředí: COMPILER (výchozí ./ifj26),
# INTERPRET (výchozí ./ic26int-linux-x86_64), INTERPRET_FLAGS (výchozí prázdné),
# VALGRIND (výchozí 1), TIMEOUT (limit v sekundách na jeden běh interpretu,
# výchozí 10; 0 = bez limitu).
# CATEGORIES (výchozí všechny kategorie - viz níže).

COMPILER=${COMPILER:-./ifj26}
INTERPRET=${INTERPRET:-./ic26int-linux-x86_64}
INTERPRET_FLAGS=${INTERPRET_FLAGS:-}
VALGRIND=${VALGRIND:-1}
TIMEOUT=${TIMEOUT:-10}
DIR="$(dirname "$0")/../data"

# Návratový kód Valgrindu při chybě paměti, nesmí kolidovat s kódy překladače.
VALGRIND_ERR=100

# Standardně se spouštějí všechny kategorie. Během vývoje se dají omezit
# proměnnou CATEGORIES, např.:
#   CATEGORIES="1_lex 2_syntax" bash tests/system/run_systests.sh
CATEGORIES=${CATEGORIES:-"0_ok 1_lex 2_syntax 3_undef 4_redef 5_args 6_type 7_immut 10_other 40_runtime"}

# Vrátí cestu ke spustitelnému nástroji, nebo nic (prázdný výstup).
resolve_tool() {
  if [ -f "$1" ] && [ -x "$1" ]; then
    printf '%s\n' "$1"
    return 0
  fi
  command -v "$1" 2>/dev/null
}

# ic26int musí být skutečný ELF binární soubor (ne např. přepsaný textový
# výstup překladače, který je omylem spustitelný).
is_elf() {
  [ "$(head -c4 -- "$1" 2>/dev/null | od -An -tx1 | tr -d ' \n')" = "7f454c46" ]
}

# Slovní popis návratových kódů interpretu ic26int (spec 7.1).
interpret_msg() {
  case "$1" in
    50) echo "chybné parametry interpretu" ;;
    51) echo "lexikální/syntaktická chyba IFJcode26" ;;
    52) echo "sémantická chyba IFJcode26 (redefinice / neexistující návěští)" ;;
    53) echo "runtime: špatné typy operandů" ;;
    54) echo "runtime: neexistující proměnná" ;;
    55) echo "runtime: neexistující rámec" ;;
    56) echo "runtime: chybějící hodnota" ;;
    57) echo "runtime: špatná hodnota operandu" ;;
    58) echo "runtime: chybná práce s řetězcem/polem" ;;
    124) echo "timeout" ;;
    *) echo "neznámý kód" ;;
  esac
}

COMPILER_BIN=$(resolve_tool "$COMPILER")
INTERPRET_BIN=$(resolve_tool "$INTERPRET")

if [ -z "$COMPILER_BIN" ]; then
  echo "Překladač $COMPILER neexistuje, nejdřív spusť make." >&2
  exit 1
fi

HAVE_INTERPRET=0
if [ -n "$INTERPRET_BIN" ]; then
  if is_elf "$INTERPRET_BIN" ||
    head -n1 -- "$INTERPRET_BIN" 2>/dev/null | grep -q '^#!'; then
    HAVE_INTERPRET=1
  else
    echo "CHYBA: $INTERPRET_BIN není spustitelný program (očekáván ELF)." >&2
    echo "       Stáhni ic26int znovu z e-learningu (spec 7.1)." >&2
    exit 1
  fi
fi

HAVE_VALGRIND=0
if [ "$VALGRIND" != 0 ] && command -v valgrind >/dev/null 2>&1; then
  HAVE_VALGRIND=1
fi

TMP_CODE=$(mktemp)
TMP_OUT=$(mktemp)
TMP_VG=$(mktemp)
TMP_IERR=$(mktemp)
trap 'rm -f "$TMP_CODE" "$TMP_OUT" "$TMP_VG" "$TMP_IERR"' EXIT

pass=0
fail=0
skip=0
mem_fail=0

for category in $CATEGORIES; do
  for test in "$DIR"/"$category"/*.ifj; do
    [ -e "$test" ] || continue
    expected=${category%%_*}
    name="$category/$(basename "$test")"

    # Kontrola paměti překladače pod Valgrindem
    if [ "$HAVE_VALGRIND" -eq 1 ]; then
      valgrind --quiet --leak-check=full --track-origins=yes \
        --error-exitcode="$VALGRIND_ERR" "$COMPILER_BIN" <"$test" \
        >/dev/null 2>"$TMP_VG"
      if [ $? -eq "$VALGRIND_ERR" ]; then
        echo "FAIL $name: Valgrind našel chybu paměti:"
        sed 's/^/    /' "$TMP_VG"
        mem_fail=$((mem_fail + 1))
      fi
    fi

    "$COMPILER_BIN" <"$test" >"$TMP_CODE" 2>/dev/null
    rc=$?

    # Chyba při překladu (kódy 1-7, 10)
    if [ "$expected" != 0 ] && [ "$expected" != 40 ]; then
      if [ "$rc" -eq "$expected" ]; then
        pass=$((pass + 1))
      else
        echo "FAIL $name: očekáváno $expected, vráceno $rc"
        fail=$((fail + 1))
      fi
      continue
    fi

    # Správný program nebo běhová chyba: překlad musí projít
    if [ "$rc" -ne 0 ]; then
      echo "FAIL $name: překlad vrátil $rc, očekáváno 0"
      fail=$((fail + 1))
      continue
    fi

    # Překladač musí vygenerovat platný IFJcode26 (jinak by interpret
    # hlásil nesmyslnou chybu 51 a nebylo by vidět, že chyba je v generátoru).
    if ! head -n1 "$TMP_CODE" | grep -Fqx '.IFJcode26'; then
      echo "FAIL $name: výstup překladače nezačíná řádkem .IFJcode26"
      fail=$((fail + 1))
      continue
    fi

    if [ "$HAVE_INTERPRET" -eq 0 ]; then
      skip=$((skip + 1))
      continue
    fi

    input="${test%.ifj}.in"
    [ -f "$input" ] || input=/dev/null

    if [ "$TIMEOUT" -gt 0 ] 2>/dev/null; then
      timeout --foreground "$TIMEOUT" \
        "$INTERPRET_BIN" $INTERPRET_FLAGS "$TMP_CODE" \
        <"$input" >"$TMP_OUT" 2>"$TMP_IERR"
    else
      "$INTERPRET_BIN" $INTERPRET_FLAGS "$TMP_CODE" \
        <"$input" >"$TMP_OUT" 2>"$TMP_IERR"
    fi
    rc=$?

    if [ "$rc" -ne "$expected" ]; then
      echo "FAIL $name: interpret vrátil $rc ($(interpret_msg "$rc")), očekáváno $expected"
      sed 's/^/    /' "$TMP_IERR"
      fail=$((fail + 1))
      continue
    fi

    expected_out="${test%.ifj}.out"
    if [ -f "$expected_out" ] && ! cmp -s "$TMP_OUT" "$expected_out"; then
      echo "FAIL $name: výstup se liší od $(basename "$expected_out")"
      diff -u "$expected_out" "$TMP_OUT" | sed 's/^/    /'
      fail=$((fail + 1))
      continue
    fi
    pass=$((pass + 1))
  done
done

echo ""
echo "Prošlo: $pass, selhalo: $fail, přeskočeno (chybí interpret $INTERPRET): $skip"
if [ "$HAVE_VALGRIND" -eq 1 ]; then
  echo "Chyby paměti (Valgrind): $mem_fail"
else
  echo "Valgrind se nespouštěl (není nainstalovaný nebo VALGRIND=0)."
fi
[ "$fail" -eq 0 ] && [ "$mem_fail" -eq 0 ]
