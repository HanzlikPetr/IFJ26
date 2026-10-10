/// @file test_scanner.c
/// @brief Projekt IFJ26 - jednotkové testy lexikálního analyzátoru
///
/// @author xjurokv00 Vojtěch Jurok

// fmemopen() je POSIX funkce, s -std=c11 ji je nutné zpřístupnit.
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>

#include "setup_test.h"

#include "scanner.h"

// ---------------------------------------------------------------------------
// Pomocné funkce
// ---------------------------------------------------------------------------

/// @brief Otevře řetězec jako vstupní soubor (v paměti, žádný soubor na
/// disku nevzniká).
/// @param src Zdrojový text.
/// @return Otevřený proud, nebo NULL při chybě.
static FILE *open_src(const char *src) {
  return fmemopen((void *)src, strlen(src), "r");
}

/// @brief Inicializuje scanner nad zadaným textem.
/// @param src Zdrojový text.
/// @return Otevřený proud (uzavírá se funkcí close_scanner).
static FILE *start_scanner(const char *src) {
  FILE *f = open_src(src);
  assert_non_null(f);
  assert_int_equal(scanner_init(f), IFJ_SUCCESS);
  return f;
}

/// @brief Uvolní scanner a zavře vstup.
/// @param f Proud vrácený ze start_scanner.
static void close_scanner(FILE *f) {
  scanner_destroy();
  fclose(f);
}

/// @brief Načte další token a ověří, že se načetl bez chyby.
/// @param tok Výstupní token.
static void next_ok(token_t *tok) {
  assert_int_equal(scanner_next_token(tok), IFJ_SUCCESS);
}

/// @brief Ověří, že vstup dá přesně zadanou posloupnost typů tokenů.
/// Poslední očekávaný typ musí být TOK_EOF.
/// @param src Zdrojový text.
/// @param types Očekávané typy.
/// @param count Počet očekávaných typů.
static void expect_types(const char *src, const token_type_t *types,
                         size_t count) {
  FILE *f = start_scanner(src);
  token_t tok;
  for (size_t i = 0; i < count; i++) {
    next_ok(&tok);
    if (tok.type != types[i]) {
      fail_msg("vstup \"%s\": token %zu ma typ %d, ocekavan %d", src, i,
               tok.type, types[i]);
    }
    token_free(&tok);
  }
  close_scanner(f);
}

/// @brief Ověří, že po zadaném počtu platných tokenů scanner vrátí chybu.
/// @param src Zdrojový text.
/// @param ok_tokens Počet tokenů, které se načtou bez chyby.
/// @param expected_err Očekávaný chybový kód.
static void expect_error(const char *src, size_t ok_tokens,
                         IFJ_ERR expected_err) {
  FILE *f = start_scanner(src);
  token_t tok;
  for (size_t i = 0; i < ok_tokens; i++) {
    next_ok(&tok);
    token_free(&tok);
  }
  assert_int_equal(scanner_next_token(&tok), expected_err);
  close_scanner(f);
}

/// @brief Načte první token vstupu (pro kontrolu jeho hodnoty).
/// @param src Zdrojový text.
/// @param tok Výstupní token, volající ho uvolní přes token_free.
/// @return Otevřený proud (uzavírá se funkcí close_scanner).
static FILE *first_token(const char *src, token_t *tok) {
  FILE *f = start_scanner(src);
  next_ok(tok);
  return f;
}

#define EXPECT(src, ...)                                                       \
  do {                                                                         \
    const token_type_t expected_[] = {__VA_ARGS__};                            \
    expect_types((src), expected_, sizeof expected_ / sizeof expected_[0]);    \
  } while (0)

// ---------------------------------------------------------------------------
// Inicializace a základní chování
// ---------------------------------------------------------------------------

/// @brief scanner_init s NULL vrací interní chybu.
static void test_init_null(void **state) {
  (void)state;
  assert_int_equal(scanner_init(NULL), IFJ_EINTERNAL);
}

/// @brief Prázdný vstup dá rovnou EOF, opakovaně.
static void test_empty_input(void **state) {
  (void)state;
  FILE *f = start_scanner("");
  token_t tok;
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_EOF);
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_EOF);
  close_scanner(f);
}

/// @brief Vstup jen z mezer a prázdných řádků dá jen EOF.
static void test_only_whitespace(void **state) {
  (void)state;
  EXPECT("   \n\n  \n", TOK_EOF);
}

/// @brief token_free s NULL a s tokenem bez řetězce nespadne.
static void test_token_free_safe(void **state) {
  (void)state;
  token_free(NULL);
  token_t tok = {.type = TOK_ADD};
  token_free(&tok);
}

/// @brief Neznámé znaky jsou lexikální chyba.
static void test_unknown_chars(void **state) {
  (void)state;
  expect_error("#", 0, IFJ_ELEX);
  expect_error("$", 0, IFJ_ELEX);
  expect_error("x ? y", 1, IFJ_ELEX);
  expect_error("{", 0, IFJ_ELEX);
  expect_error("( # )", 1, IFJ_ELEX);
}

// ---------------------------------------------------------------------------
// Operátory a oddělovače
// ---------------------------------------------------------------------------

/// @brief Jednoznakové tokeny.
static void test_single_char_tokens(void **state) {
  (void)state;
  EXPECT("( ) [ ] , : ; * - /", TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET,
         TOK_RBRACKET, TOK_COMMA, TOK_COLON, TOK_SEMICOLON, TOK_MUL, TOK_SUB,
         TOK_DIV, TOK_EOL, TOK_EOF);
}

/// @brief Víceznakové operátory i jejich jednoznakové předpony.
static void test_multi_char_operators(void **state) {
  (void)state;
  EXPECT("< <= <- > >= = == ! != + ++ && ||", TOK_LT, TOK_LEQ, TOK_LARROW,
         TOK_GT, TOK_GEQ, TOK_ASSIGN, TOK_EQ, TOK_NOT, TOK_NEQ, TOK_ADD,
         TOK_CONCAT, TOK_AND, TOK_OR, TOK_EOL, TOK_EOF);
}

/// @brief Operátory bez mezer se nesmí slít, přečtený znak se vrátí.
static void test_operators_without_spaces(void **state) {
  (void)state;
  EXPECT("<(", TOK_LT, TOK_LPAREN, TOK_EOL, TOK_EOF);
  EXPECT("a<b", TOK_ID, TOK_LT, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a<=-b", TOK_ID, TOK_LEQ, TOK_SUB, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("!!", TOK_NOT, TOK_NOT, TOK_EOL, TOK_EOF);
  EXPECT("+++", TOK_CONCAT, TOK_ADD, TOK_EOL, TOK_EOF);
  EXPECT("===", TOK_EQ, TOK_ASSIGN, TOK_EOL, TOK_EOF);
}

/// @brief Operátor na úplném konci souboru.
static void test_operator_at_eof(void **state) {
  (void)state;
  EXPECT("<", TOK_LT, TOK_EOL, TOK_EOF);
  EXPECT("!", TOK_NOT, TOK_EOL, TOK_EOF);
  EXPECT("x /", TOK_ID, TOK_DIV, TOK_EOL, TOK_EOF);
}

/// @brief Samotné & a | jsou chyba.
static void test_single_amp_pipe(void **state) {
  (void)state;
  expect_error("&", 0, IFJ_ELEX);
  expect_error("|", 0, IFJ_ELEX);
  expect_error("a & b", 1, IFJ_ELEX);
  expect_error("a | b", 1, IFJ_ELEX);
}

// ---------------------------------------------------------------------------
// Identifikátory a klíčová slova
// ---------------------------------------------------------------------------

/// @brief Identifikátor nese svůj text.
static void test_identifier_value(void **state) {
  (void)state;
  token_t tok;
  FILE *f = first_token("x_1Abc rest", &tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_string_equal(tok.value.str_value, "x_1Abc");
  token_free(&tok);
  assert_null(tok.value.str_value);
  close_scanner(f);
}

/// @brief Identifikátor začínající podtržítkem.
static void test_identifier_underscore(void **state) {
  (void)state;
  token_t tok;
  FILE *f = first_token("_a", &tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_string_equal(tok.value.str_value, "_a");
  token_free(&tok);
  close_scanner(f);

  f = first_token("__", &tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_string_equal(tok.value.str_value, "__");
  token_free(&tok);
  close_scanner(f);
}

/// @brief Samotné podtržítko není identifikátor.
static void test_lone_underscore(void **state) {
  (void)state;
  expect_error("_", 0, IFJ_ELEX);
  expect_error("_ x", 0, IFJ_ELEX);
  expect_error("x _", 1, IFJ_ELEX);
}

/// @brief Dlouhý identifikátor (vynutí zvětšení dynamického řetězce).
static void test_long_identifier(void **state) {
  (void)state;
  char src[201];
  memset(src, 'a', 200);
  src[200] = '\0';

  token_t tok;
  FILE *f = first_token(src, &tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_string_equal(tok.value.str_value, src);
  token_free(&tok);
  close_scanner(f);
}

/// @brief Každé klíčové slovo se rozpozná se správnou hodnotou.
static void test_all_keywords(void **state) {
  (void)state;
  const struct {
    const char *text;
    keyword_t kw;
  } cases[] = {
      {"def", KW_DEF},
      {"do", KW_DO},
      {"else", KW_ELSE},
      {"Double", KW_DOUBLE},
      {"if", KW_IF},
      {"Int", KW_INT},
      {"return", KW_RETURN},
      {"String", KW_STRING},
      {"import", KW_IMPORT},
      {"while", KW_WHILE},
      {"for", KW_FOR},
      {"then", KW_THEN},
      {"Unit", KW_UNIT},
      {"var", KW_VAR},
      {"val", KW_VAL},
      {"Boolean", KW_BOOLEAN},
      {"true", KW_TRUE},
      {"false", KW_FALSE},
      {"List", KW_LIST},
      {"case", KW_CASE},
      {"match", KW_MATCH},
      {"class", KW_CLASS},
      {"extension", KW_EXTENSION},
      {"type", KW_TYPE},
  };

  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    token_t tok;
    FILE *f = first_token(cases[i].text, &tok);
    if (tok.type != TOK_KEYWORD || tok.value.keyword != cases[i].kw) {
      fail_msg("\"%s\" neni rozpoznano jako klicove slovo %d", cases[i].text,
               cases[i].kw);
    }
    close_scanner(f);
  }
}

/// @brief Klíčová slova rozlišují velikost písmen a musí sedět celá.
static void test_keyword_like_identifiers(void **state) {
  (void)state;
  const char *ids[] = {"int",  "double", "DEF",  "If",    "vals",
                       "def1", "_do",    "main", "print", "readInt"};
  for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    token_t tok;
    FILE *f = first_token(ids[i], &tok);
    if (tok.type != TOK_ID) {
      fail_msg("\"%s\" ma byt identifikator", ids[i]);
    }
    assert_string_equal(tok.value.str_value, ids[i]);
    token_free(&tok);
    close_scanner(f);
  }
}

// ---------------------------------------------------------------------------
// Celá čísla
// ---------------------------------------------------------------------------

/// @brief Ověří, že vstup začíná celým číslem dané hodnoty.
static void check_int(const char *src, long long value) {
  token_t tok;
  FILE *f = first_token(src, &tok);
  if (tok.type != TOK_INT) {
    fail_msg("\"%s\" ma byt Int, typ je %d", src, tok.type);
  }
  assert_int_equal(tok.value.int_value, value);
  close_scanner(f);
}

/// @brief Platná celá čísla.
static void test_int_values(void **state) {
  (void)state;
  check_int("0", 0);
  check_int("7", 7);
  check_int("42", 42);
  check_int("1234567", 1234567);
  check_int("2147483647", 2147483647LL);
}

/// @brief Podtržítka mezi číslicemi.
static void test_int_underscores(void **state) {
  (void)state;
  check_int("1_000_000", 1000000);
  check_int("1__0", 10);
  check_int("9_9", 99);
}

/// @brief Chybná celá čísla.
static void test_int_errors(void **state) {
  (void)state;
  expect_error("012", 0, IFJ_ELEX);
  expect_error("00", 0, IFJ_ELEX);
  expect_error("0_1", 0, IFJ_ELEX);
  expect_error("1_", 0, IFJ_ELEX);
  expect_error("1_a", 0, IFJ_ELEX);
  expect_error("1__", 0, IFJ_ELEX);
}

/// @brief Záporné číslo jsou dva tokeny.
static void test_negative_number(void **state) {
  (void)state;
  EXPECT("-5", TOK_SUB, TOK_INT, TOK_EOL, TOK_EOF);
}

/// @brief Číslo následované identifikátorem nejsou lexikální chyba.
static void test_int_followed_by_id(void **state) {
  (void)state;
  EXPECT("10abc", TOK_INT, TOK_ID, TOK_EOL, TOK_EOF);
}

// ---------------------------------------------------------------------------
// Desetinná čísla
// ---------------------------------------------------------------------------

/// @brief Ověří, že vstup začíná desetinným číslem dané hodnoty.
static void check_double(const char *src, double value) {
  token_t tok;
  FILE *f = first_token(src, &tok);
  if (tok.type != TOK_FLOAT) {
    fail_msg("\"%s\" ma byt Double, typ je %d", src, tok.type);
  }
  assert_double_equal(tok.value.double_value, value, 1e-12 * (value + 1));
  close_scanner(f);
}

/// @brief Desetinná tečka.
static void test_double_dot(void **state) {
  (void)state;
  check_double("1.5", 1.5);
  check_double("0.25", 0.25);
  check_double("0.1", 0.1);
  check_double("123.456", 123.456);
}

/// @brief Desetinné číslo bez celé části.
static void test_double_no_int_part(void **state) {
  (void)state;
  check_double(".5", 0.5);
  check_double(".25_0", 0.25);
}

/// @brief Podtržítka v desetinných číslech.
static void test_double_underscores(void **state) {
  (void)state;
  check_double("1.5_0", 1.5);
  check_double("12_3.4_5", 123.45);
}

/// @brief Exponent.
static void test_double_exponent(void **state) {
  (void)state;
  check_double("1e5", 1e5);
  check_double("1E5", 1e5);
  check_double("2.5e-3", 2.5e-3);
  check_double("2.5E+3", 2.5e3);
  check_double("0e5", 0.0);
  check_double("0.5e1", 5.0);
  check_double("1e+1_0", 1e10);
}

/// @brief Přípona d/D.
static void test_double_suffix(void **state) {
  (void)state;
  check_double("1.5d", 1.5);
  check_double("1.5D", 1.5);
  check_double("2e3d", 2e3);
  check_double(".5d", 0.5);
  EXPECT("1.5dx", TOK_FLOAT, TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Chybná desetinná čísla.
static void test_double_errors(void **state) {
  (void)state;
  expect_error("1e", 0, IFJ_ELEX);
  expect_error("1e+", 0, IFJ_ELEX);
  expect_error("1e_5", 0, IFJ_ELEX);
  expect_error("1ea", 0, IFJ_ELEX);
  expect_error("1.5_", 0, IFJ_ELEX);
  expect_error("1.5_d", 0, IFJ_ELEX);
  expect_error("1e5_", 0, IFJ_ELEX);
}

/// @brief Číslo s tečkou bez desetinné části: celé číslo a tečka.
static void test_int_then_dot(void **state) {
  (void)state;
  EXPECT("1.toString", TOK_INT, TOK_DOT, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("0.x", TOK_INT, TOK_DOT, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("1.", TOK_INT, TOK_DOT, TOK_EOL, TOK_EOF);
  check_int("1.toString", 1);
}

/// @brief Samostatná tečka a druhá tečka za desetinným číslem.
static void test_dot_token(void **state) {
  (void)state;
  EXPECT("ifj26.x . y", TOK_ID, TOK_DOT, TOK_ID, TOK_DOT, TOK_ID, TOK_EOL,
         TOK_EOF);
  EXPECT("import ifj26.*", TOK_KEYWORD, TOK_ID, TOK_DOT, TOK_MUL, TOK_EOL,
         TOK_EOF);
  EXPECT("1.5.x", TOK_FLOAT, TOK_DOT, TOK_ID, TOK_EOL, TOK_EOF);
}

// ---------------------------------------------------------------------------
// Řetězce
// ---------------------------------------------------------------------------

/// @brief Ověří, že vstup začíná řetězcem s daným obsahem.
static void check_string(const char *src, const char *value) {
  token_t tok;
  FILE *f = first_token(src, &tok);
  if (tok.type != TOK_STRING) {
    fail_msg("vstup ma zacinat retezcem, typ je %d", tok.type);
  }
  assert_string_equal(tok.value.str_value, value);
  token_free(&tok);
  close_scanner(f);
}

/// @brief Obyčejné řetězce.
static void test_string_simple(void **state) {
  (void)state;
  check_string("\"ahoj\"", "ahoj");
  check_string("\" s mezerami \"", " s mezerami ");
  check_string("\"a#b//c/*d\"", "a#b//c/*d");
}

/// @brief Prázdný řetězec.
static void test_string_empty(void **state) {
  (void)state;
  check_string("\"\"", "");
  EXPECT("\"\" x \"\"", TOK_STRING, TOK_ID, TOK_STRING, TOK_EOL, TOK_EOF);
}

/// @brief Escape sekvence se převedou na skutečné znaky.
static void test_string_escapes(void **state) {
  (void)state;
  check_string("\"a\\\\b\"", "a\\b");
  check_string("\"c\\\"d\"", "c\"d");
  check_string("\"x\\ny\"", "x\ny");
  check_string("\"x\\ry\"", "x\ry");
  check_string("\"x\\ty\"", "x\ty");
  check_string("\"\\n\"", "\n");
}

/// @brief Chybné řetězce.
static void test_string_errors(void **state) {
  (void)state;
  expect_error("\"neukonceny", 0, IFJ_ELEX);
  expect_error("\"pres\nradek\"", 0, IFJ_ELEX);
  expect_error("\"a\\qb\"", 0, IFJ_ELEX);
  expect_error("\"a\\x41\"", 0, IFJ_ELEX);
  expect_error("\"a\\", 0, IFJ_ELEX);
  expect_error("\"", 0, IFJ_ELEX);
}

/// @brief Víceřádkový řetězec (příklad ze specifikace).
static void test_ml_string_spec_example(void **state) {
  (void)state;
  check_string("\"\"\"A\n hoj s\\nvete\n \"\"\"", "A\n hoj s\\nvete\n ");
}

/// @brief Uvozovky uvnitř víceřádkového řetězce.
static void test_ml_string_quotes(void **state) {
  (void)state;
  check_string("\"\"\"a\"b\"\"c\"\"\"", "a\"b\"\"c");
  check_string("\"\"\"\"\"\"", "");
}

/// @brief Řádky ve víceřádkovém řetězci se započítají.
static void test_ml_string_lines(void **state) {
  (void)state;
  FILE *f = start_scanner("\"\"\"a\nb\nc\"\"\" x");
  token_t tok;
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_STRING);
  assert_int_equal(tok.line, 1);
  token_free(&tok);
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_int_equal(tok.line, 3);
  token_free(&tok);
  close_scanner(f);
}

/// @brief Neukončený víceřádkový řetězec.
static void test_ml_string_errors(void **state) {
  (void)state;
  expect_error("\"\"\"neukonceny\n", 0, IFJ_ELEX);
  expect_error("\"\"\"a\"", 0, IFJ_ELEX);
  expect_error("\"\"\"a\"\"", 0, IFJ_ELEX);
}

// ---------------------------------------------------------------------------
// @main
// ---------------------------------------------------------------------------

/// @brief Anotace @main.
static void test_annot_main(void **state) {
  (void)state;
  EXPECT("@main def", TOK_ANNOT_MAIN, TOK_KEYWORD, TOK_EOL, TOK_EOF);
  EXPECT("@main", TOK_ANNOT_MAIN, TOK_EOL, TOK_EOF);
}

/// @brief Chybné anotace.
static void test_annot_errors(void **state) {
  (void)state;
  expect_error("@ main", 0, IFJ_ELEX);
  expect_error("@mainx", 0, IFJ_ELEX);
  expect_error("@foo", 0, IFJ_ELEX);
  expect_error("@1", 0, IFJ_ELEX);
  expect_error("@", 0, IFJ_ELEX);
}

// ---------------------------------------------------------------------------
// Komentáře
// ---------------------------------------------------------------------------

/// @brief Řádkový komentář.
static void test_line_comment(void **state) {
  (void)state;
  EXPECT("x // komentar\ny", TOK_ID, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("// jen komentar", TOK_EOF);
  EXPECT("x // bez konce radku", TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Blokový komentář na jednom řádku se chová jako mezera.
static void test_block_comment_inline(void **state) {
  (void)state;
  EXPECT("a /* x */ b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a/**/b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a /***/ b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a /* ** / */ b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Vnořené blokové komentáře.
static void test_block_comment_nested(void **state) {
  (void)state;
  EXPECT("a /* 1 /* 2 */ 1 */ b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a /* /* /* */ */ */ b", TOK_ID, TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Neukončené blokové komentáře.
static void test_block_comment_unterminated(void **state) {
  (void)state;
  expect_error("a /* neukonceny", 1, IFJ_ELEX);
  expect_error("a /* /* */", 1, IFJ_ELEX);
  expect_error("/* *", 0, IFJ_ELEX);
  expect_error("/* /", 0, IFJ_ELEX);
}

/// @brief Blokový komentář s novým řádkem se chová jako konec řádku.
static void test_block_comment_multiline(void **state) {
  (void)state;
  EXPECT("x /* a\nb */\ny", TOK_ID, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("x\n/* a\nb */\ny", TOK_ID, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
  // Za víceřádkovým komentářem nesmí na stejném řádku být token.
  expect_error("x /* a\nb */ y", 2, IFJ_ELEX);
  // Uvnitř závorek je to jen bílý znak.
  EXPECT("f(x /* a\nb */ y)", TOK_ID, TOK_LPAREN, TOK_ID, TOK_ID, TOK_RPAREN,
         TOK_EOL, TOK_EOF);
}

/// @brief Komentáře nezkazí čísla řádků.
static void test_comment_line_numbers(void **state) {
  (void)state;
  FILE *f = start_scanner("/* a\n*/ b");
  token_t tok;
  // "/* a\n*/" na samostatném řádku, za ním je token -> chyba.
  assert_int_equal(scanner_next_token(&tok), IFJ_ELEX);
  close_scanner(f);

  f = start_scanner("/* a\nb */\nc");
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_int_equal(tok.line, 3);
  token_free(&tok);
  close_scanner(f);
}

// ---------------------------------------------------------------------------
// Konce řádků
// ---------------------------------------------------------------------------

/// @brief Čísla řádků tokenů.
static void test_line_numbers(void **state) {
  (void)state;
  FILE *f = start_scanner("a\nb\n\nc");
  token_t tok;
  int expected_lines[] = {1, 1, 2, 2, 4, 4};
  token_type_t expected_types[] = {TOK_ID,  TOK_EOL, TOK_ID,
                                   TOK_EOL, TOK_ID,  TOK_EOL};
  for (size_t i = 0; i < 6; i++) {
    next_ok(&tok);
    assert_int_equal(tok.type, expected_types[i]);
    assert_int_equal(tok.line, expected_lines[i]);
    token_free(&tok);
  }
  close_scanner(f);
}

/// @brief Prázdné řádky a řádky jen s komentářem nedávají EOL.
static void test_empty_lines_skipped(void **state) {
  (void)state;
  EXPECT("x\n\n   \n// komentar\n/* blok */\ny\n", TOK_ID, TOK_EOL, TOK_ID,
         TOK_EOL, TOK_EOF);
  EXPECT("\n\nx\n", TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Poslední řádek bez '\n' dostane EOL.
static void test_eol_added_at_eof(void **state) {
  (void)state;
  EXPECT("a", TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("a\n", TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Uvnitř závorek není '\n' konec řádku.
static void test_newline_in_parens(void **state) {
  (void)state;
  EXPECT("f(a,\n  b)\nx\n", TOK_ID, TOK_LPAREN, TOK_ID, TOK_COMMA, TOK_ID,
         TOK_RPAREN, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT("g(\n// koment\n1)\n", TOK_ID, TOK_LPAREN, TOK_INT, TOK_RPAREN,
         TOK_EOL, TOK_EOF);
  EXPECT("l[\n1]\n", TOK_ID, TOK_LBRACKET, TOK_INT, TOK_RBRACKET, TOK_EOL,
         TOK_EOF);
  EXPECT("f((\n))\n", TOK_ID, TOK_LPAREN, TOK_LPAREN, TOK_RPAREN, TOK_RPAREN,
         TOK_EOL, TOK_EOF);
}

/// @brief Přebytečná zavírací závorka nerozbije počítání.
static void test_extra_closing_paren(void **state) {
  (void)state;
  EXPECT(")\nx\n", TOK_RPAREN, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
  EXPECT(")\n  x\n", TOK_RPAREN, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_DEDENT, TOK_EOF);
}

// ---------------------------------------------------------------------------
// Tabulátory a odsazení
// ---------------------------------------------------------------------------

/// @brief Tabulátor uprostřed řádku je bílý znak.
static void test_tab_inside_line(void **state) {
  (void)state;
  EXPECT("x =\t5\n", TOK_ID, TOK_ASSIGN, TOK_INT, TOK_EOL, TOK_EOF);
}

/// @brief Tabulátor v odsazení je chyba.
static void test_tab_in_indent(void **state) {
  (void)state;
  expect_error("\tx\n", 0, IFJ_ELEX);
  expect_error("x\n  \ty\n", 2, IFJ_ELEX);
}

/// @brief INDENT při větším odsazení.
static void test_indent(void **state) {
  (void)state;
  EXPECT("a\n  b\n  c\n    d\n", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT, TOK_DEDENT,
         TOK_EOF);
}

/// @brief Jeden i více DEDENTů najednou.
static void test_dedent(void **state) {
  (void)state;
  EXPECT("a\n  b\n    c\nd\n", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT, TOK_DEDENT, TOK_ID, TOK_EOL,
         TOK_EOF);
  EXPECT("a\n  b\n    c\n  d\n", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT, TOK_ID, TOK_EOL, TOK_DEDENT,
         TOK_EOF);
}

/// @brief Odsazení neodpovídající žádné předchozí úrovni.
static void test_bad_dedent(void **state) {
  (void)state;
  expect_error("a\n    b\n  c\n", 5, IFJ_ELEX);
}

/// @brief Na konci souboru se zavřou všechny bloky.
static void test_dedent_at_eof(void **state) {
  (void)state;
  EXPECT("a\n  b\n    c\n", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT, TOK_DEDENT, TOK_EOF);
  EXPECT("a\n  b", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT,
         TOK_EOF);
}

/// @brief Prázdné řádky a komentáře uvnitř bloku odsazení nemění.
static void test_indent_ignores_empty_lines(void **state) {
  (void)state;
  EXPECT("a\n  b\n\n// k\n  c\n", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL,
         TOK_ID, TOK_EOL, TOK_DEDENT, TOK_EOF);
}

/// @brief Pokračování řádku v závorce nemá vlastní odsazení.
static void test_indent_in_parens(void **state) {
  (void)state;
  EXPECT("f(a,\n      b)\nc\n", TOK_ID, TOK_LPAREN, TOK_ID, TOK_COMMA, TOK_ID,
         TOK_RPAREN, TOK_EOL, TOK_ID, TOK_EOL, TOK_EOF);
}

/// @brief Hluboké vnoření (vynutí zvětšení zásobníku odsazení).
static void test_deep_indent(void **state) {
  (void)state;
  enum { DEPTH = 40 };
  char src[DEPTH * (DEPTH + 3) + 1];
  size_t pos = 0;
  for (int i = 0; i < DEPTH; i++) {
    for (int s = 0; s < i; s++) {
      src[pos++] = ' ';
    }
    src[pos++] = 'x';
    src[pos++] = '\n';
  }
  src[pos] = '\0';

  FILE *f = start_scanner(src);
  token_t tok;
  int indents = 0;
  int dedents = 0;
  do {
    next_ok(&tok);
    if (tok.type == TOK_INDENT) {
      indents++;
    } else if (tok.type == TOK_DEDENT) {
      dedents++;
    }
    token_free(&tok);
  } while (tok.type != TOK_EOF);
  assert_int_equal(indents, DEPTH - 1);
  assert_int_equal(dedents, DEPTH - 1);
  close_scanner(f);
}

// ---------------------------------------------------------------------------
// Celý program
// ---------------------------------------------------------------------------

/// @brief Ukázkový program ze specifikace.
static void test_spec_program(void **state) {
  (void)state;
  const char *src = "import ifj26.*\n"
                    "def factorial(n: Int): Int =\n"
                    "  var result = 1\n"
                    "  while n > 1 do\n"
                    "    result = result * n\n"
                    "  result\n"
                    "\n"
                    "@main def run(): Unit =\n"
                    "  print(\"\\n\")\n";
  EXPECT(src,
         // import ifj26.*
         TOK_KEYWORD, TOK_ID, TOK_DOT, TOK_MUL, TOK_EOL,
         // def factorial(n: Int): Int =
         TOK_KEYWORD, TOK_ID, TOK_LPAREN, TOK_ID, TOK_COLON, TOK_KEYWORD,
         TOK_RPAREN, TOK_COLON, TOK_KEYWORD, TOK_ASSIGN, TOK_EOL,
         //   var result = 1
         TOK_INDENT, TOK_KEYWORD, TOK_ID, TOK_ASSIGN, TOK_INT, TOK_EOL,
         //   while n > 1 do
         TOK_KEYWORD, TOK_ID, TOK_GT, TOK_INT, TOK_KEYWORD, TOK_EOL,
         //     result = result * n
         TOK_INDENT, TOK_ID, TOK_ASSIGN, TOK_ID, TOK_MUL, TOK_ID, TOK_EOL,
         //   result
         TOK_DEDENT, TOK_ID, TOK_EOL,
         // @main def run(): Unit =
         TOK_DEDENT, TOK_ANNOT_MAIN, TOK_KEYWORD, TOK_ID, TOK_LPAREN,
         TOK_RPAREN, TOK_COLON, TOK_KEYWORD, TOK_ASSIGN, TOK_EOL,
         //   print("\n")
         TOK_INDENT, TOK_ID, TOK_LPAREN, TOK_STRING, TOK_RPAREN, TOK_EOL,
         TOK_DEDENT, TOK_EOF);
}

/// @brief Scanner jde znovu inicializovat po zničení (bez úniku paměti).
static void test_reinit(void **state) {
  (void)state;
  EXPECT("a\n  b", TOK_ID, TOK_EOL, TOK_INDENT, TOK_ID, TOK_EOL, TOK_DEDENT,
         TOK_EOF);
  // Druhý běh musí začít s čistým stavem (řádek 1, odsazení 0).
  FILE *f = start_scanner("c");
  token_t tok;
  next_ok(&tok);
  assert_int_equal(tok.type, TOK_ID);
  assert_int_equal(tok.line, 1);
  token_free(&tok);
  close_scanner(f);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      // Inicializace
      cmocka_unit_test(test_init_null),
      cmocka_unit_test(test_empty_input),
      cmocka_unit_test(test_only_whitespace),
      cmocka_unit_test(test_token_free_safe),
      cmocka_unit_test(test_unknown_chars),
      // Operátory
      cmocka_unit_test(test_single_char_tokens),
      cmocka_unit_test(test_multi_char_operators),
      cmocka_unit_test(test_operators_without_spaces),
      cmocka_unit_test(test_operator_at_eof),
      cmocka_unit_test(test_single_amp_pipe),
      // Identifikátory
      cmocka_unit_test(test_identifier_value),
      cmocka_unit_test(test_identifier_underscore),
      cmocka_unit_test(test_lone_underscore),
      cmocka_unit_test(test_long_identifier),
      cmocka_unit_test(test_all_keywords),
      cmocka_unit_test(test_keyword_like_identifiers),
      // Celá čísla
      cmocka_unit_test(test_int_values),
      cmocka_unit_test(test_int_underscores),
      cmocka_unit_test(test_int_errors),
      cmocka_unit_test(test_negative_number),
      cmocka_unit_test(test_int_followed_by_id),
      // Desetinná čísla
      cmocka_unit_test(test_double_dot),
      cmocka_unit_test(test_double_no_int_part),
      cmocka_unit_test(test_double_underscores),
      cmocka_unit_test(test_double_exponent),
      cmocka_unit_test(test_double_suffix),
      cmocka_unit_test(test_double_errors),
      cmocka_unit_test(test_int_then_dot),
      cmocka_unit_test(test_dot_token),
      // Řetězce
      cmocka_unit_test(test_string_simple),
      cmocka_unit_test(test_string_empty),
      cmocka_unit_test(test_string_escapes),
      cmocka_unit_test(test_string_errors),
      cmocka_unit_test(test_ml_string_spec_example),
      cmocka_unit_test(test_ml_string_quotes),
      cmocka_unit_test(test_ml_string_lines),
      cmocka_unit_test(test_ml_string_errors),
      // @main
      cmocka_unit_test(test_annot_main),
      cmocka_unit_test(test_annot_errors),
      // Komentáře
      cmocka_unit_test(test_line_comment),
      cmocka_unit_test(test_block_comment_inline),
      cmocka_unit_test(test_block_comment_nested),
      cmocka_unit_test(test_block_comment_unterminated),
      cmocka_unit_test(test_block_comment_multiline),
      cmocka_unit_test(test_comment_line_numbers),
      // Konce řádků
      cmocka_unit_test(test_line_numbers),
      cmocka_unit_test(test_empty_lines_skipped),
      cmocka_unit_test(test_eol_added_at_eof),
      cmocka_unit_test(test_newline_in_parens),
      cmocka_unit_test(test_extra_closing_paren),
      // Odsazení
      cmocka_unit_test(test_tab_inside_line),
      cmocka_unit_test(test_tab_in_indent),
      cmocka_unit_test(test_indent),
      cmocka_unit_test(test_dedent),
      cmocka_unit_test(test_bad_dedent),
      cmocka_unit_test(test_dedent_at_eof),
      cmocka_unit_test(test_indent_ignores_empty_lines),
      cmocka_unit_test(test_indent_in_parens),
      cmocka_unit_test(test_deep_indent),
      // Celý program
      cmocka_unit_test(test_spec_program),
      cmocka_unit_test(test_reinit),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}