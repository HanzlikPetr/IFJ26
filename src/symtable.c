/**
 * @file symtable.c
 * @brief IFJ26 - Implementace tabulky symbolů a AVL stromu
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#define _POSIX_C_SOURCE 200809L // Kvuli strdup

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "symtable.h"

/**
 * @brief Struktura pro uzel AVL stromu.
 */
struct avl_node {
  char *key;              /**< Klíč uzlu */
  symtable_data_t data;   /**< Data uzlu */
  int height;             /**< Výška uzlu */
  struct avl_node *right; /**< Pravý potomek uzlu */
  struct avl_node *left;  /**< Levý potomek uzlu */
};

IFJ_ERR symtable_init(symtable_t *table) {
  // Kontrola proti NULL pointeru
  if (table == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table address cannot be null.");
    return IFJ_EINTERNAL;
  }

  // Nulovani stromu
  table->root = NULL;

  return IFJ_SUCCESS;
}

/**
IFJ_ERR symtable_destroy(symtable_t *table) {
  // Kontrola proti NULL pointeru
  if (table == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table address cannot be null.");
    return IFJ_EINTERNAL;
  }

  // Uvolnění stromu
  avl_destroy(table->root);

  // Nastavení ukazatele na NULL
  table->root = NULL;

  return IFJ_SUCCESS;
}
