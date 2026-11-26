/**
 * @file generator.h
 * @brief IFJ26 - Definice generátoru
 *
 * @author xberanm00 Marek Beran
 */

#ifndef IFJ_GENERATOR_H
#define IFJ_GENERATOR_H

// --- Naše soubory ---
#include "ast.h"
#include "error.h"

/**
 * @brief Generuje kód IFJcode26.
 *
 * @param root Ukazatel na kořen abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
IFJ_ERR generate(ast_node_t *root);

#endif // IFJ_GENERATOR_H
