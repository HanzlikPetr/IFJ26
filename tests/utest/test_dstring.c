/**
 * @file test_dstring.c
 * @brief Projekt IFJ26 - jednotkové testy dynamického řetězce
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#include <string.h>

#include "setup_test.h"

#include "dstring.h"

/**
 * @brief Inicializace vytvoří prázdný, ukončený řetězec.
 */
static void test_init_empty(void **state) {
  (void)state;
  dstring_t s;

  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  assert_non_null(s.data);
  assert_int_equal(s.len, 0);
  assert_true(s.cap > 0);
  assert_string_equal(s.data, "");

  dstring_free(&s);
}

/**
 * @brief Přidání jednoho znaku.
 */
static void test_append_one(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);

  assert_int_equal(dstring_append(&s, 'a'), IFJ_SUCCESS);
  assert_int_equal(s.len, 1);
  assert_string_equal(s.data, "a");

  dstring_free(&s);
}

/**
 * @brief Několik znaků za sebou zachová pořadí.
 */
static void test_append_several(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);

  const char *text = "ahoj svete";
  for (size_t i = 0; text[i] != '\0'; i++) {
    assert_int_equal(dstring_append(&s, text[i]), IFJ_SUCCESS);
  }
  assert_int_equal(s.len, strlen(text));
  assert_string_equal(s.data, text);

  dstring_free(&s);
}

/**
 * @brief Přidání přes hranici kapacity pole zvětší a obsah zachová.
 */
static void test_append_grows(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  size_t initial_cap = s.cap;

  char expected[1001];
  for (int i = 0; i < 1000; i++) {
    char c = (char)('a' + i % 26);
    expected[i] = c;
    assert_int_equal(dstring_append(&s, c), IFJ_SUCCESS);
    // Vždy musí zbýt místo pro ukončovací nulu.
    assert_true(s.len < s.cap);
  }
  expected[1000] = '\0';

  assert_int_equal(s.len, 1000);
  assert_true(s.cap > initial_cap);
  assert_string_equal(s.data, expected);

  dstring_free(&s);
}

/**
 * @brief Hraniční případ: přesně tolik znaků, kolik se vejde bez zvětšení.
 */
static void test_append_exact_boundary(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  size_t initial_cap = s.cap;

  // cap - 1 znaků + '\0' se vejde přesně do původního pole.
  for (size_t i = 0; i + 1 < initial_cap; i++) {
    assert_int_equal(dstring_append(&s, 'x'), IFJ_SUCCESS);
  }
  assert_int_equal(s.cap, initial_cap);
  assert_int_equal(s.data[s.len], '\0');

  // Další znak už vyžaduje zvětšení.
  assert_int_equal(dstring_append(&s, 'y'), IFJ_SUCCESS);
  assert_true(s.cap > initial_cap);
  assert_int_equal(s.data[s.len - 1], 'y');
  assert_int_equal(s.data[s.len], '\0');

  dstring_free(&s);
}

/**
 * @brief Řetězec smí obsahovat speciální znaky (nový řádek, tabulátor).
 */
static void test_append_special_chars(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);

  assert_int_equal(dstring_append(&s, '\n'), IFJ_SUCCESS);
  assert_int_equal(dstring_append(&s, '\t'), IFJ_SUCCESS);
  assert_int_equal(dstring_append(&s, '\\'), IFJ_SUCCESS);
  assert_string_equal(s.data, "\n\t\\");

  dstring_free(&s);
}

/**
 * @brief Uvolnění vynuluje strukturu a opakované uvolnění je bezpečné.
 */
static void test_free_resets(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  assert_int_equal(dstring_append(&s, 'a'), IFJ_SUCCESS);

  dstring_free(&s);
  assert_null(s.data);
  assert_int_equal(s.len, 0);
  assert_int_equal(s.cap, 0);

  dstring_free(&s); // Druhé volání nesmí spadnout (free(NULL)).
  assert_null(s.data);
}

/**
 * @brief Po uvolnění lze strukturu znovu inicializovat.
 */
static void test_reinit_after_free(void **state) {
  (void)state;
  dstring_t s;
  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  assert_int_equal(dstring_append(&s, 'a'), IFJ_SUCCESS);
  dstring_free(&s);

  assert_int_equal(dstring_init(&s), IFJ_SUCCESS);
  assert_string_equal(s.data, "");
  assert_int_equal(dstring_append(&s, 'b'), IFJ_SUCCESS);
  assert_string_equal(s.data, "b");

  dstring_free(&s);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_init_empty),
      cmocka_unit_test(test_append_one),
      cmocka_unit_test(test_append_several),
      cmocka_unit_test(test_append_grows),
      cmocka_unit_test(test_append_exact_boundary),
      cmocka_unit_test(test_append_special_chars),
      cmocka_unit_test(test_free_resets),
      cmocka_unit_test(test_reinit_after_free),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}