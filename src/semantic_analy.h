/**
 * @file semantic_analy.h
 * @brief IFJ26 - Implementace sémantického analyzátoru
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "ast.h"
#include "error.h"
#include "symtable.h"

/**
 * @brief Funkce pro naplnění tabulky symbolů vestavěnými funkcemi a
 * uživatelskými funkcemi.
 * @param table Ukazatel na tabulku symbolů.
 * @param root Ukazatel na kořen AST stromu.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_ESEM_UNDEF pokud neexistuje funkce @main.
 * @return IFJ_ESEM_REDEF pokud je redefinice funkce nebo parametru.
 * @return IFJ_SUCCESS při úspěšném naplnění.
 */
IFJ_ERR symtable_fill_funcitons(symtable_t *table, const ast_node_t *root);

/**
 * @brief Funkce pro kontrolu sémantiky funkcí (celková kontrola).
 * @param root Ukazatel na kořen AST stromu.
 * @param functions_table Ukazatel na tabulku symbolů s funkcemi.
 * @return IFJ_EINTERNAL při poslaní NULL pointeru.
 * @return IFJ_SUCCESS při úspěšné kontrole.
 */
IFJ_ERR check_functions(const ast_node_t *root, symtable_t *functions_table);

#endif // SEMANTIC_ANALYZER_H
