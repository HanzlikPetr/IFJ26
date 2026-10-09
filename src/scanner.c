/**
 * @file scanner.c
 * @brief IFJ26 - Implementace scanneru
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "scanner.h"

/// @brief Stavy konečného automatu
typedef enum {
  STATE_START,

  // Identifikátory a klíčová slova
  STATE_ID,

  // Čísla
  STATE_ZERO,
  STATE_INT,
  STATE_INT_UNDERSCORE,    // 1_
  STATE_DOT,               // .
  STATE_INT_DOT,           // 1.
  STATE_DOUBLE,            // 1.5
  STATE_DOUBLE_UNDERSCORE, // 1.5_
  STATE_EXP,               // 1e
  STATE_EXP_SIGN,          // 1e+
  STATE_EXP_NUM,           // 1e5
  STATE_EXP_UNDERSCORE,    // 1e5_

  // Řetězce
  STATE_QUOTE,         // "
  STATE_EMPTY_STRING,  // "" (prázdný, nebo začátek """)
  STATE_STRING,        // "abc
  STATE_STRING_ESCAPE, // "abc\ (esacape sekvence)
  STATE_ML_STRING,     // """abc
  STATE_ML_STRING_Q1,  // """abc"
  STATE_ML_STRING_Q2,  // """abc""

  // Lomítko a komentáře
  STATE_SLASH,               // /
  STATE_LINE_COMMENT,        // //...
  STATE_BLOCK_COMMENT,       // /*...
  STATE_BLOCK_COMMENT_STAR,  // /*...*
  STATE_BLOCK_COMMENT_SLASH, // /*.../

  // Víceznakové operátory
  STATE_LT,     // <
  STATE_GT,     // >
  STATE_ASSIGN, // =
  STATE_NOT,    // !
  STATE_PLUS,   // +
  STATE_AMP,    // &
  STATE_PIPE,   // |

  // @main
  STATE_AT,    // @
  STATE_AT_ID, // @xxx (po dočtení se porovná s "main")
} scanner_state_t;

/// @brief Tabulka klíčových slov, index odpovídá hodnotě keyword_t.
static const char *const keywords[] = {
    [KW_DEF] = "def",
    [KW_DO] = "do",
    [KW_ELSE] = "else",
    [KW_DOUBLE] = "Double",
    [KW_IF] = "if",
    [KW_INT] = "Int",
    [KW_RETURN] = "return",
    [KW_STRING] = "String",
    [KW_IMPORT] = "import",
    [KW_WHILE] = "while",
    [KW_FOR] = "for",
    [KW_THEN] = "then",
    [KW_UNIT] = "Unit",
    [KW_VAR] = "var",
    [KW_VAL] = "val",
    [KW_BOOLEAN] = "Boolean",
    [KW_TRUE] = "true",
    [KW_FALSE] = "false",
    [KW_LIST] = "List",
    [KW_CASE] = "case",
    [KW_MATCH] = "match",
    [KW_CLASS] = "class",
    [KW_EXTENSION] = "extension",
    [KW_TYPE] = "type",
};

// Vnitřní stav scanneru
static FILE *input = NULL;  // Vstupní soubor
static int line = 1;        // Číslo aktuálního řádku

IFJ_ERR scanner_init(FILE *file) {
  if (file == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Input file is NULL");
    return IFJ_EINTERNAL;
  }
  input = file;
  line = 1;
  return IFJ_SUCCESS;
}

void scanner_destroy(void) { input = NULL; }

void token_free(token_t *token) {
  if (token == NULL) {
    return;
  }
  if (token->type == TOK_STRING || token->type == TOK_ID) {
    free(token->value.str_value);
    token->value.str_value = NULL;
  }
}

IFJ_ERR scanner_next_token(token_t *token) {
  scanner_state_t state = STATE_START;
  token->line = line;

  while (true) {
    int c = fgetc(input);

    switch (state) {
    case STATE_START:
      switch (c) {
      case ' ':
        break;

      case '(':
        token->type = TOK_LPAREN;
        return IFJ_SUCCESS;

      case ')':
        token->type = TOK_RPAREN;
        return IFJ_SUCCESS;

      case '[':
        token->type = TOK_LBRACKET;
        return IFJ_SUCCESS;

      case ']':
        token->type = TOK_RBRACKET;
        return IFJ_SUCCESS;

      case ',':
        token->type = TOK_COMMA;
        return IFJ_SUCCESS;

      case ':':
        token->type = TOK_COLON;
        return IFJ_SUCCESS;

      case ';':
        token->type = TOK_SEMICOLON;
        return IFJ_SUCCESS;

      case '-':
        token->type = TOK_SUB;
        return IFJ_SUCCESS;

      case '*':
        token->type = TOK_MUL;
        return IFJ_SUCCESS;

      case EOF:
        token->type = TOK_EOF;
        return IFJ_SUCCESS;

      case '\n':
        token->type = TOK_EOL;
        line++;
        return IFJ_SUCCESS;

      case '<':
        state = STATE_LT;
        break;

      case '>':
        state = STATE_GT;
        break;

      case '=':
        state = STATE_ASSIGN;
        break;

      case '!':
        state = STATE_NOT;
        break;

      case '+':
        state = STATE_PLUS;
        break;  

      case '&':
        state = STATE_AMP;
        break;
        
      case '|':
        state = STATE_PIPE;
        break;

      default:
        return IFJ_ELEX;
      }
      break;

    case STATE_LT:
      switch (c)
      {
      case '=':
        token->type = TOK_LEQ;
        return IFJ_SUCCESS;

      case '-':
        token->type = TOK_LARROW;
        return IFJ_SUCCESS;
      
      default:
        ungetc(c, input);
        token->type = TOK_LT;
        return IFJ_SUCCESS;
      }

      case STATE_GT:
      switch (c)
      {
      case '=':
        token->type = TOK_GEQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_GT;
        return IFJ_SUCCESS;
      }

      case STATE_ASSIGN:
      switch (c)
      {
      case '=':
        token->type = TOK_EQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_ASSIGN;
        return IFJ_SUCCESS;
      }

      case STATE_NOT:
      switch (c)
      {
      case '=':
        token->type = TOK_NEQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_NOT;
        return IFJ_SUCCESS;
      }

      case STATE_PLUS:
      switch (c)
      {
      case '+':
        token->type = TOK_CONCAT;
        return IFJ_SUCCESS; 
      
        default:
        ungetc(c, input);
        token->type = TOK_ADD;
        return IFJ_SUCCESS;
      }

      case STATE_AMP:
      switch (c)
      {
      case '&':
        token->type = TOK_AND;
        return IFJ_SUCCESS; 

        default:
        return IFJ_ELEX;
      }

      case STATE_PIPE:
      switch (c)
      {
      case '|':
        token->type = TOK_OR;
        return IFJ_SUCCESS; 

        default:
        return IFJ_ELEX;
      }

    default:
      return IFJ_EINTERNAL;
    }
  }
}
