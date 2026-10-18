/**
 * @file symtable.h
 * @brief IFJ26 - Definice funkcí pro práci s tabulkou symbolů
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

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
 * @brief Typy dat v tabulce symbolů.
 */
typedef enum {
  TYPE_INT,        /**< Celé číslo */
  TYPE_DOUBLE,     /**< Desetinné číslo */
  TYPE_STRING,     /**< Řetězec */
  TYPE_UNIT,       /**< Navratový typ UNIT (), pro funkci co nic vraci */
  TYPE_BOOL,       /**< Logická hodnota */
  TYPE_ANY,        /**< Pro vestavěne funkce */
  TYPE_UNRESOLVED /**< Pro funkce, které nemají definovaný návratový typ */
} data_type_t;

/**
 * @brief Wrapper pro typy dat v tabulce symbolů, který obsahuje informace o
 * tom, zda je proměnná seznam nebo ne.
 */
typedef struct {
  data_type_t type; /**< Typ dat */
  bool is_list; /**< Flag, true = proměnná je seznam, false = proměnná není
                   seznam */
} ifj26_type_t;

/**
 * @brief Struktura pro proměnné uložená v tabulce symbolů.
 */
typedef struct {
  ifj26_type_t type; /**< Typ dat */
  bool is_mutable; /**< Flag, true = proměnná je proměnlivá (var), false =
                      konstantní (val) nebo paramter funkce  */
  char *code_gen_name; /**< Název proměnné pro generování kódu,
                         kvůli překrytí */
} var_data_t;

/**
 * @brief Struktura pro seznam parametrů funkce.
 */
typedef struct {
  int count; /**< Počet parametrů */
  ifj26_type_t *types; /**< Pole typů parametrů */
} param_list_t;

/**
 * @brief Struktura pro data funkce uložené v tabulce symbolů.
 */
typedef struct {
  ifj26_type_t return_type; /**< Návratový typ funkce */
  bool return_type_explicit; /**< Flag, true = typ byl uveden v hlavičce, false
                                = typ je odvozen */
  bool is_resolving; /**< Flag pro detekci cyklu v rekurzi při
                          odvozování typu */
  bool is_main; /**< Flag, true = funkce je main, false = jiná funkce */
  int param_list_count; /**< Počet seznamu parametrů funkce */
  param_list_t *params; /**< Pole seznamu parametrů funkce */
} func_data_t;

/**
 * @brief Struktura pro data uložená v tabulce symbolů.
 */
typedef struct symtable_data {
  bool is_function; /**< Flag, true = prvek je funkce, false = prvek je proměnná
                     */
  union {
    var_data_t var; /**< Data proměnné */
    func_data_t func; /**< Data funkce */
  };
} symtable_data_t;

/**
 * @brief Typ callback funkce pro procházení tabulky symbolů.
 * @param key Klíč prvku.
 * @param value Hodnota prvku.
 * @param user_data Uživatelská data předaná při volání symtable_traverse().
 * @return IFJ_EINTERNAL při chybě, jinak IFJ_SUCCESS.
 */
typedef IFJ_ERR (*symtable_callback_t)(const char *key,
                                       const symtable_data_t *value,
                                       void *user_data);

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
 * @param value Data pro uložení do tabulky symbolů, vytváří se hluboká kopie,
 * vaše data uvolněte po vložení.
 * @return IFJ_EINTERNAL při chybě alokace paměti, nebo pokud je poslán NULL
 * pointer.
 * @return IFJ_ESEM_REDEF pokud klíč již existuje v tabulce.
 * @return IFJ_SUCCESS při úspěšném vložení.
 */
IFJ_ERR symtable_insert(symtable_t *table, const char *key,
                        const symtable_data_t *value);

/**
 * @brief Vyhledá prvek v tabulce symbolů a vrátí jeho hodnotu.
 * @param table Ukazatel na tabulku symbolů.
 * @param key Klíč prvku.
 * @param value Ukazatel na hodnotu prvku, kam se uloží výsledek.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_ESEM_UNDEF pokud prvek neexistuje.
 * @return IFJ_SUCCESS při úspěšném vyhledání.
 */
IFJ_ERR symtable_find(const symtable_t *table, const char *key,
                      symtable_data_t **value);

/**
 * @brief Uvolní tabulku symbolů a všechny její prvky.
 * @param table Ukazatel na tabulku symbolů.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném uvolnění.
 */
IFJ_ERR symtable_destroy(symtable_t *table);

/**
 * @brief Prochází tabulku symbolů a volá callback funkci pro každý prvek.
 * @param table Ukazatel na tabulku symbolů.
 * @param callback Callback funkce, která bude zavolána pro každý prvek.
 * @param user_data Uživatelská data, která budou předány callback funkci.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném procházení.
 */
IFJ_ERR symtable_traverse(const symtable_t *table, symtable_callback_t callback,
                          void *user_data);

#endif // SYMTABLE_H
