/**
 * @file parser.h
 * @brief IFJ26 - Definice parseru
 *
 * @author xgleton00 Ondřej Glet
 */

#include "scanner.h"

#ifndef IFJ_PARSER_H
#define IFJ_PARSER_H

typedef struct {
  scanner_t *scanner;
  token_t current_token;
} parser_t;

#endif
