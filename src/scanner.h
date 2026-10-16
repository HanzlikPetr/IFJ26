/**
 * @file scanner.h
 * @brief IFJ26 - Definice scanneru a tokenů
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stdio.h>

#include "error.h"

/**
 * @brief Klíčová slova jazyka IFJ26
 */
typedef enum {
  KW_DEF,
  KW_DO,
  KW_ELSE,
  KW_DOUBLE,
  KW_IF,
  KW_INT,
  KW_RETURN,
  KW_STRING,
  KW_IMPORT,
  KW_WHILE,
  KW_FOR,
  KW_THEN,
  KW_UNIT,
  KW_VAR,
  KW_VAL,
  KW_BOOLEAN,
  KW_TRUE,
  KW_FALSE,
  KW_LIST,
  KW_CASE,
  KW_MATCH,
  KW_CLASS,
  KW_EXTENSION,
  KW_TYPE,
} keyword_t;

/**
 * @brief Typy tokenů vracených scannerem
 */
typedef enum {
  TOK_EOF,
  TOK_EOL,
  TOK_ID,
  TOK_KEYWORD,
  TOK_INT,
  TOK_FLOAT,
  TOK_STRING,

  TOK_ASSIGN,
  TOK_EQ,
  TOK_NEQ,
  TOK_LT,
  TOK_GT,
  TOK_LEQ,
  TOK_GEQ,

  TOK_ADD,
  TOK_SUB,
  TOK_MUL,
  TOK_DIV,

  TOK_NOT,
  TOK_AND,
  TOK_OR,

  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACKET,
  TOK_RBRACKET,
  TOK_COMMA,
  TOK_COLON,
  TOK_DOT,
  TOK_SEMICOLON,

  TOK_CONCAT,
  TOK_LARROW,     // <-
  TOK_ANNOT_MAIN, // @main

  TOK_INDENT, // začátek odsazení
  TOK_DEDENT, // konec odsazení
} token_type_t;

/**
 * @brief Hodnota tokenu
 */
typedef union {
  long long int_value;
  double double_value;
  char *str_value;
  keyword_t keyword;
} token_value_t;

/**
 * @brief Token predavany parseru
 */
typedef struct {
  token_type_t type;
  token_value_t value;
  int line;
} token_t;

/**
 * @brief Inicializace scanneru
 * @return IFJ_ERR úspěch, nebo chyba při alokaci
 */
IFJ_ERR scanner_init(void);

/**
 * @brief Získani následujícího tokenu
 * @param token Výstupní token
 * @return IFJ_ERR úspěch, lexikální chyba, nebo interní chyba
 */
IFJ_ERR scanner_next_token(token_t *token);

/**
 * @brief Uvolnění paměti tokenu
 * @param token Token k uvolnění
 */
void token_free(token_t *token);

/**
 * @brief Uvolnění vnitřní struktury scanneru
 */
void scanner_destroy(void);

#endif
