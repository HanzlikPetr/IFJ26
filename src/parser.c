/**
 * @file parser.c
 * @brief IFJ26 - Implementace rekurzivního syntaktického analyzátoru
 *
 * @author xgleton00 Ondřej Glet
 */

#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "psa.h"

/**
 * @brief Načte další token do parseru.
 *
 * @param parser Parser, jehož aktuální token se má posunout.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_next(parser_t *parser) {
  CHECK_ARG_NULL_RET(parser);

  CHECK_ERR_RET(token_free(&parser->current_token));

  parser->current_token.type = TOK_EOF;
  parser->current_token.value.str_value = NULL;
  return scanner_next_token(parser->scanner, &parser->current_token);
}

static bool parser_is_type(const parser_t *parser, token_type_t type);
static bool parser_is_keyword(const parser_t *parser, keyword_t keyword);
static bool parser_is_identifier(const parser_t *parser, const char *name);
static IFJ_ERR parser_accept_type(parser_t *parser, token_type_t type,
                                  bool *accepted);
static IFJ_ERR parser_accept_keyword(parser_t *parser, keyword_t keyword,
                                     bool *accepted);
static IFJ_ERR parser_accept_identifier(parser_t *parser, const char *name,
                                        bool *accepted);
static IFJ_ERR parse_functions(parser_t *parser, ast_node_t *root);
static IFJ_ERR parse_function(parser_t *parser, ast_node_t **function);
static IFJ_ERR parse_main(parser_t *parser, bool *is_main);
static IFJ_ERR parse_param_lists(parser_t *parser, ast_node_t *function);
static IFJ_ERR parse_params(parser_t *parser, ast_node_t *param_list);
static IFJ_ERR parse_param(parser_t *parser, ast_node_t **param);
static IFJ_ERR parse_param_next(parser_t *parser, ast_node_t *param_list);
static IFJ_ERR parse_type_id(parser_t *parser, ifj26_type_t *type);
static IFJ_ERR parse_opt_type(parser_t *parser, ifj26_type_t *type);
static IFJ_ERR parse_func_body(parser_t *parser, ast_node_t **body);
static IFJ_ERR parse_rhs(parser_t *parser, ast_node_t **rhs);
static IFJ_ERR parse_definition(parser_t *parser, ast_node_t **definition,
                                bool mutable);
static IFJ_ERR parse_var_def(parser_t *parser, ast_node_t **definition);
static IFJ_ERR parse_val_def(parser_t *parser, ast_node_t **definition);
static IFJ_ERR parse_func_ret(parser_t *parser, ast_node_t **ret);
static IFJ_ERR parse_func_ret_opt(parser_t *parser, ast_node_t **rhs);
static IFJ_ERR parse_block(parser_t *parser, ast_node_t **block);
static IFJ_ERR parse_block_cmd(parser_t *parser, ast_node_t **command);
static IFJ_ERR parse_assign_or_expr(parser_t *parser, ast_node_t **node);
static IFJ_ERR parse_inline_cmd(parser_t *parser, ast_node_t **command);
static IFJ_ERR parse_inline_if(parser_t *parser, ast_node_t **if_node);
static IFJ_ERR parse_if_body(parser_t *parser, ast_node_t **body);
static IFJ_ERR parse_if_statement(parser_t *parser, ast_node_t **if_node);
static IFJ_ERR parse_if_expression(parser_t *parser, ast_node_t **if_node);
static IFJ_ERR parse_while(parser_t *parser, ast_node_t **while_node);
static IFJ_ERR parse_for(parser_t *parser, ast_node_t **for_node);
static IFJ_ERR parse_generator(parser_t *parser, ast_node_t **generator);
static IFJ_ERR parse_range(parser_t *parser, ast_node_t **range);
static IFJ_ERR parse_guards(parser_t *parser, ast_node_t *for_node,
                            bool line_ended);

/**
 * @brief Zkopíruje jméno z aktuálního tokenu do nově alokované paměti.
 *
 * @param parser Parser s tokenem identifikátoru.
 * @param identifier Výstupní ukazatel na zkopírované jméno.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_copy_identifier(parser_t *parser, char **identifier) {
  CHECK_ARGS_NULL_RET(parser, identifier);

  if (!parser_is_type(parser, TOK_ID)) {
    LOG_ERR(IFJ_ESYNTAX, "Expected identifier on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  size_t length = strlen(parser->current_token.value.str_value);
  *identifier = malloc(length + 1);
  CHECK_MEM_ERR(*identifier);
  memcpy(*identifier, parser->current_token.value.str_value, length + 1);

  return parser_next(parser);
}

/**
 * @brief Ověří, že aktuální token má zadaný typ.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param type Očekávaný typ tokenu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_expect_type(parser_t *parser, token_type_t type) {
  CHECK_ARG_NULL_RET(parser);

  bool accepted = false;
  CHECK_ERR_RET(parser_accept_type(parser, type, &accepted));

  if (!accepted) {
    LOG_ERR(IFJ_ESYNTAX, "Unexpected token on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Ověří, že aktuální token je zadané klíčové slovo.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param keyword Očekávané klíčové slovo.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_expect_keyword(parser_t *parser, keyword_t keyword) {
  CHECK_ARG_NULL_RET(parser);

  bool accepted = false;
  CHECK_ERR_RET(parser_accept_keyword(parser, keyword, &accepted));

  if (!accepted) {
    LOG_ERR(IFJ_ESYNTAX, "Unexpected keyword on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Zkontroluje typ aktuálního tokenu bez jeho spotřebování.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param type Kontrolovaný typ tokenu.
 * @return true při shodě typu, jinak false.
 */
static bool parser_is_type(const parser_t *parser, token_type_t type) {
  if (parser == NULL) {
    return false;
  }

  return parser->current_token.type == type;
}

/**
 * @brief Zkontroluje klíčové slovo aktuálního tokenu bez jeho spotřebování.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param keyword Kontrolované klíčové slovo.
 * @return true při shodě klíčového slova, jinak false.
 */
static bool parser_is_keyword(const parser_t *parser, keyword_t keyword) {
  if (parser == NULL) {
    return false;
  }

  return parser->current_token.type == TOK_KEYWORD &&
         parser->current_token.value.keyword == keyword;
}

/**
 * @brief Zkontroluje jméno aktuálního identifikátoru bez jeho spotřebování.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param name Očekávané jméno identifikátoru.
 * @return true při shodě jména, jinak false.
 */
static bool parser_is_identifier(const parser_t *parser, const char *name) {
  if (parser == NULL || name == NULL || parser->current_token.type != TOK_ID) {
    return false;
  }

  return strcmp(parser->current_token.value.str_value, name) == 0;
}

/**
 * @brief Volitelně spotřebuje token zadaného typu.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param type Volitelný typ tokenu.
 * @param accepted Výstupní příznak, zda byl token spotřebován.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_accept_type(parser_t *parser, token_type_t type,
                                  bool *accepted) {
  CHECK_ARGS_NULL_RET(parser, accepted);

  *accepted = false;
  if (!parser_is_type(parser, type)) {
    return IFJ_SUCCESS;
  }

  CHECK_ERR_RET(parser_next(parser));
  *accepted = true;
  return IFJ_SUCCESS;
}

/**
 * @brief Volitelně spotřebuje zadané klíčové slovo.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param keyword Volitelné klíčové slovo.
 * @param accepted Výstupní příznak, zda bylo klíčové slovo spotřebováno.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_accept_keyword(parser_t *parser, keyword_t keyword,
                                     bool *accepted) {
  CHECK_ARGS_NULL_RET(parser, accepted);

  *accepted = false;
  if (!parser_is_keyword(parser, keyword)) {
    return IFJ_SUCCESS;
  }

  CHECK_ERR_RET(parser_next(parser));
  *accepted = true;
  return IFJ_SUCCESS;
}

/**
 * @brief Volitelně spotřebuje identifikátor se zadaným jménem.
 *
 * @param parser Parser s kontrolovaným tokenem.
 * @param name Volitelné jméno identifikátoru.
 * @param accepted Výstupní příznak, zda byl identifikátor spotřebován.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parser_accept_identifier(parser_t *parser, const char *name,
                                        bool *accepted) {
  CHECK_ARGS_NULL_RET(parser, name, accepted);

  *accepted = false;
  if (!parser_is_identifier(parser, name)) {
    return IFJ_SUCCESS;
  }

  CHECK_ERR_RET(parser_next(parser));
  *accepted = true;
  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje volitelnou anotaci @main.
 *
 * @param parser Parser s aktuálním tokenem.
 * @param is_main Výstupní příznak anotace @main.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_main(parser_t *parser, bool *is_main) {
  CHECK_ARGS_NULL_RET(parser, is_main);

  *is_main = false;
  bool accepted = false;
  CHECK_ERR_RET(parser_accept_type(parser, TOK_ANNOT_MAIN, &accepted));

  if (!accepted) {
    return IFJ_SUCCESS;
  }

  *is_main = true;
  CHECK_ERR_RET(parser_accept_type(parser, TOK_EOL, &accepted));
  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje jeden typ podle pravidla <type_id>.
 *
 * @param parser Parser s aktuálním tokenem typu.
 * @param type Výstupní typ.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_type_id(parser_t *parser, ifj26_type_t *type) {
  CHECK_ARGS_NULL_RET(parser, type);

  type->type = TYPE_UNRESOLVED;
  type->is_list = false;

  if (parser_is_keyword(parser, KW_LIST)) {
    CHECK_ERR_RET(parser_next(parser));
    CHECK_ERR_RET(parser_expect_type(parser, TOK_LBRACKET));
    CHECK_ERR_RET(parse_type_id(parser, type));
    type->is_list = true;
    return parser_expect_type(parser, TOK_RBRACKET);
  }

  if (!parser_is_keyword(parser, KW_INT) &&
      !parser_is_keyword(parser, KW_DOUBLE) &&
      !parser_is_keyword(parser, KW_STRING) &&
      !parser_is_keyword(parser, KW_UNIT) &&
      !parser_is_keyword(parser, KW_BOOLEAN)) {
    LOG_ERR(IFJ_ESYNTAX, "Expected type on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  switch (parser->current_token.value.keyword) {
  case KW_INT:
    type->type = TYPE_INT;
    break;
  case KW_DOUBLE:
    type->type = TYPE_DOUBLE;
    break;
  case KW_STRING:
    type->type = TYPE_STRING;
    break;
  case KW_UNIT:
    type->type = TYPE_UNIT;
    break;
  case KW_BOOLEAN:
    type->type = TYPE_BOOL;
    break;
  default:
    return IFJ_ESYNTAX;
  }

  return parser_next(parser);
}

/**
 * @brief Zpracuje volitelný návratový typ funkce.
 *
 * @param parser Parser s aktuálním tokenem.
 * @param type Výstupní návratový typ.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_opt_type(parser_t *parser, ifj26_type_t *type) {
  CHECK_ARGS_NULL_RET(parser, type);

  type->type = TYPE_UNRESOLVED;
  type->is_list = false;

  bool accepted = false;
  CHECK_ERR_RET(parser_accept_type(parser, TOK_COLON, &accepted));
  if (!accepted) {
    return IFJ_SUCCESS;
  }

  return parse_type_id(parser, type);
}

/**
 * @brief Zpracuje jeden formální parametr podle pravidla <param>.
 *
 * @param parser Parser s aktuálním tokenem parametru.
 * @param param Výstupní uzel parametru.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_param(parser_t *parser, ast_node_t **param) {
  CHECK_ARGS_NULL_RET(parser, param);

  *param = NULL;
  CHECK_ERR_RET(ast_create_node(param, AST_PARAM));

  IFJ_ERR err = parser_copy_identifier(parser, &(*param)->as.param.id);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_COLON);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_type_id(parser, &(*param)->as.param.param_type);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*param);
  *param = NULL;
  return err;
}

/**
 * @brief Zpracuje pokračování seznamu parametrů.
 *
 * @param parser Parser s aktuálním tokenem.
 * @param param_list Seznam parametrů, do kterého se přidává.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_param_next(parser_t *parser, ast_node_t *param_list) {
  CHECK_ARGS_NULL_RET(parser, param_list);

  while (parser_is_type(parser, TOK_COMMA)) {
    CHECK_ERR_RET(parser_next(parser));

    /* A trailing comma is valid only before a closing parenthesis on a
     * later physical line.  Newlines inside parentheses are otherwise
     * whitespace, so the scanner records this on the ')' token. */
    if (parser_is_type(parser, TOK_RPAREN)) {
      if (!parser->current_token.newline_before) {
        return IFJ_ESYNTAX;
      }
      return IFJ_SUCCESS;
    }

    ast_node_t *param = NULL;
    IFJ_ERR err = parse_param(parser, &param);
    CHECK_ERR_GOTO(err, cleanup);

    err = ast_param_list_add_param(param_list, param);
    CHECK_ERR_GOTO(err, cleanup);
    continue;

  cleanup:
    ast_free(param);
    return err;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje obsah jednoho seznamu parametrů.
 *
 * @param parser Parser s aktuálním tokenem prvního parametru.
 * @param param_list Seznam parametrů, do kterého se přidává.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_params(parser_t *parser, ast_node_t *param_list) {
  CHECK_ARGS_NULL_RET(parser, param_list);

  if (parser_is_type(parser, TOK_RPAREN)) {
    return IFJ_SUCCESS;
  }

  ast_node_t *param = NULL;
  IFJ_ERR err = parse_param(parser, &param);
  CHECK_ERR_GOTO(err, cleanup);

  err = ast_param_list_add_param(param_list, param);
  CHECK_ERR_GOTO(err, cleanup);

  return parse_param_next(parser, param_list);

cleanup:
  ast_free(param);
  return err;
}

/**
 * @brief Zpracuje všechny seznamy parametrů funkce.
 *
 * @param parser Parser s aktuálním tokenem seznamu nebo další části funkce.
 * @param function Funkce, do které se seznamy přidávají.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_param_lists(parser_t *parser, ast_node_t *function) {
  CHECK_ARGS_NULL_RET(parser, function);

  while (parser_is_type(parser, TOK_LPAREN)) {
    ast_node_t *param_list = NULL;
    IFJ_ERR err = ast_create_node(&param_list, AST_PARAM_LIST);
    CHECK_ERR_GOTO(err, cleanup);

    err = parser_next(parser);
    CHECK_ERR_GOTO(err, cleanup);

    err = parse_params(parser, param_list);
    CHECK_ERR_GOTO(err, cleanup);

    err = parser_expect_type(parser, TOK_RPAREN);
    CHECK_ERR_GOTO(err, cleanup);

    err = ast_func_add_param_list(function, param_list);
    CHECK_ERR_GOTO(err, cleanup);
    continue;

  cleanup:
    ast_free(param_list);
    return err;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje tělo funkce.
 *
 * @param parser Parser s aktuálním tokenem začátku těla.
 * @param body Výstupní tělo funkce.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_func_body(parser_t *parser, ast_node_t **body) {
  CHECK_ARGS_NULL_RET(parser, body);

  if (parser_is_type(parser, TOK_EOL)) {
    CHECK_ERR_RET(parser_next(parser));
    return parse_block(parser, body);
  }

  IFJ_ERR err = parse_rhs(parser, body);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_type(parser, TOK_EOL);
  CHECK_ERR_GOTO(err, cleanup);
  return IFJ_SUCCESS;

cleanup:
  ast_free(*body);
  *body = NULL;
  return err;
}

/**
 * @brief Zpracuje pravou stranu definice, přiřazení nebo návratu.
 *
 * @param parser Parser s aktuálním tokenem začátku pravé strany.
 * @param rhs Výstupní uzel pravé strany.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_rhs(parser_t *parser, ast_node_t **rhs) {
  CHECK_ARGS_NULL_RET(parser, rhs);

  if (parser_is_keyword(parser, KW_IF)) {
    return parse_if_expression(parser, rhs);
  }

  return psa_parse_expression(parser, rhs);
}

/**
 * @brief Zpracuje definici proměnné var nebo val.
 *
 * @param parser Parser s aktuálním klíčovým slovem var nebo val.
 * @param definition Výstupní uzel definice.
 * @param mutable Příznak, zda jde o modifikovatelnou proměnnou var.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_definition(parser_t *parser, ast_node_t **definition,
                                bool mutable) {
  CHECK_ARGS_NULL_RET(parser, definition);

  *definition = NULL;
  CHECK_ERR_RET(ast_create_node(definition, AST_DEF));
  (*definition)->as.def.mut = mutable;
  (*definition)->as.def.type.type = TYPE_UNRESOLVED;
  (*definition)->as.def.type.is_list = false;

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_copy_identifier(parser, &(*definition)->as.def.id);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_opt_type(parser, &(*definition)->as.def.type);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_ASSIGN);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_rhs(parser, &(*definition)->as.def.rhs);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_EOL);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*definition);
  *definition = NULL;
  return err;
}

/**
 * @brief Zpracuje definici modifikovatelné proměnné var.
 *
 * @param parser Parser s klíčovým slovem var.
 * @param definition Výstupní uzel definice.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_var_def(parser_t *parser, ast_node_t **definition) {
  return parse_definition(parser, definition, true);
}

/**
 * @brief Zpracuje definici nemodifikovatelné proměnné val.
 *
 * @param parser Parser s klíčovým slovem val.
 * @param definition Výstupní uzel definice.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_val_def(parser_t *parser, ast_node_t **definition) {
  return parse_definition(parser, definition, false);
}

/**
 * @brief Zpracuje volitelný výraz příkazu return.
 *
 * @param parser Parser s aktuálním tokenem výrazu nebo EOL.
 * @param rhs Výstupní výraz, případně NULL.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_func_ret_opt(parser_t *parser, ast_node_t **rhs) {
  CHECK_ARGS_NULL_RET(parser, rhs);

  *rhs = NULL;
  if (parser_is_type(parser, TOK_EOL)) {
    return IFJ_SUCCESS;
  }

  return parse_rhs(parser, rhs);
}

/**
 * @brief Zpracuje příkaz návratu z funkce.
 *
 * @param parser Parser s klíčovým slovem return.
 * @param ret Výstupní uzel návratu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_func_ret(parser_t *parser, ast_node_t **ret) {
  CHECK_ARGS_NULL_RET(parser, ret);

  *ret = NULL;
  CHECK_ERR_RET(ast_create_node(ret, AST_FUNC_RET));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_func_ret_opt(parser, &(*ret)->as.func_ret.rhs);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_EOL);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*ret);
  *ret = NULL;
  return err;
}

/**
 * @brief Zpracuje přiřazení nebo samostatný výraz.
 *
 * @param parser Parser s aktuálním tokenem příkazu nebo výrazu.
 * @param node Výstupní uzel příkazu nebo výrazu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_assign_or_expr(parser_t *parser, ast_node_t **node) {
  CHECK_ARGS_NULL_RET(parser, node);

  *node = NULL;
  if (!parser_is_type(parser, TOK_ID)) {
    return psa_parse_expression(parser, node);
  }

  token_t next_token;
  CHECK_ERR_RET(scanner_peek_next_token(parser->scanner, &next_token));
  bool is_assignment = next_token.type == TOK_ASSIGN;

  if (!is_assignment) {
    return psa_parse_expression(parser, node);
  }

  CHECK_ERR_RET(ast_create_node(node, AST_ASSIGN));
  IFJ_ERR err = parser_copy_identifier(parser, &(*node)->as.assign.id);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_ASSIGN);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_rhs(parser, &(*node)->as.assign.rhs);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*node);
  *node = NULL;
  return err;
}

/**
 * @brief Zpracuje příkaz v jednořádkovém těle konstrukce.
 *
 * @param parser Parser s aktuálním tokenem příkazu.
 * @param command Výstupní uzel příkazu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_inline_cmd(parser_t *parser, ast_node_t **command) {
  CHECK_ARGS_NULL_RET(parser, command);

  *command = NULL;
  IFJ_ERR err;
  if (parser_is_keyword(parser, KW_RETURN)) {
    CHECK_ERR_RET(ast_create_node(command, AST_FUNC_RET));
    err = parser_next(parser);
    CHECK_ERR_GOTO(err, cleanup);
    err = parse_func_ret_opt(parser, &(*command)->as.func_ret.rhs);
    CHECK_ERR_GOTO(err, cleanup);
    return IFJ_SUCCESS;
  }

  if (parser_is_keyword(parser, KW_IF)) {
    return parse_inline_if(parser, command);
  }

  return parse_assign_or_expr(parser, command);

cleanup:
  ast_free(*command);
  *command = NULL;
  return err;
}

/**
 * @brief Zpracuje vnořený if v jednořádkovém těle.
 *
 * @param parser Parser s klíčovým slovem if.
 * @param if_node Výstupní uzel podmínky.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_inline_if(parser_t *parser, ast_node_t **if_node) {
  CHECK_ARGS_NULL_RET(parser, if_node);

  *if_node = NULL;
  CHECK_ERR_RET(ast_create_node(if_node, AST_IF));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);
  err = psa_parse_expression(parser, &(*if_node)->as.if_st.expr);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_keyword(parser, KW_THEN);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_inline_cmd(parser, &(*if_node)->as.if_st.body);
  CHECK_ERR_GOTO(err, cleanup);

  bool has_else = false;
  err = parser_accept_keyword(parser, KW_ELSE, &has_else);
  CHECK_ERR_GOTO(err, cleanup);
  if (has_else) {
    err = parse_inline_cmd(parser, &(*if_node)->as.if_st.else_st);
    CHECK_ERR_GOTO(err, cleanup);
  }

  return IFJ_SUCCESS;

cleanup:
  ast_free(*if_node);
  *if_node = NULL;
  return err;
}

/**
 * @brief Zpracuje blok nebo jednořádkové tělo konstrukce.
 *
 * @param parser Parser s aktuálním tokenem EOL nebo inline příkazu.
 * @param body Výstupní tělo konstrukce.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_if_body(parser_t *parser, ast_node_t **body) {
  CHECK_ARGS_NULL_RET(parser, body);

  *body = NULL;
  if (parser_is_type(parser, TOK_EOL)) {
    CHECK_ERR_RET(parser_next(parser));
    return parse_block(parser, body);
  }

  IFJ_ERR err = parse_inline_cmd(parser, body);
  CHECK_ERR_RET(err);
  return parser_expect_type(parser, TOK_EOL);
}

/**
 * @brief Zpracuje podmíněný příkaz if včetně větví else if a else.
 *
 * @param parser Parser s klíčovým slovem if.
 * @param if_node Výstupní uzel podmínky.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_if_statement(parser_t *parser, ast_node_t **if_node) {
  CHECK_ARGS_NULL_RET(parser, if_node);

  *if_node = NULL;
  CHECK_ERR_RET(ast_create_node(if_node, AST_IF));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);
  err = psa_parse_expression(parser, &(*if_node)->as.if_st.expr);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_keyword(parser, KW_THEN);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_if_body(parser, &(*if_node)->as.if_st.body);
  CHECK_ERR_GOTO(err, cleanup);

  while (parser_is_keyword(parser, KW_ELSE)) {
    err = parser_next(parser);
    CHECK_ERR_GOTO(err, cleanup);

    if (parser_is_keyword(parser, KW_IF)) {
      ast_node_t *else_if = NULL;
      err = ast_create_node(&else_if, AST_ELSE_IF);
      CHECK_ERR_GOTO(err, cleanup);

      err = parser_next(parser);
      if (err == IFJ_SUCCESS) {
        err = psa_parse_expression(parser, &else_if->as.else_if_st.expr);
      }
      if (err == IFJ_SUCCESS) {
        err = parser_expect_keyword(parser, KW_THEN);
      }
      if (err == IFJ_SUCCESS) {
        err = parse_if_body(parser, &else_if->as.else_if_st.body);
      }
      if (err != IFJ_SUCCESS) {
        ast_free(else_if);
        goto cleanup;
      }

      err = ast_if_add_else_if(*if_node, else_if);
      if (err != IFJ_SUCCESS) {
        ast_free(else_if);
        goto cleanup;
      }
      continue;
    }

    err = parse_if_body(parser, &(*if_node)->as.if_st.else_st);
    CHECK_ERR_GOTO(err, cleanup);
    break;
  }

  return IFJ_SUCCESS;

cleanup:
  ast_free(*if_node);
  *if_node = NULL;
  return err;
}

/**
 * @brief Zpracuje jednu větev výrazu if.
 *
 * @param parser Parser s aktuálním tokenem větve.
 * @param branch Výstupní tělo větve.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_if_expression_branch(parser_t *parser,
                                          ast_node_t **branch) {
  CHECK_ARGS_NULL_RET(parser, branch);

  *branch = NULL;
  if (parser_is_type(parser, TOK_EOL)) {
    CHECK_ERR_RET(parser_next(parser));
    return parse_block(parser, branch);
  }

  IFJ_ERR err = psa_parse_expression(parser, branch);
  CHECK_ERR_RET(err);
  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje if použitý jako výraz.
 *
 * @param parser Parser s klíčovým slovem if.
 * @param if_node Výstupní uzel výrazu if.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_if_expression(parser_t *parser, ast_node_t **if_node) {
  CHECK_ARGS_NULL_RET(parser, if_node);

  *if_node = NULL;
  CHECK_ERR_RET(ast_create_node(if_node, AST_IF));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);
  err = psa_parse_expression(parser, &(*if_node)->as.if_st.expr);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_keyword(parser, KW_THEN);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_if_expression_branch(parser, &(*if_node)->as.if_st.body);
  CHECK_ERR_GOTO(err, cleanup);
  bool has_eol = false;
  err = parser_accept_type(parser, TOK_EOL, &has_eol);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_keyword(parser, KW_ELSE);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_if_expression_branch(parser, &(*if_node)->as.if_st.else_st);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*if_node);
  *if_node = NULL;
  return err;
}

/**
 * @brief Zpracuje blok příkazů podle pravidla <block>.
 *
 * @param parser Parser s tokenem INDENT.
 * @param block Výstupní uzel bloku.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_block(parser_t *parser, ast_node_t **block) {
  CHECK_ARGS_NULL_RET(parser, block);

  *block = NULL;
  CHECK_ERR_RET(ast_create_node(block, AST_BLOCK));

  IFJ_ERR err = parser_expect_type(parser, TOK_INDENT);
  CHECK_ERR_GOTO(err, cleanup);

  if (parser_is_type(parser, TOK_DEDENT)) {
    LOG_ERR(IFJ_ESYNTAX, "Block cannot be empty on line %d",
            parser->current_token.line);
    err = IFJ_ESYNTAX;
    goto cleanup;
  }

  while (!parser_is_type(parser, TOK_DEDENT)) {
    ast_node_t *command = NULL;
    err = parse_block_cmd(parser, &command);
    CHECK_ERR_GOTO(err, cleanup);

    err = ast_block_add_command(*block, command);
    if (err != IFJ_SUCCESS) {
      ast_free(command);
      goto cleanup;
    }
  }

  err = parser_expect_type(parser, TOK_DEDENT);
  CHECK_ERR_GOTO(err, cleanup);
  return IFJ_SUCCESS;

cleanup:
  ast_free(*block);
  *block = NULL;
  return err;
}

/**
 * @brief Zvolí a zpracuje příkaz v bloku.
 *
 * @param parser Parser s aktuálním tokenem příkazu.
 * @param command Výstupní uzel příkazu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_block_cmd(parser_t *parser, ast_node_t **command) {
  CHECK_ARGS_NULL_RET(parser, command);

  *command = NULL;

  if (parser_is_keyword(parser, KW_VAR)) {
    return parse_var_def(parser, command);
  }
  if (parser_is_keyword(parser, KW_VAL)) {
    return parse_val_def(parser, command);
  }
  if (parser_is_keyword(parser, KW_RETURN)) {
    return parse_func_ret(parser, command);
  }
  if (parser_is_keyword(parser, KW_IF)) {
    return parse_if_statement(parser, command);
  }
  if (parser_is_keyword(parser, KW_WHILE)) {
    return parse_while(parser, command);
  }
  if (parser_is_keyword(parser, KW_FOR)) {
    return parse_for(parser, command);
  }

  IFJ_ERR err = parse_assign_or_expr(parser, command);
  CHECK_ERR_RET(err);
  err = parser_expect_type(parser, TOK_EOL);
  if (err != IFJ_SUCCESS) {
    ast_free(*command);
    *command = NULL;
  }
  return err;
}

/**
 * @brief Zpracuje cyklus while.
 *
 * @param parser Parser s klíčovým slovem while.
 * @param while_node Výstupní uzel cyklu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_while(parser_t *parser, ast_node_t **while_node) {
  CHECK_ARGS_NULL_RET(parser, while_node);

  *while_node = NULL;
  CHECK_ERR_RET(ast_create_node(while_node, AST_WHILE));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);
  err = psa_parse_expression(parser, &(*while_node)->as.while_st.expr);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_keyword(parser, KW_DO);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_type(parser, TOK_EOL);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_block(parser, &(*while_node)->as.while_st.body);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*while_node);
  *while_node = NULL;
  return err;
}

/**
 * @brief Zpracuje generátor cyklu for.
 *
 * @param parser Parser s identifikátorem generátoru.
 * @param generator Výstupní uzel generátoru.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_generator(parser_t *parser, ast_node_t **generator) {
  CHECK_ARGS_NULL_RET(parser, generator);

  *generator = NULL;
  CHECK_ERR_RET(ast_create_node(generator, AST_FOR_GEN));

  IFJ_ERR err = parser_copy_identifier(parser, &(*generator)->as.for_gen.id);
  CHECK_ERR_GOTO(err, cleanup);
  err = parser_expect_type(parser, TOK_LARROW);
  CHECK_ERR_GOTO(err, cleanup);
  err = psa_parse_expression(parser, &(*generator)->as.for_gen.expr);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_range(parser, &(*generator)->as.for_gen.range);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*generator);
  *generator = NULL;
  return err;
}

/**
 * @brief Zpracuje volitelný rozsah generátoru.
 *
 * @param parser Parser s tokenem to, until nebo dalším prvkem hlavičky.
 * @param range Výstupní uzel rozsahu, případně NULL.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_range(parser_t *parser, ast_node_t **range) {
  CHECK_ARGS_NULL_RET(parser, range);

  *range = NULL;
  bool is_to = false;
  bool is_until = false;
  CHECK_ERR_RET(parser_is_identifier(parser, "to")
                    ? parser_accept_identifier(parser, "to", &is_to)
                    : IFJ_SUCCESS);
  if (!is_to) {
    CHECK_ERR_RET(parser_accept_identifier(parser, "until", &is_until));
  }
  if (!is_to && !is_until) {
    return IFJ_SUCCESS;
  }

  CHECK_ERR_RET(ast_create_node(range, AST_FOR_RANGE));
  (*range)->as.for_range.until = is_until;

  IFJ_ERR err = psa_parse_expression(parser, &(*range)->as.for_range.expr);
  CHECK_ERR_GOTO(err, cleanup);

  if (is_to) {
    bool has_by = false;
    err = parser_accept_identifier(parser, "by", &has_by);
    CHECK_ERR_GOTO(err, cleanup);
    if (has_by) {
      err = psa_parse_expression(parser, &(*range)->as.for_range.by_expr);
      CHECK_ERR_GOTO(err, cleanup);
    }
  }

  return IFJ_SUCCESS;

cleanup:
  ast_free(*range);
  *range = NULL;
  return err;
}

/**
 * @brief Zpracuje stráže cyklu for.
 *
 * @param parser Parser s aktuální stráží nebo dalším tokenem.
 * @param for_node Cyklus, do kterého se stráže přidávají.
 * @param line_ended Zda každá stráž končí tokenem EOL.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_guards(parser_t *parser, ast_node_t *for_node,
                            bool line_ended) {
  CHECK_ARGS_NULL_RET(parser, for_node);

  while (parser_is_keyword(parser, KW_IF)) {
    ast_node_t *guard = NULL;
    CHECK_ERR_RET(ast_create_node(&guard, AST_FOR_GUARD));

    IFJ_ERR err = parser_next(parser);
    if (err == IFJ_SUCCESS) {
      err = psa_parse_expression(parser, &guard->as.for_guard.expr);
    }
    if (err == IFJ_SUCCESS && line_ended) {
      err = parser_expect_type(parser, TOK_EOL);
    }
    if (err == IFJ_SUCCESS) {
      err = ast_for_add_guard(for_node, guard);
    }
    if (err != IFJ_SUCCESS) {
      ast_free(guard);
      return err;
    }
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Zpracuje cyklus for s generátorem a strážemi.
 *
 * @param parser Parser s klíčovým slovem for.
 * @param for_node Výstupní uzel cyklu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_for(parser_t *parser, ast_node_t **for_node) {
  CHECK_ARGS_NULL_RET(parser, for_node);

  *for_node = NULL;
  CHECK_ERR_RET(ast_create_node(for_node, AST_FOR));

  IFJ_ERR err = parser_next(parser);
  CHECK_ERR_GOTO(err, cleanup);

  if (parser_is_type(parser, TOK_EOL)) {
    err = parser_next(parser);
    CHECK_ERR_GOTO(err, cleanup);
    err = parser_expect_type(parser, TOK_INDENT);
    CHECK_ERR_GOTO(err, cleanup);
    err = parse_generator(parser, &(*for_node)->as.for_st.gen);
    CHECK_ERR_GOTO(err, cleanup);
    err = parser_expect_type(parser, TOK_EOL);
    CHECK_ERR_GOTO(err, cleanup);
    err = parse_guards(parser, *for_node, true);
    CHECK_ERR_GOTO(err, cleanup);
    err = parser_expect_type(parser, TOK_DEDENT);
    CHECK_ERR_GOTO(err, cleanup);
  } else {
    err = parse_generator(parser, &(*for_node)->as.for_st.gen);
    CHECK_ERR_GOTO(err, cleanup);
    err = parse_guards(parser, *for_node, false);
    CHECK_ERR_GOTO(err, cleanup);
  }

  err = parser_expect_keyword(parser, KW_DO);
  CHECK_ERR_GOTO(err, cleanup);
  err = parse_if_body(parser, &(*for_node)->as.for_st.body);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*for_node);
  *for_node = NULL;
  return err;
}

/**
 * @brief Zpracuje jednu definici funkce podle pravidla <func>.
 *
 * @param parser Parser s aktuálním tokenem anotace nebo klíčového slova def.
 * @param function Výstupní uzel funkce.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_function(parser_t *parser, ast_node_t **function) {
  CHECK_ARGS_NULL_RET(parser, function);

  *function = NULL;
  CHECK_ERR_RET(ast_create_node(function, AST_FUNC));
  (*function)->as.func.return_type.type = TYPE_UNRESOLVED;

  IFJ_ERR err = parse_main(parser, &(*function)->as.func.main);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_keyword(parser, KW_DEF);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_copy_identifier(parser, &(*function)->as.func.id);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_param_lists(parser, *function);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_opt_type(parser, &(*function)->as.func.return_type);
  CHECK_ERR_GOTO(err, cleanup);

  err = parser_expect_type(parser, TOK_ASSIGN);
  CHECK_ERR_GOTO(err, cleanup);

  err = parse_func_body(parser, &(*function)->as.func.body);
  CHECK_ERR_GOTO(err, cleanup);

  return IFJ_SUCCESS;

cleanup:
  ast_free(*function);
  *function = NULL;
  return err;
}

/**
 * @brief Zpracuje definice funkcí podle pravidla <functions>.
 *
 * @param parser Parser s aktuálním tokenem začátku funkce nebo EOF.
 * @param root Kořen AST, do kterého se funkce přidávají.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_functions(parser_t *parser, ast_node_t *root) {
  CHECK_ARGS_NULL_RET(parser, root);

  while (parser_is_type(parser, TOK_ANNOT_MAIN) ||
         parser_is_keyword(parser, KW_DEF)) {
    ast_node_t *function = NULL;
    CHECK_ERR_RET(parse_function(parser, &function));

    IFJ_ERR err = ast_root_add_func(root, function);
    if (err != IFJ_SUCCESS) {
      ast_free(function);
      return err;
    }
  }

  if (!parser_is_type(parser, TOK_EOF)) {
    LOG_ERR(IFJ_ESYNTAX, "Expected function or end of input on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Ověří prolog podle pravidla <prolog>.
 *
 * Prolog má přesný tvar `import ifj26.* EOL`. Po úspěšném dokončení
 * je aktuální token prvním tokenem následujícím po prologu.
 *
 * @param parser Parser s aktuálním tokenem na začátku prologu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
static IFJ_ERR parse_prolog(parser_t *parser) {
  CHECK_ARG_NULL_RET(parser);

  CHECK_ERR_RET(parser_expect_keyword(parser, KW_IMPORT));

  bool accepted = false;
  CHECK_ERR_RET(parser_accept_identifier(parser, "ifj26", &accepted));

  if (!accepted) {
    LOG_ERR(IFJ_ESYNTAX, "Expected ifj26 on line %d",
            parser->current_token.line);
    return IFJ_ESYNTAX;
  }

  CHECK_ERR_RET(parser_expect_type(parser, TOK_DOT));
  CHECK_ERR_RET(parser_expect_type(parser, TOK_MUL));
  return parser_expect_type(parser, TOK_EOL);
}

IFJ_ERR parser_init(parser_t *parser, scanner_t *scanner) {
  CHECK_ARGS_NULL_RET(parser, scanner);

  parser->scanner = scanner;
  parser->current_token.type = TOK_EOF;
  parser->current_token.value.str_value = NULL;
  parser->current_token.line = 0;
  parser->current_token.newline_before = false;

  CHECK_ERR_RET(scanner_next_token(scanner, &parser->current_token));
  return IFJ_SUCCESS;
}

IFJ_ERR parser_parse(parser_t *parser, ast_node_t **root) {
  CHECK_ARGS_NULL_RET(parser, root);

  CHECK_ERR_RET(parse_prolog(parser));

  CHECK_ERR_RET(ast_create_node(root, AST_ROOT));
  IFJ_ERR err = parse_functions(parser, *root);
  CHECK_ERR_GOTO(err, cleanup);

  if (parser->current_token.type != TOK_EOF) {
    LOG_ERR(IFJ_ESYNTAX, "Expected end of input on line %d",
            parser->current_token.line);
    err = IFJ_ESYNTAX;
    goto cleanup;
  }

  return IFJ_SUCCESS;

cleanup:
  ast_free(*root);
  *root = NULL;
  return err;
}

IFJ_ERR parser_destroy(parser_t *parser) {
  CHECK_ARG_NULL_RET(parser);

  CHECK_ERR_RET(token_free(&parser->current_token));
  parser->scanner = NULL;
  return IFJ_SUCCESS;
}
