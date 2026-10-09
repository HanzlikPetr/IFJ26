/**
 * @file dstring.c
 * @brief IFJ26 - Dynamický řetězec
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#include <stdlib.h>

#include "dstring.h"

#define DSTRING_INITIAL_CAP 16 // Počáteční velikost pole

IFJ_ERR dstring_init(dstring_t *str) {
  str->data = malloc(DSTRING_INITIAL_CAP * sizeof(char));
  CHECK_MEM_ERR(str->data);

  str->len = 0;
  str->cap = DSTRING_INITIAL_CAP;
  str->data[0] = '\0';

  return IFJ_SUCCESS;
}

IFJ_ERR dstring_append(dstring_t *str, char c) {
  if (str->len + 1 >= str->cap) {
    char *bigger = realloc(str->data, str->cap * 2);
    CHECK_MEM_ERR(bigger);
    str->data = bigger;
    str->cap *= 2;
  }
  str->data[str->len] = c;
  str->len++;
  str->data[str->len] = '\0';
  return IFJ_SUCCESS;
}

void dstring_free(dstring_t *str) {
  free(str->data);
  str->data = NULL;
  str->len = 0;
  str->cap = 0;
}