/**
 * @file parser.h
 * @brief IFJ26 - Definice parseru
 *
 * @author xgleton00 Ondřej Glet
 */

#ifndef IFJ_PARSER_H
#define IFJ_PARSER_H

#include "ast.h"
#include "scanner.h"

/**
 * @brief Stav syntaktického analyzátoru.
 *
 * Parser uchovává scanner a token, který je právě zpracováván.
 */
typedef struct {
  scanner_t *scanner;      /**< Scanner poskytující vstupní tokeny. */
  token_t current_token;   /**< Aktuálně zpracovávaný token. */
} parser_t;

/**
 * @brief Inicializuje parser a načte první token vstupu.
 *
 * Parser scanner nevlastní. Scanner musí být inicializovaný volajícím
 * a musí zůstat platný po celou dobu používání parseru.
 *
 * @param parser Parser, který se má inicializovat.
 * @param scanner Inicializovaný scanner poskytující vstup.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR parser_init(parser_t *parser, scanner_t *scanner);

/**
 * @brief Zpracuje program začínající prologem IFJ26.
 *
 * @param parser Inicializovaný parser.
 * @param root Výstupní ukazatel na kořen AST.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR parser_parse(parser_t *parser, ast_node_t **root);

/**
 * @brief Uvolní aktuální token parseru.
 *
 * @param parser Parser, který se má uvolnit.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR parser_destroy(parser_t *parser);

/**
 * @brief Zpracuje blok příkazů "INDENT <příkazy> DEDENT".
 *
 * Blok je tvořen tokenem INDENT, následovaným jedním či více příkazy a
 * tokenem DEDENT. Funkci volá parser.c pro těla příkazů if/while/for a
 * psa.c pro blokové větve víceřádkového if výrazu (rozšíření CONTROL).
 *
 * @param parser Parser s tokenem INDENT.
 * @param block Výstupní uzel bloku.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR parse_block(parser_t *parser, ast_node_t **block);

#endif
