/**
 * @file printer_tree.c
 * @brief IFJ26 - Implementace výpisu abstraktního syntaktického stromu (AST)
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef PRINTER_TREE_H
#define PRINTER_TREE_H

#include "ast.h"

void ast_print(const ast_node_t *root);

#endif // PRINTER_TREE_H
