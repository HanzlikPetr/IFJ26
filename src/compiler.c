/**
 * @file compiler.c
 * @brief IFJ26 - Implementace kompilátoru (main)
 *
 * @author xhanzlp00 Petr Hanzlík, xgleton00 Ondřej Glet
 */

#include "ast.h"
#include "error.h"
#include "generator.h"
#include "parser.h"
#include "scanner.h"
#include "semantic_analy.h"
#include "symtable.h"

int main() {
  scanner_t scanner = {0};
  parser_t parser = {0};
  ast_node_t *root = NULL;
  symtable_t functions_table = {0};

  IFJ_ERR err = scanner_init(&scanner);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_init(&parser, &scanner);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_parse(&parser, &root);
  CHECK_ERR_GOTO(err, cleanup);

  err = symtable_init(&functions_table);
  CHECK_ERR_GOTO(err, cleanup);

  err = symtable_fill_funcitons(&functions_table, root);
  CHECK_ERR_GOTO(err, cleanup);

  err = check_functions(root, &functions_table);
  CHECK_ERR_GOTO(err, cleanup);

  err = generator_generate(root);
  CHECK_ERR_GOTO(err, cleanup);

cleanup:
  ast_free(root);
  parser_destroy(&parser);
  scanner_destroy(&scanner);
  symtable_destroy(&functions_table);

  return err;
}
