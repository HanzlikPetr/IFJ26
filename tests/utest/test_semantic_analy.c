/**
 * @file test_semantic_analy.c
 * @brief IFJ26 - Unit testy pro sémantický analyzátor (cmocka)
 *
 * @author xhanzlp00 Petr Hanzlík
 *
 * Testy vygenerovala AI (GLM 5.3, přes opencode) podle tohoto zadání:
 *
 * "make unit test for semantic.c use only function in semantic.h"
 *
 * Testy volají výhradně funkce deklarované v semantic_analy.h
 * (symtable_fill_funcitons() a check_functions()). Vstupní AST stromy se
 * sestavují pomocí veřejných sestavovacích funkcí z ast.h a tabulka symbolů
 * se spravuje přes rozhraní symtable.h. Testy ověřují návratové kódy a
 * zároveň vedlejší produkty analýzy: obsah tabulky symbolů (vestavěné i
 * uživatelské funkce), přejmenování identifikátorů na code_gen_name a
 * odvozené typy zapsané do AST uzlů.
 */

// strdup() je POSIX funkce, s -std=c11 ji je nutné zpřístupnit.
#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "setup_test.h"

#include "ast.h"
#include "error.h"
#include "semantic_analy.h"
#include "symtable.h"

// ---------------------------------------------------------------------------
// Stav testů a pomocné funkce
// ---------------------------------------------------------------------------

// Ukončovací zástupce pro variadické sestavovače AST uzlů.
#define END (ast_node_t *)NULL

typedef struct {
  symtable_t table; // Tabulka symbolů naplňovaná v testech
  ast_node_t *root; // Kořen testovaného AST programu
} sem_state_t;

static int sem_setup(void **state) {
  sem_state_t *s = calloc(1, sizeof(sem_state_t));
  assert_non_null(s);
  assert_int_equal(symtable_init(&s->table), IFJ_SUCCESS);
  s->root = NULL;
  *state = s;
  return 0;
}

static int sem_teardown(void **state) {
  sem_state_t *s = (sem_state_t *)*state;
  if (s == NULL) {
    return 0;
  }
  symtable_destroy(&s->table);
  ast_free(s->root);
  free(s);
  return 0;
}

static sem_state_t *get_state(void **state) { return (sem_state_t *)*state; }

// Zničení a znovu-inicializace stavu v rámci jednoho testu. Používá se při
// ověřování více nezávislých případů za sebou, protože tabulka symbolů po
// prvním naplnění obsahuje vestavěné funkce a další naplnění by skončilo
// chybou IFJ_ESEM_REDEF.
static void reset_state(sem_state_t *s) {
  assert_int_equal(symtable_destroy(&s->table), IFJ_SUCCESS);
  assert_int_equal(symtable_init(&s->table), IFJ_SUCCESS);
  ast_free(s->root);
  s->root = NULL;
}

static void set_root(sem_state_t *s, ast_node_t *root) { s->root = root; }

// Naplnění tabulky symbolů s očekávaným výsledkem
static void expect_fill(sem_state_t *s, IFJ_ERR expected) {
  assert_int_equal(symtable_fill_funcitons(&s->table, s->root), expected);
}

// Sémantická kontrola s očekávaným výsledkem
static void check_result(sem_state_t *s, IFJ_ERR expected) {
  assert_int_equal(check_functions(s->root, &s->table), expected);
}

// Kompletní případ: sestavení programu, naplnění tabulky a kontrola
static void run_case(sem_state_t *s, ast_node_t *root, IFJ_ERR fill_err,
                     IFJ_ERR check_err) {
  set_root(s, root);
  expect_fill(s, fill_err);
  if (fill_err == IFJ_SUCCESS) {
    check_result(s, check_err);
  }
}

// Ověření dat vestavěné funkce v tabulce symbolů
static void expect_builtin_fn(symtable_t *table, const char *name,
                              ifj26_type_t ret, size_t param_cnt,
                              const ifj26_type_t *types) {
  symtable_data_t *data = NULL;
  assert_int_equal(symtable_find(table, name, &data), IFJ_SUCCESS);
  assert_non_null(data);
  assert_true(data->is_function);
  assert_int_equal(data->func.return_type.type, ret.type);
  assert_int_equal(data->func.return_type.is_list, ret.is_list);
  assert_true(data->func.return_type_explicit);
  assert_true(data->func.is_resolving);
  assert_true(data->func.is_checked);
  assert_true(data->func.has_return_stmt);
  assert_null(data->func.node);
  assert_int_equal(data->func.param_list_count, 1);
  assert_non_null(data->func.param_list);
  assert_int_equal(data->func.param_list[0]->count, param_cnt);
  if (param_cnt > 0) {
    assert_non_null(data->func.param_list[0]->types);
  }
  for (size_t i = 0; i < param_cnt; i++) {
    assert_int_equal(data->func.param_list[0]->types[i].type, types[i].type);
    assert_int_equal(data->func.param_list[0]->types[i].is_list,
                     types[i].is_list);
  }
}

// ---------------------------------------------------------------------------
// Pomocné typy a sestavovače AST uzlů
// ---------------------------------------------------------------------------

static ifj26_type_t t_int(void) {
  return (ifj26_type_t){.type = TYPE_INT, .is_list = false};
}

static ifj26_type_t t_double(void) {
  return (ifj26_type_t){.type = TYPE_DOUBLE, .is_list = false};
}

static ifj26_type_t t_string(void) {
  return (ifj26_type_t){.type = TYPE_STRING, .is_list = false};
}

static ifj26_type_t t_unit(void) {
  return (ifj26_type_t){.type = TYPE_UNIT, .is_list = false};
}

static ifj26_type_t t_any(void) {
  return (ifj26_type_t){.type = TYPE_ANY, .is_list = false};
}

static ifj26_type_t t_unresolved(void) {
  return (ifj26_type_t){.type = TYPE_UNRESOLVED, .is_list = false};
}

static ifj26_type_t t_list(data_type_t type) {
  return (ifj26_type_t){.type = type, .is_list = true};
}

static ast_node_t *node_new(ast_node_type_t type) {
  ast_node_t *node = NULL;
  IFJ_ERR err = ast_create_node(&node, type);
  if (err != IFJ_SUCCESS || node == NULL) {
    fail_msg("ast_create_node selhalo (err=%d)", (int)err);
  }
  return node;
}

static char *dup_str(const char *s) {
  char *copy = strdup(s);
  if (copy == NULL) {
    fail_msg("strdup selhal");
  }
  return copy;
}

static ast_node_t *lit_int(int64_t value) {
  ast_node_t *n = node_new(AST_LITERAL);
  n->as.literal.type = TYPE_INT;
  n->as.literal.is_folded = true;
  n->as.literal.int_value = value;
  return n;
}

static ast_node_t *lit_double(double value) {
  ast_node_t *n = node_new(AST_LITERAL);
  n->as.literal.type = TYPE_DOUBLE;
  n->as.literal.is_folded = true;
  n->as.literal.double_value = value;
  return n;
}

static ast_node_t *lit_string(const char *value) {
  ast_node_t *n = node_new(AST_LITERAL);
  n->as.literal.type = TYPE_STRING;
  n->as.literal.is_folded = true;
  n->as.literal.str_value = dup_str(value);
  return n;
}

static ast_node_t *lit_bool(bool value) {
  ast_node_t *n = node_new(AST_LITERAL);
  n->as.literal.type = TYPE_BOOL;
  n->as.literal.is_folded = true;
  n->as.literal.bool_value = value;
  return n;
}

static ast_node_t *lit_unit(void) {
  ast_node_t *n = node_new(AST_LITERAL);
  n->as.literal.type = TYPE_UNIT;
  n->as.literal.is_folded = true;
  return n;
}

static ast_node_t *var(const char *id) {
  ast_node_t *n = node_new(AST_VAR);
  n->as.var.id = dup_str(id);
  return n;
}

static ast_node_t *bin(token_type_t op, ast_node_t *lhs, ast_node_t *rhs) {
  ast_node_t *n = node_new(AST_EXPR);
  n->as.expr.is_unary = false;
  n->as.expr.operator_type = op;
  n->as.expr.binary.lhs = lhs;
  n->as.expr.binary.rhs = rhs;
  return n;
}

static ast_node_t *un(token_type_t op, ast_node_t *operand) {
  ast_node_t *n = node_new(AST_EXPR);
  n->as.expr.is_unary = true;
  n->as.expr.operator_type = op;
  n->as.expr.unary.operand = operand;
  return n;
}

// Seznam formálních parametrů funkce (uzly AST_PARAM)
static ast_node_t *params(ast_node_t *first, ...) {
  ast_node_t *list = node_new(AST_PARAM_LIST);
  va_list ap;
  va_start(ap, first);
  for (ast_node_t *item = first; item != END; item = va_arg(ap, ast_node_t *)) {
    if (ast_param_list_add_param(list, item) != IFJ_SUCCESS) {
      fail_msg("ast_param_list_add_param selhalo");
    }
  }
  va_end(ap);
  return list;
}

// Seznam argumentů volání funkce (uzly výrazů)
static ast_node_t *args(ast_node_t *first, ...) {
  ast_node_t *list = node_new(AST_PARAM_LIST);
  va_list ap;
  va_start(ap, first);
  for (ast_node_t *item = first; item != END; item = va_arg(ap, ast_node_t *)) {
    if (ast_func_call_param_list_add_arg(list, item) != IFJ_SUCCESS) {
      fail_msg("ast_func_call_param_list_add_arg selhalo");
    }
  }
  va_end(ap);
  return list;
}

static ast_node_t *param(const char *id, ifj26_type_t type) {
  ast_node_t *n = node_new(AST_PARAM);
  n->as.param.id = dup_str(id);
  n->as.param.param_type = type;
  return n;
}

// Volání funkce s jedním nebo více seznamy argumentů (např. f(1)(2))
static ast_node_t *call(const char *id, ast_node_t *first_list, ...) {
  ast_node_t *n = node_new(AST_FUNC_CALL);
  n->as.func_call.id = dup_str(id);
  va_list ap;
  va_start(ap, first_list);
  for (ast_node_t *list = first_list; list != END;
       list = va_arg(ap, ast_node_t *)) {
    if (ast_func_call_add_param_list(n, list) != IFJ_SUCCESS) {
      fail_msg("ast_func_call_add_param_list selhalo");
    }
  }
  va_end(ap);
  return n;
}

static ast_node_t *func(const char *id, ifj26_type_t ret, ast_node_t *body,
                        ast_node_t *first_list, ...) {
  ast_node_t *n = node_new(AST_FUNC);
  n->as.func.main = false;
  n->as.func.id = dup_str(id);
  n->as.func.return_type = ret;
  n->as.func.body = body;
  va_list ap;
  va_start(ap, first_list);
  for (ast_node_t *list = first_list; list != END;
       list = va_arg(ap, ast_node_t *)) {
    if (ast_func_add_param_list(n, list) != IFJ_SUCCESS) {
      fail_msg("ast_func_add_param_list selhalo");
    }
  }
  va_end(ap);
  return n;
}

static ast_node_t *main_func(ast_node_t *body) {
  ast_node_t *n = func("main", t_unit(), body, params(END), END);
  n->as.func.main = true;
  return n;
}

// Hlavní funkce s explicitním návratovým typem (pro negativní testy)
static ast_node_t *main_func_rt(ifj26_type_t ret, ast_node_t *body) {
  ast_node_t *n = func("main", ret, body, params(END), END);
  n->as.func.main = true;
  return n;
}

static ast_node_t *program(ast_node_t *first, ...) {
  ast_node_t *root = node_new(AST_ROOT);
  va_list ap;
  va_start(ap, first);
  for (ast_node_t *item = first; item != END; item = va_arg(ap, ast_node_t *)) {
    if (ast_root_add_func(root, item) != IFJ_SUCCESS) {
      fail_msg("ast_root_add_func selhalo");
    }
  }
  va_end(ap);
  return root;
}

static ast_node_t *block_st(ast_node_t *first, ...) {
  ast_node_t *block = node_new(AST_BLOCK);
  va_list ap;
  va_start(ap, first);
  for (ast_node_t *item = first; item != END; item = va_arg(ap, ast_node_t *)) {
    if (ast_block_add_command(block, item) != IFJ_SUCCESS) {
      fail_msg("ast_block_add_command selhalo");
    }
  }
  va_end(ap);
  return block;
}

static ast_node_t *def_st(bool mut, const char *id, ifj26_type_t ann,
                          ast_node_t *rhs) {
  ast_node_t *n = node_new(AST_DEF);
  n->as.def.mut = mut;
  n->as.def.id = dup_str(id);
  n->as.def.type = ann;
  n->as.def.rhs = rhs;
  return n;
}

static ast_node_t *assign_st(const char *id, ast_node_t *rhs) {
  ast_node_t *n = node_new(AST_ASSIGN);
  n->as.assign.id = dup_str(id);
  n->as.assign.rhs = rhs;
  return n;
}

static ast_node_t *return_st(ast_node_t *rhs) {
  ast_node_t *n = node_new(AST_FUNC_RET);
  n->as.func_ret.rhs = rhs;
  return n;
}

static ast_node_t *if_st(ast_node_t *cond, ast_node_t *body,
                         ast_node_t *else_st) {
  ast_node_t *n = node_new(AST_IF);
  n->as.if_st.expr = cond;
  n->as.if_st.body = body;
  n->as.if_st.else_st = else_st;
  return n;
}

static ast_node_t *else_if(ast_node_t *cond, ast_node_t *body) {
  ast_node_t *n = node_new(AST_ELSE_IF);
  n->as.else_if_st.expr = cond;
  n->as.else_if_st.body = body;
  return n;
}

static ast_node_t *while_st(ast_node_t *cond, ast_node_t *body) {
  ast_node_t *n = node_new(AST_WHILE);
  n->as.while_st.expr = cond;
  n->as.while_st.body = body;
  return n;
}

static ast_node_t *for_range_st(bool until, ast_node_t *upper, ast_node_t *by) {
  ast_node_t *n = node_new(AST_FOR_RANGE);
  n->as.for_range.until = until;
  n->as.for_range.expr = upper;
  n->as.for_range.by_expr = by;
  return n;
}

static ast_node_t *for_gen(const char *id, ast_node_t *src, ast_node_t *range) {
  ast_node_t *n = node_new(AST_FOR_GEN);
  n->as.for_gen.id = dup_str(id);
  n->as.for_gen.expr = src;
  n->as.for_gen.range = range;
  return n;
}

static ast_node_t *guard(ast_node_t *expr) {
  ast_node_t *n = node_new(AST_FOR_GUARD);
  n->as.for_guard.expr = expr;
  return n;
}

static ast_node_t *for_st(ast_node_t *gen, ast_node_t *body,
                          ast_node_t *first_guard, ...) {
  ast_node_t *n = node_new(AST_FOR);
  n->as.for_st.gen = gen;
  n->as.for_st.body = body;
  va_list ap;
  va_start(ap, first_guard);
  for (ast_node_t *item = first_guard; item != END;
       item = va_arg(ap, ast_node_t *)) {
    if (ast_for_add_guard(n, item) != IFJ_SUCCESS) {
      fail_msg("ast_for_add_guard selhalo");
    }
  }
  va_end(ap);
  return n;
}

// ---------------------------------------------------------------------------
// Testy symtable_fill_funcitons
// ---------------------------------------------------------------------------

static void test_fill_null_args(void **state) {
  sem_state_t *s = get_state(state);
  assert_int_equal(symtable_fill_funcitons(NULL, NULL), IFJ_EINTERNAL);
  assert_int_equal(symtable_fill_funcitons(&s->table, NULL), IFJ_EINTERNAL);
}

static void test_fill_wrong_root_type(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, block_st(END));
  expect_fill(s, IFJ_EINTERNAL);
}

static void test_fill_empty_program(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, program(END));
  expect_fill(s, IFJ_ESEM_UNDEF);
}

static void test_fill_main_only(void **state) {
  sem_state_t *s = get_state(state);
  ast_node_t *main_node = main_func(block_st(END));
  set_root(s, program(main_node, END));
  expect_fill(s, IFJ_SUCCESS);

  // Vestavěné funkce: print(value: any): Unit, readLine(): String,
  // readInt(): Int, readDouble(): Double, toStr(value: any): String,
  // toInt(value: String): Int, strLength(value: String): Int,
  // subStr(value: String, start: Int, end: Int): String,
  // ord(value: String, index: Int): Int, chr(code: Int): String
  expect_builtin_fn(&s->table, "print", t_unit(), 1, (ifj26_type_t[]){t_any()});
  expect_builtin_fn(&s->table, "readLine", t_string(), 0, NULL);
  expect_builtin_fn(&s->table, "readInt", t_int(), 0, NULL);
  expect_builtin_fn(&s->table, "readDouble", t_double(), 0, NULL);
  expect_builtin_fn(&s->table, "toStr", t_string(), 1,
                    (ifj26_type_t[]){t_any()});
  expect_builtin_fn(&s->table, "toInt", t_int(), 1,
                    (ifj26_type_t[]){t_string()});
  expect_builtin_fn(&s->table, "strLength", t_int(), 1,
                    (ifj26_type_t[]){t_string()});
  expect_builtin_fn(&s->table, "subStr", t_string(), 3,
                    (ifj26_type_t[]){t_string(), t_int(), t_int()});
  expect_builtin_fn(&s->table, "ord", t_int(), 2,
                    (ifj26_type_t[]){t_string(), t_int()});
  expect_builtin_fn(&s->table, "chr", t_string(), 1, (ifj26_type_t[]){t_int()});

  // Data uživatelsky definované hlavní funkce
  symtable_data_t *data = NULL;
  assert_int_equal(symtable_find(&s->table, "main", &data), IFJ_SUCCESS);
  assert_non_null(data);
  assert_true(data->is_function);
  assert_int_equal(data->func.return_type.type, TYPE_UNIT);
  assert_int_equal(data->func.return_type.is_list, false);
  assert_true(data->func.return_type_explicit);
  assert_false(data->func.is_checked);
  assert_false(data->func.has_return_stmt);
  assert_ptr_equal(data->func.node, main_node);
  assert_int_equal(data->func.param_list_count, 1);
  assert_non_null(data->func.param_list);
  assert_int_equal(data->func.param_list[0]->count, 0);
}

static void test_fill_main_bad_return_type(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func_rt(t_int(), block_st(END)), END));
  expect_fill(s, IFJ_ESEM_TYPE);
}

static void test_fill_main_with_params(void **state) {
  sem_state_t *s = get_state(state);
  ast_node_t *m = func("main", t_unit(), block_st(END),
                       params(param("a", t_int()), END), END);
  m->as.func.main = true;
  set_root(s, program(m, END));
  expect_fill(s, IFJ_ESEM_ARGS);
}

static void test_fill_main_two_paramlists(void **state) {
  sem_state_t *s = get_state(state);
  ast_node_t *m =
      func("main", t_unit(), block_st(END), params(END), params(END), END);
  m->as.func.main = true;
  set_root(s, program(m, END));
  expect_fill(s, IFJ_ESEM_ARGS);
}

static void test_fill_two_mains(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func(block_st(END)), main_func(block_st(END)), END));
  expect_fill(s, IFJ_ESEM_REDEF);
}

static void test_fill_missing_main(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s,
           program(func("f", t_unit(), block_st(END), params(END), END), END));
  expect_fill(s, IFJ_ESEM_UNDEF);
}

static void test_fill_redef_builtin(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func(block_st(END)),
                      func("print", t_unit(), block_st(END), params(END), END),
                      END));
  expect_fill(s, IFJ_ESEM_REDEF);
}

static void test_fill_duplicate_func(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s,
           program(main_func(block_st(END)),
                   func("f", t_unit(), block_st(END), params(END), END),
                   func("f", t_unit(), block_st(END), params(END), END), END));
  expect_fill(s, IFJ_ESEM_REDEF);
}

static void test_fill_duplicate_param_in_list(void **state) {
  sem_state_t *s = get_state(state);
  set_root(
      s, program(main_func(block_st(END)),
                 func("f", t_unit(), block_st(END),
                      params(param("a", t_int()), param("a", t_double()), END),
                      END),
                 END));
  expect_fill(s, IFJ_ESEM_REDEF);
}

static void test_fill_duplicate_param_across_lists(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func(block_st(END)),
                      func("f", t_unit(), block_st(END),
                           params(param("a", t_int()), END),
                           params(param("a", t_double()), END), END),
                      END));
  expect_fill(s, IFJ_ESEM_REDEF);
}

static void test_fill_user_func_data(void **state) {
  sem_state_t *s = get_state(state);
  ast_node_t *add_node =
      func("add", t_int(), block_st(END),
           params(param("a", t_int()), param("b", t_int()), END), END);
  ast_node_t *f_node =
      func("f", t_unresolved(), block_st(END), params(END), END);
  set_root(s, program(add_node, f_node, main_func(block_st(END)), END));
  expect_fill(s, IFJ_SUCCESS);

  symtable_data_t *data = NULL;
  assert_int_equal(symtable_find(&s->table, "add", &data), IFJ_SUCCESS);
  assert_true(data->is_function);
  assert_int_equal(data->func.return_type.type, TYPE_INT);
  assert_int_equal(data->func.return_type.is_list, false);
  assert_true(data->func.return_type_explicit);
  assert_false(data->func.is_checked);
  assert_ptr_equal(data->func.node, add_node);
  assert_int_equal(data->func.param_list_count, 1);
  assert_int_equal(data->func.param_list[0]->count, 2);
  assert_int_equal(data->func.param_list[0]->types[0].type, TYPE_INT);
  assert_int_equal(data->func.param_list[0]->types[1].type, TYPE_INT);

  // Funkce bez explicitního návratového typu
  assert_int_equal(symtable_find(&s->table, "f", &data), IFJ_SUCCESS);
  assert_int_equal(data->func.return_type.type, TYPE_UNRESOLVED);
  assert_false(data->func.return_type_explicit);
}

static void test_fill_is_not_idempotent(void **state) {
  // Opakované naplnění téže tabulky skončí chybou (vestavěné funkce se
  // vkládají znovu) - z toho důvodu testy používají reset_state()
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func(block_st(END)), END));
  expect_fill(s, IFJ_SUCCESS);
  expect_fill(s, IFJ_ESEM_REDEF);
}

// ---------------------------------------------------------------------------
// Testy check_functions - základní případy
// ---------------------------------------------------------------------------

static void test_check_null_args(void **state) {
  sem_state_t *s = get_state(state);
  assert_int_equal(check_functions(NULL, NULL), IFJ_EINTERNAL);
  assert_int_equal(check_functions(NULL, &s->table), IFJ_EINTERNAL);
  set_root(s, main_func(block_st(END)));
  assert_int_equal(check_functions(s->root, NULL), IFJ_EINTERNAL);
}

static void test_check_wrong_root_type(void **state) {
  sem_state_t *s = get_state(state);
  set_root(s, block_st(END));
  assert_int_equal(check_functions(s->root, &s->table), IFJ_EINTERNAL);
}

static void test_check_main_minimal(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s, program(main_func(block_st(END)), END), IFJ_SUCCESS, IFJ_SUCCESS);

  symtable_data_t *data = NULL;
  assert_int_equal(symtable_find(&s->table, "main", &data), IFJ_SUCCESS);
  assert_true(data->func.is_checked);
  assert_false(data->func.has_return_stmt);
  // Odvozený návratový typ se zapíše do AST uzlu funkce
  assert_int_equal(s->root->as.root.funcs[0]->as.func.return_type.type,
                   TYPE_UNIT);
}

static void test_check_infer_return_type(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_unresolved(), block_st(lit_int(42), END),
                        params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_SUCCESS);

  symtable_data_t *data = NULL;
  assert_int_equal(symtable_find(&s->table, "f", &data), IFJ_SUCCESS);
  assert_int_equal(data->func.return_type.type, TYPE_INT);
  assert_false(data->func.return_type_explicit);
  assert_true(data->func.is_checked);
  assert_int_equal(s->root->as.root.funcs[0]->as.func.return_type.type,
                   TYPE_INT);
}

static void test_check_return_without_explicit_type(void **state) {
  // Příkaz return ve funkci s odvozovaným návratovým typem není povolen
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_unresolved(),
                        block_st(return_st(lit_int(1)), END), params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_return_explicit_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_int(), block_st(return_st(lit_int(1)), END),
                        params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_return_type_mismatch(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_int(), block_st(return_st(lit_string("a")), END),
                        params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_bare_return_non_unit(void **state) {
  // return bez výrazu je povolen jen ve funkcích s návratovým typem Unit
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_int(), block_st(return_st(NULL), END),
                        params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_ESEM_OTHER);
}

static void test_check_return_value_in_unit(void **state) {
  // return s výrazem ve funkci s návratovým typem Unit je v pořádku
  // (hodnota výrazu se ignoruje)
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_unit(), block_st(return_st(lit_int(1)), END),
                        params(END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_SUCCESS);
  reset_state(s);
  run_case(s, program(main_func(block_st(return_st(lit_int(1)), END)), END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

// ---------------------------------------------------------------------------
// Testy proměnných a přiřazení
// ---------------------------------------------------------------------------

static void test_check_def_assign_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  def_st(true, "x", t_unresolved(), lit_int(1)),
                  assign_st("x", bin(TOK_ADD, var("x"), lit_int(1))), END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);

  // Identifikátory se přepíšou na code_gen_name s úrovní zanoření (tělo
  // funkce sdílí tabulku s parametry a je na úrovni 1)
  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_string_equal(body->as.block.commands[0]->as.def.id, "x$1");
  assert_string_equal(body->as.block.commands[1]->as.assign.id, "x$1");
  ast_node_t *rhs = body->as.block.commands[1]->as.assign.rhs;
  assert_string_equal(rhs->as.expr.binary.lhs->as.var.id, "x$1");
}

static void test_check_def_explicit_ok(void **state) {
  // Implicitní konverze Int -> Double při explicitním typu
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "x", t_double(), lit_int(1)), END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_def_explicit_mismatch(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "x", t_int(), lit_double(1.5)), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_assign_to_val(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(def_st(false, "x", t_unresolved(), lit_int(1)),
                                 assign_st("x", lit_int(2)), END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_IMMUT);
}

static void test_check_assign_to_param(void **state) {
  // Parametry jsou implicitně neměnné
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("f", t_unit(), block_st(assign_st("a", lit_int(1)), END),
                   params(param("a", t_int()), END), END),
              main_func(block_st(END)), END),
      IFJ_SUCCESS, IFJ_ESEM_IMMUT);
}

static void test_check_assign_undefined_var(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(assign_st("x", lit_int(1)), END)), END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

static void test_check_undefined_var_in_expr(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(def_st(false, "x", t_unresolved(),
                                        bin(TOK_ADD, var("y"), lit_int(1))),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

static void test_check_use_before_def(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "y", t_unresolved(),
                              bin(TOK_ADD, var("x"), lit_int(1))),
                       def_st(false, "x", t_unresolved(), lit_int(1)), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

static void test_check_redefine_same_block(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "x", t_unresolved(), lit_int(1)),
                       def_st(false, "x", t_unresolved(), lit_int(2)), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_REDEF);
}

static void test_check_shadow_param_error(void **state) {
  // Tělo funkce sdílí tabulku symbolů s parametry, překrytí není povoleno
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_unit(),
                        block_st(def_st(false, "a", t_unresolved(), lit_int(1)),
                                 END),
                        params(param("a", t_int()), END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_ESEM_REDEF);
}

static void test_check_shadow_in_nested_block_ok(void **state) {
  // Vnořené bloky mají vlastní tabulku symbolů, překrytí je povoleno
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "x", t_unresolved(), lit_int(1)),
              while_st(lit_bool(true),
                       block_st(def_st(false, "x", t_unresolved(), lit_int(2)),
                                END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_var_out_of_scope_after_block(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              while_st(lit_bool(true),
                       block_st(def_st(false, "x", t_unresolved(), lit_int(1)),
                                END)),
              def_st(false, "y", t_unresolved(), var("x")), END)),
          END),
      IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

// ---------------------------------------------------------------------------
// Testy binárních a unárních operátorů
// ---------------------------------------------------------------------------

static void test_check_binary_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "a", t_unresolved(),
                     bin(TOK_ADD, lit_int(1), lit_int(2))),
              def_st(false, "b", t_unresolved(),
                     bin(TOK_MUL, lit_double(1.5), lit_int(2))),
              def_st(false, "c", t_unresolved(),
                     bin(TOK_DIV, lit_int(10), lit_int(2))),
              def_st(false, "d", t_unresolved(),
                     bin(TOK_LT, lit_int(1), lit_int(2))),
              def_st(false, "e", t_unresolved(),
                     bin(TOK_LEQ, lit_double(1.5), lit_int(2))),
              def_st(false, "f", t_unresolved(),
                     bin(TOK_AND, lit_bool(true), lit_bool(false))),
              def_st(false, "g", t_unresolved(), un(TOK_NOT, lit_bool(false))),
              def_st(false, "h", t_unresolved(), un(TOK_SUB, lit_int(5))),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);

  // Typy porovnání jsou Bool, aritmetika Int zůstává Int
  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands[3]->as.def.type.type, TYPE_BOOL);
  assert_int_equal(body->as.block.commands[0]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[1]->as.def.type.type, TYPE_DOUBLE);
}

static void test_check_binary_type_errors(void **state) {
  sem_state_t *s = get_state(state);

  // 1 + "a"
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "a", t_unresolved(),
                              bin(TOK_ADD, lit_int(1), lit_string("a"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // true + 1
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "a", t_unresolved(),
                              bin(TOK_ADD, lit_bool(true), lit_int(1))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // () + 1 (Unit v aritmetice)
  run_case(
      s,
      program(main_func(block_st(def_st(false, "a", t_unresolved(),
                                        bin(TOK_ADD, lit_unit(), lit_int(1))),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // 1 < "a"
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "a", t_unresolved(),
                              bin(TOK_LT, lit_int(1), lit_string("a"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // true && 1
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "a", t_unresolved(),
                              bin(TOK_AND, lit_bool(true), lit_int(1))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // "a" ++ "b" (operátor ++ je vyhrazen pro seznamy)
  run_case(s,
           program(main_func(block_st(def_st(false, "a", t_unresolved(),
                                             bin(TOK_CONCAT, lit_string("a"),
                                                 lit_string("b"))),
                                      END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Seznam v aritmetické operaci
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "l", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "a", t_unresolved(),
                              bin(TOK_ADD, var("l"), lit_int(1))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Testy podmínek if a cyklů while
// ---------------------------------------------------------------------------

static void test_check_if_expr_ok(void **state) {
  // if jako poslední příkaz bloku je výraz, musí mít větev else
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "c", t_unresolved(), lit_bool(true)),
                       if_st(var("c"), block_st(lit_int(1), END),
                             block_st(lit_int(2), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_if_expr_missing_else(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       if_st(lit_bool(true), block_st(END), NULL), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_OTHER);
}

static void test_check_if_expr_branch_mismatch(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(
                       block_st(if_st(lit_bool(true), block_st(lit_int(1), END),
                                      block_st(lit_string("a"), END)),
                                END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_if_cond_not_bool(void **state) {
  sem_state_t *s = get_state(state);

  // if jako výraz s ne-Bool podmínkou
  run_case(
      s,
      program(main_func(block_st(if_st(lit_int(1), block_st(lit_int(1), END),
                                       block_st(lit_int(2), END)),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // if jako příkaz s ne-Bool podmínkou
  run_case(s,
           program(main_func(block_st(
                       if_st(lit_int(1), block_st(END), NULL),
                       def_st(false, "x", t_unresolved(), lit_int(1)), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_if_statement_no_else_ok(void **state) {
  // if, které není posledním příkazem bloku, nemusí mít větev else
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  if_st(lit_bool(true),
                        block_st(def_st(false, "x", t_unresolved(), lit_int(1)),
                                 END),
                        NULL),
                  def_st(false, "y", t_unresolved(), lit_int(2)), END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_if_else_if_ok(void **state) {
  sem_state_t *s = get_state(state);
  ast_node_t *if_node = if_st(lit_bool(false), block_st(END), NULL);
  assert_int_equal(
      ast_if_add_else_if(if_node, else_if(lit_bool(true), block_st(END))),
      IFJ_SUCCESS);
  run_case(s,
           program(main_func(block_st(
                       if_node, def_st(false, "x", t_unresolved(), lit_int(1)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_while_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              while_st(lit_bool(true),
                       block_st(def_st(false, "x", t_unresolved(), lit_int(1)),
                                END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);
  reset_state(s);

  // Podmínka while musí být typu Bool
  run_case(
      s,
      program(main_func(block_st(while_st(lit_int(1), block_st(END)), END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Testy cyklů for
// ---------------------------------------------------------------------------

static void test_check_for_range_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  for_st(for_gen("i", lit_int(1),
                                 for_range_st(false, lit_int(10), NULL)),
                         block_st(def_st(false, "x", t_unresolved(), var("i")),
                                  END),
                         END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
  reset_state(s);

  // Výlučná horní mez (until) a krok by
  run_case(
      s,
      program(main_func(block_st(
                  for_st(for_gen("i", lit_int(1),
                                 for_range_st(true, lit_int(10), lit_int(2))),
                         block_st(END), END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_for_zero_step(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  for_st(for_gen("i", lit_int(1),
                                 for_range_st(false, lit_int(10), lit_int(0))),
                         block_st(END), END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_OTHER);
}

static void test_check_for_range_non_int(void **state) {
  sem_state_t *s = get_state(state);

  // Horní mez není Int
  run_case(
      s,
      program(main_func(block_st(
                  for_st(for_gen("i", lit_int(1),
                                 for_range_st(false, lit_string("a"), NULL)),
                         block_st(END), END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Krok není Int
  run_case(
      s,
      program(main_func(block_st(for_st(for_gen("i", lit_int(1),
                                                for_range_st(false, lit_int(10),
                                                             lit_double(1.5))),
                                        block_st(END), END),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Počáteční výraz generátoru není Int
  run_case(s,
           program(main_func(block_st(
                       for_st(for_gen("i", lit_double(1.5),
                                      for_range_st(false, lit_int(10), NULL)),
                              block_st(END), END),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_for_list_source_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List", args(lit_int(1), lit_int(2), END), END)),
                  for_st(for_gen("i", var("lst"), NULL),
                         block_st(def_st(false, "y", t_unresolved(), var("i")),
                                  END),
                         END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_for_list_with_range(void **state) {
  // Zdrojem generátoru je seznam, rozsah není povolen
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List", args(lit_int(1), lit_int(2), END), END)),
                  for_st(for_gen("i", var("lst"),
                                 for_range_st(false, lit_int(3), NULL)),
                         block_st(END), END),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_for_empty_list_source(void **state) {
  // Prázdný seznam s anotací typu je platným zdrojem cyklu for
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(def_st(false, "lst", t_list(TYPE_INT),
                                             call("List", args(END), END)),
                                      for_st(for_gen("i", var("lst"), NULL),
                                             block_st(END), END),
                                      END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_for_var_immutable(void **state) {
  // Řídící proměnná cyklu for je neměnná
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       for_st(for_gen("i", lit_int(1),
                                      for_range_st(false, lit_int(3), NULL)),
                              block_st(assign_st("i", lit_int(1)), END), END),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_IMMUT);
}

static void test_check_for_var_out_of_scope(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       for_st(for_gen("i", lit_int(1),
                                      for_range_st(false, lit_int(3), NULL)),
                              block_st(END), END),
                       def_st(false, "y", t_unresolved(), var("i")), END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

static void test_check_for_guard_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       for_st(for_gen("i", lit_int(1),
                                      for_range_st(false, lit_int(10), NULL)),
                              block_st(END),
                              guard(bin(TOK_GT, var("i"), lit_int(1))), END),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_for_guard_not_bool(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       for_st(for_gen("i", lit_int(1),
                                      for_range_st(false, lit_int(10), NULL)),
                              block_st(END),
                              guard(bin(TOK_ADD, var("i"), lit_int(1))), END),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Testy volání funkcí a rekurze
// ---------------------------------------------------------------------------

static void test_check_call_user_func_ok(void **state) {
  // Uživatelská funkce s dvěma seznamy parametrů
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("f", t_double(), block_st(lit_double(1.5), END),
                   params(param("a", t_int()), END),
                   params(param("b", t_string()), END), END),
              main_func(block_st(def_st(false, "x", t_unresolved(),
                                        call("f", args(lit_int(1), END),
                                             args(lit_string("a"), END), END)),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_call_wrong_paramlist_count(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("f", t_double(), block_st(lit_double(1.5), END),
                   params(param("a", t_int()), END),
                   params(param("b", t_string()), END), END),
              main_func(block_st(def_st(false, "x", t_unresolved(),
                                        call("f", args(lit_int(1), END), END)),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_ARGS);
}

static void test_check_call_undefined(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(
                       block_st(def_st(false, "x", t_unresolved(),
                                       call("foo", args(lit_int(1), END), END)),
                                END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
}

static void test_check_builtin_calls_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              call("print", args(lit_int(1), END), END),
              call("print", args(lit_string("a"), END), END),
              def_st(false, "n", t_unresolved(),
                     call("readInt", args(END), END)),
              def_st(false, "sl", t_unresolved(),
                     call("readLine", args(END), END)),
              def_st(false, "d", t_unresolved(),
                     call("readDouble", args(END), END)),
              def_st(false, "i", t_unresolved(),
                     call("toInt", args(lit_string("42"), END), END)),
              def_st(false, "l", t_unresolved(),
                     call("strLength", args(lit_string("abc"), END), END)),
              def_st(false, "sub", t_unresolved(),
                     call("subStr",
                          args(lit_string("abc"), lit_int(0), lit_int(1), END),
                          END)),
              def_st(false, "o", t_unresolved(),
                     call("ord", args(lit_string("a"), lit_int(0), END), END)),
              def_st(false, "c", t_unresolved(),
                     call("chr", args(lit_int(65), END), END)),
              def_st(false, "t", t_unresolved(),
                     call("toStr", args(lit_int(1), END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_builtin_wrong_arg_count(void **state) {
  sem_state_t *s = get_state(state);

  // print(1, 2)
  run_case(
      s,
      program(main_func(block_st(
                  call("print", args(lit_int(1), lit_int(2), END), END), END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // readInt(1)
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              call("readInt", args(lit_int(1), END), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // print(1)(2) - dva seznamy argumentů
  run_case(s,
           program(main_func(block_st(call("print", args(lit_int(1), END),
                                           args(lit_int(2), END), END),
                                      END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
}

static void test_check_builtin_wrong_arg_type(void **state) {
  sem_state_t *s = get_state(state);

  // toInt(1) - očekáván String
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              call("toInt", args(lit_int(1), END), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // subStr(1, 2, 3) - první parametr má být String
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "n", t_unresolved(),
                     call("subStr",
                          args(lit_int(1), lit_int(2), lit_int(3), END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_ESEM_ARGS);
}

static void test_check_call_implicit_int_to_double(void **state) {
  // Implicitní konverze Int -> Double při volání funkce
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("g", t_double(), block_st(var("x"), END),
                   params(param("x", t_double()), END), END),
              main_func(block_st(def_st(false, "y", t_unresolved(),
                                        call("g", args(lit_int(1), END), END)),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_call_before_def(void **state) {
  // Volání funkce definované později v souboru je v pořádku (tabulka
  // symbolů je naplněna před kontrolou těl); návratový typ volané funkce
  // se přitom odvodí rekurzivně
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(def_st(false, "x", t_unresolved(),
                                        call("f", args(lit_int(1), END), END)),
                                 END)),
              func("f", t_unresolved(), block_st(lit_int(1), END),
                   params(param("n", t_int()), END), END),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_recursion_explicit_ok(void **state) {
  // Přímá rekurze s explicitním návratovým typem je povolena
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("fact", t_int(),
                   block_st(if_st(bin(TOK_LEQ, var("n"), lit_int(1)),
                                  block_st(lit_int(1), END),
                                  block_st(bin(TOK_MUL, var("n"),
                                               call("fact",
                                                    args(bin(TOK_SUB, var("n"),
                                                             lit_int(1)),
                                                         END),
                                                    END)),
                                           END)),
                            END),
                   params(param("n", t_int()), END), END),
              main_func(block_st(END)), END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_recursion_untyped(void **state) {
  // Přímá rekurze bez explicitního návratového typu není povolena
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(func("fact", t_unresolved(),
                   block_st(if_st(bin(TOK_LEQ, var("n"), lit_int(1)),
                                  block_st(lit_int(1), END),
                                  block_st(bin(TOK_MUL, var("n"),
                                               call("fact",
                                                    args(bin(TOK_SUB, var("n"),
                                                             lit_int(1)),
                                                         END),
                                                    END)),
                                           END)),
                            END),
                   params(param("n", t_int()), END), END),
              main_func(block_st(END)), END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_indirect_recursion(void **state) {
  // Nepřímá rekurze bez explicitních návratových typů
  sem_state_t *s = get_state(state);
  run_case(s,
           program(func("f", t_unresolved(),
                        block_st(call("g", args(lit_int(1), END), END), END),
                        params(param("x", t_int()), END), END),
                   func("g", t_unresolved(),
                        block_st(call("f", args(lit_int(1), END), END), END),
                        params(param("x", t_int()), END), END),
                   main_func(block_st(END)), END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Testy seznamů
// ---------------------------------------------------------------------------

static void test_check_lists_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "lst", t_unresolved(),
                     call("List", args(lit_int(1), lit_int(2), lit_int(3), END),
                          END)),
              def_st(false, "d", t_unresolved(),
                     bin(TOK_CONCAT, var("lst"),
                         call("List", args(lit_int(4), END), END))),
              def_st(false, "e", t_list(TYPE_INT),
                     call("List", args(END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);

  // Odvozený typ seznamu se zapíše do AST uzlu definice
  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands[0]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[0]->as.def.type.is_list, true);
  assert_int_equal(body->as.block.commands[1]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[2]->as.def.type.type, TYPE_INT);
}

static void test_check_list_in_var(void **state) {
  // Seznam lze ukládat pouze do val
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       def_st(true, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_empty_list_without_annotation(void **state) {
  // Prázdný seznam vyžaduje explicitní typovou anotaci
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(def_st(false, "lst", t_unresolved(),
                                             call("List", args(END), END)),
                                      END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_list_mixed_types(void **state) {
  sem_state_t *s = get_state(state);

  // List(1, "a")
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "lst", t_unresolved(),
                     call("List", args(lit_int(1), lit_string("a"), END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Int a Double se sjednotí na Double
  run_case(
      s,
      program(
          main_func(block_st(
              def_st(false, "lst", t_unresolved(),
                     call("List", args(lit_int(1), lit_double(2.5), END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);
  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands[0]->as.def.type.type, TYPE_DOUBLE);
  assert_int_equal(body->as.block.commands[0]->as.def.type.is_list, true);
  reset_state(s);

  // Vnořený seznam smíchaný se skalárem
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List",
                              args(call("List", args(lit_int(1), END), END),
                                   lit_int(2), END),
                              END)),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_list_index_ok(void **state) {
  // Indexace seznamu lst(i)
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List", args(lit_int(1), lit_int(2), END), END)),
                  def_st(false, "x", t_unresolved(),
                         call("lst", args(lit_int(0), END), END)),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);

  // Identifikátor indexovaného seznamu se přepíše na code_gen_name
  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  ast_node_t *index_call = body->as.block.commands[1]->as.def.rhs;
  assert_int_equal(index_call->type, AST_FUNC_CALL);
  assert_string_equal(index_call->as.func_call.id, "lst$1");
  assert_int_equal(index_call->inferred_type.type, TYPE_INT);
  assert_int_equal(index_call->inferred_type.is_list, false);
}

static void test_check_list_index_errors(void **state) {
  sem_state_t *s = get_state(state);

  // Index není Int
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "x", t_unresolved(),
                              call("lst", args(lit_string("a"), END), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Dva indexy najednou
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "x", t_unresolved(),
                              call("lst", args(lit_int(0), END),
                                   args(lit_int(0), END), END)),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // Indexace proměnné, která není seznam
  run_case(
      s,
      program(main_func(block_st(def_st(false, "n", t_unresolved(), lit_int(1)),
                                 def_st(false, "x", t_unresolved(),
                                        call("n", args(lit_int(0), END), END)),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Testy metod volaných přes tečku
// ---------------------------------------------------------------------------

static void test_check_dot_methods_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(s,
           program(main_func(block_st(
                       // "abc".length -> Int
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, lit_string("abc"), var("length"))),
                       // 3.14.floor -> Double
                       def_st(false, "f", t_unresolved(),
                              bin(TOK_DOT, lit_double(3.14), var("floor"))),
                       // 3.14.toInt -> Int
                       def_st(false, "i", t_unresolved(),
                              bin(TOK_DOT, lit_double(3.14), var("toInt"))),
                       // 42.toString -> String
                       def_st(false, "str", t_unresolved(),
                              bin(TOK_DOT, lit_int(42), var("toString"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_SUCCESS);

  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands[0]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[1]->as.def.type.type, TYPE_DOUBLE);
  assert_int_equal(body->as.block.commands[2]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[3]->as.def.type.type, TYPE_STRING);
}

static void test_check_dot_methods_errors(void **state) {
  sem_state_t *s = get_state(state);

  // length na Int
  run_case(s,
           program(main_func(
                       block_st(def_st(false, "n", t_unresolved(),
                                       bin(TOK_DOT, lit_int(1), var("length"))),
                                END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Neznámá metoda
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, lit_string("a"), var("foo"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
  reset_state(s);

  // floor na Int
  run_case(
      s,
      program(main_func(block_st(def_st(false, "n", t_unresolved(),
                                        bin(TOK_DOT, lit_int(1), var("floor"))),
                                 END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

static void test_check_list_methods_ok(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List", args(lit_int(1), lit_int(2), END), END)),
                       // lst.head -> Int
                  def_st(false, "h", t_unresolved(),
                         bin(TOK_DOT, var("lst"), var("head"))),
                       // lst.tail -> List[Int]
                  def_st(false, "t", t_unresolved(),
                         bin(TOK_DOT, var("lst"), var("tail"))),
                       // lst.drop(1) -> List[Int]
                  def_st(false, "d", t_unresolved(),
                         bin(TOK_DOT, var("lst"),
                             call("drop", args(lit_int(1), END), END))),
                       // lst.take(1) -> List[Int]
                  def_st(false, "k", t_unresolved(),
                         bin(TOK_DOT, var("lst"),
                             call("take", args(lit_int(1), END), END))),
                       // lst.slice(0, 1) -> List[Int]
                  def_st(false, "sl", t_unresolved(),
                         bin(TOK_DOT, var("lst"),
                             call("slice", args(lit_int(0), lit_int(1), END),
                                  END))),
                       // List.range(1, 5) -> List[Int]
                  def_st(false, "r", t_unresolved(),
                         bin(TOK_DOT, var("List"),
                             call("range", args(lit_int(1), lit_int(5), END),
                                  END))),
                       // List.range(1, 5, 2) -> List[Int]
                  def_st(false, "r2", t_unresolved(),
                         bin(TOK_DOT, var("List"),
                             call("range",
                                  args(lit_int(1), lit_int(5), lit_int(2), END),
                                  END))),
                       // (1 to 3).toList -> List[Int]
                  def_st(false, "tl", t_unresolved(),
                         bin(TOK_DOT, bin(TOK_TO, lit_int(1), lit_int(3)),
                             var("toList"))),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_SUCCESS);

  ast_node_t *body = s->root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands[1]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[1]->as.def.type.is_list, false);
  assert_int_equal(body->as.block.commands[2]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[2]->as.def.type.is_list, true);
  assert_int_equal(body->as.block.commands[3]->as.def.type.type, TYPE_INT);
  assert_int_equal(body->as.block.commands[3]->as.def.type.is_list, true);
}

static void test_check_list_methods_errors(void **state) {
  sem_state_t *s = get_state(state);

  // Vlastnost length na seznamu
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("lst"), var("length"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Neznámá metoda s parametry
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("lst"),
                                  call("foo", args(lit_int(1), END), END))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
  reset_state(s);

  // Metoda volaná na konstruktoru List
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("List"),
                                  call("drop", args(lit_int(1), END), END))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // Parametr metody není Int
  run_case(
      s,
      program(main_func(block_st(
                  def_st(false, "lst", t_unresolved(),
                         call("List", args(lit_int(1), END), END)),
                  def_st(false, "n", t_unresolved(),
                         bin(TOK_DOT, var("lst"),
                             call("drop", args(lit_string("a"), END), END))),
                  END)),
              END),
      IFJ_SUCCESS, IFJ_ESEM_TYPE);
  reset_state(s);

  // slice potřebuje dva parametry
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("lst"),
                                  call("slice", args(lit_int(0), END), END))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // range potřebuje dva nebo tři parametry
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("List"),
                                  call("range", args(lit_int(1), END), END))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_ARGS);
  reset_state(s);

  // head volaný s parametry (vlastnost není funkce)
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "lst", t_unresolved(),
                              call("List", args(lit_int(1), END), END)),
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, var("lst"),
                                  call("head", args(lit_int(1), END), END))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_UNDEF);
  reset_state(s);

  // head na ne-seznamu
  run_case(s,
           program(main_func(block_st(
                       def_st(false, "n", t_unresolved(),
                              bin(TOK_DOT, lit_string("abc"), var("head"))),
                       END)),
                   END),
           IFJ_SUCCESS, IFJ_ESEM_TYPE);
}

// ---------------------------------------------------------------------------
// Integrační a další testy
// ---------------------------------------------------------------------------

static void test_check_full_program(void **state) {
  sem_state_t *s = get_state(state);
  run_case(
      s,
      program(
          func("fib", t_int(),
               block_st(def_st(true, "a", t_unresolved(), lit_int(0)),
                        def_st(true, "b", t_unresolved(), lit_int(1)),
                        for_st(for_gen("i", lit_int(0),
                                       for_range_st(false, var("n"), NULL)),
                               block_st(assign_st("a", var("b")),
                                        assign_st("b", bin(TOK_ADD, var("a"),
                                                           var("b"))),
                                        END),
                               END),
                        return_st(var("a")), END),
               params(param("n", t_int()), END), END),
          main_func(block_st(
              def_st(false, "lst", t_unresolved(),
                     call("List", args(lit_int(1), lit_int(2), lit_int(3), END),
                          END)),
              def_st(false, "rest", t_unresolved(),
                     bin(TOK_DOT, var("lst"),
                         call("drop", args(lit_int(1), END), END))),
              def_st(false, "n", t_unresolved(),
                     call("readInt", args(END), END)),
              def_st(false, "text", t_unresolved(),
                     call("subStr",
                          args(lit_string("hello"), lit_int(0), var("n"), END),
                          END)),
              call("print", args(var("text"), END), END),
              call("print", args(call("fib", args(lit_int(5), END), END), END),
                   END),
              for_st(
                  for_gen("x", var("lst"), NULL),
                  block_st(def_st(false, "y", t_unresolved(), var("x")), END),
                  END),
              if_st(bin(TOK_LT, var("n"), lit_int(0)),
                    block_st(call("print", args(lit_int(0), END), END), END),
                    block_st(call("print", args(lit_int(1), END), END), END)),
              END)),
          END),
      IFJ_SUCCESS, IFJ_SUCCESS);
}

static void test_check_idempotent(void **state) {
  // Opakovaná kontrola téhož programu je v pořádku (funkce označené jako
  // zkontrolované se přeskočí)
  sem_state_t *s = get_state(state);
  set_root(s, program(main_func(block_st(END)), END));
  expect_fill(s, IFJ_SUCCESS);
  check_result(s, IFJ_SUCCESS);
  check_result(s, IFJ_SUCCESS);
}

// ---------------------------------------------------------------------------
// Spuštění testů
// ---------------------------------------------------------------------------

int main(void) {
  const struct CMUnitTest tests[] = {
      // symtable_fill_funcitons
      cmocka_unit_test(test_fill_null_args),
      cmocka_unit_test_setup_teardown(test_fill_wrong_root_type, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_empty_program, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_main_only, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_main_bad_return_type, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_main_with_params, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_main_two_paramlists, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_two_mains, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_missing_main, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_redef_builtin, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_duplicate_func, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_duplicate_param_in_list,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_duplicate_param_across_lists,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_user_func_data, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_fill_is_not_idempotent, sem_setup,
                                      sem_teardown),

      // check_functions - základní případy
      cmocka_unit_test_setup_teardown(test_check_null_args, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_wrong_root_type, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_main_minimal, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_infer_return_type, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_return_without_explicit_type,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_return_explicit_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_return_type_mismatch,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_bare_return_non_unit,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_return_value_in_unit,
                                      sem_setup, sem_teardown),

      // Proměnné a přiřazení
      cmocka_unit_test_setup_teardown(test_check_def_assign_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_def_explicit_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_def_explicit_mismatch,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_assign_to_val, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_assign_to_param, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_assign_undefined_var,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_undefined_var_in_expr,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_use_before_def, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_redefine_same_block, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_shadow_param_error, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_shadow_in_nested_block_ok,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_var_out_of_scope_after_block,
                                      sem_setup, sem_teardown),

      // Operátory
      cmocka_unit_test_setup_teardown(test_check_binary_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_binary_type_errors, sem_setup,
                                      sem_teardown),

      // Podmínky if a cykly while
      cmocka_unit_test_setup_teardown(test_check_if_expr_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_if_expr_missing_else,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_if_expr_branch_mismatch,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_if_cond_not_bool, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_if_statement_no_else_ok,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_if_else_if_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_while_ok, sem_setup,
                                      sem_teardown),

      // Cykly for
      cmocka_unit_test_setup_teardown(test_check_for_range_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_zero_step, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_range_non_int, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_list_source_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_list_with_range, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_empty_list_source,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_var_immutable, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_var_out_of_scope,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_guard_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_for_guard_not_bool, sem_setup,
                                      sem_teardown),

      // Volání funkcí a rekurze
      cmocka_unit_test_setup_teardown(test_check_call_user_func_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_call_wrong_paramlist_count,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_call_undefined, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_builtin_calls_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_builtin_wrong_arg_count,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_builtin_wrong_arg_type,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_call_implicit_int_to_double,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_call_before_def, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_recursion_explicit_ok,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_recursion_untyped, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_indirect_recursion, sem_setup,
                                      sem_teardown),

      // Seznamy
      cmocka_unit_test_setup_teardown(test_check_lists_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_in_var, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_empty_list_without_annotation,
                                      sem_setup, sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_mixed_types, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_index_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_index_errors, sem_setup,
                                      sem_teardown),

      // Metody volané přes tečku
      cmocka_unit_test_setup_teardown(test_check_dot_methods_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_dot_methods_errors, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_methods_ok, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_list_methods_errors, sem_setup,
                                      sem_teardown),

      // Integrační a další testy
      cmocka_unit_test_setup_teardown(test_check_full_program, sem_setup,
                                      sem_teardown),
      cmocka_unit_test_setup_teardown(test_check_idempotent, sem_setup,
                                      sem_teardown),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
