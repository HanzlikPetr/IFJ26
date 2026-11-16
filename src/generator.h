/**
 * @file generator.h
 * @brief IFJ26 - Definice generátoru
 *
 * @author xberanm00 Marek Beran
 */

#ifndef IFJ_GENERATOR_H
#define IFJ_GENERATOR_H

// --- libraries ---

// --- our files ---
#include "ast.h"
#include "error.h"

IFJ_ERR generator_generate(ast_node_t *root);

#endif // IFJ_GENERATOR_H
