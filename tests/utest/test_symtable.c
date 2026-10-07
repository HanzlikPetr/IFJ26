/**
 * @file test_symtable.c
 * @brief IFJ26 - Unit testy pro tabulku symbolů (cmocka)
 *
 * @author xhanzlp00 Petr Hanzlík
 *
 * Tests created from the following prompt (model: GLM-5.3 via opencode):
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

static void test_insert_and_find(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  const char *keys[] = {"a", "b", "c"};
  int values[] = {1, 2, 3};

  for (size_t i = 0; i < 3; i++) {
    symtable_data_t data = {.cislo = values[i]};
    IFJ_ERR err = symtable_insert(&ts->table, keys[i], &data);
    assert_int_equal(err, IFJ_SUCCESS);
  }

  symtable_data_t *found = NULL;
  IFJ_ERR err = symtable_find(&ts->table, "b", &found);
  assert_int_equal(err, IFJ_SUCCESS);
  assert_non_null(found);
  assert_int_equal(found->cislo, 2);
}

static void test_duplicated_insert(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  symtable_data_t data = {.cislo = 1};
  IFJ_ERR err = symtable_insert(&ts->table, "b", &data);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t data2 = {.cislo = 2};
  err = symtable_insert(&ts->table, "b", &data2);
  assert_int_equal(err, IFJ_ESEM_REDEF);
}

static void test_find_nonexistent(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  symtable_data_t data = {.cislo = 1};
  IFJ_ERR err = symtable_insert(&ts->table, "b", &data);
  assert_int_equal(err, IFJ_SUCCESS);

  symtable_data_t *found = NULL;
  err = symtable_find(&ts->table, "a", &found);
  assert_int_equal(err, IFJ_ESEM_UNDEF);
}

static void test_rotations_and_complex_use(void **state) {
  test_state_t *ts = (test_state_t *)*state;

  // LL rotation trigger
  const char *ll_keys[] = {"g", "e", "c"};
  int ll_values[] = {1, 2, 3};
  // LR rotation trigger
  const char *lr_keys[] = {"k", "i", "j"};
  int lr_values[] = {4, 5, 6};
  // RR rotation trigger
  const char *rr_keys[] = {"m", "o", "q"};
  int rr_values[] = {7, 8, 9};
  // RL rotation trigger
  const char *rl_keys[] = {"u", "w", "v"};
  int rl_values[] = {10, 11, 12};

  const char **key_sets[] = {ll_keys, lr_keys, rr_keys, rl_keys};
  int *value_sets[] = {ll_values, lr_values, rr_values, rl_values};

  for (size_t s = 0; s < 4; s++) {
    for (size_t i = 0; i < 3; i++) {
      symtable_data_t data = {.cislo = value_sets[s][i]};
      IFJ_ERR err = symtable_insert(&ts->table, key_sets[s][i], &data);
      assert_int_equal(err, IFJ_SUCCESS);
    }
  }

  // Black-box verification of tree integrity after rotations
  for (size_t s = 0; s < 4; s++) {
    for (size_t i = 0; i < 3; i++) {
      symtable_data_t *found = NULL;
      IFJ_ERR err = symtable_find(&ts->table, key_sets[s][i], &found);
      assert_int_equal(err, IFJ_SUCCESS);
      assert_non_null(found);
      assert_int_equal(found->cislo, value_sets[s][i]);
    }
  }
}

static void test_null_pointers(void **state) {
  (void)state;

  symtable_data_t data = {.cislo = 1};
  symtable_data_t *found = NULL;

  IFJ_ERR err = symtable_init(NULL);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_insert(NULL, "a", &data);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_find(NULL, "a", &found);
  assert_int_equal(err, IFJ_EINTERNAL);

  err = symtable_destroy(NULL);
  assert_int_equal(err, IFJ_EINTERNAL);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_insert_and_find, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_duplicated_insert, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_find_nonexistent, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_rotations_and_complex_use,
                                      test_setup, test_teardown),
      cmocka_unit_test(test_null_pointers),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
