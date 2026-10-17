/**
 * @file test_symtable.c
 * @brief IFJ26 - Unit testy pro tabulku symbolů (cmocka)
 *
 * @author xhanzlp00 Petr Hanzlík
 *
 * Tests (v2) created from the following prompt (model: GLM-5.3 via opencode):
 *
 * "please update tests for symtable.c to reflect the changes made in the
 * symtable.c file. Now i need to test new structure data, sometimes use
 * variable and sometimes function, also test create paramlist with 0 or more
 * paramlist, also check that insert create deep copy of data and that find
 * returns a deep copy of data, also check that destroy frees all memory
 * allocated for data, including paramlist and types."
 *
 * Note on find semantics: symtable_find returns a pointer to the table's own
 * stored copy (the one made by symtable_insert), not a newly allocated copy.
 * Mutations through that pointer persist in the table - this is intended for
 * type inference in semantic analysis. The tests assert this semantic.
 * Freeing of all data by symtable_destroy (code_gen_name, param lists and
 * types) is verified by Valgrind, which is run by `make test`.
 * Rotation, NULL-pointer and empty-key tests were carried over from v1.
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

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "setup_test.h"
#include "symtable.h"

typedef struct {
  symtable_t table;
} test_state_t;

static int test_setup(void **state) {
  test_state_t *ts = malloc(sizeof(test_state_t));
  assert_non_null(ts);

  IFJ_ERR err = symtable_init(&ts->table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(ts->table.root);

  *state = ts;
  return 0;
}

static int test_teardown(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  IFJ_ERR err = symtable_destroy(&ts->table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(ts->table.root);

  free(ts);
  return 0;
}

/* ------------------------- helpers for test data ------------------------- */

/* Deterministic type used to fill the j-th position of a param type array. */
static data_type_t expected_type(size_t j) { return (data_type_t)(j % 3); }

/* Builds variable data with a heap-allocated code_gen_name (caller owns). */
static symtable_data_t make_var_data(data_type_t type, bool is_mutable,
                                     const char *code_gen_name) {
  symtable_data_t data = {0};

  data.is_function = false;
  data.var.type = type;
  data.var.is_mutable = is_mutable;
  if (code_gen_name != NULL) {
    data.var.code_gen_name = strdup(code_gen_name);
    assert_non_null(data.var.code_gen_name);
  }

  return data;
}

/* Frees the caller-owned heap members of variable data. */
static void free_var_data(symtable_data_t *data) {
  free(data->var.code_gen_name);
  data->var.code_gen_name = NULL;
}

/* Builds function data with param_list_count lists; list_counts[i] gives the
 * number of types in the i-th list (may be 0). Caller owns all allocations. */
static symtable_data_t make_func_data(int param_list_count,
                                      const int *list_counts) {
  symtable_data_t data = {0};

  data.is_function = true;
  data.func.return_type = TYPE_DOUBLE;
  data.func.return_type_explicit = true;
  data.func.is_resolving = false;
  data.func.is_main = false;
  data.func.param_list_count = param_list_count;

  if (param_list_count <= 0) {
    return data;
  }

  data.func.params = malloc(sizeof(param_list_t) * (size_t)param_list_count);
  assert_non_null(data.func.params);

  for (int i = 0; i < param_list_count; i++) {
    data.func.params[i].count = list_counts[i];
    if (list_counts[i] > 0) {
      data.func.params[i].types =
          malloc(sizeof(data_type_t) * (size_t)list_counts[i]);
      assert_non_null(data.func.params[i].types);
      for (int j = 0; j < list_counts[i]; j++) {
        data.func.params[i].types[j] = expected_type((size_t)j);
      }
    } else {
      data.func.params[i].types = NULL;
    }
  }

  return data;
}

/* Frees the caller-owned heap members of function data. */
static void free_func_data(symtable_data_t *data) {
  for (int i = 0; i < data->func.param_list_count; i++) {
    free(data->func.params[i].types);
  }
  free(data->func.params);
  data->func.params = NULL;
}

/* Checks that stored function data matches what make_func_data produced. */
static void verify_func_data(const symtable_data_t *found, int param_list_count,
                             const int *list_counts) {
  assert_true(found->is_function);
  assert_int_equal(found->func.return_type, TYPE_DOUBLE);
  assert_true(found->func.return_type_explicit);
  assert_false(found->func.is_resolving);
  assert_false(found->func.is_main);
  assert_int_equal(found->func.param_list_count, param_list_count);

  if (param_list_count <= 0) {
    assert_null(found->func.params);
    return;
  }

  assert_non_null(found->func.params);
  for (int i = 0; i < param_list_count; i++) {
    assert_int_equal(found->func.params[i].count, list_counts[i]);
    if (list_counts[i] > 0) {
      assert_non_null(found->func.params[i].types);
      for (int j = 0; j < list_counts[i]; j++) {
        assert_int_equal(found->func.params[i].types[j],
                         expected_type((size_t)j));
      }
    } else {
      assert_null(found->func.params[i].types);
    }
  }
}

/* ------------------------------ test cases ------------------------------- */

/* Insert and find variable data, including a NULL code_gen_name. */
static void test_insert_and_find_variable(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  symtable_data_t v1 = make_var_data(TYPE_INT, true, "LF@x_1");
  err = symtable_insert(&ts->table, "x", &v1);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v1);

  symtable_data_t v2 = make_var_data(TYPE_BOOL, false, NULL);
  err = symtable_insert(&ts->table, "flag", &v2);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v2);

  err = symtable_find(&ts->table, "x", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_false(found->is_function);
  assert_int_equal(found->var.type, TYPE_INT);
  assert_true(found->var.is_mutable);
  assert_non_null(found->var.code_gen_name);
  assert_string_equal(found->var.code_gen_name, "LF@x_1");

  err = symtable_find(&ts->table, "flag", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_int_equal(found->var.type, TYPE_BOOL);
  assert_false(found->var.is_mutable);
  assert_null(found->var.code_gen_name);
}

/* Insert and find function data with several param lists. */
static void test_insert_and_find_function(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const int list_counts[] = {2, 0, 3};
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  symtable_data_t f = make_func_data(3, list_counts);
  err = symtable_insert(&ts->table, "f", &f);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f);

  err = symtable_find(&ts->table, "f", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  verify_func_data(found, 3, list_counts);
}

/* Param lists with 0, 1 and N lists, and with 0 or more types per list. */
static void test_param_list_variants(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  /* no param list at all */
  symtable_data_t f0 = make_func_data(0, NULL);
  err = symtable_insert(&ts->table, "f0", &f0);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f0);

  /* single empty list */
  const int c1[] = {0};
  symtable_data_t f1 = make_func_data(1, c1);
  err = symtable_insert(&ts->table, "f1", &f1);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f1);

  /* single list with several types */
  const int c2[] = {5};
  symtable_data_t f2 = make_func_data(1, c2);
  err = symtable_insert(&ts->table, "f2", &f2);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f2);

  /* several lists, mixed sizes */
  const int c3[] = {0, 2, 0, 1};
  symtable_data_t f3 = make_func_data(4, c3);
  err = symtable_insert(&ts->table, "f3", &f3);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f3);

  /* verify all four */
  err = symtable_find(&ts->table, "f0", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  verify_func_data(found, 0, NULL);

  err = symtable_find(&ts->table, "f1", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  verify_func_data(found, 1, c1);

  err = symtable_find(&ts->table, "f2", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  verify_func_data(found, 1, c2);

  err = symtable_find(&ts->table, "f3", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  verify_func_data(found, 4, c3);
}

/* Insert deep-copies the data: the caller frees its copies after insert and
 * the stored data stays intact and uses different allocations. */
static void test_insert_creates_deep_copy(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  symtable_data_t v = make_var_data(TYPE_STRING, true, "LF@v_0");
  const void *caller_code_gen_name = v.var.code_gen_name;

  const int list_counts[] = {3};
  symtable_data_t f = make_func_data(1, list_counts);
  const param_list_t *caller_params = f.func.params;
  const data_type_t *caller_types = f.func.params[0].types;

  err = symtable_insert(&ts->table, "v", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  err = symtable_insert(&ts->table, "g", &f);
  assert_int_equal(err, IFJ_SUCCESS);

  /* the caller releases its own copies after insert */
  free_var_data(&v);
  free_func_data(&f);

  /* stored copies are separate allocations with identical contents */
  err = symtable_find(&ts->table, "v", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_non_null(found->var.code_gen_name);
  assert_string_equal(found->var.code_gen_name, "LF@v_0");
  assert_true((const void *)found->var.code_gen_name != caller_code_gen_name);

  err = symtable_find(&ts->table, "g", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  verify_func_data(found, 1, list_counts);
  assert_true((const void *)found->func.params != caller_params);
  assert_true((const void *)found->func.params[0].types != caller_types);
}

/* find returns a pointer to the table's stored copy; changes through it
 * persist (used by semantic analysis for type inference). */
static void test_find_returns_mutable_pointer(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  symtable_data_t *again = NULL;
  IFJ_ERR err;

  symtable_data_t v = make_var_data(TYPE_UNRESOLVED, false, NULL);
  err = symtable_insert(&ts->table, "n", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v);

  err = symtable_find(&ts->table, "n", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_int_equal(found->var.type, TYPE_UNRESOLVED);

  found->var.type = TYPE_INT; /* inference-style update in place */

  err = symtable_find(&ts->table, "n", &again);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_ptr_equal(again, found);
  assert_int_equal(again->var.type, TYPE_INT);
}

/* Function data with param_list_count > 0 but NULL params is rejected. */
static void test_invalid_function_data(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  symtable_data_t data = {0};
  data.is_function = true;
  data.func.param_list_count = 2; /* params stays NULL */

  IFJ_ERR err = symtable_insert(&ts->table, "bad", &data);
  assert_int_equal(err, IFJ_EINTERNAL);
}

/* Duplicate key must fail with IFJ_ESEM_REDEF and keep the original data. */
static void test_duplicated_insert(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  symtable_data_t v = make_var_data(TYPE_INT, true, NULL);
  err = symtable_insert(&ts->table, "b", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v);

  const int list_counts[] = {1};
  symtable_data_t f = make_func_data(1, list_counts);
  err = symtable_insert(&ts->table, "b", &f);
  assert_int_equal(err, IFJ_ESEM_REDEF);
  free_func_data(&f);

  err = symtable_find(&ts->table, "b", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_false(found->is_function);
  assert_int_equal(found->var.type, TYPE_INT);
}

/* Find on an empty table and on a missing key returns IFJ_ESEM_UNDEF. */
static void test_find_nonexistent(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  err = symtable_find(&ts->table, "a", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
  assert_null(found);

  symtable_data_t v = make_var_data(TYPE_INT, false, NULL);
  err = symtable_insert(&ts->table, "b", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v);

  found = NULL;
  err = symtable_find(&ts->table, "a", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
}

/* Rotation triggers (LL, LR, RR, RL) with mixed variable/function data. */
static void test_rotations_and_complex_use(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  const char *ll_keys[] = {"g", "e", "c"};
  const char *lr_keys[] = {"k", "i", "j"};
  const char *rr_keys[] = {"m", "o", "q"};
  const char *rl_keys[] = {"u", "w", "v"};
  const char **key_sets[] = {ll_keys, lr_keys, rr_keys, rl_keys};
  IFJ_ERR err;

  for (size_t s = 0; s < 4; s++) {
    for (size_t i = 0; i < 3; i++) {
      char code_gen_name[16];
      snprintf(code_gen_name, sizeof code_gen_name, "k%zu_%zu", s, i);

      if ((s + i) % 2 == 0) {
        symtable_data_t v =
            make_var_data(TYPE_DOUBLE, i % 2 == 0, code_gen_name);
        err = symtable_insert(&ts->table, key_sets[s][i], &v);
        assert_int_equal(err, IFJ_SUCCESS);
        free_var_data(&v);
      } else {
        const int list_counts[] = {(int)i, 1};
        symtable_data_t f = make_func_data(2, list_counts);
        err = symtable_insert(&ts->table, key_sets[s][i], &f);
        assert_int_equal(err, IFJ_SUCCESS);
        free_func_data(&f);
      }
    }
  }

  /* Black-box verification of tree integrity after rotations */
  for (size_t s = 0; s < 4; s++) {
    for (size_t i = 0; i < 3; i++) {
      symtable_data_t *found = NULL;
      err = symtable_find(&ts->table, key_sets[s][i], &found);
      assert_int_equal(err, IFJ_SUCCESS);
      assert_non_null(found);

      if ((s + i) % 2 == 0) {
        char code_gen_name[16];
        snprintf(code_gen_name, sizeof code_gen_name, "k%zu_%zu", s, i);
        assert_string_equal(found->var.code_gen_name, code_gen_name);
      } else {
        const int list_counts[] = {(int)i, 1};
        verify_func_data(found, 2, list_counts);
      }
    }
  }
}

/* Larger batch: 127 keys inserted in a deterministic scrambled order. */
static void test_stress_many_keys(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  enum { N = 127 };
  enum { STRIDE = 37 }; /* coprime with N, so (k * STRIDE) % N is a shuffle */
  IFJ_ERR err;

  for (size_t k = 0; k < N; k++) {
    /* j is the final key index: the data must be derived from j (the same
     * index the verification loop below uses), not from the loop position */
    int j = (int)((k * STRIDE) % N);
    char key[16];
    snprintf(key, sizeof key, "k%03d", j);

    if (j % 2 == 0) {
      symtable_data_t v = make_var_data((data_type_t)(j % 3), j % 4 != 0, key);
      err = symtable_insert(&ts->table, key, &v);
      assert_int_equal(err, IFJ_SUCCESS);
      free_var_data(&v);
    } else {
      int list_counts[2] = {(int)(j % 3), (int)(j % 2)};
      symtable_data_t f = make_func_data(2, list_counts);
      err = symtable_insert(&ts->table, key, &f);
      assert_int_equal(err, IFJ_SUCCESS);
      free_func_data(&f);
    }
  }

  for (size_t k = 0; k < N; k++) {
    char key[16];
    symtable_data_t *found = NULL;
    snprintf(key, sizeof key, "k%03d", (int)k);

    err = symtable_find(&ts->table, key, &found);
    assert_int_equal(err, IFJ_SUCCESS);
    assert_non_null(found);

    if (k % 2 == 0) {
      assert_int_equal(found->var.type, (int)(k % 3));
      assert_true(found->var.is_mutable == (k % 4 != 0));
      assert_string_equal(found->var.code_gen_name, key);
    } else {
      int list_counts[2] = {(int)(k % 3), (int)(k % 2)};
      verify_func_data(found, 2, list_counts);
    }
  }
}

/* The empty string is a valid key. */
static void test_empty_key(void **state) {
  test_state_t *ts = (test_state_t *)*state;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  symtable_data_t v = make_var_data(TYPE_INT, true, "");
  err = symtable_insert(&ts->table, "", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v);

  err = symtable_find(&ts->table, "", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_non_null(found->var.code_gen_name);
  assert_string_equal(found->var.code_gen_name, "");
}

/* NULL arguments are safely rejected with IFJ_EINTERNAL. */
static void test_null_pointers(void **state) {
  (void)state;
  symtable_data_t data = {0};
  symtable_data_t *found = NULL;

  IFJ_ERR err = symtable_init(NULL);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_insert(NULL, "a", &data);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_insert(NULL, NULL, NULL);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_find(NULL, "a", &found);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_find(NULL, "a", NULL);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_destroy(NULL);
  assert_int_equal(err, IFJ_EINTERNAL);
}

/* Destroy frees all data (code_gen_name, param lists, types - the actual
 * freeing is verified by Valgrind in `make test`) and the table structure
 * can be re-initialized and reused afterwards. */
static void test_destroy_and_reuse(void **state) {
  (void)state;
  symtable_t table;
  symtable_data_t *found = NULL;
  IFJ_ERR err;

  /* destroy on a fresh, empty table */
  err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(table.root);

  err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t v = make_var_data(TYPE_STRING, false, "LF@x_1");
  err = symtable_insert(&table, "x", &v);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v);

  const int list_counts[] = {2, 0, 3};
  symtable_data_t f = make_func_data(3, list_counts);
  err = symtable_insert(&table, "f", &f);
  assert_int_equal(err, IFJ_SUCCESS);
  free_func_data(&f);

  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(table.root);

  /* the table structure can be re-initialized and reused */
  err = symtable_init(&table);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t v2 = make_var_data(TYPE_INT, true, NULL);
  err = symtable_insert(&table, "y", &v2);
  assert_int_equal(err, IFJ_SUCCESS);
  free_var_data(&v2);

  err = symtable_find(&table, "y", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_int_equal(found->var.type, TYPE_INT);

  err = symtable_destroy(&table);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_null(table.root);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_insert_and_find_variable, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_insert_and_find_function, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_param_list_variants, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_insert_creates_deep_copy, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_find_returns_mutable_pointer,
                                      test_setup, test_teardown),
      cmocka_unit_test_setup_teardown(test_invalid_function_data, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_duplicated_insert, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_find_nonexistent, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_and_complex_use,
                                      test_setup, test_teardown),
      cmocka_unit_test_setup_teardown(test_stress_many_keys, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_empty_key, test_setup,
                                      test_teardown),
      cmocka_unit_test(test_null_pointers),
      cmocka_unit_test(test_destroy_and_reuse),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
