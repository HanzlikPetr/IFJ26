/**
 * @file dstring.h
 * @brief IFJ26 - Dynamický řetězec
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#ifndef DSTRING_H
#define DSTRING_H

#include <stddef.h>

#include "error.h"

/**
 * @brief Dynamický řetězec
 */
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} dstring_t;

/**
 * @brief Inicializuje prazdný řetězec
 * @param str Řetězec k inicializaci
 * @return IFJ_SUCCESS úspěch, nebo IFJ_EINTERNAL chyba při alokaci
 */
IFJ_ERR dstring_init(dstring_t *str);

/**
 * @brief Přidá znak na konec řetězce
 * @param str Řetězec
 * @param c Znak k přidání
 * @return IFJ_SUCCESS úspěch, nebo IFJ_EINTERNAL chyba při alokaci
 */
IFJ_ERR dstring_append(dstring_t *str, char c);

/**
 * @brief Uvolní paměť řetězce
 * @param str Řetězec
 * @return IFJ_SUCCESS úspěch, nebo IFJ_EINTERNAL pokud je str NULL
 */
IFJ_ERR dstring_free(dstring_t *str);

#endif
