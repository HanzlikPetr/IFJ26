/**
 * @file test_symtable.c
 * @brief IFJ26 - Unit testy pro tabulku symbolů (cmocka)
 *
 * @author xhanzlp00 Petr Hanzlík
 * Tests (v3) created from the following prompt (model: GLM-5.3 via opencode):
 *
 * "add test for traversal and add to test also type LIST to cover it"
 *
 * --------------------------------------------------------------------------
 * Tests (v2) created from the following prompt (model: GLM-5.3 via opencode):
 *
 * "please update tests for symtable.c to reflect the changes made in the
 * symtable.c file. Now i need to test new structure data, sometimes use
 * variable and sometimes function, also test create paramlist with 0 or more
 * paramlist, also check that insert create deep copy of data and that find
 * value of a deep copy of data, also check that destroy frees all memory
 * allocated for data, including paramlist and types."
 *
 * --------------------------------------------------------------------------
 * Tests (v1) created from the following prompt (model: GLM-5.3 via opencode):
 *
 * "Please create unit tests for the symtable interface in C using the cmocka
 * framework. The file will be placed in tests/utest/ and named
 * test_symtable.c. Include setup_test.h (for cmocka includes), symtable.h,
 * and error.h and also write to the comment this prompt and model you are.
 *
 * Setup: init
 * Initialize the symtable using symtable_init. Check that it returns
 * IFJ_SUCCESS and assert that the allocated table pointer is not NULL.
 * Teardown: destroy
 * Destroy the symtable using symtable_destroy. Check that it returns
 * IFJ_SUCCESS and assert that the table pointer is set to NULL afterwards.
 * Test Case 1: Insert and Find
 * Insert keys ["a", "b", "c"] with values [1, 2, 3]. Always check that the
 * insert function returns IFJ_SUCCESS. After inserting all of them, try to
 * find key "b" and check if the return code is IFJ_SUCCESS and the retrieved
 * value is 2.
 * Test Case 2: Duplicated Insert
 * Insert key "b" with value 1, check for IFJ_SUCCESS. Then insert key "b"
 * again with value 2. The second insert must fail and return IFJ_ESEM_REDEF.
 * Test Case 3: Item Didn't Exist
 * Insert key "b" with value 1 and check for IFJ_SUCCESS. Try to find key "a".
 * The find function must return IFJ_ESEM_UNDEF.
 * Test Case 4: Rotations and Complex Use (Black-box verification)
 * Insert specific sequences of keys to trigger AVL rotations implicitly,
 * checking for IFJ_SUCCESS after every insert:
 * LL rotation trigger: Insert keys ["g", "e", "c"] with values [1, 2, 3].
 * LR rotation trigger: Insert keys ["k", "i", "j"] with values [4, 5, 6].
 * RR rotation trigger: Insert keys ["m", "o", "q"] with values [7, 8, 9].
 * RL rotation trigger: Insert keys ["u", "w", "v"] with values [10, 11, 12].
 * verify the tree's integrity after all these rotations by calling
 * symtable_find on all inserted keys. Assert that all are found successfully
 * (IFJ_SUCCESS) and that their values match the inserted ones (e.g., finding
 * "e" returns 2).
 * Test Case 5: NULL Pointers
 * Try calling all API functions (symtable_init, symtable_insert,
 * symtable_find, symtable_destroy) with NULL where a valid pointer is
 * expected. Assert that they safely return IFJ_EINTERNAL."
 */

#define _POSIX_C_SOURCE 200809L // Kvuli strdup

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "setup_test.h"
#include "symtable.h"

/** Pocet klicu, ktere muze traversal test navstivit. */
#define TRAVERSE_MAX_KEYS 32

/** Maximalni delka jednoho klice v traversal testu. */
#define TRAVERSE_KEY_LEN 24

/** Pocet klicu ve stresovacim testu. */
#define STRESS_KEYS 127

/** Stride pro zamichani poradi vkladani (37 je nesoudelne se 127). */
#define STRESS_STRIDE 37

/**
 * @brief Stav testu sdileny setup/teardown funkci.
 */
typedef struct {
  symtable_t table; /**< Testovaci tabulka symbolu */
} test_state_t;

/**
 * @brief Stav pro callback funkce traversal testu.
 */
typedef struct {
  char visited[TRAVERSE_MAX_KEYS][TRAVERSE_KEY_LEN]; /**< Navstivene klice */
  size_t count;                                       /**< Pocet navstiveni */
  const char *fail_on;  /**< Klic, na kterem callback selze (NULL = nikdy) */
  IFJ_ERR fail_err;     /**< Chyba vracena pri selhani */
} traverse_state_t;

/**
 * @brief Stav pro test integrity dat predavanych callbacku.
 */
typedef struct {
  bool saw_var;  /**< Byl navstiven testovaci var? */
  bool saw_func; /**< Byla navstivena testovaci funkce? */
} traverse_seen_t;

/**
 * @brief Setup funkce pro testy - inicializuje tabulku symbolu.
 */
static int test_setup(void **state) {
  test_state_t *ts = malloc(sizeof(test_state_t));
  assert_non_null(ts);

  IFJ_ERR err = symtable_init(&ts->table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(ts->table.root);

  *state = ts;
  return 0;
}

/**
 * @brief Teardown funkce pro testy - uvolni tabulku symbolu.
 */
static int test_teardown(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  IFJ_ERR err = symtable_destroy(&ts->table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(ts->table.root);

  free(ts);
  return 0;
}

/**
 * @brief Deterministicky typ pro t-tou polozku pole typu parametru.
 *
 * @param t Index typu v seznamu parametru.
 * @return Typ z cyklu INT/DOUBLE/STRING se stridavym priznakem is_list.
 */
static ifj26_type_t expected_type(size_t t) {
  ifj26_type_t type = {(data_type_t)(t % 3), (t % 2) == 0};
  return type;
}

/**
 * @brief Vytvori data promenne pro vlozeni (kopii uvolni volajici
 * free_var_data).
 *
 * @param base Zakladni typ promenne.
 * @param is_list true, pokud je promenna seznam (List[T]).
 * @param is_mutable true, pokud je promenna menitelna (var).
 * @param code_gen_name Nazev pro generovani kodu (muze byt NULL).
 * @return Vyplnena data promenne.
 */
static symtable_data_t make_var_data(data_type_t base, bool is_list,
                                     bool is_mutable,
                                     const char *code_gen_name) {
  symtable_data_t data = {0};

  data.is_function = false;
  data.var.type.type = base;
  data.var.type.is_list = is_list;
  data.var.is_mutable = is_mutable;
  if (code_gen_name != NULL) {
    data.var.code_gen_name = strdup(code_gen_name);
    assert_non_null(data.var.code_gen_name);
  } else {
    data.var.code_gen_name = NULL;
  }

  return data;
}

/**
 * @brief Uvolni data promenne vytvorena make_var_data.
 *
 * @param data Data promenne k uvolneni.
 */
static void free_var_data(symtable_data_t *data) {
  free(data->var.code_gen_name);
  data->var.code_gen_name = NULL;
}

/**
 * @brief Vytvori data funkce pro vlozeni (kopii uvolni volajici
 * free_func_data).
 *
 * Navigratovy typ je vzdy List[Double], typy parametru jsou vyplneny
 * expected_type().
 *
 * @param param_list_count Pocet seznamu parametru.
 * @param list_counts Pole poctu typu v jednotlivych seznamech.
 * @return Vyplnena data funkce.
 */
static symtable_data_t make_func_data(int param_list_count,
                                      const int *list_counts) {
  symtable_data_t data = {0};

  data.is_function = true;
  data.func.return_type.type = TYPE_DOUBLE;
  data.func.return_type.is_list = true;
  data.func.return_type_explicit = true;
  data.func.is_resolving = false;
  data.func.is_main = false;
  data.func.param_list_count = param_list_count;

  if (param_list_count > 0) {
    data.func.params = malloc(sizeof(param_list_t) * (size_t)param_list_count);
    assert_non_null(data.func.params);
    for (int i = 0; i < param_list_count; i++) {
      data.func.params[i].count = list_counts[i];
      if (list_counts[i] > 0) {
        data.func.params[i].types =
            malloc(sizeof(ifj26_type_t) * (size_t)list_counts[i]);
        assert_non_null(data.func.params[i].types);
        for (int t = 0; t < list_counts[i]; t++) {
          data.func.params[i].types[t] = expected_type((size_t)t);
        }
      } else {
        data.func.params[i].types = NULL;
      }
    }
  } else {
    data.func.params = NULL;
  }

  return data;
}

/**
 * @brief Uvolni data funkce vytvorena make_func_data.
 *
 * @param data Data funkce k uvolneni.
 */
static void free_func_data(symtable_data_t *data) {
  if (data->func.params != NULL) {
    for (int i = 0; i < data->func.param_list_count; i++) {
      free(data->func.params[i].types);
    }
    free(data->func.params);
    data->func.params = NULL;
  }
}

/**
 * @brief Overi nalezenna data funkce proti make_func_data.
 *
 * @param found Nalezenna data funkce.
 * @param param_list_count Ocekavany pocet seznamu parametru.
 * @param list_counts Ocekavane pocty typu v seznamech parametru.
 */
static void verify_func_data(const symtable_data_t *found, int param_list_count,
                             const int *list_counts) {
  assert_true(found->is_function);
  assert_int_equal(found->func.return_type.type, TYPE_DOUBLE);
  assert_true(found->func.return_type.is_list);
  assert_true(found->func.return_type_explicit);
  assert_false(found->func.is_resolving);
  assert_false(found->func.is_main);
  assert_int_equal(found->func.param_list_count, param_list_count);

  if (param_list_count == 0) {
    assert_null(found->func.params);
    return;
  }

  assert_non_null(found->func.params);
  for (int i = 0; i < param_list_count; i++) {
    assert_int_equal(found->func.params[i].count, list_counts[i]);
    if (list_counts[i] == 0) {
      assert_null(found->func.params[i].types);
      continue;
    }
    assert_non_null(found->func.params[i].types);
    for (int t = 0; t < list_counts[i]; t++) {
      ifj26_type_t exp = expected_type((size_t)t);
      assert_int_equal(found->func.params[i].types[t].type, exp.type);
      assert_true(found->func.params[i].types[t].is_list == exp.is_list);
    }
  }
}

/**
 * @brief Vlozi promennou do tabulky a overi uspech.
 *
 * @param table Tabulka symbolu.
 * @param key Klic promenne.
 * @param base Zakladni typ.
 * @param is_list Priznak seznamu.
 * @param is_mutable Priznak menitelnosti.
 */
static void insert_var(symtable_t *table, const char *key, data_type_t base,
                       bool is_list, bool is_mutable) {
  symtable_data_t data = make_var_data(base, is_list, is_mutable, key);
  IFJ_ERR err = symtable_insert(table, key, &data);
  free_var_data(&data);
  assert_int_equal(err, IFJ_SUCCESS);
}

/**
 * @brief Vlozi funkci do tabulky a overi uspech.
 *
 * @param table Tabulka symbolu.
 * @param key Klic funkce.
 * @param param_list_count Pocet seznamu parametru.
 * @param list_counts Pocty typu v seznamech parametru.
 */
static void insert_func(symtable_t *table, const char *key,
                        int param_list_count, const int *list_counts) {
  symtable_data_t data = make_func_data(param_list_count, list_counts);
  IFJ_ERR err = symtable_insert(table, key, &data);
  free_func_data(&data);
  assert_int_equal(err, IFJ_SUCCESS);
}

/**
 * @brief Nalezne klic v tabulce a overi uspech.
 *
 * @param table Tabulka symbolu.
 * @param key Klic hledaneho prvku.
 * @return Ukazatel na nalezenna data (neni NULL).
 */
static symtable_data_t *find_key(symtable_t *table, const char *key) {
  symtable_data_t *found = NULL;
  IFJ_ERR err = symtable_find(table, key, &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  return found;
}

/**
 * @brief Callback pro traversal testy - sbira navstivene klice.
 *
 * Pri shode klice s fail_on vraci fail_err (simulace chyby callbacku).
 */
static IFJ_ERR cb_collect(const char *key, const symtable_data_t *value,
                          void *user_data) {
  traverse_state_t *state = (traverse_state_t *)user_data;
  (void)value;

  if (state->fail_on != NULL && strcmp(key, state->fail_on) == 0) {
    return state->fail_err;
  }

  if (state->count < TRAVERSE_MAX_KEYS) {
    strcpy(state->visited[state->count], key);
  }
  state->count++;

  return IFJ_SUCCESS;
}

/**
 * @brief Callback pro test integrity dat predavanych traversal callbacku.
 */
static IFJ_ERR cb_check_data(const char *key, const symtable_data_t *value,
                             void *user_data) {
  traverse_seen_t *seen = (traverse_seen_t *)user_data;
  static const int list_counts[] = {2, 1};

  if (strcmp(key, "lst") == 0) {
    assert_false(value->is_function);
    assert_int_equal(value->var.type.type, TYPE_INT);
    assert_true(value->var.type.is_list);
    assert_false(value->var.is_mutable);
    assert_non_null(value->var.code_gen_name);
    assert_string_equal(value->var.code_gen_name, "LF@lst");
    seen->saw_var = true;
  } else if (strcmp(key, "fn") == 0) {
    verify_func_data(value, 2, list_counts);
    seen->saw_func = true;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Test vlozeni a vyhledani promenne.
 */
static void test_insert_find_var(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "x", TYPE_INT, false, true);
  insert_var(&ts->table, "y", TYPE_DOUBLE, true, false);

  symtable_data_t *found = find_key(&ts->table, "x");
  assert_false(found->is_function);
  assert_int_equal(found->var.type.type, TYPE_INT);
  assert_false(found->var.type.is_list);
  assert_true(found->var.is_mutable);
  assert_string_equal(found->var.code_gen_name, "x");

  found = find_key(&ts->table, "y");
  assert_false(found->is_function);
  assert_int_equal(found->var.type.type, TYPE_DOUBLE);
  assert_true(found->var.type.is_list);
  assert_false(found->var.is_mutable);
  assert_string_equal(found->var.code_gen_name, "y");
}

/**
 * @brief Test vlozeni a vyhledani promenne s NULL code_gen_name.
 */
static void test_insert_find_var_null_codegen(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  symtable_data_t data = make_var_data(TYPE_STRING, false, true, NULL);
  IFJ_ERR err = symtable_insert(&ts->table, "s", &data);
  free_var_data(&data);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t *found = find_key(&ts->table, "s");
  assert_int_equal(found->var.type.type, TYPE_STRING);
  assert_null(found->var.code_gen_name);
}

/**
 * @brief Test vlozeni a vyhledani funkce se seznamy parametru.
 */
static void test_insert_find_func(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {2, 0, 3};

  insert_func(&ts->table, "fn", 3, list_counts);

  symtable_data_t *found = find_key(&ts->table, "fn");
  verify_func_data(found, 3, list_counts);
}

/**
 * @brief Test ruznych variant seznamu parametru funkce.
 */
static void test_param_list_variants(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int empty_lists[] = {0};
  const int single[] = {5};
  const int mixed[] = {0, 2, 0, 1};

  insert_func(&ts->table, "no_lists", 0, NULL);
  insert_func(&ts->table, "one_empty", 1, empty_lists);
  insert_func(&ts->table, "one_full", 1, single);
  insert_func(&ts->table, "mixed", 4, mixed);

  verify_func_data(find_key(&ts->table, "no_lists"), 0, NULL);
  verify_func_data(find_key(&ts->table, "one_empty"), 1, empty_lists);
  verify_func_data(find_key(&ts->table, "one_full"), 1, single);
  verify_func_data(find_key(&ts->table, "mixed"), 4, mixed);
}

/**
 * @brief Test hluboke kopie - tabulka nevlastni data volajiciho.
 */
static void test_deep_copy_semantics(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {2, 0};

  symtable_data_t data = make_func_data(2, list_counts);
  param_list_t *caller_params = data.func.params;
  ifj26_type_t *caller_types = data.func.params[0].types;

  IFJ_ERR err = symtable_insert(&ts->table, "fn", &data);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&data);

  symtable_data_t *found = find_key(&ts->table, "fn");
  verify_func_data(found, 2, list_counts);
  assert_true(found->func.params != caller_params);
  assert_true(found->func.params[0].types != caller_types);

  symtable_data_t vdata = make_var_data(TYPE_STRING, true, false, "LF@s");
  char *caller_cgn = vdata.var.code_gen_name;
  err = symtable_insert(&ts->table, "s", &vdata);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&vdata);

  found = find_key(&ts->table, "s");
  assert_string_equal(found->var.code_gen_name, "LF@s");
  assert_true(found->var.code_gen_name != caller_cgn);
}

/**
 * @brief Test ze find vraci zapisovatelny ukazatel do tabulky.
 */
static void test_find_mutable_pointer(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "x", TYPE_UNRESOLVED, false, true);
  insert_func(&ts->table, "g", 0, NULL);

  symtable_data_t *p1 = find_key(&ts->table, "x");
  p1->var.type.type = TYPE_BOOL; // Odvozeni typu po analyzе
  p1->var.is_mutable = false;

  symtable_data_t *p2 = find_key(&ts->table, "x");
  assert_true(p1 == p2);
  assert_int_equal(p2->var.type.type, TYPE_BOOL);
  assert_false(p2->var.is_mutable);

  // Simulace priznaku reseni rekurze pri odvozovani typu
  symtable_data_t *f1 = find_key(&ts->table, "g");
  f1->func.is_resolving = true;
  symtable_data_t *f2 = find_key(&ts->table, "g");
  assert_true(f1 == f2);
  assert_true(f2->func.is_resolving);
}

/**
 * @brief Test neplatnych dat funkce - param_list_count > 0 a params NULL.
 */
static void test_invalid_func_data(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  symtable_data_t data = {0};
  data.is_function = true;
  data.func.return_type.type = TYPE_UNIT;
  data.func.param_list_count = 2;
  data.func.params = NULL;

  IFJ_ERR err = symtable_insert(&ts->table, "bad", &data);
  assert_int_equal(err, IFJ_EINTERNAL);

  // Tabulka zustava pouzitelna
  insert_var(&ts->table, "ok", TYPE_INT, false, true);
  symtable_data_t *found = NULL;
  err = symtable_find(&ts->table, "bad", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
  assert_null(found);
  assert_non_null(find_key(&ts->table, "ok"));
}

/**
 * @brief Test opakovaneho vlozeni stejneho klice.
 */
static void test_duplicated_insert(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "b", TYPE_INT, false, true);

  symtable_data_t data = make_var_data(TYPE_DOUBLE, true, false, "b");
  IFJ_ERR err = symtable_insert(&ts->table, "b", &data);
  free_var_data(&data);
  assert_int_equal(err, IFJ_ESEM_REDEF);

  // Puvodni data zustavaji nedotcena
  symtable_data_t *found = find_key(&ts->table, "b");
  assert_int_equal(found->var.type.type, TYPE_INT);
  assert_false(found->var.type.is_list);
}

/**
 * @brief Test vyhledani neexistujiciho klice (prazdna tabulka i po vlozeni).
 */
static void test_find_nonexistent(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = (symtable_data_t *)0x1;

  IFJ_ERR err = symtable_find(&ts->table, "missing", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
  assert_null(found);

  insert_var(&ts->table, "b", TYPE_INT, false, true);
  err = symtable_find(&ts->table, "a", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
  assert_null(found);
}

/**
 * @brief Test LL rotace (vlozeni g, e, c).
 */
static void test_rotations_ll(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "g", TYPE_INT, false, true);
  insert_var(&ts->table, "e", TYPE_DOUBLE, true, false);
  insert_var(&ts->table, "c", TYPE_STRING, false, false);

  assert_int_equal(find_key(&ts->table, "g")->var.type.type, TYPE_INT);
  assert_int_equal(find_key(&ts->table, "e")->var.type.type, TYPE_DOUBLE);
  assert_true(find_key(&ts->table, "e")->var.type.is_list);
  assert_int_equal(find_key(&ts->table, "c")->var.type.type, TYPE_STRING);
}

/**
 * @brief Test LR rotace (vlozeni k, i, j).
 */
static void test_rotations_lr(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {1};

  insert_var(&ts->table, "k", TYPE_INT, false, true);
  insert_var(&ts->table, "i", TYPE_DOUBLE, false, false);
  insert_func(&ts->table, "j", 1, list_counts);

  assert_int_equal(find_key(&ts->table, "k")->var.type.type, TYPE_INT);
  assert_int_equal(find_key(&ts->table, "i")->var.type.type, TYPE_DOUBLE);
  verify_func_data(find_key(&ts->table, "j"), 1, list_counts);
}

/**
 * @brief Test RR rotace (vlozeni m, o, q).
 */
static void test_rotations_rr(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "m", TYPE_INT, false, true);
  insert_var(&ts->table, "o", TYPE_UNIT, true, true);
  insert_var(&ts->table, "q", TYPE_BOOL, false, false);

  assert_int_equal(find_key(&ts->table, "m")->var.type.type, TYPE_INT);
  assert_true(find_key(&ts->table, "o")->var.type.is_list);
  assert_int_equal(find_key(&ts->table, "q")->var.type.type, TYPE_BOOL);
}

/**
 * @brief Test RL rotace (vlozeni u, w, v).
 */
static void test_rotations_rl(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {0, 2};

  insert_var(&ts->table, "u", TYPE_INT, false, true);
  insert_var(&ts->table, "w", TYPE_DOUBLE, false, false);
  insert_func(&ts->table, "v", 2, list_counts);

  assert_int_equal(find_key(&ts->table, "u")->var.type.type, TYPE_INT);
  assert_int_equal(find_key(&ts->table, "w")->var.type.type, TYPE_DOUBLE);
  verify_func_data(find_key(&ts->table, "v"), 2, list_counts);
}

/**
 * @brief Test traverse - serazene in-order poradi, kazdy klic prave jednou.
 */
static void test_traverse_sorted_order(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const char *keys[] = {"m", "a", "t", "b", "l", "c", "z", "d"};
  const char *sorted[] = {"a", "b", "c", "d", "l", "m", "t", "z"};
  const int list_counts[] = {1};

  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    if (i % 2 == 0) {
      insert_var(&ts->table, keys[i], (data_type_t)(i % 3), i % 2 == 0, true);
    } else {
      insert_func(&ts->table, keys[i], 1, list_counts);
    }
  }

  traverse_state_t trav = {0};
  IFJ_ERR err = symtable_traverse(&ts->table, cb_collect, &trav);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_int_equal(trav.count, sizeof(keys) / sizeof(keys[0]));

  for (size_t i = 0; i < trav.count; i++) {
    assert_string_equal(trav.visited[i], sorted[i]);
  }
}

/**
 * @brief Test traverse nad prazdnou tabulkou.
 */
static void test_traverse_empty_table(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  traverse_state_t trav = {0};

  IFJ_ERR err = symtable_traverse(&ts->table, cb_collect, &trav);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_int_equal(trav.count, 0);
}

/**
 * @brief Test traverse s NULL callbackem a NULL tabulkou.
 */
static void test_traverse_null_callback(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  traverse_state_t trav = {0};

  IFJ_ERR err = symtable_traverse(&ts->table, NULL, &trav);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_traverse(NULL, cb_collect, &trav);
  assert_int_equal(err, IFJ_EINTERNAL);
}

/**
 * @brief Test ze chyba callbacku se propaguje a zastavi prochazeni.
 */
static void test_traverse_error_propagates(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const char *keys[] = {"e", "a", "d", "b", "c"};

  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    insert_var(&ts->table, keys[i], TYPE_INT, false, true);
  }

  traverse_state_t trav = {0};
  trav.fail_on = "c";
  trav.fail_err = IFJ_ESEM_ARGS;

  IFJ_ERR err = symtable_traverse(&ts->table, cb_collect, &trav);
  assert_int_equal(err, IFJ_ESEM_ARGS);

  // Prochazeni zastaveno na "c" - navstiveny jsou pouze "a" a "b"
  assert_int_equal(trav.count, 2);
  assert_string_equal(trav.visited[0], "a");
  assert_string_equal(trav.visited[1], "b");
}

/**
 * @brief Test integrity dat predavanych callbacku traverse.
 */
static void test_traverse_data_integrity(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {2, 1};
  traverse_seen_t seen = {false, false};

  symtable_data_t data =
      make_var_data(TYPE_INT, true, false, "LF@lst"); // List[Int]
  IFJ_ERR err = symtable_insert(&ts->table, "lst", &data);
  free_var_data(&data);
  assert_int_equal(err, IFJ_SUCCESS);

  insert_func(&ts->table, "fn", 2, list_counts);

  err = symtable_traverse(&ts->table, cb_check_data, &seen);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_true(seen.saw_var);
  assert_true(seen.saw_func);
}

/**
 * @brief Test seznamovych typu - List[Int] a obycejna promenna.
 */
static void test_list_var_roundtrip(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  insert_var(&ts->table, "l", TYPE_INT, true, false);   // List[Int]
  insert_var(&ts->table, "s", TYPE_STRING, false, true);

  symtable_data_t *found = find_key(&ts->table, "l");
  assert_int_equal(found->var.type.type, TYPE_INT);
  assert_true(found->var.type.is_list);
  assert_false(found->var.is_mutable);

  found = find_key(&ts->table, "s");
  assert_int_equal(found->var.type.type, TYPE_STRING);
  assert_false(found->var.type.is_list);
  assert_true(found->var.is_mutable);
}

/**
 * @brief Stresovy test - 127 klicu v zamichanem poradi, data odvozena z j.
 */
static void test_stress_127_keys(void **state) {
  (void)state;
  symtable_t table;
  char key[8];
  IFJ_ERR err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);

  // Vkladani v poradi dane stride permutaci, data odvozena od finalniho
  // indexu klice j (nikoli od poradi vkladani k)
  for (int k = 0; k < STRESS_KEYS; k++) {
    int j = (k * STRESS_STRIDE) % STRESS_KEYS;
    snprintf(key, sizeof(key), "k%03d", j);

    if (j % 2 == 0) {
      symtable_data_t data =
          make_var_data((data_type_t)(j % 3), (j % 4) >= 2, (j % 3) != 0,
                        (j % 5 == 0) ? NULL : key);
      err = symtable_insert(&table, key, &data);
      free_var_data(&data);
    } else {
      int list_counts[2] = {j % 3, j % 2};
      symtable_data_t data = make_func_data(2, list_counts);
      err = symtable_insert(&table, key, &data);
      free_func_data(&data);
    }
    assert_int_equal(err, IFJ_SUCCESS);
  }

  // Overeni vsech klicu a dat odvozenych od finalniho indexu j
  for (int j = 0; j < STRESS_KEYS; j++) {
    snprintf(key, sizeof(key), "k%03d", j);
    symtable_data_t *found = find_key(&table, key);

    if (j % 2 == 0) {
      assert_false(found->is_function);
      assert_int_equal(found->var.type.type, (data_type_t)(j % 3));
      assert_true(found->var.type.is_list == ((j % 4) >= 2));
      assert_true(found->var.is_mutable == ((j % 3) != 0));
      if (j % 5 == 0) {
        assert_null(found->var.code_gen_name);
      } else {
        assert_string_equal(found->var.code_gen_name, key);
      }
    } else {
      int list_counts[2] = {j % 3, j % 2};
      verify_func_data(found, 2, list_counts);
    }
  }

  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
}

/**
 * @brief Test NULL pointeru u vsech funkci API.
 */
static void test_null_pointers(void **state) {
  (void)state;
  symtable_t table;
  symtable_data_t *found = NULL;
  traverse_state_t trav = {0};

  assert_int_equal(symtable_init(NULL), IFJ_EINTERNAL);
  assert_int_equal(symtable_destroy(NULL), IFJ_EINTERNAL);

  IFJ_ERR err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t data = make_var_data(TYPE_INT, false, true, "x");
  assert_int_equal(symtable_insert(NULL, "k", &data), IFJ_EINTERNAL);
  assert_int_equal(symtable_insert(&table, NULL, &data), IFJ_EINTERNAL);
  assert_int_equal(symtable_insert(&table, "k", NULL), IFJ_EINTERNAL);
  free_var_data(&data);

  assert_int_equal(symtable_find(NULL, "k", &found), IFJ_EINTERNAL);
  assert_int_equal(symtable_find(&table, NULL, &found), IFJ_EINTERNAL);
  assert_int_equal(symtable_find(&table, "k", NULL), IFJ_EINTERNAL);

  assert_int_equal(symtable_traverse(NULL, cb_collect, &trav), IFJ_EINTERNAL);
  assert_int_equal(symtable_traverse(&table, NULL, &trav), IFJ_EINTERNAL);

  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
}

/**
 * @brief Test uvolneni a opetovne inicializace tabulky.
 */
static void test_destroy_and_reuse(void **state) {
  (void)state;
  symtable_t table;
  IFJ_ERR err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);

  // Uvolneni prazdne tabulky
  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(table.root);

  // Opetovna inicializace a pouziti
  err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  insert_var(&table, "again", TYPE_INT, false, true);
  symtable_data_t *found = find_key(&table, "again");
  assert_int_equal(found->var.type.type, TYPE_INT);

  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
}

/**
 * @brief Hlavni funkce testu.
 *
 * @return Vysledek behu testu.
 */
int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_insert_find_var, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_insert_find_var_null_codegen,
                                      test_setup, test_teardown),
      cmocka_unit_test_setup_teardown(test_insert_find_func, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_param_list_variants, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_deep_copy_semantics, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_find_mutable_pointer, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_invalid_func_data, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_duplicated_insert, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_find_nonexistent, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_ll, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_lr, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_rr, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_rl, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_traverse_sorted_order, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_traverse_empty_table, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_traverse_null_callback, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_traverse_error_propagates,
                                      test_setup, test_teardown),
      cmocka_unit_test_setup_teardown(test_traverse_data_integrity, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_list_var_roundtrip, test_setup,
                                      test_teardown),
      cmocka_unit_test(test_stress_127_keys),
      cmocka_unit_test(test_null_pointers),
      cmocka_unit_test(test_destroy_and_reuse),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
