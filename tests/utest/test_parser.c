/**
 * @file test_parser.c
 * @brief IFJ26 - Jednotkové testy syntaktického analyzátoru
 *
 * @author xgleton00 Ondřej Glet
 *
 * Testy vytvořené pomocí AI (opencode, GPT-6 Luna) podle zadání:
 * „write cmocka tests for parser according to the grammar @gramatika.txt and
 * specification @Specifikace_IFJ26_a_IFJcode26.pdf, test syntax of prolog,
 * functions, types, blocks, commands, expression, control flow structures and
 * error states through the parser_parse from @src/parser.h, tests
 * shouldnt cover semantic errors
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "setup_test.h"

#include "parser.h"

static scanner_t scanner;
static FILE *input_file;
static FILE *original_stdin;

static void start_parser(const char *source, parser_t *parser) {
  input_file = fmemopen((void *)source, strlen(source), "r");
  assert_non_null(input_file);
  original_stdin = stdin;
  stdin = input_file;

  assert_int_equal(scanner_init(&scanner), IFJ_SUCCESS);
  assert_int_equal(parser_init(parser, &scanner), IFJ_SUCCESS);
}

static void stop_parser(parser_t *parser) {
  assert_int_equal(parser_destroy(parser), IFJ_SUCCESS);
  scanner_destroy(&scanner);
  fclose(input_file);
  stdin = original_stdin;
  input_file = NULL;
}

static IFJ_ERR parse_source(const char *source, ast_node_t **root) {
  parser_t parser;
  start_parser(source, &parser);
  IFJ_ERR err = parser_parse(&parser, root);
  stop_parser(&parser);
  return err;
}

static void test_valid_prolog(void **state) {
  (void)state;
  parser_t parser;
  ast_node_t *root = (ast_node_t *)1;

  start_parser("import ifj26.*\n", &parser);

  assert_int_equal(parser_parse(&parser, &root), IFJ_SUCCESS);
  assert_non_null(root);
  assert_int_equal(root->type, AST_ROOT);
  assert_int_equal(parser.current_token.type, TOK_EOF);
  ast_free(root);

  stop_parser(&parser);
}

static void test_valid_prolog_without_final_newline(void **state) {
  (void)state;
  parser_t parser;
  ast_node_t *root = (ast_node_t *)1;

  start_parser("import ifj26.*", &parser);

  assert_int_equal(parser_parse(&parser, &root), IFJ_SUCCESS);
  assert_non_null(root);
  assert_int_equal(root->type, AST_ROOT);
  ast_free(root);

  stop_parser(&parser);
}

static void test_invalid_prolog(void **state) {
  (void)state;
  const char *sources[] = {
      "include ifj26.*\n", "import other.*\n", "import ifj26\n",
      "import ifj26.*x\n", "import ifj26.\n",
  };

  for (size_t i = 0; i < sizeof sources / sizeof sources[0]; i++) {
    parser_t parser;
    ast_node_t *root = NULL;

    start_parser(sources[i], &parser);
    assert_int_equal(parser_parse(&parser, &root), IFJ_ESYNTAX);
    assert_null(root);
    stop_parser(&parser);
  }
}

static void test_parser_null_arguments(void **state) {
  (void)state;
  parser_t parser;
  ast_node_t *root = NULL;

  assert_int_equal(parser_init(NULL, &scanner), IFJ_EINTERNAL);
  assert_int_equal(parser_init(&parser, NULL), IFJ_EINTERNAL);
  assert_int_equal(parser_parse(NULL, &root), IFJ_EINTERNAL);
  assert_int_equal(parser_parse(&parser, NULL), IFJ_EINTERNAL);
  assert_int_equal(parser_destroy(NULL), IFJ_EINTERNAL);
}

static void test_function_headers_and_types(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(a: Int, b: List[String]): List[Int] = 1\n"
                       "def answer: Unit = ()\n"
                       "@main def run(): Unit = ()\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  assert_non_null(root);
  assert_int_equal(root->as.root.funcs_cnt, 3);

  ast_node_t *foo = root->as.root.funcs[0];
  assert_int_equal(foo->type, AST_FUNC);
  assert_string_equal(foo->as.func.id, "foo");
  assert_int_equal(foo->as.func.param_lists_cnt, 1);
  assert_int_equal(foo->as.func.param_lists[0]->as.param_list.params_cnt, 2);
  assert_int_equal(foo->as.func.param_lists[0]
                       ->as.param_list.params[0]
                       ->as.param.param_type.type,
                   TYPE_INT);
  assert_true(foo->as.func.param_lists[0]
                  ->as.param_list.params[1]
                  ->as.param.param_type.is_list);
  assert_int_equal(foo->as.func.return_type.type, TYPE_INT);
  assert_true(foo->as.func.return_type.is_list);

  ast_node_t *main = root->as.root.funcs[2];
  assert_true(main->as.func.main);
  assert_string_equal(main->as.func.id, "run");

  ast_free(root);
}

static void test_multiline_function_and_definitions(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "  val x: Int = 1\n"
                       "  var y = x\n"
                       "  y = y + 1\n"
                       "  x\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *func = root->as.root.funcs[0];
  assert_int_equal(func->as.func.body->type, AST_BLOCK);
  assert_int_equal(func->as.func.body->as.block.commands_cnt, 4);
  assert_int_equal(func->as.func.body->as.block.commands[0]->type, AST_DEF);
  assert_false(func->as.func.body->as.block.commands[0]->as.def.mut);
  assert_int_equal(func->as.func.body->as.block.commands[1]->type, AST_DEF);
  assert_true(func->as.func.body->as.block.commands[1]->as.def.mut);
  assert_int_equal(func->as.func.body->as.block.commands[2]->type, AST_ASSIGN);

  ast_free(root);
}

static void test_returns_and_expressions(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "  return\n"
                       "  return 1 + 2 * 3\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *body = root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands_cnt, 2);
  assert_int_equal(body->as.block.commands[0]->type, AST_FUNC_RET);
  assert_null(body->as.block.commands[0]->as.func_ret.rhs);
  assert_int_equal(body->as.block.commands[1]->type, AST_FUNC_RET);
  assert_non_null(body->as.block.commands[1]->as.func_ret.rhs);

  ast_free(root);
}

static void test_if_while_and_if_expression(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "  val x = if 1 < 2 then 1 else 2\n"
                       "  if x < 3 then\n"
                       "    ()\n"
                       "  else if x == 3 then\n"
                       "    ()\n"
                       "  else\n"
                       "    ()\n"
                       "  while x < 5 do\n"
                       "    ()\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *body = root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands_cnt, 3);
  assert_int_equal(body->as.block.commands[0]->type, AST_DEF);
  assert_int_equal(body->as.block.commands[0]->as.def.rhs->type, AST_IF);
  assert_int_equal(body->as.block.commands[1]->type, AST_IF);
  assert_int_equal(body->as.block.commands[1]->as.if_st.else_if_cnt, 1);
  assert_non_null(body->as.block.commands[1]->as.if_st.else_st);
  assert_int_equal(body->as.block.commands[2]->type, AST_WHILE);

  ast_free(root);
}

static void test_for_forms(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "  for i <- 1 to 3 by 1 do\n"
                       "    ()\n"
                       "  for\n"
                       "    j <- 0 until 3\n"
                       "    if j < 2\n"
                       "  do\n"
                       "    ()\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *body = root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands_cnt, 2);

  ast_node_t *inline_for = body->as.block.commands[0];
  assert_int_equal(inline_for->type, AST_FOR);
  assert_int_equal(inline_for->as.for_st.gen->type, AST_FOR_GEN);
  assert_non_null(inline_for->as.for_st.gen->as.for_gen.range);
  assert_non_null(
      inline_for->as.for_st.gen->as.for_gen.range->as.for_range.by_expr);

  ast_node_t *multiline_for = body->as.block.commands[1];
  assert_int_equal(multiline_for->type, AST_FOR);
  assert_int_equal(multiline_for->as.for_st.guards_cnt, 1);
  assert_true(
      multiline_for->as.for_st.gen->as.for_gen.range->as.for_range.until);

  ast_free(root);
}

static void test_newlines_inside_parentheses(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(\n"
                       "  first: Int,\n"
                       "  second: String,\n"
                       ") : Unit =\n"
                       "  print(\n"
                       "    first,\n"
                       "    second,\n"
                       "  )\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *function = root->as.root.funcs[0];
  assert_int_equal(function->as.func.param_lists_cnt, 1);
  assert_int_equal(function->as.func.param_lists[0]->as.param_list.params_cnt,
                   2);
  assert_int_equal(function->as.func.body->as.block.commands_cnt, 1);
  assert_int_equal(function->as.func.body->as.block.commands[0]->type,
                   AST_FUNC_CALL);

  ast_free(root);
}

static void test_parenthesized_expression_and_ignored_lines(void **state) {
  (void)state;
  const char *source = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "  // ignored command line\n"
                       "  val x = (1 +\n"
                       "    /* comment between continuation lines */\n"
                       "    (2 * 3))\n"
                       "\n"
                       "  /* ignored block comment */\n"
                       "  x\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(source, &root), IFJ_SUCCESS);
  ast_node_t *body = root->as.root.funcs[0]->as.func.body;
  assert_int_equal(body->as.block.commands_cnt, 2);
  assert_int_equal(body->as.block.commands[0]->type, AST_DEF);
  assert_int_equal(body->as.block.commands[1]->type, AST_VAR);

  ast_free(root);
}

static void test_multiline_block_comment(void **state) {
  (void)state;
  const char *valid = "import ifj26.*\n"
                      "def foo(): Unit =\n"
                      "  val /* same-line comment */ x = 1\n"
                      "  /* multiline comment\n"
                      "     still comment */\n"
                      "  x\n";
  ast_node_t *root = NULL;

  assert_int_equal(parse_source(valid, &root), IFJ_SUCCESS);
  assert_int_equal(root->as.root.funcs[0]->as.func.body->as.block.commands_cnt,
                   2);
  ast_free(root);

  const char *invalid = "import ifj26.*\n"
                        "def foo(): Unit =\n"
                        "  /* comment\n"
                        "     ends here */ val x = 1\n";
  root = NULL;
  assert_int_equal(parse_source(invalid, &root), IFJ_ESYNTAX);
  assert_null(root);
}

static void test_invalid_newline_and_indentation_rules(void **state) {
  (void)state;
  const char *syntax_errors[] = {
      "import ifj26.*\n"
      "def foo(): Unit =\n"
      "  val x = 1 +\n"
      "    2\n",

      "import ifj26.*\n"
      "def foo(): Unit =\n"
      "  val x = (1 +\n"
      "  2)\n",

      "import ifj26.*\n"
      "def foo(a: Int,) : Unit = ()\n",
  };

  for (size_t i = 0; i < sizeof syntax_errors / sizeof syntax_errors[0]; i++) {
    ast_node_t *root = NULL;
    assert_int_equal(parse_source(syntax_errors[i], &root), IFJ_ESYNTAX);
    assert_null(root);
  }

  const char *tabbed = "import ifj26.*\n"
                       "def foo(): Unit =\n"
                       "\t()\n";
  ast_node_t *root = NULL;
  assert_int_equal(parse_source(tabbed, &root), IFJ_ELEX);
  assert_null(root);
}

static void test_invalid_parser_syntax(void **state) {
  (void)state;
  const char *sources[] = {
      "import ifj26.*\n"
      "def foo(a) = ()\n",
      "import ifj26.*\n"
      "def foo(): Unit =\n",
      "import ifj26.*\n"
      "def foo(): Unit =\n"
      "  while 1 < 2\n"
      "    ()\n",
      "import ifj26.*\n"
      "def foo(): Unit =\n"
      "  for i <- 1 by 2 do\n"
      "    ()\n",
  };

  for (size_t i = 0; i < sizeof sources / sizeof sources[0]; i++) {
    ast_node_t *root = NULL;
    assert_int_equal(parse_source(sources[i], &root), IFJ_ESYNTAX);
    assert_null(root);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_valid_prolog),
      cmocka_unit_test(test_valid_prolog_without_final_newline),
      cmocka_unit_test(test_invalid_prolog),
      cmocka_unit_test(test_parser_null_arguments),
      cmocka_unit_test(test_function_headers_and_types),
      cmocka_unit_test(test_multiline_function_and_definitions),
      cmocka_unit_test(test_returns_and_expressions),
      cmocka_unit_test(test_if_while_and_if_expression),
      cmocka_unit_test(test_for_forms),
      cmocka_unit_test(test_newlines_inside_parentheses),
      cmocka_unit_test(test_parenthesized_expression_and_ignored_lines),
      cmocka_unit_test(test_multiline_block_comment),
      cmocka_unit_test(test_invalid_newline_and_indentation_rules),
      cmocka_unit_test(test_invalid_parser_syntax),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
