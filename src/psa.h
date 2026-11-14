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
 * Smlouva pro autora parseru (parser.c):
 *
 * - Při chybě je pozice current_token nedefinovaná (PSA se zastaví tam, kde
 *   chybu našla) - volající musí kompilaci ukončit chybovým kódem.
 * - PSA nikdy nekonzumuje "to"/"until" na nejvyšší úrovni (mimo závorky)
 *   ani "by" - syntaxi for-generátoru "for i <- výraz to výraz [by výraz]
 *   do" musí zpracovat parser sám. Pozor: "by" skener nevydává jako klíčové
 *   slovo, jde o běžné TOK_ID (identifikátor).
 * - Klíčové slovo List, po kterém nenásleduje '(', se redukuje na AST_VAR
 *   s identifikátorem "List" - vestavěné členy jako List.range musí
 *   rozpoznat sémantická analýza.
 * - "(1 to 10 by 2)" uvnitř závorek je syntaktická chyba - "by" je podle
 *   specifikace povoleno pouze v hlavičce for-generátoru (vědomé
 *   rozhodnutí implementace).
 * - Argumenty volání funkce se analyzují rekurzivním voláním
 *   psa_parse_expression - u patologicky zanořeného vstupu hrozí přetečení
 *   zásobníku.
 * - Klíčové slovo if na pozici operandu zahajuje if výraz
 *   "if <výraz> then <výraz> else <výraz>". Podmínka i obě větve se
 *   zpracují rekurzivním voláním psa_parse_expression, větev else je
 *   povinná. Výsledkem je neterminál E (uzel AST_IF) na zásobníku PSA, takže
 *   if výraz může být operandem libovolného výrazu. Pozor: uzel AST_IF
 *   vytvářejí OBĚ soubory - psa.c pro if výraz (parse_if_expression, obě
 *   větve jsou vždy výrazy) a parser.c pro if příkaz (parse_if_statement,
 *   parse_inline_if, jejichž větve mohou být i bloky příkazů).
 *
 * @param parser Ukazatel na parser.
 * @param out_ast Ukazatel na výstupní AST uzel.
 * @return IFJ_SUCCESS, pokud je analýza úspěšná, jinak chybový kód.
 */
IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast);

#endif // PSA_H
