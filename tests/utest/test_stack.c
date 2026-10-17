/**
 * @file test_stack.c
 * @brief IFJ26 - Unit testy pro zásobník (stack)
 *
 * @author xberanm00 Marek Beran
 *
 * Prompt:
 * "Please create unit tests for the stack interface in C using the cmocka
 * framework. The file will be placed in tests/utest/ and named
 * test_stack.c. Include setup_test.h (for cmocka includes), stack.h,
 * and error.h and also write to the comment this prompt and model you are.
 *
 * Setup: init
 * Initialize the stack using the stack_init. Check that it returns
 * IFJ_SUCCESS.
 * Teardown: destroy
 * Destroy the stack using stack_dtor. Check that it returns
 * IFJ_SUCCESS and assert that the stack pointer is set to NULL afterwards.
 * If there is a complicated structure in the stack_elem.data attribute,
 * use stack_deep_dtor, create a destructor for that structure and pass it.
 * Test Case 1: Push and check top
 * Push elements with data 1, 2, 3 in this order to the stack.
 * Always check that the push function returns IFJ_SUCCESS. After pushing all of
 * them, assert the top element data is 3, and return code is IFJ_SUCCESS.
 * Test Case 2: Pop top element
 * Push elements with data 1, 2, 3 in this order to the
 * stack. Always check that the push function returns IFJ_SUCCESS. After pushing
 * all of them, pop the top element. Check that the popped data is 3 and return
 * code is IFJ_SUCCESS. Then check that the top element's data is 2 and return
 * code is IFJ_SUCCESS.
 * Test Case 3: Check empty stack
 * Check that the stack is empty and check for IFJ_SUCCESS. After that, push "a"
 * to the stack and check for IFJ_SUCCESS. Then check that the stack is not
 * empty and return code is IFJ_SUCCESS.
 * Test Case 4: Deep destruction
 * Define a structure complicated_stuct_t with an attribute char *text.
 * Implement a destructor function for complicated_struct_t, that frees
 * allocated memory of attribute text. Create an instance of
 * complicated_struct_t and for the text attribute, allocate memory and store
 * "hello world!" in it. Check for memory allocation error. After that, push the
 * instance onto a stack and check for IFJ_SUCCESS. Then, call stack_deep_dtor
 * for the stack and pass it the complicated_struct_t destructor. Check for
 * IFJ_SUCCESS.
 * Test Case 5: NULL Pointers
 * Try calling all API functions (stack_init, stack_dtor, stack_deep_dtor,
 * stack_is_empty, stack_push, stack_top, stack_pop) with NULL where a valid
 * pointer is expected. Assert that they safely return IFJ_EINTERNAL."
 *
 * Model: Gemini 3.1 Pro
 *
 * Testy test_peek_empty_stack, test_pop_empty_stack a test_pop_no_data jsou
 * dopsány ručně.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "setup_test.h"
#include "stack.h"

/* --- Setup and Teardown --- */

static int test_setup(void **state) {
  stack_t *stack = malloc(sizeof *stack);
  assert_non_null(stack);
  assert_int_equal(stack_init(stack), IFJ_SUCCESS);
  *state = stack;
  return 0;
}

static int test_teardown(void **state) {
  stack_t *stack = (stack_t *)(*state);

  assert_int_equal(stack_dtor(stack), IFJ_SUCCESS);
  assert_null(stack->top);

  // zásobník se za normálních okolností nemusí alokovat,
  // proto ho jeho destruktor neuvolňuje.
  free(*state);
  return 0;
}

/* --- Test Case 1: Push and check top --- */

static void test_push_and_check_top(void **state) {
  stack_t *stack = (stack_t *)(*state);
  int x1 = 1;
  int x2 = 2;
  int x3 = 3;
  int *p1 = &x1;
  int *p2 = &x2;
  int *p3 = &x3;

  assert_int_equal(stack_push(stack, p1), IFJ_SUCCESS);
  assert_int_equal(stack_push(stack, p2), IFJ_SUCCESS);
  assert_int_equal(stack_push(stack, p3), IFJ_SUCCESS);

  void *top_data;
  assert_int_equal(stack_top(stack, &top_data), IFJ_SUCCESS);
  assert_int_equal(*(int *)top_data, 3);
}

/* --- Test Case 2: Pop top element --- */

static void test_pop_top_element(void **state) {
  stack_t *stack = (stack_t *)(*state);
  int x1 = 1;
  int x2 = 2;
  int x3 = 3;
  int *p1 = &x1;
  int *p2 = &x2;
  int *p3 = &x3;

  assert_int_equal(stack_push(stack, p1), IFJ_SUCCESS);
  assert_int_equal(stack_push(stack, p2), IFJ_SUCCESS);
  assert_int_equal(stack_push(stack, p3), IFJ_SUCCESS);

  void *popped_data;
  assert_int_equal(stack_pop(stack, &popped_data), IFJ_SUCCESS);
  assert_int_equal(*(int *)popped_data, 3);

  void *top_data;
  assert_int_equal(stack_top(stack, &top_data), IFJ_SUCCESS);
  assert_int_equal(*(int *)top_data, 2);
}

/* --- Test Case 3: Check empty stack --- */

static void test_check_empty_stack(void **state) {
  stack_t *stack = (stack_t *)(*state);
  bool is_empty = false;

  assert_int_equal(stack_is_empty(stack, &is_empty), IFJ_SUCCESS);
  assert_true(is_empty);

  assert_int_equal(stack_push(stack, "a"), IFJ_SUCCESS);

  assert_int_equal(stack_is_empty(stack, &is_empty), IFJ_SUCCESS);
  assert_false(is_empty);
}

/* --- Test Case 4: Deep destruction --- */

typedef struct {
  char *text;
} complicated_struct_t;

IFJ_ERR complicated_struct_dtor(void *data) {
  if (data == NULL)
    return IFJ_EINTERNAL;

  complicated_struct_t *st = (complicated_struct_t *)data;
  if (st->text != NULL) {
    free(st->text);
  }
  free(st);

  return IFJ_SUCCESS;
}

static void test_deep_destruction(void **state) {
  stack_t *stack = (stack_t *)(*state);

  complicated_struct_t *inst = malloc(sizeof(complicated_struct_t));
  assert_non_null(inst);

  inst->text = malloc(strlen("hello world!") + 1);
  assert_non_null(inst->text);
  strcpy(inst->text, "hello world!");

  assert_int_equal(stack_push(stack, inst), IFJ_SUCCESS);

    // Destroy stack and pass the custom destructor
  assert_int_equal(stack_deep_dtor(stack, complicated_struct_dtor),
                   IFJ_SUCCESS);
}

static void test_peek_empty_stack(void **state) {
  stack_t *stack = (stack_t *)(*state);
  void *data;

  assert_int_equal(stack_top(stack, &data), IFJ_EINTERNAL);
}

static void test_pop_empty_stack(void **state) {
  stack_t *stack = (stack_t *)(*state);
  void *data;

  assert_int_equal(stack_pop(stack, &data), IFJ_EINTERNAL);
}

static void test_pop_no_data(void **state) {
  stack_t *stack = (stack_t *)(*state);
  int x = 1;
  void *ptr = &x;

  assert_int_equal(stack_push(stack, ptr), IFJ_SUCCESS);
  assert_int_equal(stack_pop(stack, NULL), IFJ_SUCCESS);
}

/* --- Test Case 5: NULL Pointers --- */

static void test_null_pointers(void **state) {
  (void)state; // Unused in this specific test block

  assert_int_equal(stack_init(NULL), IFJ_EINTERNAL);
  assert_int_equal(stack_dtor(NULL), IFJ_EINTERNAL);
  assert_int_equal(stack_deep_dtor(NULL, NULL), IFJ_EINTERNAL);

  bool is_empty;
  assert_int_equal(stack_is_empty(NULL, &is_empty), IFJ_EINTERNAL);

  assert_int_equal(stack_push(NULL, (void *)(intptr_t)1), IFJ_EINTERNAL);

  void *data;
  assert_int_equal(stack_top(NULL, &data), IFJ_EINTERNAL);
  assert_int_equal(stack_pop(NULL, &data), IFJ_EINTERNAL);
}

/* --- Main runner --- */

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_push_and_check_top, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_pop_top_element, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_check_empty_stack, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_deep_destruction, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_peek_empty_stack, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_pop_empty_stack, test_setup,
                                      test_teardown),
      cmocka_unit_test_setup_teardown(test_pop_no_data, test_setup,
                                      test_teardown),
      cmocka_unit_test(
          test_null_pointers) // Does not require valid initialization
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
