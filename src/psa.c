/**
 * @file psa.c
 * @brief IFJ26 - Implementace precedenční syntaktické analýzy (PSA)
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#define _POSIX_C_SOURCE 200809L // Kvuli strdup

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "psa.h"
#include "scanner.h"
#include "stack.h"

/**
 * @brief Velikost pole pro uchování uzlů zásobníku při redukci
 */
#define STACK_TOP_ARRAY_REDUCE_SIZE 5

/**
 * @brief Velikost pole pro uchování uzlů zásobníku při získávání neterminálu
 */
#define STACK_TOP_ARRAY_NONTERM_SIZE 2

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
 * @brief Struktura pro uzel zásobníku PSA
 */
typedef struct psa_stack_node {
  psa_symbol_t symbol; /**< Symbol PSA */
  ast_node_t *ast_node; /**< AST uzel odpovídající symbolu */
  token_t *token; /**< Token odpovídající symbolu */
} psa_stack_node_t;

/**
 * @brief Tabulka precedenčních vztahů pro PSA
 *
 * Hodnoty:
 * '<' - posunout (shift)
 * '>' - redukovat (reduce)
 * '=' - rovnost (např. při zpracování závorek)
 * 'f' - syntaktická chyba
 * 'e' - konec výrazu
 */
char psa_table[12][12] = {
    // DOT | UNARY | MUL_DIV | ADD_SUB | REL_OP | EQ_OP | AND_OP | OR_OP |
    // L_PAREN | R_PAREN | ID | DOLLAR
    {'>', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', '<', '>'}, // DOT
    {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // UNARY
    {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // MUL_DIV
    {'<', '<', '<', '>', '>', '>', '>', '>', '<', '>', '<', '>'}, // ADD_SUB
    {'<', '<', '<', '<', 'f', '>', '>', '>', '<', '>', '<', '>'}, // REL_OP
    {'<', '<', '<', '<', '<', 'f', '>', '>', '<', '>', '<', '>'}, // EQ_OP
    {'<', '<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // AND_OP
    {'<', '<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'}, // OR_OP
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '=', '<', 'f'}, // L_PAREN
    {'>', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', 'f', '>'}, // R_PAREN
    {'>', 'f', '>', '>', '>', '>', '>', '>', 'f', '>', 'f', '>'}, // ID
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', 'f', '<', 'e'}, // DOLLAR
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
    return expect_operand
               ? PSA_UNARY
               : PSA_ADD_SUB; // unární mínus/plus nebo binární mínus/plus

  case TOK_ADD:
    return PSA_ADD_SUB;

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
  case TOK_NULL:
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

/**
 * @brief Vytvoření uzlu zásobníku pro precedenční analýzu
 * @param symbol Symbol uzlu
 * @param ast_node AST uzel
 * @param token Token odpovídající symbolu
 * @return Ukazatel na nově vytvořený uzel zásobníku nebo NULL v případě chyby
 */
psa_stack_node_t *create_psa_stack_node(psa_symbol_t symbol,
                                        ast_node_t *ast_node, token_t *token) {
  psa_stack_node_t *node = malloc(sizeof(psa_stack_node_t));
  if (node == NULL) {
    return NULL; // Chyba alokace paměti
  }
  node->symbol = symbol;
  node->ast_node = ast_node;
  node->token = token;
  return node;
}

/**
 * @brief Zkopíruje token.
 * @param source Ukazatel na zdrojový token.
 * @return Ukazatel na zkopírovaný token nebo NULL v případě chyby.
 */
static token_t *copy_token(const token_t *source) {
  token_t *copy;

  if (source == NULL) {
    return NULL;
  }

  copy = malloc(sizeof *copy);
  if (copy == NULL) {
    return NULL;
  }
  *copy = *source;

  if (source->type == TOK_STRING || source->type == TOK_ID) {
    if (source->value.str_value == NULL) {
      free(copy);
      return NULL;
    }

    copy->value.str_value = malloc(strlen(source->value.str_value) + 1);
    if (copy->value.str_value == NULL) {
      free(copy);
      return NULL;
    }
    strcpy(copy->value.str_value, source->value.str_value);
  }

  return copy;
}

/**
 * @brief Získá neterminál z vrcholu zásobníku, který není neterminál ani
 * zarážka.
 * @param stack Zásobník PSA.
 * @param stack_array Pole pro uchování uzlů zásobníku při získávání
 * neterminálu.
 * @param out_count Počet uzlů v poli stack_array.
 * @param stack_top_symbol Symbol na vrcholu zásobníku, který není neterminál
 * ani zarážka.
 * @param parser Parser používaný pro diagnostiku chyb.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR get_nonterm_from_stack(stack_t *stack,
                                      psa_stack_node_t *stack_array[],
                                      int *out_count,
                                      psa_symbol_t *stack_top_symbol,
                                      parser_t *parser) {
  CHECK_ARGS_NULL_RET(stack, stack_array, out_count);
  void *data_out;
  psa_stack_node_t *node;
  bool found_nonterm = false;

  *out_count = 0;

  for (int i = 0; i < STACK_TOP_ARRAY_NONTERM_SIZE; i++) {
    CHECK_ERR_LOG_RET(stack_top(stack, &data_out),
                      "Failed to get top of stack");

    node = (psa_stack_node_t *)data_out;

    if (node->symbol != PSA_NONTERM && node->symbol != PSA_HANDLE) {
      // Nalezen symbol, který není neterminál ani zarážka
      *stack_top_symbol = node->symbol;
      found_nonterm = true;
      break;
    }

    CHECK_ERR_LOG_RET(stack_pop(stack, &data_out), "Failed to pop from stack");

    stack_array[i] = node;
    *out_count += 1;
  }

  if (!found_nonterm) {
    // Pokud nebyl nalezen symbol, který není neterminál ani zarážka, tak je
    // to chyba
    CHECK_ERR_LOG_RET(IFJ_ESYNTAX,
                      "Failed to find a valid symbol on the stack, on line %d",
                      parser->current_token.line);
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR traverse_stack(stack_t *stack) {
  CHECK_ARG_NULL_RET(stack);

  psa_stack_node_t **nodes = NULL;
  size_t count = 0;
  size_t capacity = 0;
  void *data_out;

  while (true) {
    bool is_empty;
    CHECK_ERR_RET(stack_is_empty(stack, &is_empty));
    if (is_empty) {
      break;
    }

    CHECK_ERR_RET(stack_pop(stack, &data_out));
    if (count == capacity) {
      size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
      psa_stack_node_t **new_nodes =
          realloc(nodes, new_capacity * sizeof *new_nodes);
      if (new_nodes == NULL) {
        for (size_t i = count; i > 0; --i) {
          CHECK_ERR_RET(stack_push(stack, nodes[i - 1]));
        }
        free(nodes);
        return IFJ_EINTERNAL;
      }
      nodes = new_nodes;
      capacity = new_capacity;
    }
    nodes[count++] = (psa_stack_node_t *)data_out;
  }

  for (size_t i = count; i > 0; --i) {
    psa_stack_node_t *node = nodes[i - 1];
    switch (node->symbol) {
    case PSA_EQ_OP:
      printf("PSA_EQ_OP ");
      break;
    case PSA_AND_OP:
      printf("PSA_AND_OP ");
      break;
    case PSA_OR_OP:
      printf("PSA_OR_OP ");
      break;
    case PSA_L_PAREN:
      printf("( ");
      break;
    case PSA_R_PAREN:
      printf(") ");
      break;
    case PSA_DOLLAR:
      printf("$ ");
      break;
    case PSA_ID:
      printf("PSA_ID ");
      break;
    case PSA_NONTERM:
      printf("E ");
      break;
    case PSA_HANDLE:
      printf("< ");
      break;
    case PSA_ADD_SUB:
      printf("PSA_ADD_SUB ");
      break;
    case PSA_MUL_DIV:
      printf("PSA_MUL_DIV ");
      break;
    case PSA_REL_OP:
      printf("PSA_REL_OP ");
      break;
    case PSA_UNARY:
      printf("PSA_UNARY ");
      break;
    case PSA_DOT:
      printf("PSA_DOT ");
      break;
    default:
      printf("Unknown symbol: %d", node->symbol);
    }
  }

  for (size_t i = count; i > 0; --i) {
    CHECK_ERR_RET(stack_push(stack, nodes[i - 1]));
  }
  free(nodes);
  printf("\n");
  return IFJ_SUCCESS;
}

static IFJ_ERR push_single_nonterm(stack_t *stack, psa_stack_node_t *nodes[],
                                   int node_count, parser_t *parser) {
  CHECK_ARGS_NULL_RET(stack, nodes, parser);

  if (node_count == 1) {
    CHECK_ERR_LOG_RET(stack_push(stack, (void *)nodes[0]),
                      "Failed to push symbol onto the stack");
  } else if (node_count > 1) {
    LOG_ERR(IFJ_ESYNTAX, "Expected 1 symbol, got %d, on line %d", node_count,
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Uvolní uzel zásobníku PSA.
 * @param data Ukazatel na uzel zásobníku PSA.
 * @return IFJ_SUCCESS při úspěšném uvolnění nebo IFJ_EINTERNAL při chybě.
 */
static IFJ_ERR psa_stack_node_dtor(void *data) {
  psa_stack_node_t *node = data;

  if (node == NULL) {
    return IFJ_EINTERNAL;
  }

  if (node->ast_node != NULL) {
    ast_free(node->ast_node);
  }

  if (node->token != NULL) {
    token_free(node->token);
    free(node->token);
  }

  free(node);
  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje posun (shift) v precedenční analýze
 * @param stack Zásobník PSA
 * @param parser Parser používaný pro načtení dalšího tokenu
 * @param input_symbol Symbol aktuálního tokenu
 * @param expect_operand Určuje, zda se očekává operand
 * @param open_parens Počet otevřených závorek
 * @param stack_array_get_nonterm Pole pro uchování uzlů zásobníku při získávání
 * neterminálu
 * @param stack_array_get_nonterm_count Počet uzlů v poli
 * stack_array_get_nonterm
 * @param stack_top_symbol Symbol na vrcholu zásobníku, který není neterminál
 * ani zarážka
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód
 */
static IFJ_ERR psa_shift(stack_t *stack, parser_t *parser,
                         psa_symbol_t input_symbol, bool *expect_operand,
                         int *open_parens,
                         psa_stack_node_t *stack_array_get_nonterm[],
                         int *stack_array_get_nonterm_count,
                         psa_symbol_t *stack_top_symbol) {
  IFJ_ERR err;
  token_t *token;
  psa_stack_node_t *push_node;

  CHECK_ARGS_NULL_RET(stack, parser, expect_operand, open_parens,
                      stack_array_get_nonterm, stack_array_get_nonterm_count,
                      stack_top_symbol);

  err = get_nonterm_from_stack(stack, stack_array_get_nonterm,
                               stack_array_get_nonterm_count, stack_top_symbol,
                               parser);
  CHECK_ERR_LOG_RET(err, "Failed to get non-terminal from stack");

  push_node = create_psa_stack_node(PSA_HANDLE, NULL, NULL);
  CHECK_MEM_ERR(push_node);
  err = stack_push(stack, (void *)push_node);
  CHECK_ERR_LOG_RET(err, "Failed to push handle onto the stack");

  err = push_single_nonterm(stack, stack_array_get_nonterm,
                            *stack_array_get_nonterm_count, parser);
  CHECK_ERR_LOG_RET(err, "Failed to restore non-terminal on the stack");

  token = copy_token(&parser->current_token);
  CHECK_MEM_ERR(token);
  push_node = create_psa_stack_node(input_symbol, NULL, token);
  CHECK_MEM_ERR(push_node);
  err = stack_push(stack, (void *)push_node);
  CHECK_ERR_LOG_RET(err, "Failed to push symbol onto the stack");

  if (input_symbol == PSA_L_PAREN) {
    (*open_parens)++;
  } else if (input_symbol == PSA_R_PAREN) {
    (*open_parens)--;
  }

  *expect_operand = input_symbol != PSA_ID && input_symbol != PSA_R_PAREN;

  token_free(&parser->current_token);
  return scanner_next_token(parser->scanner, &parser->current_token);
}

/**
 * @brief Zpracuje vztah rovnosti v precedenční tabulce.
 * @param stack Zásobník PSA.
 * @param parser Parser používaný pro načtení dalšího tokenu.
 * @param input_symbol Symbol aktuálního tokenu.
 * @param expect_operand Určuje, zda se očekává operand.
 * @param open_parens Počet otevřených závorek.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR psa_equal(stack_t *stack, parser_t *parser,
                         psa_symbol_t input_symbol, bool *expect_operand,
                         int *open_parens) {
  IFJ_ERR err;
  token_t *token;
  psa_stack_node_t *push_node;

  CHECK_ARGS_NULL_RET(stack, parser, expect_operand, open_parens);

  token = copy_token(&parser->current_token);
  CHECK_MEM_ERR(token);

  push_node = create_psa_stack_node(input_symbol, NULL, token);
  if (push_node == NULL) {
    token_free(token);
    free(token);
    return IFJ_EINTERNAL;
  }

  err = stack_push(stack, (void *)push_node);
  if (err != IFJ_SUCCESS) {
    psa_stack_node_dtor(push_node);
    return err;
  }

  if (input_symbol == PSA_L_PAREN) {
    (*open_parens)++;
  } else if (input_symbol == PSA_R_PAREN) {
    (*open_parens)--;
  }

  *expect_operand = input_symbol != PSA_ID && input_symbol != PSA_R_PAREN;

  token_free(&parser->current_token);
  return scanner_next_token(parser->scanner, &parser->current_token);
}

static IFJ_ERR create_ast_literal(token_t *token, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(token, out_ast);

  ast_node_t *new_ast_node = malloc(sizeof *new_ast_node);
  CHECK_MEM_ERR(new_ast_node);

  new_ast_node->type = AST_LITERAL;

  switch (token->type) {
  case TOK_INT:
    new_ast_node->as.literal.type = TYPE_INT;
    new_ast_node->as.literal.int_value = token->value.int_value;
    break;

  case TOK_FLOAT:
    new_ast_node->as.literal.type = TYPE_DOUBLE;
    new_ast_node->as.literal.double_value = token->value.double_value;
    break;

  case TOK_STRING:
    new_ast_node->as.literal.type = TYPE_STRING;
    new_ast_node->as.literal.str_value = strdup(token->value.str_value);
    CHECK_MEM_ERR(new_ast_node->as.literal.str_value);
    break;

  case TOK_NULL:
    new_ast_node->as.literal.type = TYPE_UNIT;
    break;

  case TOK_KEYWORD:
    if (token->value.keyword == KW_TRUE || token->value.keyword == KW_FALSE) {
      new_ast_node->as.literal.type = TYPE_BOOL;
      new_ast_node->as.literal.bool_value = (token->value.keyword == KW_TRUE);
    } else {
      free(new_ast_node);
      return IFJ_ESYNTAX;
    }
    break;

  default:
    free(new_ast_node);
    return IFJ_ESYNTAX;
  }

  *out_ast = new_ast_node;
  return IFJ_SUCCESS;
}

static IFJ_ERR create_ast_var(token_t *token, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(token, out_ast);

  if (token->type != TOK_ID) {
    return IFJ_ESYNTAX;
  }

  ast_node_t *new_ast_node = malloc(sizeof *new_ast_node);
  CHECK_MEM_ERR(new_ast_node);

  new_ast_node->type = AST_VAR;
  new_ast_node->as.var.id = strdup(token->value.str_value);
  CHECK_MEM_ERR(new_ast_node->as.var.id);

  *out_ast = new_ast_node;
  return IFJ_SUCCESS;
}

/**
 * @brief Vytvoří AST uzel pro binární výraz.
 * @param operator_token Token binárního operátoru.
 * @param lhs Levý operand.
 * @param rhs Pravý operand.
 * @param out_ast Ukazatel na vytvořený AST uzel.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR create_ast_binary_expr(token_t *operator_token, ast_node_t **lhs,
                                      ast_node_t **rhs, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(operator_token, lhs, rhs, out_ast);

  ast_node_t *new_ast_node = malloc(sizeof *new_ast_node);
  CHECK_MEM_ERR(new_ast_node);

  new_ast_node->type = AST_EXPR;
  new_ast_node->as.expr.is_unary = false;
  new_ast_node->as.expr.operator_type = operator_token->type;
  new_ast_node->as.expr.binary.lhs = *lhs;
  new_ast_node->as.expr.binary.rhs = *rhs;

  // Předání vlastnictví AST uzlů a nastavení původních ukazatelů na NULL
  *lhs = NULL;
  *rhs = NULL;

  *out_ast = new_ast_node;
  return IFJ_SUCCESS;
}

/**
 * @brief Vytvoří AST uzel pro unární výraz.
 * @param operator_token Token unárního operátoru.
 * @param operand Operand unárního výrazu.
 * @param out_ast Ukazatel na vytvořený AST uzel.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR create_ast_unary_expr(token_t *operator_token,
                                     ast_node_t **operand,
                                     ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(operator_token, operand, out_ast);

  ast_node_t *new_ast_node = malloc(sizeof *new_ast_node);
  CHECK_MEM_ERR(new_ast_node);

  new_ast_node->type = AST_EXPR;
  new_ast_node->as.expr.is_unary = true;
  new_ast_node->as.expr.operator_type = operator_token->type;
  new_ast_node->as.expr.unary.operand = *operand;

  // Předání vlastnictví AST uzlu a nastavení původního ukazatele na NULL
  *operand = NULL;

  *out_ast = new_ast_node;
  return IFJ_SUCCESS;
}

/**
 * @brief Redukuje handle na neterminál E.
 * @param stack Zásobník PSA.
 * @param parser Parser používaný pro diagnostiku chyb.
 * @param input_symbol Aktuální vstupní symbol.
 * @param expression_done Nastaví se na true při úspěšném ukončení výrazu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR psa_reduce(stack_t *stack, parser_t *parser,
                          psa_symbol_t input_symbol, bool *expression_done) {
  IFJ_ERR err = IFJ_SUCCESS;
  bool found_handle = false;
  bool is_empty;
  void *data_out = NULL;
  int reduce_count = 0;
  psa_stack_node_t *reduce_nodes[STACK_TOP_ARRAY_REDUCE_SIZE];
  psa_stack_node_t *push_node = NULL;
  ast_node_t *new_ast_node = NULL;

  CHECK_ARGS_NULL_RET(stack, parser, expression_done);
  *expression_done = false;

  err = stack_is_empty(stack, &is_empty);
  CHECK_ERR_LOG_RET(err, "Failed to check if stack is empty");

  for (int i = 0; i < STACK_TOP_ARRAY_REDUCE_SIZE && !is_empty; i++) {
    err = stack_is_empty(stack, &is_empty);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to check if stack is empty");
    if (is_empty) {
      break;
    }

    err = stack_pop(stack, &data_out);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to pop from stack");
    reduce_nodes[reduce_count++] = (psa_stack_node_t *)data_out;

    if (reduce_nodes[reduce_count - 1]->symbol == PSA_HANDLE) {
      found_handle = true;
      break;
    }
  }

  // Pokud nebyla nalezena zarážka, zkontrolujeme, zda-li se jedná o konec
  // výrazu
  if (!found_handle) {
    if (input_symbol == PSA_DOLLAR && reduce_count == 3 &&
        reduce_nodes[0]->symbol == PSA_DOLLAR &&
        reduce_nodes[1]->symbol == PSA_NONTERM &&
        reduce_nodes[2]->symbol == PSA_DOLLAR) {
      *expression_done = true;
      goto cleanup;
    }

    err = IFJ_ESYNTAX;
    LOG_ERR(err, "Failed to find a handle symbol on the stack, on line %d",
            parser->current_token.line);
    goto cleanup;
  }

  if (reduce_nodes[0]->symbol == PSA_ID) {
    // Pravidlo 1: E -> id
    switch (reduce_nodes[0]->token->type) {
    case TOK_ID:
      err = create_ast_var(reduce_nodes[0]->token, &new_ast_node);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to create AST variable from token");
      break;

    case TOK_INT:
    case TOK_FLOAT:
    case TOK_STRING:
    case TOK_NULL:
      err = create_ast_literal(reduce_nodes[0]->token, &new_ast_node);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to create AST literal from token");
      break;

    case TOK_KEYWORD:
      if (reduce_nodes[0]->token->value.keyword == KW_TRUE ||
          reduce_nodes[0]->token->value.keyword == KW_FALSE) {
        err = create_ast_literal(reduce_nodes[0]->token, &new_ast_node);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to create AST literal from token");
      } else {
        err = IFJ_ESYNTAX;
        LOG_ERR(err, "Invalid keyword for E -> id reduction, on line %d",
                parser->current_token.line);
        goto cleanup;
      }
      break;

    default:
      err = IFJ_ESYNTAX;
      LOG_ERR(err, "Invalid token type for E -> id reduction, on line %d",
              parser->current_token.line);
      goto cleanup;
    }
  } else if (reduce_count == 4 && reduce_nodes[0]->symbol == PSA_R_PAREN &&
             reduce_nodes[1]->symbol == PSA_NONTERM &&
             reduce_nodes[2]->symbol == PSA_L_PAREN) {
    // Pravidlo 2: E -> (E)
    new_ast_node = reduce_nodes[1]->ast_node;
    reduce_nodes[1]->ast_node = NULL; // Předejme vlastnictví AST uzlu
  } else if (reduce_count == 4 && reduce_nodes[0]->symbol == PSA_NONTERM &&
             (reduce_nodes[1]->symbol == PSA_ADD_SUB ||
              reduce_nodes[1]->symbol == PSA_MUL_DIV ||
              reduce_nodes[1]->symbol == PSA_DOT ||
              reduce_nodes[1]->symbol == PSA_EQ_OP ||
              reduce_nodes[1]->symbol == PSA_REL_OP ||
              reduce_nodes[1]->symbol == PSA_AND_OP ||
              reduce_nodes[1]->symbol == PSA_OR_OP) &&
             reduce_nodes[2]->symbol == PSA_NONTERM) {
    // Pravidlo 3: E -> E op E
    err = create_ast_binary_expr(reduce_nodes[1]->token,
                                 &reduce_nodes[2]->ast_node,
                                 &reduce_nodes[0]->ast_node, &new_ast_node);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to create AST binary expression");
  } else if (reduce_count == 3 && reduce_nodes[0]->symbol == PSA_NONTERM &&
             reduce_nodes[1]->symbol == PSA_UNARY) {
    // Pravidlo 4: E -> op E
    err = create_ast_unary_expr(reduce_nodes[1]->token,
                                &reduce_nodes[0]->ast_node, &new_ast_node);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to create AST unary expression");
  } else {
    err = IFJ_ESYNTAX;
    LOG_ERR(err,
            "Failed to match a reduction rule for the stack symbols, on "
            "line %d",
            parser->current_token.line);
    goto cleanup;
  }

  // Vytvoření nového uzlu zásobníku pro neterminál E, včetně vytovřené části
  // ast, a jeho vložení na zásobník
  new_ast_node->inferred_type.type = TYPE_UNRESOLVED;
  new_ast_node->inferred_type.is_list = false;
  push_node = create_psa_stack_node(PSA_NONTERM, new_ast_node, NULL);
  CHECK_MEM_ERR_GOTO(push_node, cleanup);
  err = stack_push(stack, (void *)push_node);
  CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push node onto stack");
  push_node = NULL;

cleanup:
  if (push_node != NULL) {
    psa_stack_node_dtor(push_node);
  }
  for (int i = 0; i < reduce_count; i++) {
    psa_stack_node_dtor(reduce_nodes[i]);
  }
  return err;
}

/**
 * @brief Získá symbol aktuálního vstupního tokenu pro PSA.
 * @param parser Parser.
 * @param expect_operand True pokud se očekává operand.
 * @param open_parens Počet otevřených závorek.
 * @param input_symbol Výstupní symbol vstupu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR get_input_symbol(parser_t *parser, bool expect_operand,
                                int open_parens, psa_symbol_t *input_symbol) {
  token_t peek_token;
  IFJ_ERR err;

  CHECK_ARGS_NULL_RET(parser, input_symbol);

  if (parser->current_token.type == TOK_LPAREN) {
    err = scanner_peek_next_token(parser->scanner, &peek_token);
    CHECK_ERR_RET(err);

    if (peek_token.type == TOK_RPAREN) {
      *input_symbol = PSA_ID;

      err = scanner_peek_next_token(parser->scanner, &peek_token);
      CHECK_ERR_RET(err);

      parser->current_token.line = peek_token.line;
      parser->current_token.type = TOK_NULL;
    } else {
      *input_symbol =
          token2psa_symbol(&parser->current_token, expect_operand, open_parens);
    }
  } else {
    *input_symbol =
        token2psa_symbol(&parser->current_token, expect_operand, open_parens);
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Zpracování výrazu pomocí precedenční analýzy
 * @param parser Parser
 * @param out_ast Výstupní AST uzel
 * @return IFJ_SUCCESS při úspěch nebo chybový kód
 */
static IFJ_ERR process_expression(parser_t *parser, ast_node_t **out_ast) {
  IFJ_ERR err;
  stack_t stack;
  bool expect_operand, expression_done;
  int open_parens;
  char relation;
  psa_symbol_t stack_top_symbol, input_symbol;
  psa_stack_node_t *push_node,
      *stack_array_get_nonterm
          [STACK_TOP_ARRAY_NONTERM_SIZE]; // Pole pro uchování uzlů zásobníku
                                          // při získávání neterminálu
  int stack_array_get_nonterm_count = 0;
  void *data_out = NULL;

  // Inicializace pomocných proměnných
  expect_operand = true; // Na začátku výrazu očekáváme operand
  open_parens = 0; // Počet otevřených závorek

  // Inicializace zásobníku a vložení dolaru na dno zásobníku
  CHECK_ERR_RET(stack_init(&stack));
  push_node = create_psa_stack_node(PSA_DOLLAR, NULL, NULL);
  CHECK_MEM_ERR(push_node);

  err = stack_push(&stack, (void *)push_node);
  if (err != IFJ_SUCCESS) {
    LOG_ERR(err, "Failed to push dollar symbol onto the stack");
    free(push_node);
    return err;
  }

  // Hlavní smyčka precedenční analýzy
  while (1) {
    // Získání symbolu na vrcholu zásobníku, který není neterminál (E) nebo
    // zarážka (<)
    stack_array_get_nonterm_count = 0;

    err = get_nonterm_from_stack(&stack, stack_array_get_nonterm,
                                 &stack_array_get_nonterm_count,
                                 &stack_top_symbol, parser);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get non-terminal from stack");

    err = push_single_nonterm(&stack, stack_array_get_nonterm,
                              stack_array_get_nonterm_count, parser);
    CHECK_ERR_LOG_GOTO(err, cleanup,
                       "Failed to restore non-terminal on the stack");

    // Získání symbolu z aktuálního tokenu a vyhledání vztahu v tabulce PSA
    err = get_input_symbol(parser, expect_operand, open_parens, &input_symbol);
    CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get input symbol");
    relation = psa_table_lookup(stack_top_symbol, input_symbol);

    CHECK_ERR_GOTO(traverse_stack(&stack), cleanup);

    if (relation == '<') {
      err = psa_shift(&stack, parser, input_symbol, &expect_operand,
                      &open_parens, stack_array_get_nonterm,
                      &stack_array_get_nonterm_count, &stack_top_symbol);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get next token from scanner");
    } else if (relation == '>') {
      err = psa_reduce(&stack, parser, input_symbol, &expression_done);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to reduce expression");
      if (expression_done) {
        break;
      }
    } else if (relation == '=') {
      err = psa_equal(&stack, parser, input_symbol, &expect_operand,
                      &open_parens);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get next token from scanner");
    } else if (relation == 'f') {
      // Syntaktická chyba
      err = IFJ_ESYNTAX;
      LOG_ERR(err, "Syntax error, on line %d", parser->current_token.line);
      goto cleanup;
    } else if (relation == 'e') {
      // Konec výrazu

      // Získání AST uzlu z vrcholu zásobníku; neodstraníme ho z uzlu, protože
      // stack_deep_dtor uvolní zbytek zásobníku i jeho ast_node.
      stack_top(&stack, &data_out);
      psa_stack_node_t *top_node = (psa_stack_node_t *)data_out;
      *out_ast = top_node->ast_node;
      top_node->ast_node = NULL;

      stack_deep_dtor(&stack, psa_stack_node_dtor);
      return IFJ_SUCCESS;
    }
  }

cleanup:
  if (*out_ast != NULL) {
    ast_free(*out_ast);
    *out_ast = NULL;
  }
  stack_deep_dtor(&stack, psa_stack_node_dtor);
  return err;
}

IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(parser, out_ast);

  return process_expression(parser, out_ast);
}