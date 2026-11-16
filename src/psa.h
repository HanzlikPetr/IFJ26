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
 * - Klíčové slovo if na pozici operandu zahajuje if výraz. Podmínka i obě větve
 *   se zpracují rekurzivním voláním psa_parse_expression, větev else je
 *   povinná. Za then, před else i za else se přeskočí jeden token EOL, takže
 *   tělo větve může být i na dalším řádku; je-li po přeskočeném EOL dalším
 *   tokenem INDENT, je tělem větve blok příkazů zpracovaný parse_block, jinak
 *   jde o výraz. Za blokovou větví else se navíc vloží syntetický EOL a
 *   skutečný token za ním se vrátí do scanneru (scanner_unread_token), takže
 *   vnější PSA i volající příkazy vidí if výraz ukončený jako každý jiný výraz;
 *   tokeny then/else/do a to/until/by se vkládání výjimečně vyhnou, protože za
 *   kompletním výrazem legitimně následují v hlavičkách if/while/for. Pozor:
 *   uzel AST_IF vytvářejí OBĚ soubory - psa.c pro if výraz
 *   (parse_if_expression) a parser.c pro if příkaz (parse_if_statement,
 *   parse_inline_if).
 *
 * @param parser Ukazatel na parser.
 * @param out_ast Ukazatel na výstupní AST uzel.
 * @return IFJ_SUCCESS, pokud je analýza úspěšná, jinak chybový kód.
 */
IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast);

#endif // PSA_H
