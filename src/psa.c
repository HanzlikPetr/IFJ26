/**
 * @file psa.c
 * @brief IFJ26 - Implementace precedenční syntaktické analýzy (PSA)
 *
 * @author xhanzlp00 Petr Hanzlík
 */

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

/**
 * @brief Posun tokenů při zpracování výrazu
 * @param current_token Aktuální token
 * @param next_token Následující token
 * @param scanner Stav scanneru
 * @return IFJ_ERR úspěch nebo chybový kód
 */
static IFJ_ERR shift_tokens(token_t *current_token, token_t *next_token,
                            scanner_t *scanner) {
  *current_token = *next_token;
  return scanner_next_token(scanner, next_token);
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

static token_t *copy_token(const token_t *source) {
  token_t *copy;

  if (source == NULL) {
    return NULL;
  }

  copy = malloc(sizeof(*copy));
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
          realloc(nodes, new_capacity * sizeof(*new_nodes));
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

/**
 * @brief Zpracování výrazu pomocí precedenční analýzy
 * @param parser Parser
 * @param out_ast Výstupní AST uzel
 * @return IFJ_SUCCESS při úspěch nebo chybový kód
 */
static IFJ_ERR process_expression(parser_t *parser, ast_node_t **out_ast) {
  IFJ_ERR err;
  stack_t stack;
  token_t current_token, next_token;
  bool expect_operand, found_handle, is_empty, found_nonterm;
  int open_parens;
  char relation;
  token_t *token = NULL;
  psa_symbol_t stack_top_symbol, input_symbol;
  psa_stack_node_t *push_node,
      *stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE], // Pole pro uchování
                                                        // uzlů zásobníku při
                                                        // redukci
      *stack_array_get_nonterm
          [STACK_TOP_ARRAY_NONTERM_SIZE]; // Pole pro uchování uzlů zásobníku
                                          // při získávání neterminálu
  int stack_array_reduce_count = 0, stack_array_get_nonterm_count = 0;
  void *data_out;

  // Inicializace pomocných proměnných
  current_token = parser->current_token;
  CHECK_ERR_RET(scanner_next_token(parser->scanner, &next_token));
  expect_operand = true; // Na začátku výrazu očekáváme operand
  // Počet otevřených závorek (pro správné zpracování konce výrazu)
  open_parens = 0;

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

  while (1) {
    // Získání symbolu na vrcholu zásobníku, který není neterminál (E) nebo
    // zarážka (<)
    stack_array_get_nonterm_count = 0;
    data_out = NULL;

    for (int i = 0; i < STACK_TOP_ARRAY_NONTERM_SIZE; i++) {
      err = stack_top(&stack, &data_out);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get top of stack");

      push_node = (psa_stack_node_t *)data_out;

      if (push_node->symbol != PSA_NONTERM && push_node->symbol != PSA_HANDLE) {
        // Nalezen symbol, který není neterminál ani zarážka
        stack_top_symbol = push_node->symbol;
        found_nonterm = true;
        break;
      }

      err = stack_pop(&stack, &data_out);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to pop from stack");

      stack_array_get_nonterm[i] = push_node;
      stack_array_get_nonterm_count++;
    }

    if (!found_nonterm) {
      // Pokud nebyl nalezen symbol, který není neterminál ani zarážka, tak je
      // to chyba
      err = IFJ_ESYNTAX;
      LOG_ERR(err, "Failed to find a valid symbol on the stack");
      goto cleanup;
    }

    if (stack_array_get_nonterm_count == 1) {
      err = stack_push(&stack, (void *)stack_array_get_nonterm[0]);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push symbol onto the stack");
    } else if (stack_array_get_nonterm_count > 1) {
      // Pokud bylo nalezeno více než 1 symbol, tak je to chyba
      err = IFJ_ESYNTAX;
      CHECK_ERR_LOG_GOTO(err, cleanup, "Expected 1 symbol, got %d",
                         stack_array_get_nonterm_count);
    }

    // Získání symbolu z aktuálního tokenu a vyhledání vztahu v tabulce PSA
    input_symbol =
        token2psa_symbol(&current_token, expect_operand, open_parens);
    relation = psa_table_lookup(stack_top_symbol, input_symbol);

    CHECK_ERR_GOTO(traverse_stack(&stack), cleanup);

    if (relation == '<') {
      // Vložení zarážky (<) za neterminální symbol na zásobník
      stack_array_get_nonterm_count = 0;
      data_out = NULL;
      for (int i = 0; i < STACK_TOP_ARRAY_NONTERM_SIZE; i++) {
        err = stack_top(&stack, &data_out);
        CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to get top of stack");

        push_node = (psa_stack_node_t *)data_out;

        if (push_node->symbol != PSA_NONTERM &&
            push_node->symbol != PSA_HANDLE) {
          // Nalezen symbol, který není neterminál ani zarážka
          stack_top_symbol = push_node->symbol;
          found_nonterm = true;
          break;
        }

        err = stack_pop(&stack, &data_out);
        CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to pop from stack");

        stack_array_get_nonterm[i] = push_node;
        stack_array_get_nonterm_count++;
      }

      if (!found_nonterm) {
        // Pokud nebyl nalezen symbol, který není neterminál ani zarážka, tak je
        // to chyba
        err = IFJ_ESYNTAX;
        LOG_ERR(err, "Failed to find a valid symbol on the stack");
        goto cleanup;
      }

      // Vyvoření zarážky (<) na zásobník
      push_node = create_psa_stack_node(PSA_HANDLE, NULL, NULL);
      CHECK_MEM_ERR_GOTO(push_node, cleanup);
      err = stack_push(&stack, (void *)push_node);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push handle onto the stack");

      if (stack_array_get_nonterm_count == 1) {
        err = stack_push(&stack, (void *)stack_array_get_nonterm[0]);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to push symbol onto the stack");
      }

      // Zde by se měl vytvořit AST uzel pro operand nebo operátor
      // Pokud je to identifikátor, tak by se sem měl uložit jeho název

      // Vložení symbolu z vstupu na zásobník
      token = copy_token(&parser->current_token);
      CHECK_MEM_ERR_GOTO(token, cleanup);
      push_node = create_psa_stack_node(input_symbol, NULL, token);
      CHECK_MEM_ERR_GOTO(push_node, cleanup);
      err = stack_push(&stack, (void *)push_node);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push symbol onto the stack");

      // Aktualizace počtu otevřených závoreks
      if (input_symbol == PSA_L_PAREN) {
        open_parens++;
      } else if (input_symbol == PSA_R_PAREN) {
        open_parens--;
      }

      // Aktualizace expect_operand
      if (input_symbol == PSA_ID || input_symbol == PSA_R_PAREN) {
        expect_operand = false; // Přečetli jsme hodnotu nebo ')', další '-'
                                // bude binární odčítání
      } else {
        expect_operand = true; // Přečetli jsme operátor nebo '(', další '-'
                               // bude unární minus
      }

      // Posun tokenů
      err = shift_tokens(&current_token, &next_token, parser->scanner);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to shift tokens");
    } else if (relation == '>') {
      // Pravidla:
      // E -> id (<ID)
      // E -> (E) (<L_PAREN E R_PAREN)
      // E -> E op E (<E op E)
      // E -> op E (<op E)

      // Získání symbolů ze zásobníku pro redukci
      found_handle = false;
      stack_array_reduce_count = 0;
      data_out = NULL;
      err = stack_is_empty(&stack, &is_empty);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to check if stack is empty");

      for (int i = 0; i < STACK_TOP_ARRAY_REDUCE_SIZE && !is_empty; i++) {
        err = stack_is_empty(&stack, &is_empty);
        CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to check if stack is empty");
        if (is_empty) {
          break;
        }

        err = stack_pop(&stack, &data_out);
        CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to pop from stack");

        stack_array_reduce[i] = (psa_stack_node_t *)data_out;
        stack_array_reduce_count++;

        if (stack_array_reduce[i]->symbol == PSA_HANDLE) {
          // Zastavení při nalezení zarážky
          found_handle = true;
          break;
        }
      }

      if (!found_handle) {
        // Pokud nebyla nalezena zarážka, zkontrolujeme, zda-li se jedná o konec
        // výrazu
        printf("End");
        if (input_symbol == PSA_DOLLAR &&
            stack_array_reduce_count == STACK_TOP_ARRAY_REDUCE_SIZE - 2 &&
            stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 1]->symbol ==
                PSA_NONTERM &&
            stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 2]->symbol ==
                PSA_DOLLAR) {
          // Konec výrazu, úspěšně jsme zpracovali celý výraz
          break;
        }

        err = IFJ_ESYNTAX;
        LOG_ERR(err, "Failed to find a handle symbol on the stack");
        goto cleanup;
      }

      if (stack_array_reduce[0]->symbol == PSA_ID) {
        // Redukce E -> id
        // Vytvoření AST uzlu pro identifikátor
        // ast_node_t *id_node;
        // CHECK_ERR_GOTO(ast_create_node(&id_node, AST_EXPR), cleanup);
        // stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 1]->ast_node =
        // id_node;
        printf("Redukce E -> id\n");
      } else if (stack_array_reduce_count == 4 &&
                 stack_array_reduce[0]->symbol == PSA_R_PAREN &&
                 stack_array_reduce[1]->symbol == PSA_NONTERM &&
                 stack_array_reduce[2]->symbol == PSA_L_PAREN) {
        // Redukce E -> (E)
        // Vytvoření AST uzlu pro závorky
        // ast_node_t *paren_node;
        // CHECK_ERR_GOTO(ast_create_node(&paren_node, AST_EXPR), cleanup);
        // stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 2]->ast_node =
        // paren_node;
        printf("Redukce E -> (E)\n");
      } else if (stack_array_reduce_count == 4 &&
                 stack_array_reduce[0]->symbol == PSA_NONTERM &&
                 (stack_array_reduce[1]->symbol == PSA_ADD_SUB ||
                  stack_array_reduce[1]->symbol == PSA_MUL_DIV ||
                  stack_array_reduce[1]->symbol == PSA_DOT ||
                  stack_array_reduce[1]->symbol == PSA_EQ_OP ||
                  stack_array_reduce[1]->symbol == PSA_REL_OP ||
                  stack_array_reduce[1]->symbol == PSA_AND_OP ||
                  stack_array_reduce[1]->symbol == PSA_OR_OP) &&
                 stack_array_reduce[2]->symbol == PSA_NONTERM) {
        // Redukce E -> E op E
        // Vytvoření AST uzlu pro binární operaci
        // ast_node_t *bin_op_node;
        // CHECK_ERR_GOTO(ast_create_node(&bin_op_node, AST_EXPR), cleanup);
        // stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 3]->ast_node =
        // bin_op_node;
        printf("Redukce E -> E op E\n");
      } else if (stack_array_reduce_count == 3 &&
                 stack_array_reduce[0]->symbol == PSA_NONTERM &&
                 stack_array_reduce[1]->symbol == PSA_UNARY) {
        // Redukce E -> op E
        // Vytvoření AST uzlu pro unární operaci
        // ast_node_t *unary_op_node;
        // CHECK_ERR_GOTO(ast_create_node(&unary_op_node, AST_EXPR), cleanup);
        // stack_array_reduce[STACK_TOP_ARRAY_REDUCE_SIZE - 4]->ast_node =
        // unary_op_node;
        printf("Redukce E -> op E\n");
      } else {
        err = IFJ_ESYNTAX;
        LOG_ERR(err, "Failed to match a reduction rule for the stack symbols");
        goto cleanup;
      }

      // Vložení neterminálu E na zásobník
      push_node = create_psa_stack_node(PSA_NONTERM, NULL, NULL);
      CHECK_MEM_ERR_GOTO(push_node, cleanup);
      err = stack_push(&stack, (void *)push_node);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push node onto stack");

    } else if (relation == '=') {
      // Vložení symbolu z vstupu na zásobník
      token = copy_token(&parser->current_token);
      CHECK_MEM_ERR_GOTO(token, cleanup);
      push_node = create_psa_stack_node(input_symbol, NULL, token);
      CHECK_MEM_ERR_GOTO(push_node, cleanup);
      err = stack_push(&stack, (void *)push_node);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to push node onto stack");

      // Aktualizace počtu otevřených závoreks
      if (input_symbol == PSA_L_PAREN) {
        open_parens++;
      } else if (input_symbol == PSA_R_PAREN) {
        open_parens--;
      }

      // Aktualizace expect_operand
      if (input_symbol == PSA_ID || input_symbol == PSA_R_PAREN) {
        expect_operand = false; // Přečetli jsme hodnotu nebo ')', další '-'
                                // bude binární odčítání
      } else {
        expect_operand = true; // Přečetli jsme operátor nebo '(', další '-'
                               // bude unární minus
      }

      // Posun tokenů
      err = shift_tokens(&current_token, &next_token, parser->scanner);
      CHECK_ERR_LOG_GOTO(err, cleanup, "Failed to shift tokens");
    } else if (relation == 'f') {
      // Syntaktická chyba
      err = IFJ_ESYNTAX;
      LOG_ERR(err, "Syntax error");
      goto cleanup;
    } else if (relation == 'e') {
      // Konec výrazu
      break;
    }
  }

  return IFJ_SUCCESS;
  
cleanup:
  /*dodelat*/
  return err;
}

IFJ_ERR psa_parse_expression(parser_t *parser, ast_node_t **out_ast) {
  CHECK_ARGS_NULL_RET(parser, out_ast);

  return process_expression(parser, out_ast);
}