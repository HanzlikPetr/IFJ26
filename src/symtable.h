/**
 * @file symtable.h
 * @brief IFJ26 - Definice funkcí pro práci s tabulkou symbolů
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "error.h"

/**
 * @brief Struktura pro uzel AVL stromu.
 */
typedef struct avl_node avl_node_t;

/**
 * @brief Struktura pro tabulku symbolů.
 */
typedef struct symtable {
  avl_node_t *root; /**< Ukazatel na kořen AVL stromu */
} symtable_t;

/**
 * @brief Struktura pro data uložená v tabulce symbolů.
 */
typedef struct symtable_data {
  int cislo;
} symtable_data_t;

/**
 * @brief Inicializace tabulky symbolu.
 * @param table Ukazatel na tabulku symbolu.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_SUCCESS při úspěšné inicializaci.
 */
IFJ_ERR symtable_init(symtable_t *table);

/**
 * @brief Vloží prvek do tabulky symbolů.
 * @param table Ukazatel na tabulku symbolů.
 * @param key Klíč prvku.
 * @param value Hodnota prvku.
 * @return IFJ_EINTERNAL při chybě alokace paměti, nebo pokud je poslán NULL pointer.
 * @return IFJ_ESEM_REDEF pokud klíč již existuje v tabulce.
 * @return IFJ_SUCCESS při úspěšném vložení.
 */
IFJ_ERR symtable_insert(symtable_t *table, const char *key,
                        const symtable_data_t *value);

/**
 * @brief Vyhledá prvek v tabulce symbolů a vrátí jeho hodnotu.
 * @param table Ukazatel na tabulku symbolů.
 * @param key Klíč prvku.
 * @param value Ukazatel na hodnotu prvku.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_ESEM_UNDEF pokud prvek neexistuje.
 * @return IFJ_SUCCESS při úspěšném vyhledání.
 */
IFJ_ERR symtable_find(symtable_t *table, const char *key,
                      symtable_data_t **value);

/**
 * @brief Uvolní tabulku symbolů a všechny její prvky.
 * @param table Ukazatel na tabulku symbolů.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném uvolnění.
 */
IFJ_ERR symtable_destroy(symtable_t *table);

#endif // SYMTABLE_H
