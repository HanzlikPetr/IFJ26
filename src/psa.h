/**
 * @file psa.h
 * @brief IFJ26 - Definice precedenční syntaktické analýzy (PSA)
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef PSA_H
#define PSA_H

#include "ast.h"
#include "error.h"
#include "parser.h"

/**
 * @brief Analyzuje výraz pomocí precedenční syntaktické analýzy.
 *
 * @param parser Ukazatel na parser.
 * @param out_ast Ukazatel na výstupní AST uzel.
 * @return IFJ_SUCCESS, pokud je analýza úspěšná, jinak chybový kód.
 */
IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast);

#endif // PSA_H