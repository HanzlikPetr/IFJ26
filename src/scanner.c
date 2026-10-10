/**
 * @file scanner.c
 * @brief IFJ26 - Implementace scanneru
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstring.h"
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
#define KEYWORD_COUNT (sizeof keywords / sizeof keywords[0])

// Vnitřní stav scanneru
static FILE *input = NULL;  // Vstupní soubor
static int line = 1;        // Číslo aktuálního řádku
static bool pending_dot = false; // Příští token je tečka (1.)

IFJ_ERR scanner_init(FILE *file) {
  if (file == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Input file is NULL");
    return IFJ_EINTERNAL;
  }
  input = file;
  line = 1;
  pending_dot = false;
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
  if (pending_dot) {  // případ "1.", zbyla tečka
    pending_dot = false;
    token->line = line;
    token->type = TOK_DOT;
    return IFJ_SUCCESS;
  }

  scanner_state_t state = STATE_START;
  dstring_t str;
  int comment_depth = 0;

  while (true) {
    int c = fgetc(input);

    switch (state) {
    case STATE_START:
      token->line = line;
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

      case '/':
        state = STATE_SLASH;
        break;

      case '.':
        state = STATE_DOT;
        break;

      default:
        // Písmena
        if (isalpha(c) || c == '_') {
          IFJ_ERR err = dstring_init(&str);
          if (err != IFJ_SUCCESS) {
            return err;
          }

          err = dstring_append(&str, (char)c);
          if (err != IFJ_SUCCESS) {
            dstring_free(&str);
            return err;
          }
          state = STATE_ID;
          break;
        }
        // Čísla
        if (isdigit(c)) {
          IFJ_ERR err = dstring_init(&str);
          if (err != IFJ_SUCCESS) {
            return err;
          }
          err = dstring_append(&str, (char)c);
          if (err != IFJ_SUCCESS) {
            dstring_free(&str);
            return err;
          }
          if (c == '0') {
            state = STATE_ZERO;
          } else {
            state = STATE_INT;
          }
          break;
        }

        return IFJ_ELEX;
      }
      break;

    case STATE_LT:
      switch (c) {
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
      switch (c) {
      case '=':
        token->type = TOK_GEQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_GT;
        return IFJ_SUCCESS;
      }

    case STATE_ASSIGN:
      switch (c) {
      case '=':
        token->type = TOK_EQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_ASSIGN;
        return IFJ_SUCCESS;
      }

    case STATE_NOT:
      switch (c) {
      case '=':
        token->type = TOK_NEQ;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_NOT;
        return IFJ_SUCCESS;
      }

    case STATE_PLUS:
      switch (c) {
      case '+':
        token->type = TOK_CONCAT;
        return IFJ_SUCCESS;

      default:
        ungetc(c, input);
        token->type = TOK_ADD;
        return IFJ_SUCCESS;
      }

    case STATE_AMP:
      switch (c) {
      case '&':
        token->type = TOK_AND;
        return IFJ_SUCCESS;

      default:
        return IFJ_ELEX;
      }

    case STATE_PIPE:
      switch (c) {
      case '|':
        token->type = TOK_OR;
        return IFJ_SUCCESS;

      default:
        return IFJ_ELEX;
      }

    case STATE_ID:
      if (isalnum(c) || c == '_') {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
      } else {
        ungetc(c, input);

        if (str.len == 1 && str.data[0] == '_') {
          dstring_free(&str);
          return IFJ_ELEX;
        }
        // Zkontrolovat jestli je to klíčové slovo
        for (size_t i = 0; i < KEYWORD_COUNT; i++) {
          if (keywords[i] != NULL && strcmp(str.data, keywords[i]) == 0) {
            token->type = TOK_KEYWORD;
            token->value.keyword = (keyword_t)i;
            dstring_free(&str);
            return IFJ_SUCCESS;
          }
        }
        // Není to klíčové slovo, je to identifikátor
        token->type = TOK_ID;
        token->value.str_value = str.data;
        return IFJ_SUCCESS;
      }
      break;

    case STATE_SLASH:
      switch (c) {
      case '/':
        state = STATE_LINE_COMMENT;
        break;

      case '*':
        comment_depth = 1;
        state = STATE_BLOCK_COMMENT;
        break;

      default:
        ungetc(c, input);
        token->type = TOK_DIV;
        return IFJ_SUCCESS;
      }
      break;

    case STATE_LINE_COMMENT:
      if (c == '\n' || c == EOF) {
        ungetc(c, input);
        state = STATE_START;
      }
      break;

    case STATE_BLOCK_COMMENT:
      switch (c) {
      case '*':
        state = STATE_BLOCK_COMMENT_STAR;
        break;

      case '/':
        state = STATE_BLOCK_COMMENT_SLASH;
        break;

      case '\n':
        line++;
        break;

      case EOF:
        return IFJ_ELEX;

      default:
        break;
      }
      break;

    case STATE_BLOCK_COMMENT_STAR:
      switch (c) {
      case '/':
        comment_depth--;
        if (comment_depth == 0) {
          state = STATE_START;
        } else {
          state = STATE_BLOCK_COMMENT;
        }
        break;

      case '*':
        break;

      case '\n':
        line++;
        state = STATE_BLOCK_COMMENT;
        break;

      case EOF:
        return IFJ_ELEX;

      default:
        state = STATE_BLOCK_COMMENT;
        break;
      }
      break;

    case STATE_BLOCK_COMMENT_SLASH:
      switch (c) {
      case '*':
        comment_depth++;
        state = STATE_BLOCK_COMMENT;
        break;

      case '/':
        break;

      case '\n':
        line++;
        state = STATE_BLOCK_COMMENT;
        break;

      case EOF:
        return IFJ_ELEX;

      default:
        state = STATE_BLOCK_COMMENT;
        break;
      }
      break;

    case STATE_INT:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
      }

      else if (c == '_') {
        state = STATE_INT_UNDERSCORE;
      }

      else if (c == '.') {
        IFJ_ERR err = dstring_append(&str, '.');
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_INT_DOT;
      }

      else if (c == 'e' || c == 'E') {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP;
      }

      else {
        ungetc(c, input);

        token->type = TOK_INT;
        token->value.int_value = strtoll(str.data, NULL, 10);

        dstring_free(&str);
        return IFJ_SUCCESS;
      }

      break;

    case STATE_INT_UNDERSCORE:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_INT;

      } else if (c == '_') {
        break;
      } else {
        dstring_free(&str);
        return IFJ_ELEX;
      }
      break;

    case STATE_ZERO:
      if (isdigit(c) || c == '_') {
        dstring_free(&str);
        return IFJ_ELEX;
      } else if (c == '.') {
        IFJ_ERR err = dstring_append(&str, '.');
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_INT_DOT;
      } else if (c == 'e' || c == 'E') {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP;
      }

      else {
        ungetc(c, input);
        token->type = TOK_INT;
        token->value.int_value = 0;
        dstring_free(&str);
        return IFJ_SUCCESS;
      }
      break;

    case STATE_INT_DOT:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_DOUBLE;
      } else {
        ungetc(c, input);
        pending_dot = true;
        token->type = TOK_INT;
        token->value.int_value = strtoll(str.data, NULL, 10);
        dstring_free(&str);
        return IFJ_SUCCESS;
      }
      break;

    case STATE_DOUBLE:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
      } else if (c == '_') {
        state = STATE_DOUBLE_UNDERSCORE;
      } else if (c == 'e' || c == 'E') {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP;
      }
      else if (c == 'd' || c == 'D') {
        token->type = TOK_FLOAT;
        token->value.double_value = strtod(str.data, NULL);
        dstring_free(&str);
        return IFJ_SUCCESS;
      }

      else {
        ungetc(c, input);
        token->type = TOK_FLOAT;
        token->value.double_value = strtod(str.data, NULL);
        dstring_free(&str);
        return IFJ_SUCCESS;
      }
      break;

    case STATE_DOUBLE_UNDERSCORE:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_DOUBLE;

      } else if (c == '_') {
        // zůstáváme, další podtržítko
      } else {
        dstring_free(&str);
        return IFJ_ELEX;
      }
      break;

    case STATE_DOT:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_init(&str);
        if (err != IFJ_SUCCESS) {
          return err;
        }
        err = dstring_append(&str, '.');
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }

        err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }

        state = STATE_DOUBLE;

      } else {
        ungetc(c, input);
        token->type = TOK_DOT;
        return IFJ_SUCCESS;
      }
      break;

    case STATE_EXP:
      if (c == '+' || c == '-') {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP_SIGN;
      } else if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP_NUM;
      } else {
        dstring_free(&str);
        return IFJ_ELEX;
      }
      break;

    case STATE_EXP_SIGN:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP_NUM;
      } else {
        dstring_free(&str);
        return IFJ_ELEX;
      }
      break;

    case STATE_EXP_NUM:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
      } else if (c == '_') {
        state = STATE_EXP_UNDERSCORE;
      } 
      else if (c == 'd' || c == 'D') {
        token->type = TOK_FLOAT;
        token->value.double_value = strtod(str.data, NULL);
        dstring_free(&str);
        return IFJ_SUCCESS;
      }
      else {
        ungetc(c, input);
        token->type = TOK_FLOAT;
        token->value.double_value = strtod(str.data, NULL);
        dstring_free(&str);
        return IFJ_SUCCESS;
      }
      break;

    case STATE_EXP_UNDERSCORE:
      if (isdigit(c)) {
        IFJ_ERR err = dstring_append(&str, (char)c);
        if (err != IFJ_SUCCESS) {
          dstring_free(&str);
          return err;
        }
        state = STATE_EXP_NUM;

      } else if (c == '_') {
        // zůstáváme, další podtržítko
      } else {
        dstring_free(&str);
        return IFJ_ELEX;
      }
      break;

    default:
      return IFJ_EINTERNAL;
    }
  }
}
