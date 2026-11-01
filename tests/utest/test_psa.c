/**
 * @file test_psa.c
 * @brief IFJ26 - Unit testy pro psa (cmocka)
 *
 * @author xhanzlp00 Petr Hanzlík
 *
 * Testy vygenerovala AI (GLM 5.3, přes opencode) podle tohoto zadání:
 *
 * "write test for psa.c. Test are written using cmocka and test are in
 * /tests/utest dir. File will be named test_psa.c. You must mock
 * get_next_token from scanner to separate the tests from the scanner
 * implementation. Call only funciton in psa.h. Specification of details is
 * in /zadani/Specifikace_IFJ26_a_IFJcode26.pdf. In the test check output ast
 * and verify that the is_folden flag is set correctly for constant folding
 * operations. Use assertions to validate the expected results of the
 * operations and ensure that operator precedence is respected in the
 * evaluation of expressions. Additionally, include tests for expression
 * evaluation, ensuring that the correct values are computed based on the
 * input tokens. To the header of the test file, include [tuto hlavičku
 * s autorem a tímto promptem včetně informace o modelu, který testy
 * vygeneroval]. The tests will cover the following tasks in psa.c:
 * - constant folding: operations is folded if both operands are constants
 *   and the result can be computed at compile time. The result is stored in
 *   the ast node. Binary operation (is_folded = true in ast result):
 *   addition, subtraction, multiplication, division (10 / 0 cant be folded,
 *   but 10 / 2 can), list concat, string concat. Unary operation (is_folded
 *   is set based of the operand's is_folden value): not, negation.
 * - operator precedence
 * - expression evaluation
 * - test extention: FUN EXP, BOOL THEN, CONTROL, VALMEM, LIST, PARAM LIST
 * - stress tests: large expressions, deeply nested expressions, edge cases
 *   for constant folding (e.g., division by zero, large numbers)."
 *
 * Doplněno navazujícím požadavkem: "continue and add negative test case and
 * check right error code" (negativní testy ověřují správné chybové kódy
 * IFJ_ESYNTAX / IFJ_EINTERNAL).
 *
 * Scanner je od testů oddělen mockem: volání scanner_next_token a
 * scanner_peek_next_token jsou při linkování testu nahrazena (GNU ld
 * --wrap) implementací nad statickou frontou tokenů (viz Makefile, cíl
 * utest_psa). Testy volají výhradně funkci psa_parse_expression z psa.h
 * podle smlouvy v její dokumentaci: parser.current_token je při volání
 * nastaven na první token výrazu a po návratu obsahuje token za výrazem.
 */

// strdup() je POSIX funkce, s -std=c11 ji je nutné zpřístupnit.
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "setup_test.h"

#include "psa.h"

// ---------------------------------------------------------------------------
// Mock scanneru (get_next_token)
// ---------------------------------------------------------------------------

/**
 * psa.c čte vstupní tokeny výhradně přes scanner_next_token a
 * scanner_peek_next_token. Aby testy nezávisely na implementaci scanneru,
 * při linkování testu nahrazují obě funkce GNU ld mockem (--wrap, viz
 * Makefile, cíl utest_psa). Mock vrací tokeny ze statické fronty, kterou si
 * test naplní pomocnými funkcemi feed_*.
 */

/** Maximální počet tokenů ve frontě mocku. */
#define MOCK_MAX_TOKENS 8192

/** Jeden token ve frontě mocku. */
typedef struct {
  token_type_t type; /**< Typ tokenu */
  union {
    long long int_value;  /**< Hodnota celočíselného literálu */
    double double_value;  /**< Hodnota desetinného literálu */
    const char *str_value; /**< Hodnota řetězce nebo identifikátoru */
    keyword_t keyword;    /**< Hodnota klíčového slova */
  } value;                 /**< Hodnota tokenu */
  int line;                /**< Číslo řádku */
} mock_token_t;

/** Fronta tokenů mocku. */
static mock_token_t mock_queue[MOCK_MAX_TOKENS];

/** Počet tokenů ve frontě. */
static int mock_count = 0;

/** Pozice dalšího tokenu vráceného funkcí __wrap_scanner_next_token. */
static int mock_pos = 0;

// Prototypy wrap funkcí (definovány níže)
IFJ_ERR __wrap_scanner_next_token(scanner_t *scanner, token_t *token);
IFJ_ERR __wrap_scanner_peek_next_token(scanner_t *scanner, token_t *token);

/**
 * @brief Vyprázdní frontu tokenů mocku.
 */
static void mock_reset(void) {
  mock_count = 0;
  mock_pos = 0;
}

/**
 * @brief Vrátí další volný slot ve frontě tokenů.
 * @return Ukazatel na slot ve frontě.
 */
static mock_token_t *mock_next_slot(void) {
  if (mock_count >= MOCK_MAX_TOKENS) {
    fail_msg("Mock token queue overflow (%d tokens)", MOCK_MAX_TOKENS);
  }
  return &mock_queue[mock_count++];
}

/**
 * @brief Přidá do fronty token bez hodnoty (operátor, závorka, EOF, ...).
 * @param type Typ tokenu.
 */
static void feed_op(token_type_t type) {
  mock_token_t *t = mock_next_slot();
  t->type = type;
  t->value.int_value = 0;
  t->line = 1;
}

/**
 * @brief Přidá do fronty celočíselný literál.
 * @param value Hodnota literálu.
 */
static void feed_int(long long value) {
  mock_token_t *t = mock_next_slot();
  t->type = TOK_INT;
  t->value.int_value = value;
  t->line = 1;
}

/**
 * @brief Přidá do fronty desetinný literál.
 * @param value Hodnota literálu.
 */
static void feed_dbl(double value) {
  mock_token_t *t = mock_next_slot();
  t->type = TOK_FLOAT;
  t->value.double_value = value;
  t->line = 1;
}

/**
 * @brief Přidá do fronty řetězcový literál.
 * @param value Hodnota literálu.
 */
static void feed_str(const char *value) {
  mock_token_t *t = mock_next_slot();
  t->type = TOK_STRING;
  t->value.str_value = value;
  t->line = 1;
}

/**
 * @brief Přidá do fronty identifikátor.
 * @param value Identifikátor.
 */
static void feed_id(const char *value) {
  mock_token_t *t = mock_next_slot();
  t->type = TOK_ID;
  t->value.str_value = value;
  t->line = 1;
}

/**
 * @brief Přidá do fronty klíčové slovo.
 * @param keyword Klíčové slovo.
 */
static void feed_kw(keyword_t keyword) {
  mock_token_t *t = mock_next_slot();
  t->type = TOK_KEYWORD;
  t->value.keyword = keyword;
  t->line = 1;
}

/**
 * @brief Přidá do fronty EOF - konec výrazu.
 */
static void feed_end(void) { feed_op(TOK_EOF); }

/**
 * @brief Vyplní token podle šablony z fronty mocku.
 * @param token Výstupní token.
 * @param t Šablona ve frontě mocku.
 * @param own_str True = řetězcová hodnota se zkopíruje (token patří
 * volajícímu), false = ukazatel se jen předá (náhled přes peek).
 */
static void mock_fill_token(token_t *token, const mock_token_t *t,
                            bool own_str) {
  token->type = t->type;
  token->line = t->line;
  token->newline_before = false;
  token->value.int_value = 0; // vynulování celé uniony

  switch (t->type) {
  case TOK_INT:
    token->value.int_value = t->value.int_value;
    break;
  case TOK_FLOAT:
    token->value.double_value = t->value.double_value;
    break;
  case TOK_KEYWORD:
    token->value.keyword = t->value.keyword;
    break;
  case TOK_ID:
  case TOK_STRING:
    if (own_str) {
      // Token převzatý přes next_token patří volajícímu a ten ho uvolňuje
      // přes token_free - mock tedy vrací vlastní kopii řetězce.
      token->value.str_value = strdup(t->value.str_value);
      assert_non_null(token->value.str_value);
    } else {
      // Token z peeku podle smlouvy scanneru volající nevlastní a
      // neuvolňuje, ukazatel do statické fronty je tedy bezpečný.
      token->value.str_value = (char *)t->value.str_value;
    }
    break;
  default:
    break;
  }
}

/**
 * @brief Mock scanner_next_token - vrátí další token z fronty.
 *
 * Všechny tokeny mají line = 1, aby detekce čárky na konci volání
 * (comma_line == řádek RPAREN) fungovala pro "f(1,)" a neposoudila
 * "f(1, 2)".
 */
IFJ_ERR __wrap_scanner_next_token(scanner_t *scanner, token_t *token) {
  const mock_token_t *t;

  (void)scanner; // Scanner se v mocku nepoužívá

  if (token == NULL) {
    return IFJ_EINTERNAL;
  }

  t = (mock_pos < mock_count) ? &mock_queue[mock_pos] : NULL;
  mock_pos++;
  if (t == NULL) {
    // Obranná větev - vyčerpaná fronta se chová jako konec vstupu
    token->type = TOK_EOF;
    token->line = 1;
    token->newline_before = false;
    token->value.int_value = 0;
    return IFJ_SUCCESS;
  }

  mock_fill_token(token, t, true);
  return IFJ_SUCCESS;
}

/**
 * @brief Mock scanner_peek_next_token - vrátí další token z fronty bez
 * posunu pozice.
 */
IFJ_ERR __wrap_scanner_peek_next_token(scanner_t *scanner, token_t *token) {
  const mock_token_t *t;

  (void)scanner; // Scanner se v mocku nepoužívá

  if (token == NULL) {
    return IFJ_EINTERNAL;
  }

  t = (mock_pos < mock_count) ? &mock_queue[mock_pos] : NULL;
  if (t == NULL) {
    token->type = TOK_EOF;
    token->line = 1;
    token->newline_before = false;
    token->value.int_value = 0;
    return IFJ_SUCCESS;
  }

  mock_fill_token(token, t, false);
  return IFJ_SUCCESS;
}

// ---------------------------------------------------------------------------
// Příprava parseru a spuštění analýzy
// ---------------------------------------------------------------------------

/** Parser používaný testy (scanner se v mocku nepoužívá, může být NULL). */
static parser_t test_parser;

/**
 * @brief Nastaví parser na začátek výrazu a spustí psa_parse_expression.
 *
 * Podle smlouvy z psa.h musí být parser.current_token nastaven na první
 * token výrazu - mock ho převezme z fronty jako první.
 * @param out_ast Výstupní AST uzel.
 * @return Návratová hodnota psa_parse_expression.
 */
static IFJ_ERR run_parse(ast_node_t **out_ast) {
  IFJ_ERR err;

  memset(&test_parser, 0, sizeof(test_parser));
  err = __wrap_scanner_next_token(NULL, &test_parser.current_token);
  if (err != IFJ_SUCCESS) {
    return err;
  }

  return psa_parse_expression(&test_parser, out_ast);
}

/**
 * @brief Spustí analýzu, ověří úspěch a vrátí kořen výrazu.
 * @return Kořen výrazu.
 */
static ast_node_t *parse_ok(void) {
  ast_node_t *ast = NULL;

  assert_int_equal(run_parse(&ast), IFJ_SUCCESS);
  assert_non_null(ast);
  return ast;
}

/**
 * @brief Uvolní kořen výrazu a ukončovací token parseru po testu.
 * @param ast Kořen výrazu (může být NULL).
 */
static void parse_done(ast_node_t *ast) {
  ast_free(ast);
  token_free(&test_parser.current_token);
}

// ---------------------------------------------------------------------------
// Pomocné asserty nad AST
// ---------------------------------------------------------------------------

/**
 * @brief Ověří, že uzel je celočíselný literál s danou hodnotou a flagem
 * is_folded.
 * @param node Testovaný uzel.
 * @param value Očekávaná hodnota.
 * @param is_folded Očekávaná hodnota flagu is_folded.
 */
static void assert_literal_int(const ast_node_t *node, int64_t value,
                               bool is_folded) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_LITERAL);
  assert_int_equal(node->as.literal.type, TYPE_INT);
  assert_int_equal(node->as.literal.is_folded, is_folded);
  assert_int_equal(node->as.literal.int_value, value);
}

/**
 * @brief Ověří, že uzel je desetinný literál s danou hodnotou a flagem
 * is_folded.
 * @param node Testovaný uzel.
 * @param value Očekávaná hodnota.
 * @param is_folded Očekávaná hodnota flagu is_folded.
 */
static void assert_literal_double(const ast_node_t *node, double value,
                                  bool is_folded) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_LITERAL);
  assert_int_equal(node->as.literal.type, TYPE_DOUBLE);
  assert_int_equal(node->as.literal.is_folded, is_folded);
  assert_true(fabs(node->as.literal.double_value - value) < 1e-9);
}

/**
 * @brief Ověří, že uzel je řetězcový literál s danou hodnotou a flagem
 * is_folded.
 * @param node Testovaný uzel.
 * @param value Očekávaná hodnota.
 * @param is_folded Očekávaná hodnota flagu is_folded.
 */
static void assert_literal_string(const ast_node_t *node, const char *value,
                                  bool is_folded) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_LITERAL);
  assert_int_equal(node->as.literal.type, TYPE_STRING);
  assert_int_equal(node->as.literal.is_folded, is_folded);
  assert_string_equal(node->as.literal.str_value, value);
}

/**
 * @brief Ověří, že uzel je booleovský literál s danou hodnotou a flagem
 * is_folded.
 * @param node Testovaný uzel.
 * @param value Očekávaná hodnota.
 * @param is_folded Očekávaná hodnota flagu is_folded.
 */
static void assert_literal_bool(const ast_node_t *node, bool value,
                                bool is_folded) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_LITERAL);
  assert_int_equal(node->as.literal.type, TYPE_BOOL);
  assert_int_equal(node->as.literal.is_folded, is_folded);
  assert_int_equal(node->as.literal.bool_value, value);
}

/**
 * @brief Ověří, že uzel je literál typu unit "()" bez složení.
 * @param node Testovaný uzel.
 */
static void assert_literal_unit(const ast_node_t *node) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_LITERAL);
  assert_int_equal(node->as.literal.type, TYPE_UNIT);
  assert_int_equal(node->as.literal.is_folded, false);
}

/**
 * @brief Ověří, že uzel je proměnná s daným identifikátorem.
 * @param node Testovaný uzel.
 * @param id Očekávaný identifikátor.
 */
static void assert_var(const ast_node_t *node, const char *id) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_VAR);
  assert_string_equal(node->as.var.id, id);
}

/**
 * @brief Ověří, že uzel je binární výraz s daným operátorem.
 * @param node Testovaný uzel.
 * @param op Očekávaný operátor.
 * @param lhs Výstup: levý operand (může být NULL).
 * @param rhs Výstup: pravý operand (může být NULL).
 */
static void assert_binary(const ast_node_t *node, token_type_t op,
                          const ast_node_t **lhs, const ast_node_t **rhs) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_EXPR);
  assert_int_equal(node->as.expr.is_unary, false);
  assert_int_equal(node->as.expr.operator_type, op);
  assert_non_null(node->as.expr.binary.lhs);
  assert_non_null(node->as.expr.binary.rhs);
  if (lhs != NULL) {
    *lhs = node->as.expr.binary.lhs;
  }
  if (rhs != NULL) {
    *rhs = node->as.expr.binary.rhs;
  }
}

/**
 * @brief Ověří, že uzel je unární výraz s daným operátorem.
 * @param node Testovaný uzel.
 * @param op Očekávaný operátor.
 * @param operand Výstup: operand (může být NULL).
 */
static void assert_unary(const ast_node_t *node, token_type_t op,
                         const ast_node_t **operand) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_EXPR);
  assert_int_equal(node->as.expr.is_unary, true);
  assert_int_equal(node->as.expr.operator_type, op);
  assert_non_null(node->as.expr.unary.operand);
  if (operand != NULL) {
    *operand = node->as.expr.unary.operand;
  }
}

/**
 * @brief Ověří, že uzel je volání funkce s daným identifikátorem a počtem
 * seznamů argumentů.
 * @param node Testovaný uzel.
 * @param id Očekávaný identifikátor funkce.
 * @param param_lists_cnt Očekávaný počet seznamů argumentů.
 */
static void assert_func_call(const ast_node_t *node, const char *id,
                             size_t param_lists_cnt) {
  assert_non_null(node);
  assert_int_equal(node->type, AST_FUNC_CALL);
  assert_string_equal(node->as.func_call.id, id);
  assert_int_equal(node->as.func_call.param_lists_cnt, param_lists_cnt);
  for (size_t i = 0; i < param_lists_cnt; i++) {
    assert_non_null(node->as.func_call.param_lists[i]);
  }
}

/**
 * @brief Vrátí zadaný argument volání funkce.
 * @param call Uzel volání funkce.
 * @param list Index seznamu argumentů.
 * @param index Index argumentu v seznamu.
 * @return Argument volání.
 */
static const ast_node_t *call_arg(const ast_node_t *call, size_t list,
                                  size_t index) {
  const ast_node_t *param_list;

  assert_true(call->type == AST_FUNC_CALL);
  assert_true(list < call->as.func_call.param_lists_cnt);
  param_list = call->as.func_call.param_lists[list];
  assert_int_equal(param_list->type, AST_PARAM_LIST);
  assert_true(index < param_list->as.param_list.params_cnt);
  return param_list->as.param_list.params[index];
}

/**
 * @brief Vrátí počet argumentů v zadaném seznamu argumentů volání.
 * @param call Uzel volání funkce.
 * @param list Index seznamu argumentů.
 * @return Počet argumentů.
 */
static size_t call_arg_cnt(const ast_node_t *call, size_t list) {
  assert_true(call->type == AST_FUNC_CALL);
  assert_true(list < call->as.func_call.param_lists_cnt);
  return call->as.func_call.param_lists[list]->as.param_list.params_cnt;
}

/**
 * @brief Ověří typ aktuálního (ukončovacího) tokenu parseru.
 * @param type Očekávaný typ tokenu.
 */
static void assert_current_token(token_type_t type) {
  assert_int_equal(test_parser.current_token.type, type);
}

// ---------------------------------------------------------------------------
// Průchod AST pro zátěžové testy
// ---------------------------------------------------------------------------

/**
 * @brief Spočítá uzly daného typu v celém podstromu.
 * @param node Kořen podstromu.
 * @param type Hledaný typ uzlu.
 * @return Počet uzlu daného typu.
 */
static intmax_t count_nodes(const ast_node_t *node, ast_node_type_t type) {
  intmax_t count = 0;

  if (node == NULL) {
    return 0;
  }

  if (node->type == type) {
    count++;
  }

  switch (node->type) {
  case AST_EXPR:
    if (node->as.expr.is_unary) {
      count += count_nodes(node->as.expr.unary.operand, type);
    } else {
      count += count_nodes(node->as.expr.binary.lhs, type);
      count += count_nodes(node->as.expr.binary.rhs, type);
    }
    break;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.func_call.param_lists_cnt; i++) {
      const ast_node_t *param_list = node->as.func_call.param_lists[i];
      for (size_t j = 0; j < param_list->as.param_list.params_cnt; j++) {
        count += count_nodes(param_list->as.param_list.params[j], type);
      }
    }
    break;

  default:
    break;
  }

  return count;
}

/**
 * @brief Zjistí hloubku zanoření volání funkcí, např. f(f(1)) má hloubku 2.
 * @param node Kořen podstromu.
 * @return Hloubka zanoření volání.
 */
static intmax_t call_nesting_depth(const ast_node_t *node) {
  intmax_t best = 0;

  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.func_call.param_lists_cnt; i++) {
      const ast_node_t *param_list = node->as.func_call.param_lists[i];
      for (size_t j = 0; j < param_list->as.param_list.params_cnt; j++) {
        intmax_t depth =
            call_nesting_depth(param_list->as.param_list.params[j]);
        if (depth > best) {
          best = depth;
        }
      }
    }
    return best + 1;

  case AST_EXPR:
    if (node->as.expr.is_unary) {
      return call_nesting_depth(node->as.expr.unary.operand);
    }
    best = call_nesting_depth(node->as.expr.binary.lhs);
    intmax_t right = call_nesting_depth(node->as.expr.binary.rhs);
    if (right > best) {
      best = right;
    }
    return best;

  default:
    return 0;
  }
}

// ---------------------------------------------------------------------------
// Konstantní skládání - binární operace
// ---------------------------------------------------------------------------

/**
 * @brief Složení součtu dvou celočíselných literálů: 10 + 20 = 30.
 */
static void test_fold_binary_add_int(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(10);
  feed_op(TOK_ADD);
  feed_int(20);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 30, true);
  parse_done(ast);
}

/**
 * @brief Složení součtu dvou desetinných literálů: 1.5 + 2.25 = 3.75.
 */
static void test_fold_binary_add_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_dbl(1.5);
  feed_op(TOK_ADD);
  feed_dbl(2.25);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 3.75, true);
  parse_done(ast);
}

/**
 * @brief Složení smíšeného součtu Int + Double: 1 + 2.5 = 3.5.
 */
static void test_fold_binary_add_mixed_int_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(1);
  feed_op(TOK_ADD);
  feed_dbl(2.5);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 3.5, true);
  parse_done(ast);
}

/**
 * @brief Složení rozdílu celočíselných literálů: 10 - 4 = 6.
 */
static void test_fold_binary_sub_int(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(10);
  feed_op(TOK_SUB);
  feed_int(4);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 6, true);
  parse_done(ast);
}

/**
 * @brief Složení rozdílu desetinných literálů: 5.5 - 0.25 = 5.25.
 */
static void test_fold_binary_sub_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_dbl(5.5);
  feed_op(TOK_SUB);
  feed_dbl(0.25);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 5.25, true);
  parse_done(ast);
}

/**
 * @brief Složení součinu celočíselných literálů: 6 * 7 = 42.
 */
static void test_fold_binary_mul_int(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(6);
  feed_op(TOK_MUL);
  feed_int(7);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 42, true);
  parse_done(ast);
}

/**
 * @brief Složení součinu desetinných literálů: 1.5 * 4.0 = 6.0.
 */
static void test_fold_binary_mul_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_dbl(1.5);
  feed_op(TOK_MUL);
  feed_dbl(4.0);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 6.0, true);
  parse_done(ast);
}

/**
 * @brief Složení celočíselného dělení: 10 / 2 = 5 (delení bez zbytku).
 */
static void test_fold_binary_div_int(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(10);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 5, true);
  parse_done(ast);
}

/**
 * @brief Dělení nulou se neskládá: 10 / 0 zůstane nesložený výraz.
 *
 * Dělení nulou nesmí hlásit PSA - chybu odhalí sémantická analýza.
 */
static void test_no_fold_div_by_zero_int(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_int(10);
  feed_op(TOK_DIV);
  feed_int(0);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_DIV, &lhs, &rhs);
  assert_literal_int(lhs, 10, false);
  assert_literal_int(rhs, 0, false);
  parse_done(ast);
}

/**
 * @brief Desetinné dělení nulou se též neskládá: 10.0 / 0.0.
 */
static void test_no_fold_div_by_zero_double(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_dbl(10.0);
  feed_op(TOK_DIV);
  feed_dbl(0.0);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_DIV, &lhs, &rhs);
  assert_literal_double(lhs, 10.0, false);
  assert_literal_double(rhs, 0.0, false);
  parse_done(ast);
}

/**
 * @brief Dělení nulou uvnitř součtu: 1 + 2 + 10 / 0.
 *
 * Část 1 + 2 se složí na literál 3 (is_folded = true), celý součet se ale
 * kvůli dělení nulou nesloží a zůstane binární výraz.
 */
static void test_no_fold_div_by_zero_in_sum(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *div_lhs, *div_rhs;

  feed_int(1);
  feed_op(TOK_ADD);
  feed_int(2);
  feed_op(TOK_ADD);
  feed_int(10);
  feed_op(TOK_DIV);
  feed_int(0);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_literal_int(lhs, 3, true); // 1 + 2 je složené
  assert_binary(rhs, TOK_DIV, &div_lhs, &div_rhs);
  assert_literal_int(div_lhs, 10, false);
  assert_literal_int(div_rhs, 0, false);
  parse_done(ast);
}

/**
 * @brief Celé dělení usekává k nule: -7 / 2 = -3 a 7 / -2 = -3.
 */
static void test_fold_binary_div_truncates_toward_zero(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_int(7);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, -3, true);
  parse_done(ast);

  mock_reset();
  feed_int(7);
  feed_op(TOK_DIV);
  feed_op(TOK_SUB);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, -3, true);
  parse_done(ast);
}

/**
 * @brief Složení desetinného dělení: 7.5 / 2.5 = 3.0.
 */
static void test_fold_binary_div_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_dbl(7.5);
  feed_op(TOK_DIV);
  feed_dbl(2.5);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 3.0, true);
  parse_done(ast);
}

/**
 * @brief Složení zřetězení řetězcových literálů: "abc" + "def" = "abcdef".
 */
static void test_fold_binary_string_concat(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_str("abc");
  feed_op(TOK_ADD);
  feed_str("def");
  feed_end();

  ast = parse_ok();
  assert_literal_string(ast, "abcdef", true);
  parse_done(ast);
}

/**
 * @brief Složení zřetězení seznamů: List(1, 2) ++ List(3) = List(1, 2, 3).
 */
static void test_fold_binary_list_concat(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_COMMA);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_op(TOK_CONCAT);
  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_func_call(ast, "List", 1);
  assert_int_equal(call_arg_cnt(ast, 0), 3);
  assert_literal_int(call_arg(ast, 0, 0), 1, false);
  assert_literal_int(call_arg(ast, 0, 1), 2, false);
  assert_literal_int(call_arg(ast, 0, 2), 3, false);
  parse_done(ast);
}

/**
 * @brief Zřetězení tří seznamů: List(1) ++ List(2) ++ List(3) = List(1, 2,
 * 3).
 */
static void test_fold_binary_list_concat_chain(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_op(TOK_CONCAT);
  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_op(TOK_CONCAT);
  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_func_call(ast, "List", 1);
  assert_int_equal(call_arg_cnt(ast, 0), 3);
  assert_literal_int(call_arg(ast, 0, 0), 1, false);
  assert_literal_int(call_arg(ast, 0, 1), 2, false);
  assert_literal_int(call_arg(ast, 0, 2), 3, false);
  parse_done(ast);
}

/**
 * @brief Zřetězení seznamu v proměnné se neskládá: lst ++ List(1).
 */
static void test_no_fold_list_concat_with_var(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_id("lst");
  feed_op(TOK_CONCAT);
  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_CONCAT, &lhs, &rhs);
  assert_var(lhs, "lst");
  assert_func_call(rhs, "List", 1);
  parse_done(ast);
}

/**
 * @brief Výraz s proměnnou se neskládá: x + 1.
 */
static void test_no_fold_binary_with_var(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_id("x");
  feed_op(TOK_ADD);
  feed_int(1);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_var(lhs, "x");
  assert_literal_int(rhs, 1, false);
  parse_done(ast);
}

/**
 * @brief Typová kombinace Bool + Int se neskládá: true + 1.
 */
static void test_no_fold_binary_type_mismatch(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_kw(KW_TRUE);
  feed_op(TOK_ADD);
  feed_int(1);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_literal_bool(lhs, true, false);
  assert_literal_int(rhs, 1, false);
  parse_done(ast);
}

/**
 * @brief Složení relačních operátorů: 2 < 3 je true, 2 >= 3 je false.
 */
static void test_fold_binary_relational(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(2);
  feed_op(TOK_LT);
  feed_int(3);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);

  mock_reset();
  feed_int(2);
  feed_op(TOK_GEQ);
  feed_int(3);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);
}

/**
 * @brief Složení operátorů rovnosti: 1 == 2 je false, 1 != 2 je true.
 */
static void test_fold_binary_equality(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(1);
  feed_op(TOK_EQ);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);

  mock_reset();
  feed_int(1);
  feed_op(TOK_NEQ);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);
}

/**
 * @brief Složení logických operátorů nad Bool literály.
 */
static void test_fold_binary_bool_ops(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_kw(KW_TRUE);
  feed_op(TOK_AND);
  feed_kw(KW_FALSE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);

  mock_reset();
  feed_kw(KW_TRUE);
  feed_op(TOK_OR);
  feed_kw(KW_FALSE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);

  mock_reset();
  feed_kw(KW_TRUE);
  feed_op(TOK_EQ);
  feed_kw(KW_FALSE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);

  mock_reset();
  feed_kw(KW_TRUE);
  feed_op(TOK_NEQ);
  feed_kw(KW_FALSE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);
}

/**
 * @brief Porovnávání řetězců se neskládá: "a" < "b" (nechá se na sémantice).
 */
static void test_no_fold_string_relational(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs;

  feed_str("a");
  feed_op(TOK_LT);
  feed_str("b");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_LT, &lhs, &rhs);
  assert_literal_string(lhs, "a", false);
  assert_literal_string(rhs, "b", false);
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Konstantní skládání - unární operace
// ---------------------------------------------------------------------------

/**
 * @brief Unární minus nad přímým literálem: -5.
 *
 * Výsledek je literál -5, ale is_folded se bere z operandu - přímý literál
 * není složený, tedy is_folded = false.
 */
static void test_fold_unary_neg_int(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, -5, false);
  parse_done(ast);
}

/**
 * @brief Unární minus nad desetinným literálem: -2.5 (is_folded = false).
 */
static void test_fold_unary_neg_double(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_dbl(2.5);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, -2.5, false);
  parse_done(ast);
}

/**
 * @brief Negace nad přímým Bool literálem: !true (is_folded = false).
 */
static void test_fold_unary_not_bool(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_NOT);
  feed_kw(KW_TRUE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, false);
  parse_done(ast);
}

/**
 * @brief Unární minus nad složeným operandem: -(2 + 3) = -5.
 *
 * Operand 2 + 3 je složený (is_folded = true), výsledek tedy přebírá
 * is_folded = true.
 */
static void test_fold_unary_neg_of_folded_operand(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_ADD);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, -5, true);
  parse_done(ast);
}

/**
 * @brief Negace nad složeným operandem: !(1 < 2) = false.
 *
 * Operand 1 < 2 je složený Bool literál, výsledek přebírá is_folded = true.
 */
static void test_fold_unary_not_of_folded_operand(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_NOT);
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_LT);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);
}

/**
 * @brief Unární minus nad proměnnou se neskládá: -x.
 */
static void test_unary_neg_var_not_folded(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *operand;

  feed_op(TOK_SUB);
  feed_id("x");
  feed_end();

  ast = parse_ok();
  assert_unary(ast, TOK_SUB, &operand);
  assert_var(operand, "x");
  parse_done(ast);
}

/**
 * @brief Negace nad proměnnou se neskládá: !x.
 */
static void test_unary_not_var_not_folded(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *operand;

  feed_op(TOK_NOT);
  feed_id("x");
  feed_end();

  ast = parse_ok();
  assert_unary(ast, TOK_NOT, &operand);
  assert_var(operand, "x");
  parse_done(ast);
}

/**
 * @brief Unární minus nad řetězcem se neskládá: -"abc".
 */
static void test_unary_neg_string_not_folded(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *operand;

  feed_op(TOK_SUB);
  feed_str("abc");
  feed_end();

  ast = parse_ok();
  assert_unary(ast, TOK_SUB, &operand);
  assert_literal_string(operand, "abc", false);
  parse_done(ast);
}

/**
 * @brief Dvojitá negace: - - 5 = 5 (is_folded = false, přímý literál).
 */
static void test_unary_double_negation(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_op(TOK_SUB);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 5, false);
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Precedence operátorů
// ---------------------------------------------------------------------------

/**
 * @brief Násobení má přednost před sčítáním: a + b * c = a + (b * c).
 */
static void test_precedence_mul_over_add(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *mul_lhs, *mul_rhs;

  feed_id("a");
  feed_op(TOK_ADD);
  feed_id("b");
  feed_op(TOK_MUL);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_var(lhs, "a");
  assert_binary(rhs, TOK_MUL, &mul_lhs, &mul_rhs);
  assert_var(mul_lhs, "b");
  assert_var(mul_rhs, "c");
  parse_done(ast);
}

/**
 * @brief Závorky mění precedenci: (a + b) * c.
 */
static void test_precedence_parens_override(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *add_lhs, *add_rhs;

  feed_op(TOK_LPAREN);
  feed_id("a");
  feed_op(TOK_ADD);
  feed_id("b");
  feed_op(TOK_RPAREN);
  feed_op(TOK_MUL);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_MUL, &lhs, &rhs);
  assert_binary(lhs, TOK_ADD, &add_lhs, &add_rhs);
  assert_var(add_lhs, "a");
  assert_var(add_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

/**
 * @brief Sčítání a odčítání jsou levě asociativní: a - b + c = (a - b) + c.
 */
static void test_precedence_add_sub_left_assoc(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *sub_lhs, *sub_rhs;

  feed_id("a");
  feed_op(TOK_SUB);
  feed_id("b");
  feed_op(TOK_ADD);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_binary(lhs, TOK_SUB, &sub_lhs, &sub_rhs);
  assert_var(sub_lhs, "a");
  assert_var(sub_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

/**
 * @brief Relační operátory mají přednost před rovností: a < b == c =
 * (a < b) == c.
 */
static void test_precedence_rel_over_eq(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *lt_lhs, *lt_rhs;

  feed_id("a");
  feed_op(TOK_LT);
  feed_id("b");
  feed_op(TOK_EQ);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_EQ, &lhs, &rhs);
  assert_binary(lhs, TOK_LT, &lt_lhs, &lt_rhs);
  assert_var(lt_lhs, "a");
  assert_var(lt_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

/**
 * @brief Konjunkce má přednost před disjunkcí: a || b && c = a || (b && c).
 */
static void test_precedence_and_over_or(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *and_lhs, *and_rhs;

  feed_id("a");
  feed_op(TOK_OR);
  feed_id("b");
  feed_op(TOK_AND);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_OR, &lhs, &rhs);
  assert_var(lhs, "a");
  assert_binary(rhs, TOK_AND, &and_lhs, &and_rhs);
  assert_var(and_lhs, "b");
  assert_var(and_rhs, "c");
  parse_done(ast);
}

/**
 * @brief Levá asociativita && před ||: a && b || c = (a && b) || c.
 */
static void test_precedence_and_left_assoc_or(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *and_lhs, *and_rhs;

  feed_id("a");
  feed_op(TOK_AND);
  feed_id("b");
  feed_op(TOK_OR);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_OR, &lhs, &rhs);
  assert_binary(lhs, TOK_AND, &and_lhs, &and_rhs);
  assert_var(and_lhs, "a");
  assert_var(and_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

/**
 * @brief Unární negace má přednost před &&: !a && b = (!a) && b.
 */
static void test_precedence_not_over_and(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *operand;

  feed_op(TOK_NOT);
  feed_id("a");
  feed_op(TOK_AND);
  feed_id("b");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_AND, &lhs, &rhs);
  assert_unary(lhs, TOK_NOT, &operand);
  assert_var(operand, "a");
  assert_var(rhs, "b");
  parse_done(ast);
}

/**
 * @brief Unární minus má přednost před násobením: -a * b = (-a) * b.
 */
static void test_precedence_unary_minus_over_mul(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *operand;

  feed_op(TOK_SUB);
  feed_id("a");
  feed_op(TOK_MUL);
  feed_id("b");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_MUL, &lhs, &rhs);
  assert_unary(lhs, TOK_SUB, &operand);
  assert_var(operand, "a");
  assert_var(rhs, "b");
  parse_done(ast);
}

/**
 * @brief Tečka má přednost před násobením: a.b * c = (a.b) * c.
 */
static void test_precedence_dot_over_mul(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *dot_lhs, *dot_rhs;

  feed_id("a");
  feed_op(TOK_DOT);
  feed_id("b");
  feed_op(TOK_MUL);
  feed_id("c");
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_MUL, &lhs, &rhs);
  assert_binary(lhs, TOK_DOT, &dot_lhs, &dot_rhs);
  assert_var(dot_lhs, "a");
  assert_var(dot_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

/**
 * @brief Rozsah má nejnižší precedenci: (a + b to c) = (a + b) to c.
 */
static void test_precedence_range_below_add(void **state) {
  (void)state;
  ast_node_t *ast;
  const ast_node_t *lhs, *rhs, *add_lhs, *add_rhs;

  feed_op(TOK_LPAREN);
  feed_id("a");
  feed_op(TOK_ADD);
  feed_id("b");
  feed_id("to");
  feed_id("c");
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_binary(ast, TOK_TO, &lhs, &rhs);
  assert_binary(lhs, TOK_ADD, &add_lhs, &add_rhs);
  assert_var(add_lhs, "a");
  assert_var(add_rhs, "b");
  assert_var(rhs, "c");
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Vyhodnocení výrazů (vypočtené hodnoty)
// ---------------------------------------------------------------------------

/**
 * @brief Hodnota se správnou precedencí: 2 + 3 * 4 = 14.
 */
static void test_eval_precedence_value(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(2);
  feed_op(TOK_ADD);
  feed_int(3);
  feed_op(TOK_MUL);
  feed_int(4);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 14, true);
  parse_done(ast);
}

/**
 * @brief Hodnota se závorkami: (2 + 3) * 4 = 20.
 */
static void test_eval_parens_value(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_ADD);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_op(TOK_MUL);
  feed_int(4);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 20, true);
  parse_done(ast);
}

/**
 * @brief Levá asociativita odčítání: 100 - 10 - 5 = 85.
 */
static void test_eval_left_assoc_value(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(100);
  feed_op(TOK_SUB);
  feed_int(10);
  feed_op(TOK_SUB);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 85, true);
  parse_done(ast);
}

/**
 * @brief Smíšená aritmetika: 1 + 2 * 3 - 8 / 2 = 1 + 6 - 4 = 3.
 */
static void test_eval_mixed_arith(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(1);
  feed_op(TOK_ADD);
  feed_int(2);
  feed_op(TOK_MUL);
  feed_int(3);
  feed_op(TOK_SUB);
  feed_int(8);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 3, true);
  parse_done(ast);
}

/**
 * @brief Celočíselné dělení se usekává k nule: 7 / 2 = 3, 9 / 2 = 4,
 * 0 / 5 = 0.
 */
static void test_eval_int_division(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(7);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 3, true);
  parse_done(ast);

  mock_reset();
  feed_int(9);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 4, true);
  parse_done(ast);

  mock_reset();
  feed_int(0);
  feed_op(TOK_DIV);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, 0, true);
  parse_done(ast);
}

/**
 * @brief Desetinné dělení: 7.0 / 2 = 3.5 a 7 / 2.0 = 3.5.
 */
static void test_eval_double_division(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_dbl(7.0);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 3.5, true);
  parse_done(ast);

  mock_reset();
  feed_int(7);
  feed_op(TOK_DIV);
  feed_dbl(2.0);
  feed_end();

  ast = parse_ok();
  assert_literal_double(ast, 3.5, true);
  parse_done(ast);
}

/**
 * @brief Vyhodnocení porovnání: 2 < 3, 3 <= 3, 4 > 5, 5 >= 5.
 */
static void test_eval_comparisons(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_int(2);
  feed_op(TOK_LT);
  feed_int(3);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);

  mock_reset();
  feed_int(3);
  feed_op(TOK_LEQ);
  feed_int(3);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);

  mock_reset();
  feed_int(4);
  feed_op(TOK_GT);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);

  mock_reset();
  feed_int(5);
  feed_op(TOK_GEQ);
  feed_int(5);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);
}

/**
 * @brief Vyhodnocení logických výrazů: true && !false, (1 < 2) && (2 < 1),
 * false || true.
 */
static void test_eval_bool_expressions(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_kw(KW_TRUE);
  feed_op(TOK_AND);
  feed_op(TOK_NOT);
  feed_kw(KW_FALSE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);

  mock_reset();
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_LT);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_op(TOK_AND);
  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_LT);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, false, true);
  parse_done(ast);

  mock_reset();
  feed_kw(KW_FALSE);
  feed_op(TOK_OR);
  feed_kw(KW_TRUE);
  feed_end();

  ast = parse_ok();
  assert_literal_bool(ast, true, true);
  parse_done(ast);
}

/**
 * @brief Vyhodnocení zřetězení řetězců: "foo" + "bar" = "foobar".
 */
static void test_eval_string_concatenation(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_str("foo");
  feed_op(TOK_ADD);
  feed_str("bar");
  feed_end();

  ast = parse_ok();
  assert_literal_string(ast, "foobar", true);
  parse_done(ast);
}

/**
 * @brief Vyhodnocení unárního minu nad složeným operandem: -(3 * 4) = -12.
 */
static void test_eval_unary_minus_value(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_SUB);
  feed_op(TOK_LPAREN);
  feed_int(3);
  feed_op(TOK_MUL);
  feed_int(4);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_literal_int(ast, -12, true);
  parse_done(ast);
}

/**
 * @brief Prázdné závorky jsou literál unit: ().
 */
static void test_eval_unit_literal(void **state) {
  (void)state;
  ast_node_t *ast;

  feed_op(TOK_LPAREN);
  feed_op(TOK_RPAREN);
  feed_end();

  ast = parse_ok();
  assert_literal_unit(ast);
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Rozšíření: FUN EXP, BOOL THEN, CONTROL, VALMEM, LIST, PARAM LIST
// ---------------------------------------------------------------------------

/** FUN EXP - volání funkce v aritmetickém výrazu (2 * factorial(...)) */
static void test_ext_funexp_function_in_expression(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_int(2);
  feed_op(TOK_MUL);
  feed_id("factorial");
  feed_op(TOK_LPAREN);
  feed_id("readInt");
  feed_op(TOK_LPAREN);
  feed_op(TOK_RPAREN);
  feed_op(TOK_ADD);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_MUL, &lhs, &rhs);
  assert_literal_int(lhs, 2, false);
  // 2 * <call> se nesloží - pravý operand je volání funkce
  assert_func_call(rhs, "factorial", 1);
  assert_int_equal((intmax_t)call_arg_cnt(rhs, 0), (intmax_t)1);
  const ast_node_t *arg = call_arg(rhs, 0, 0);
  assert_binary(arg, TOK_ADD, &lhs, &rhs);
  assert_func_call(lhs, "readInt", 1);
  assert_int_equal((intmax_t)call_arg_cnt(lhs, 0), (intmax_t)0);
  assert_literal_int(rhs, 1, false);
  parse_done(ast);
}

/** FUN EXP - volání funkce s argumentem, který je sám voláním funkce */
static void test_ext_funexp_nested_calls(void **state) {
  (void)state;

  feed_id("foo");
  feed_op(TOK_LPAREN);
  feed_id("bar");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "foo", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)1);
  const ast_node_t *arg = call_arg(ast, 0, 0);
  assert_func_call(arg, "bar", 1);
  assert_literal_int(call_arg(arg, 0, 0), 1, false);
  parse_done(ast);
}

/** BOOL THEN - podmínka končí klíčovým slovem then (parser ji dál zpracuje) */
static void test_ext_boolthen_ends_with_then(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_id("x");
  feed_op(TOK_LT);
  feed_int(0);
  feed_kw(KW_THEN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_LT, &lhs, &rhs);
  assert_var(lhs, "x");
  assert_literal_int(rhs, 0, false);
  assert_current_token(TOK_KEYWORD);
  assert_int_equal((intmax_t)test_parser.current_token.value.keyword,
                   (intmax_t)KW_THEN);
  parse_done(ast);
}

/** BOOL THEN - složenější booleovská podmínka končící na then */
static void test_ext_boolthen_bool_condition(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;
  const ast_node_t *operand = NULL;

  feed_id("ready");
  feed_op(TOK_AND);
  feed_op(TOK_NOT);
  feed_id("stopped");
  feed_kw(KW_THEN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_AND, &lhs, &rhs);
  assert_var(lhs, "ready");
  // !stopped - unární negace má vyšší precedenci než &&
  assert_unary(rhs, TOK_NOT, &operand);
  assert_var(operand, "stopped");
  assert_current_token(TOK_KEYWORD);
  assert_int_equal((intmax_t)test_parser.current_token.value.keyword,
                   (intmax_t)KW_THEN);
  parse_done(ast);
}

/** CONTROL - uzavřený rozsah `to` v závorce je binární operátor */
static void test_ext_control_range_to(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_id("to");
  feed_int(10);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_TO, &lhs, &rhs);
  assert_literal_int(lhs, 1, false);
  assert_literal_int(rhs, 10, false);
  parse_done(ast);
}

/** CONTROL - otevřený rozsah `until` v závorce */
static void test_ext_control_range_until(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_op(TOK_LPAREN);
  feed_int(0);
  feed_id("until");
  feed_id("n");
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_UNTIL, &lhs, &rhs);
  assert_literal_int(lhs, 0, false);
  assert_var(rhs, "n");
  parse_done(ast);
}

/** CONTROL - `to` na nejvyšší úrovni výraz neskončí a NEkonsumuje se */
static void test_ext_control_to_not_consumed(void **state) {
  (void)state;

  feed_int(1);
  feed_id("to");
  feed_int(10);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_literal_int(ast, 1, false);
  assert_current_token(TOK_ID);
  assert_string_equal(test_parser.current_token.value.str_value, "to");
  parse_done(ast);
}

/** CONTROL - podmínka while cyklu končí klíčovým slovem do */
static void test_ext_control_condition_ends_with_do(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_id("i");
  feed_op(TOK_LT);
  feed_int(10);
  feed_kw(KW_DO);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_LT, &lhs, &rhs);
  assert_var(lhs, "i");
  assert_literal_int(rhs, 10, false);
  assert_current_token(TOK_KEYWORD);
  assert_int_equal((intmax_t)test_parser.current_token.value.keyword,
                   (intmax_t)KW_DO);
  parse_done(ast);
}

/** VALMEM - přístup k členu .length na řetězcovém literálu */
static void test_ext_valmem_string_length(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_str("abc");
  feed_op(TOK_DOT);
  feed_id("length");
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_DOT, &lhs, &rhs);
  assert_literal_string(lhs, "abc", false);
  assert_var(rhs, "length");
  parse_done(ast);
}

/** VALMEM - zřetězený přístup k členům (a.b.c je asociativní zleva) */
static void test_ext_valmem_chained_members(void **state) {
  (void)state;
  const ast_node_t *outer_lhs = NULL;
  const ast_node_t *outer_rhs = NULL;
  const ast_node_t *inner_lhs = NULL;
  const ast_node_t *inner_rhs = NULL;

  feed_id("number");
  feed_op(TOK_DOT);
  feed_id("floor");
  feed_op(TOK_DOT);
  feed_id("toInt");
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_DOT, &outer_lhs, &outer_rhs);
  assert_var(outer_rhs, "toInt");
  assert_binary(outer_lhs, TOK_DOT, &inner_lhs, &inner_rhs);
  assert_var(inner_lhs, "number");
  assert_var(inner_rhs, "floor");
  parse_done(ast);
}

/** LIST - konstrukce seznamu List(1, 2, 3) */
static void test_ext_list_constructor(void **state) {
  (void)state;

  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_COMMA);
  feed_int(2);
  feed_op(TOK_COMMA);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "List", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)3);
  assert_literal_int(call_arg(ast, 0, 0), 1, false);
  assert_literal_int(call_arg(ast, 0, 1), 2, false);
  assert_literal_int(call_arg(ast, 0, 2), 3, false);
  parse_done(ast);
}

/** LIST - prázdný seznam List() má jednu prázdnou parametrskou sekci */
static void test_ext_list_empty_constructor(void **state) {
  (void)state;

  feed_kw(KW_LIST);
  feed_op(TOK_LPAREN);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "List", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)0);
  parse_done(ast);
}

/** LIST - statická funkce List.range(1, 10) (člen na "proměnné" List) */
static void test_ext_list_static_range(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_kw(KW_LIST);
  feed_op(TOK_DOT);
  feed_id("range");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_COMMA);
  feed_int(10);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_DOT, &lhs, &rhs);
  assert_var(lhs, "List");
  assert_func_call(rhs, "range", 1);
  assert_int_equal((intmax_t)call_arg_cnt(rhs, 0), (intmax_t)2);
  assert_literal_int(call_arg(rhs, 0, 0), 1, false);
  assert_literal_int(call_arg(rhs, 0, 1), 10, false);
  parse_done(ast);
}

/** LIST - indexace seznamu lst(2) (syntakticky volání funkce) */
static void test_ext_list_indexing(void **state) {
  (void)state;

  feed_id("lst");
  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "lst", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)1);
  assert_literal_int(call_arg(ast, 0, 0), 2, false);
  parse_done(ast);
}

/** PARAM LIST - curryované volání add(1)(2) má dvě parametrské sekce */
static void test_ext_paramlists_curried_call(void **state) {
  (void)state;

  feed_id("add");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_op(TOK_LPAREN);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "add", 2);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)1);
  assert_literal_int(call_arg(ast, 0, 0), 1, false);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 1), (intmax_t)1);
  assert_literal_int(call_arg(ast, 1, 0), 2, false);
  parse_done(ast);
}

/** PARAM LIST - volání s více argumenty v jedné sekci */
static void test_ext_paramlists_multiple_args(void **state) {
  (void)state;

  feed_id("f");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_COMMA);
  feed_int(2);
  feed_op(TOK_COMMA);
  feed_int(3);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "f", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)3);
  assert_literal_int(call_arg(ast, 0, 0), 1, false);
  assert_literal_int(call_arg(ast, 0, 1), 2, false);
  assert_literal_int(call_arg(ast, 0, 2), 3, false);
  parse_done(ast);
}

/** PARAM LIST - volání bez argumentů foo() má jednu prázdnou sekci */
static void test_ext_paramlists_empty_call(void **state) {
  (void)state;

  feed_id("foo");
  feed_op(TOK_LPAREN);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_func_call(ast, "foo", 1);
  assert_int_equal((intmax_t)call_arg_cnt(ast, 0), (intmax_t)0);
  parse_done(ast);
}

/** Výraz ukončený středníkem (řádek s přiřazením `x + 1;`) */
static void test_expr_terminated_by_semicolon(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_id("x");
  feed_op(TOK_ADD);
  feed_int(1);
  feed_op(TOK_SEMICOLON);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_var(lhs, "x");
  assert_literal_int(rhs, 1, false);
  assert_current_token(TOK_SEMICOLON);
  parse_done(ast);
}

/** Nadbytečná pravá závorka bez otevřené úrovně výraz pouze ukončí */
static void test_rparen_terminates_expression(void **state) {
  (void)state;

  feed_int(1);
  feed_op(TOK_RPAREN);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_literal_int(ast, 1, false);
  assert_current_token(TOK_RPAREN);
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Negativní testy - chybové kódy
// ---------------------------------------------------------------------------

/**
 * @brief Spustí analýzu, ověří očekávanou chybu a vyčištění výstupu.
 *
 * Podle smlouvy z psa.h musí být při chybě *out_ast nastaveno na NULL.
 * Naplněný sentinel navíc ověřuje, že PSA výstupní ukazatel vždy přepíše
 * dřív, než by ho mohl použít (volající smí předat neinicializovaný
 * ukazatel).
 * @param expected Očekávaný chybový kód.
 */
static void parse_err(IFJ_ERR expected) {
  ast_node_t *ast = (ast_node_t *)0x1;

  assert_int_equal(run_parse(&ast), expected);
  assert_null(ast);
  token_free(&test_parser.current_token);
}

/** NULL parser musí vrátit IFJ_EINTERNAL (kontrola argumentů na vstupu). */
static void test_err_null_parser(void **state) {
  (void)state;
  ast_node_t *ast = NULL;

  assert_int_equal(psa_parse_expression(NULL, &ast), IFJ_EINTERNAL);
}

/** NULL výstupní ukazatel musí vrátit IFJ_EINTERNAL. */
static void test_err_null_out_ast(void **state) {
  (void)state;

  // Parser se připraví ručně - helper run_parse by předával platný out_ast.
  feed_int(1);
  memset(&test_parser, 0, sizeof(test_parser));
  assert_int_equal(__wrap_scanner_next_token(NULL, &test_parser.current_token),
                   IFJ_SUCCESS);
  assert_int_equal(psa_parse_expression(&test_parser, NULL), IFJ_EINTERNAL);
  token_free(&test_parser.current_token);
}

/** Prázdný výraz (pouze EOF) je syntaktická chyba. */
static void test_err_empty_expression(void **state) {
  (void)state;

  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Operátor bez pravého operandu: "1 +" je syntaktická chyba. */
static void test_err_dangling_operator(void **state) {
  (void)state;

  feed_int(1);
  feed_op(TOK_ADD);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Dva operandy bez operátoru mezi nimi: "1 2" je syntaktická chyba. */
static void test_err_missing_operator(void **state) {
  (void)state;

  feed_int(1);
  feed_int(2);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Binární operátor na začátku výrazu: "* 2" je syntaktická chyba. */
static void test_err_leading_binary_operator(void **state) {
  (void)state;

  feed_op(TOK_MUL);
  feed_int(2);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Dva binární operátory za sebou: "1 + + 2" je syntaktická chyba. */
static void test_err_double_binary_operator(void **state) {
  (void)state;

  feed_int(1);
  feed_op(TOK_ADD);
  feed_op(TOK_ADD);
  feed_int(2);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Neuzavřená závorka: "(1" je syntaktická chyba. */
static void test_err_unclosed_paren(void **state) {
  (void)state;

  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Samotná levá závorka: "(" je syntaktická chyba. */
static void test_err_lone_left_paren(void **state) {
  (void)state;

  feed_op(TOK_LPAREN);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Zřetězené relační operátory: "1 < 2 < 3" je syntaktická chyba. */
static void test_err_chained_relational(void **state) {
  (void)state;

  feed_int(1);
  feed_op(TOK_LT);
  feed_int(2);
  feed_op(TOK_LT);
  feed_int(3);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Zřetězené operátory rovnosti: "1 == 2 == 3" je syntaktická chyba. */
static void test_err_chained_equality(void **state) {
  (void)state;

  feed_int(1);
  feed_op(TOK_EQ);
  feed_int(2);
  feed_op(TOK_EQ);
  feed_int(3);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Ukončovací čárka v seznamu argumentů: "foo(1,)" je syntaktická chyba. */
static void test_err_trailing_comma_in_call(void **state) {
  (void)state;

  feed_id("foo");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_op(TOK_COMMA);
  feed_op(TOK_RPAREN);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/** Chybějící čárka mezi argumenty: "f(1 2)" je syntaktická chyba. */
static void test_err_missing_comma_in_call(void **state) {
  (void)state;

  feed_id("f");
  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

/**
 * @brief "by" uvnitř závorek: "(1 to 10 by 2)" je syntaktická chyba.
 *
 * Podle smlouvy z psa.h je "by" (běžné TOK_ID) povoleno pouze v hlavičce
 * for-generátoru, uvnitř závorek jde o chybu.
 */
static void test_err_by_inside_parens(void **state) {
  (void)state;

  feed_op(TOK_LPAREN);
  feed_int(1);
  feed_id("to");
  feed_int(10);
  feed_id("by");
  feed_int(2);
  feed_op(TOK_RPAREN);
  feed_end();
  parse_err(IFJ_ESYNTAX);
}

// ---------------------------------------------------------------------------
// Stress testy - velké a hluboce zanořené výrazy
// ---------------------------------------------------------------------------

/**
 * @brief 2000 jedniček spojených '+' se složí na jediný literál 2000.
 *
 * Levocidelný řetězec sčítání se redukuje průběžně (každé dvojčlenné
 * sčítání literálů se okamžitě složí), takže zásobník PSA zůstává mělký.
 */
static void test_stress_large_folded_sum(void **state) {
  (void)state;

  feed_int(1);
  for (int i = 0; i < 1999; i++) {
    feed_op(TOK_ADD);
    feed_int(1);
  }
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_literal_int(ast, 2000, true);
  parse_done(ast);
}

/**
 * @brief 1000 proměnných spojených '+' se neskládá, vzniká levocidelný
 * řetěz 999 binárních výrazů.
 */
static void test_stress_large_var_sum(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;

  feed_id("a");
  for (int i = 0; i < 999; i++) {
    feed_op(TOK_ADD);
    feed_id("a");
  }
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_int_equal((intmax_t)count_nodes(ast, AST_EXPR), (intmax_t)999);
  assert_int_equal((intmax_t)count_nodes(ast, AST_VAR), (intmax_t)1000);
  assert_binary(ast, TOK_ADD, &lhs, &rhs);
  assert_var(rhs, "a");
  parse_done(ast);
}

/** 1000 zanořených závorek kolem "1 + 2" se složí na literál 3. */
static void test_stress_deep_parens(void **state) {
  (void)state;

  for (int i = 0; i < 1000; i++) {
    feed_op(TOK_LPAREN);
  }
  feed_int(1);
  feed_op(TOK_ADD);
  feed_int(2);
  for (int i = 0; i < 1000; i++) {
    feed_op(TOK_RPAREN);
  }
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_literal_int(ast, 3, true);
  parse_done(ast);
}

/**
 * @brief 1001 unárních minus nad literálem 5: lichý počet negací dává -5.
 *
 * Každá unární negace literálu se složí na nový literál s opačným
 * znaménkem, is_folded se dědí z přímého literálu (false).
 */
static void test_stress_deep_unary(void **state) {
  (void)state;

  for (int i = 0; i < 1001; i++) {
    feed_op(TOK_SUB);
  }
  feed_int(5);
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_literal_int(ast, -5, false);
  parse_done(ast);
}

/** f(f(...f(1)...)) - 200 úrovní zanoření volání funkce. */
static void test_stress_deep_call_nesting(void **state) {
  (void)state;

  for (int i = 0; i < 200; i++) {
    feed_id("f");
    feed_op(TOK_LPAREN);
  }
  feed_int(1);
  for (int i = 0; i < 200; i++) {
    feed_op(TOK_RPAREN);
  }
  feed_end();

  ast_node_t *ast = parse_ok();
  assert_int_equal((intmax_t)call_nesting_depth(ast), (intmax_t)200);
  parse_done(ast);
}

/**
 * @brief Velká čísla na hranicích rozsahu Int (bez přetečení).
 *
 * Tři nezávislé analýzy - mezi nimi se resetuje fronta mocku.
 */
static void test_stress_large_numbers(void **state) {
  (void)state;
  ast_node_t *ast = NULL;

  // INT64_MAX - 1 + 1 = INT64_MAX
  feed_int(INT64_MAX - 1);
  feed_op(TOK_ADD);
  feed_int(1);
  feed_end();
  ast = parse_ok();
  assert_literal_int(ast, INT64_MAX, true);
  parse_done(ast);

  // (2^60) * 4 = 2^62 = 4611686018427387904
  mock_reset();
  feed_int(1LL << 60);
  feed_op(TOK_MUL);
  feed_int(4);
  feed_end();
  ast = parse_ok();
  assert_literal_int(ast, (int64_t)1LL << 62, true);
  parse_done(ast);

  // INT64_MAX / 2 se zkracuje směrem k nule
  mock_reset();
  feed_int(INT64_MAX);
  feed_op(TOK_DIV);
  feed_int(2);
  feed_end();
  ast = parse_ok();
  assert_literal_int(ast, INT64_MAX / 2, true);
  parse_done(ast);
}

/**
 * @brief Okrajové případy dělení nulou - všechny zůstávají nesložené.
 *
 * Dělení nulou (celočíselné i desetinné) se neskládá, zůstává binární
 * výraz TOK_DIV. Tři nezávislé analýzy - mezi nimi se resetuje fronta
 * mocku.
 */
static void test_stress_div_by_zero_edges(void **state) {
  (void)state;
  const ast_node_t *lhs = NULL;
  const ast_node_t *rhs = NULL;
  ast_node_t *ast = NULL;

  // 10 / 0
  feed_int(10);
  feed_op(TOK_DIV);
  feed_int(0);
  feed_end();
  ast = parse_ok();
  assert_binary(ast, TOK_DIV, &lhs, &rhs);
  assert_literal_int(lhs, 10, false);
  assert_literal_int(rhs, 0, false);
  parse_done(ast);

  // 0 / 0
  mock_reset();
  feed_int(0);
  feed_op(TOK_DIV);
  feed_int(0);
  feed_end();
  ast = parse_ok();
  assert_binary(ast, TOK_DIV, &lhs, &rhs);
  assert_literal_int(lhs, 0, false);
  assert_literal_int(rhs, 0, false);
  parse_done(ast);

  // 10.0 / 0.0
  mock_reset();
  feed_dbl(10.0);
  feed_op(TOK_DIV);
  feed_dbl(0.0);
  feed_end();
  ast = parse_ok();
  assert_binary(ast, TOK_DIV, &lhs, &rhs);
  assert_literal_double(lhs, 10.0, false);
  assert_literal_double(rhs, 0.0, false);
  parse_done(ast);
}

// ---------------------------------------------------------------------------
// Hlavní funkce
// ---------------------------------------------------------------------------

/**
 * @brief Příprava jednoho testu - vynulování fronty mocku.
 *
 * Fronta se musí resetovat před každým testem (ne jen před skupinou):
 * pokud PSA skončí s chybou dřív, než spotřebuje všechny nakrmené tokeny,
 * zbytky by jinak kontaminovaly následující testy.
 * @param state Nevyužito.
 * @return 0 (vždy úspěch).
 */
static int mock_setup(void **state) {
  (void)state;

  mock_reset();
  return 0;
}

/**
 * @brief Registrace testu s resetem fronty mocku před každým testem.
 * @param fn Testovací funkce.
 */
#define PSA_TEST(fn) cmocka_unit_test_setup_teardown(fn, mock_setup, NULL)

int main(void) {
  const struct CMUnitTest tests[] = {
      // Konstantní skládání - binární operace
      PSA_TEST(test_fold_binary_add_int),
      PSA_TEST(test_fold_binary_add_double),
      PSA_TEST(test_fold_binary_add_mixed_int_double),
      PSA_TEST(test_fold_binary_sub_int),
      PSA_TEST(test_fold_binary_sub_double),
      PSA_TEST(test_fold_binary_mul_int),
      PSA_TEST(test_fold_binary_mul_double),
      PSA_TEST(test_fold_binary_div_int),
      PSA_TEST(test_no_fold_div_by_zero_int),
      PSA_TEST(test_no_fold_div_by_zero_double),
      PSA_TEST(test_no_fold_div_by_zero_in_sum),
      PSA_TEST(test_fold_binary_div_truncates_toward_zero),
      PSA_TEST(test_fold_binary_div_double),
      PSA_TEST(test_fold_binary_string_concat),
      PSA_TEST(test_fold_binary_list_concat),
      PSA_TEST(test_fold_binary_list_concat_chain),
      PSA_TEST(test_no_fold_list_concat_with_var),
      PSA_TEST(test_no_fold_binary_with_var),
      PSA_TEST(test_no_fold_binary_type_mismatch),
      PSA_TEST(test_fold_binary_relational),
      PSA_TEST(test_fold_binary_equality),
      PSA_TEST(test_fold_binary_bool_ops),
      PSA_TEST(test_no_fold_string_relational),
      // Konstantní skládání - unární operace
      PSA_TEST(test_fold_unary_neg_int),
      PSA_TEST(test_fold_unary_neg_double),
      PSA_TEST(test_fold_unary_not_bool),
      PSA_TEST(test_fold_unary_neg_of_folded_operand),
      PSA_TEST(test_fold_unary_not_of_folded_operand),
      PSA_TEST(test_unary_neg_var_not_folded),
      PSA_TEST(test_unary_not_var_not_folded),
      PSA_TEST(test_unary_neg_string_not_folded),
      PSA_TEST(test_unary_double_negation),
      // Priorita operátorů
      PSA_TEST(test_precedence_mul_over_add),
      PSA_TEST(test_precedence_parens_override),
      PSA_TEST(test_precedence_add_sub_left_assoc),
      PSA_TEST(test_precedence_rel_over_eq),
      PSA_TEST(test_precedence_and_over_or),
      PSA_TEST(test_precedence_and_left_assoc_or),
      PSA_TEST(test_precedence_not_over_and),
      PSA_TEST(test_precedence_unary_minus_over_mul),
      PSA_TEST(test_precedence_dot_over_mul),
      PSA_TEST(test_precedence_range_below_add),
      // Vyhodnocení výrazů
      PSA_TEST(test_eval_precedence_value),
      PSA_TEST(test_eval_parens_value),
      PSA_TEST(test_eval_left_assoc_value),
      PSA_TEST(test_eval_mixed_arith),
      PSA_TEST(test_eval_int_division),
      PSA_TEST(test_eval_double_division),
      PSA_TEST(test_eval_comparisons),
      PSA_TEST(test_eval_bool_expressions),
      PSA_TEST(test_eval_string_concatenation),
      PSA_TEST(test_eval_unary_minus_value),
      PSA_TEST(test_eval_unit_literal),
      // Rozšíření specifikace
      PSA_TEST(test_ext_funexp_function_in_expression),
      PSA_TEST(test_ext_funexp_nested_calls),
      PSA_TEST(test_ext_boolthen_ends_with_then),
      PSA_TEST(test_ext_boolthen_bool_condition),
      PSA_TEST(test_ext_control_range_to),
      PSA_TEST(test_ext_control_range_until),
      PSA_TEST(test_ext_control_to_not_consumed),
      PSA_TEST(test_ext_control_condition_ends_with_do),
      PSA_TEST(test_ext_valmem_string_length),
      PSA_TEST(test_ext_valmem_chained_members),
      PSA_TEST(test_ext_list_constructor),
      PSA_TEST(test_ext_list_empty_constructor),
      PSA_TEST(test_ext_list_static_range),
      PSA_TEST(test_ext_list_indexing),
      PSA_TEST(test_ext_paramlists_curried_call),
      PSA_TEST(test_ext_paramlists_multiple_args),
      PSA_TEST(test_ext_paramlists_empty_call),
      PSA_TEST(test_expr_terminated_by_semicolon),
      PSA_TEST(test_rparen_terminates_expression),
      // Negativní testy - chybové kódy
      PSA_TEST(test_err_null_parser),
      PSA_TEST(test_err_null_out_ast),
      PSA_TEST(test_err_empty_expression),
      PSA_TEST(test_err_dangling_operator),
      PSA_TEST(test_err_missing_operator),
      PSA_TEST(test_err_leading_binary_operator),
      PSA_TEST(test_err_double_binary_operator),
      PSA_TEST(test_err_unclosed_paren),
      PSA_TEST(test_err_lone_left_paren),
      PSA_TEST(test_err_chained_relational),
      PSA_TEST(test_err_chained_equality),
      PSA_TEST(test_err_trailing_comma_in_call),
      PSA_TEST(test_err_missing_comma_in_call),
      PSA_TEST(test_err_by_inside_parens),
      // Stress testy
      PSA_TEST(test_stress_large_folded_sum),
      PSA_TEST(test_stress_large_var_sum),
      PSA_TEST(test_stress_deep_parens),
      PSA_TEST(test_stress_deep_unary),
      PSA_TEST(test_stress_deep_call_nesting),
      PSA_TEST(test_stress_large_numbers),
      PSA_TEST(test_stress_div_by_zero_edges),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
