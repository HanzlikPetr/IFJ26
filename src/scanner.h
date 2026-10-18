/**
 * @file scanner.h
 * @brief IFJ26 - Definice scanneru a tokenů
 *
 * @author xjurokv00 Vojtěch Jurok
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stdio.h>

#include "error.h"
#include "stack.h"

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
  bool newline_before; // Před tokenem byl uvnitř závorek konec řádku
} token_t;

/**
 * @brief Vnitřní stav scanneru, drží si ho volající.
 */
typedef struct {
  /** Číslo aktuálního řádku. */
  int line;
  /** Příští token je tečka (po "1."). */
  bool pending_dot;
  /** Od posledního '\n' byly jen mezery. */
  bool at_line_start;
  /** Počet otevřených závorek ( a [. */
  int paren_depth;
  /** Počet mezer na začátku aktuálního řádku. */
  int line_indent;
  /** Kolik DEDENTů ještě zbývá vrátit. */
  int pending_dedents;
  /** Po víceřádkovém komentáři smí být jen konec řádku. */
  bool expect_line_end;
  /** Úrovně odsazení otevřených bloků. */
  stack_t indent_stack;
} scanner_t;

/**
 * @brief Inicializace scanneru
 * @param scanner Výstup: inicializovaný stav scanneru
 * @return IFJ_ERR úspěch, NULL argument, nebo chyba při alokaci
 */
IFJ_ERR scanner_init(scanner_t *scanner);

/**
 * @brief Získani následujícího tokenu
 * @param scanner Stav scanneru
 * @param token Výstupní token
 * @return IFJ_ERR úspěch, lexikální chyba, nebo interní chyba
 */
IFJ_ERR scanner_next_token(scanner_t *scanner, token_t *token);

/**
 * @brief Uvolnění paměti tokenu
 * @param token Token k uvolnění
 * @return IFJ_SUCCESS úspěch, nebo IFJ_EINTERNAL pokud je token NULL
 */
IFJ_ERR token_free(token_t *token);

/**
 * @brief Uvolnění vnitřní struktury scanneru
 * @param scanner Stav scanneru (NULL nic neudělá)
 */
void scanner_destroy(scanner_t *scanner);

#endif