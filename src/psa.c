/**
 * @file psa.c
 * @brief IFJ26 - Implementace precedenční syntaktické analýzy (PSA)
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#include <stdbool.h>

#include "error.h"
#include "psa.h"

/**
 * @brief Symboly pro precedenční syntaktickou analýzu (PSA)
 */
typedef enum {
  PSA_DOT = 0, // '.'
  PSA_UNARY,   // unarní operátory '!' nebo '-'
  PSA_MUL_DIV, // '*' nebo '/'
  PSA_ADD_SUB, // '+', '-' nebo '++' (konkatenace)
  PSA_REL_OP,  // '<', '>', '<=', '>='
  PSA_EQ_OP,   // '==' nebo '!='
  PSA_AND_OP,  // '&&'
  PSA_OR_OP,   // '||'
  PSA_L_PAREN, // '('
  PSA_R_PAREN, // ')'
  PSA_ID,      // identifikátor (int, double, string, bool, () )
  PSA_DOLLAR,  // '$' (konec výrazu nebo dno zasobníku)

// Speialní hodnoty pro zpracování tabulky PSA
  PSA_NONTERM, // neterminál E
  PSA_HANDLE   // '<'
} psa_symbol_t;

/**
 * @brief Tabulka precedenčních vztahů pro PSA
 *
 * Hodnoty:
 * '<' - posunout (shift)
 * '>' - redukovat (reduce)
 * '=' - rovnost (např. při zpracování závorek)
 * 'f' - syntaktická chyba
 */
char psa_table[12][12] = {
    // DOT | UNARY | MUL_DIV | ADD_SUB | REL_OP | EQ_OP | AND_OP | OR_OP |
    // L_PAREN | R_PAREN | ID | DOLLAR
    {'>', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', '=', '>'}, // DOT
    {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // UNARY
    {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // MUL_DIV
    {'<', '<', '<', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // ADD_SUB
    {'<', '<', '<', '<', 'f', '>', '>', '>', '<', '>', '<', '>'}, // REL_OP
    {'<', '<', '<', '<', '<', 'f', '>', '>', '<', '>', '<', '>'}, // EQ_OP
    {'<', '<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // AND_OP
    {'<', '<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'}, // OR_OP
    {'>', '<', '<', '<', '<', '<', '<', '<', '<', '=', '<', 'f'}, // L_PAREN
    {'<', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', 'f', '>'}, // R_PAREN
    {'>', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', 'f', '>'}, // ID
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', 'f', '<', 'f'}, // DOLLAR
};

/**
 * @brief Převod tokenu na symbol PSA
 * @param token Token z lexikální analýzy.
 * @param expect_operand True pokud se očekává operand, (např. na začátku výrazu
 * nebo po operátoru), false pokud se očekává operátor.
 * @param open_parens Počet otevřených závorek (pro správné zpracování konce
 * výrazu).
 * @return Symbol PSA odpovídající tokenu.
 */
static psa_symbol_t token2psa_symbol(token_t *token, bool expect_operand,
                                     int open_parens) {
  switch (token->type) {
  case TOK_DOT:
    return PSA_DOT;

  case TOK_NOT:
    return PSA_UNARY;

  case TOK_SUB:
  case TOK_ADD:
    return expect_operand
               ? PSA_UNARY
               : PSA_ADD_SUB; // unární mínus/plus nebo binární mínus/plus

  case TOK_CONCAT:
    return PSA_ADD_SUB;

  case TOK_MUL:
  case TOK_DIV:
    return PSA_MUL_DIV;

  case TOK_LT:
  case TOK_GT:
  case TOK_LEQ:
  case TOK_GEQ:
    return PSA_REL_OP;

  case TOK_EQ:
  case TOK_NEQ:
    return PSA_EQ_OP;

  case TOK_AND:
    return PSA_AND_OP;

  case TOK_OR:
    return PSA_OR_OP;

  case TOK_LPAREN:
    return PSA_L_PAREN;

  case TOK_RPAREN:
    // Pokud neni otevrena zavorka, tak výraz je ukončen
    return open_parens > 0 ? PSA_R_PAREN : PSA_DOLLAR;

  case TOK_INT:
  case TOK_FLOAT:
  case TOK_STRING:
  case TOK_ID:
    return PSA_ID;
  case TOK_KEYWORD:
    // Pokud je klíčové slovo true, false nebo list, tak je to operand
    if (token->value.keyword == KW_TRUE || token->value.keyword == KW_FALSE ||
        token->value.keyword == KW_LIST) {
      return PSA_ID;
    }

    // Ostatní klíčová slova nejsou operandy, takže je považujeme za konec
    // výrazu
    return PSA_DOLLAR;
  default:
    return PSA_DOLLAR; // $ (konec výrazu)
  }
}

/**
 * @brief Vyhledání v tabulce precedenčních vztahů
 * @param stack_top Symbol na vrcholu zásobníku
 * @param input Symbol z vstupu
 * @return Vztah mezi symbolem na vrcholu zásobníku a symbolem z vstupu
 */
static char psa_table_lookup(psa_symbol_t stack_top, psa_symbol_t input) {
  return psa_table[stack_top][input];
}

IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(parser, out_ast);
}