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
 * Funkce očekává, že parser je nastaven na začátek výrazu např. pro výraz: "id
 * + 10;" bude parser.current_token = 'id' a po úspěšném dokončení analýzy bude
 * parser nastaven na token za výrazem (tady ';'). Výsledný AST uzel bude uložen
 * do out_ast.
 *
 * @param parser Ukazatel na parser.
 * @param out_ast Ukazatel na výstupní AST uzel.
 * @return IFJ_SUCCESS, pokud je analýza úspěšná, jinak chybový kód.
 */
IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast);

#endif // PSA_H