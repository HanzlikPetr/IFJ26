/**
 * @file compiler.c
 * @brief IFJ26 - Implementace kompilátoru (main)
 *
 * @author xhanzlp00 Petr Hanzlík, xgleton00 Ondřej Glet
 */

#include "ast.h"
#include "error.h"
#include "parser.h"
#include "scanner.h"

int main() {
  scanner_t scanner = {0};
  parser_t parser = {0};
  ast_node_t *root = NULL;
  IFJ_ERR err = scanner_init(&scanner);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_init(&parser, &scanner);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_parse(&parser, &root);

cleanup:
  ast_free(root);
  parser_destroy(&parser);
  scanner_destroy(&scanner);

  return err;
}
