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
# Proměnné prostředí: COMPILER (výchozí ./ifj26), INTERPRET (výchozí ic26int),
# VALGRIND (výchozí 1).
# CATEGORIES (výchozí "1_lex 2_syntax" - kategorie testů, které se spouští).

COMPILER=${COMPILER:-./ifj26}
INTERPRET=${INTERPRET:-ic26int}
VALGRIND=${VALGRIND:-1}
DIR="$(dirname "$0")/../data"

# Návratový kód Valgrindu při chybě paměti, nesmí kolidovat s kódy překladače.
VALGRIND_ERR=100

# Zatím je implementovaný jen lexikální a syntaktický analyzátor, takže se
# standardně spouštějí jen testy chyb 1 a 2. Po dokončení dalších fází
# překladače lze přidat další kategorie, např.:
#   CATEGORIES="1_lex 2_syntax 3_undef 4_redef" bash tests/system/run_systests.sh
CATEGORIES=${CATEGORIES:-"1_lex 2_syntax 3_undef 4_redef 5_args 6_type 7_immut 10_other"}

if [ ! -x "$COMPILER" ]; then
  echo "Překladač $COMPILER neexistuje, nejdřív spusť make."
  exit 1
fi

HAVE_INTERPRET=0
if command -v "$INTERPRET" >/dev/null 2>&1; then
  HAVE_INTERPRET=1
fi

HAVE_VALGRIND=0
if [ "$VALGRIND" != 0 ] && command -v valgrind >/dev/null 2>&1; then
  HAVE_VALGRIND=1
fi

TMP_CODE=$(mktemp)
TMP_OUT=$(mktemp)
TMP_VG=$(mktemp)
trap 'rm -f "$TMP_CODE" "$TMP_OUT" "$TMP_VG"' EXIT

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
        --error-exitcode="$VALGRIND_ERR" "$COMPILER" <"$test" \
        >/dev/null 2>"$TMP_VG"
      if [ $? -eq "$VALGRIND_ERR" ]; then
        echo "FAIL $name: Valgrind našel chybu paměti:"
        sed 's/^/    /' "$TMP_VG"
        mem_fail=$((mem_fail + 1))
      fi
    fi

    "$COMPILER" <"$test" >"$TMP_CODE" 2>/dev/null
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

    if [ "$HAVE_INTERPRET" -eq 0 ]; then
      skip=$((skip + 1))
      continue
    fi

    input="${test%.ifj}.in"
    [ -f "$input" ] || input=/dev/null
    "$INTERPRET" "$TMP_CODE" <"$input" >"$TMP_OUT" 2>/dev/null
    rc=$?

    if [ "$rc" -ne "$expected" ]; then
      echo "FAIL $name: interpret vrátil $rc, očekáváno $expected"
      fail=$((fail + 1))
      continue
    fi

    expected_out="${test%.ifj}.out"
    if [ -f "$expected_out" ] && ! cmp -s "$TMP_OUT" "$expected_out"; then
      echo "FAIL $name: výstup se liší od $(basename "$expected_out")"
      fail=$((fail + 1))
      continue
    fi
    pass=$((pass + 1))
  done
done

echo ""
echo "Prošlo: $pass, selhalo: $fail, přeskočeno (chybí $INTERPRET): $skip"
if [ "$HAVE_VALGRIND" -eq 1 ]; then
  echo "Chyby paměti (Valgrind): $mem_fail"
else
  echo "Valgrind se nespouštěl (není nainstalovaný nebo VALGRIND=0)."
fi
[ "$fail" -eq 0 ] && [ "$mem_fail" -eq 0 ]
