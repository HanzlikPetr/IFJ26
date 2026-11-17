 /**
 * @file generator.c
 * @brief IFJ26 - Implementace generátoru
 *
 * @author xberanm00 Marek Beran
 */
/*
 * TODO:
 * vyresit ty returny - podle AST_FUNC_RET
 * pridat type union ()
 */

// --- libraries ---
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// --- our files ---
#include "generator.h"
#include "printer_tree.h"

// ==================== POMOCNÉ FUNKCE ====================
static IFJ_ERR get_snippet(ast_node_t *instruction, char **snippet);
static IFJ_ERR call_func(ast_node_t *func_call);
static IFJ_ERR generate_body(ast_node_t *body);

static IFJ_ERR start_code(ast_node_t *root) {
  // CHECK_ARG_NULL_RET(root); // redundant
  puts(".IFJcode26");

  printf("\n\t# Initialization of global variables\n");
  printf("\tDEFVAR GF@temp\n");
  printf("\tDEFVAR GF@tempLeft\n");
  printf("\tDEFVAR GF@tempRight\n\n");

  // najde main funkci a zavolá jí
  ast_node_t *function;
  for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
    function = root->as.root.funcs[i];

    if (function->type != AST_FUNC) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Passed node is not a function");
    }

    if (function->as.func.main) {
      // zavolání main
      printf("\t# Run main\n");
      printf("\tCREATEFRAME\n");
      printf("\tPUSHFRAME\n");
      printf("\tCALL %s\n", function->as.func.id);
      printf("\tEXIT int@0\n\n");   // úspěšné ukončení programu
    }
  }

  printf("\t# Built-in functions\n");

  printf(
    "LABEL print\n"
      "\tDEFVAR LF@symb\n"
      "\tPOPS LF@symb\n"
      "\tWRITE LF@symb\n"
      "\tRETURN\n"
  );

  printf("\n");

  printf("\t# User defined functions\n");
  return IFJ_SUCCESS;
}

static IFJ_ERR format_str(char **value) {
  CHECK_ARG_NULL_RET(value);

  size_t len = strlen(*value);
  if (len > (SIZE_MAX - 1) / 4)
    return IFJ_EINTERNAL;
  size_t cap = len * 4 + 1;

  char *s = malloc(cap);
  CHECK_MEM_ERR(s);

  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)(*value)[i];
    if (c <= 32 || c == 35 || c == 92) {
      s[o++] = '\\';
      s[o++] = '0' + (char)(c / 100);
      s[o++] = '0' + (char)((c / 10) % 10);
      s[o++] = '0' + (char)(c % 10);
    } else {
      s[o++] = (char)c;
    }
  }
  s[o] = '\0';

  free(*value);
  *value = s;
  return IFJ_SUCCESS;
}

static IFJ_ERR format(char **snippet, data_type_t type) {
  IFJ_ERR err;
  int n;
  char *s;
  if (type == TYPE_STRING) {
    err = format_str(snippet);
    CHECK_ERR_GOTO(err, cleanup);

    n = snprintf(NULL, 0, "string@%s", *snippet);
    if (n < 0)
      return IFJ_EINTERNAL;

    s = malloc((size_t)n + 1);
    n = snprintf(s, (size_t)n + 1, "string@%s", *snippet);
    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_INT) {
    n = snprintf(NULL, 0, "int@%s", *snippet);
    if (n < 0)
      return IFJ_EINTERNAL;

    s = malloc((size_t)n + 1);
    n = snprintf(s, (size_t)n + 1, "int@%s", *snippet);
    free(*snippet);
    *snippet = s;

    if (n < 0)
      return IFJ_EINTERNAL;

  } else if (type == TYPE_DOUBLE) {
    n = snprintf(NULL, 0, "float@%s", *snippet);
    if (n < 0)
      return IFJ_EINTERNAL;

    s = malloc((size_t)n + 1);
    n = snprintf(s, (size_t)n + 1, "float@%s", *snippet);
    free(*snippet);
    *snippet = s;

    if (n < 0)
      return IFJ_EINTERNAL;

  } else if (type == TYPE_BOOL) {
    n = snprintf(NULL, 0, "bool@%s", *snippet);
    if (n < 0)
      return IFJ_EINTERNAL;

    s = malloc((size_t)n + 1);
    n = snprintf(s, (size_t)n + 1, "bool@%s", *snippet);
    free(*snippet);
    *snippet = s;

    if (n < 0)
      return IFJ_EINTERNAL;
  }

  return IFJ_SUCCESS;

cleanup:
  free(*snippet);
  return IFJ_EINTERNAL;
}

static IFJ_ERR get_op(token_type_t type, const char **snippet) {

  switch (type) {
  case TOK_EQ:
    *snippet = "\tEQ GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_NEQ:
    *snippet = "\tEQ GF@temp GF@tempLeft GF@tempRight\n\tNOT GF@temp\n";
    break;

  case TOK_LT:
    *snippet = "\tLT GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_GT:
    *snippet = "\tGT GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_LEQ:
    *snippet = "\tEQ GF@temp GF@tempLeft GF@tempRight\n\tPUSHS GF@temp\n\tLT "
               "GF@temp GF@tempLeft GF@tempRight\n\tPOPS GF@tempLeft\n\tOR "
               "GF@temp GF@temp GF@tempLeft\n";
    break;

  case TOK_GEQ:
    *snippet = "\tEQ GF@temp GF@tempLeft GF@tempRight\n\tPUSHS GF@temp\n\tGT "
               "GF@temp GF@tempLeft GF@tempRight\n\tPOPS GF@tempLeft\n\tOR "
               "GF@temp GF@temp GF@tempLeft\n";
    break;

  case TOK_ADD:
    *snippet = "\tADD GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_SUB:
    *snippet = "\tSUB GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_MUL:
    *snippet = "\tMUL GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_DIV:
    *snippet = "\tDIV GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_AND:
    *snippet = "\tAND GF@temp GF@tempLeft GF@tempRight\n";
    break;

  case TOK_OR:
    *snippet = "\tOR GF@temp GF@tempLeft GF@tempRight\n";
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown operator type");
    return IFJ_EINTERNAL;
  }
  return IFJ_SUCCESS;
}

static IFJ_ERR calc_expression(ast_node_t *expr) {
  CHECK_ARG_NULL_RET(expr);
  char *snippet;
  IFJ_ERR err;

  if (expr->as.expr.is_unary) {
    if (expr->as.expr.operator_type != TOK_NOT) {   // jediný unární operand
      return IFJ_EINTERNAL;
    }

    if (expr->as.expr.unary.operand->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.unary.operand);
      CHECK_ERR_RET(err);
    } else if (expr->as.expr.unary.operand->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.unary.operand);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@temp\n");
    } else {
      err = get_snippet(expr->as.expr.unary.operand, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@temp %s\n", snippet);
      free(snippet);
    }

    printf("\tNOT GF@temp\n");
  } else {
    if (expr->as.expr.operator_type == TOK_NOT) { // jediný nebinární operand
      return IFJ_EINTERNAL;
    }

    // získání levé strany
    if (expr->as.expr.binary.lhs->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@tempLeft GF@temp\n");
    } else if (expr->as.expr.binary.lhs->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@tempLeft\n");
    } else {
      err = get_snippet(expr->as.expr.binary.lhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@tempLeft %s\n", snippet);
      free(snippet);
    }

    printf("\tPUSHS GF@tempLeft\n\n");

    // získání pravé strany
    if (expr->as.expr.binary.rhs->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.binary.rhs);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@tempRight GF@temp\n");
    } else if (expr->as.expr.binary.rhs->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.binary.rhs);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@tempRight\n");
    } else {
      err = get_snippet(expr->as.expr.binary.rhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@tempRight %s\n", snippet);
      free(snippet);
    }

    printf("\tPUSHS GF@tempRight\n\n");
    printf("\tPOPS GF@tempRight\n");

    printf("\tPOPS GF@tempLeft\n\n");

    err = get_op(expr->as.expr.operator_type, (const char **)&snippet);
    CHECK_ERR_RET(err);

    // check_op(snippet);   // ošetření podmínek

    printf(snippet);
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR call_func(ast_node_t *func_call) {
  CHECK_ARG_NULL_RET(func_call);

  char *snippet;
  IFJ_ERR err;

  printf("\tCREATEFRAME\n");

  for (size_t i = 0; i < func_call->as.func_call.param_lists_cnt; i++) {
    ast_node_t *param_list = func_call->as.func_call.param_lists[i];

    for (size_t j = 0; j < param_list->as.param_list.params_cnt; j++) {
      ast_node_t *param = param_list->as.param_list.params[j];

      if (param->type == AST_VAR) {
        printf("\tPUSHS LF@%s\n", param->as.param.id);
      } else if (param->type == AST_LITERAL) {
        err = get_snippet(param, &snippet);
        CHECK_ERR_RET(err);

        printf("\tPUSHS %s\n", snippet);
        free(snippet);
      } else if (param->type == AST_EXPR) {
        err = calc_expression(param);
        CHECK_ERR_RET(err);
        printf("\tPUSHS GF@temp\n");
      } else {
        LOG_ERR(IFJ_EINTERNAL, "Unknown parameter type");
        return IFJ_EINTERNAL;
      }
    }
  }
  printf("\tPUSHFRAME\n");
  printf("\tCALL %s\n", func_call->as.func_call.id);
  printf("\tPOPFRAME\n");
  return IFJ_SUCCESS;
}

static IFJ_ERR define_var(ast_node_t *def) {
  CHECK_ARG_NULL_RET(def);

  printf("\tDEFVAR LF@%s\n", def->as.def.id);
  char *snippet;
  IFJ_ERR err;

  if (def->as.def.rhs->type == AST_LITERAL) {
    err = get_snippet(def->as.def.rhs, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", def->as.def.id, snippet);
    free(snippet);
  } else if (def->as.def.rhs->type == AST_VAR) {
    err = get_snippet(def->as.def.rhs, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", def->as.def.id, snippet);
    free(snippet);
  } else if (def->as.def.rhs->type == AST_FUNC_CALL) {
    // TODO
    err = call_func(def->as.def.rhs);
    CHECK_ERR_RET(err);

    printf("\tPOPS LF@%s\n", def->as.def.id);
    printf("\tCLEARS\n\n");
  } else if (def->as.def.rhs->type == AST_EXPR) {
    err = calc_expression(def->as.def.rhs);
    CHECK_ERR_RET(err);
    printf("\tMOVE LF@%s GF@temp\n", def->as.def.id);
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR assign(ast_node_t *assign) {
  CHECK_ARG_NULL_RET(assign);
  IFJ_ERR err;
  char *snippet;

  switch (assign->as.assign.rhs->type)
  {
  case AST_EXPR:
    err = calc_expression(assign->as.assign.rhs);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s GF@temp\n", assign->as.assign.id);
    break;

  case AST_VAR:
    err = get_snippet(assign->as.assign.rhs, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", assign->as.assign.id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(assign->as.assign.rhs, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", assign->as.assign.id, snippet);
    free(snippet);
    break;

  case AST_FUNC_CALL:
    err = call_func(assign->as.assign.rhs);
    CHECK_ERR_RET(err);

    printf("\tPOPS LF@%s\n", assign->as.assign.id);
    break;
  
  default:
    break;
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR else_condition_expr(ast_node_t *elif, int id) {
  CHECK_ARG_NULL_RET(elif);
  IFJ_ERR err;
  char *snippet;

  switch (elif->as.else_if_st.expr->type)
  {
  case AST_VAR:
    err = get_snippet(elif->as.else_if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(elif->as.else_if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_EXPR:
    err = calc_expression(elif->as.else_if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d GF@temp bool@true\n", id);
    break;

  case AST_FUNC_CALL:
    err = call_func(elif->as.else_if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS GF@temp\n");
    printf("\tJUMPIFEQ then%d GF@temp bool@true\n", id);
    break;
  
  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown condition expression type");
    return IFJ_EINTERNAL; 
  }

  return IFJ_SUCCESS;
}

/**
 * @param id Slouží k vytváření návěští.
 */
static IFJ_ERR condition(ast_node_t *if_st) {
  CHECK_ARG_NULL_RET(if_st);
  IFJ_ERR err;
  char *snippet;
  static int last_id = 0;
  int id = ++last_id;

  printf("LABEL start%d\n", id);    // redundant?
  
  switch (if_st->as.if_st.expr->type)
  {
  case AST_VAR:
    err = get_snippet(if_st->as.if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(if_st->as.if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_EXPR:
    err = calc_expression(if_st->as.if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ then%d GF@temp bool@true\n", id);
    break;

  case AST_FUNC_CALL:
    err = call_func(if_st->as.if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS GF@temp\n");
    printf("\tJUMPIFEQ then%d GF@temp bool@true\n", id);
    break;
  
  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown condition expression type");
    return IFJ_EINTERNAL; 
  }

  int elif_ids[if_st->as.if_st.else_if_cnt];

  // vytvoření else if skoků
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    elif_ids[i] = ++last_id;
    err = else_condition_expr(if_st->as.if_st.else_if[i], elif_ids[i]);   // jelikoz to je jiny AST_TYPE
    CHECK_ERR_RET(err);
  }
  printf("\tJUMP else%d\n", id);

  // vytvoř else if bloků
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    printf("LABEL then%d\n", elif_ids[i]);
    err = generate_body(if_st->as.if_st.else_if[i]->as.else_if_st.body);   // jelikoz to je jiny AST_TYPE
    CHECK_ERR_RET(err);

    printf("\tJUMP end%d\n", id);
  }

  if (if_st->as.if_st.else_st) {    // else větev nemusí existovat
    printf("LABEL else%d\n", id);
    err = generate_body(if_st->as.if_st.else_st);
    CHECK_ERR_RET(err);

    printf("\tJUMP end%d\n", id);
  }

  printf("\nLABEL then%d\n", id);
  err = generate_body(if_st->as.if_st.body);
  CHECK_ERR_RET(err);

  printf("LABEL end%d\n\n", id);

  return IFJ_SUCCESS;
}

static IFJ_ERR get_literal(ast_node_t *literal, char **snippet) {
  CHECK_ARG_NULL_RET(literal);

  char *s = NULL;
  int n;

  switch (literal->as.literal.type) {
  case TYPE_INT:
    n = snprintf(NULL, 0, "%" PRId64, literal->as.literal.int_value);
    if (n < 0)
      return IFJ_EINTERNAL;
    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR(s);
    n = snprintf(s, (size_t)n + 1, "%" PRId64, literal->as.literal.int_value);
    if (n < 0)
      goto cleanup;
    break;

  case TYPE_DOUBLE:
    n = snprintf(NULL, 0, "%a", literal->as.literal.double_value);
    if (n < 0)
      return IFJ_EINTERNAL;
    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR(s);
    n = snprintf(s, (size_t)n + 1, "%.17g", literal->as.literal.double_value);
    if (n < 0)
      goto cleanup;
    break;

  case TYPE_BOOL: {
    const char *val = literal->as.literal.bool_value ? "true" : "false";
    size_t len = strlen(val) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);
    memcpy(s, val, len);
    break;
  }
  case TYPE_STRING: {
    size_t len = strlen(literal->as.literal.str_value) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);
    memcpy(s, literal->as.literal.str_value, len);
    break;
  }
  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown literal type");
    return IFJ_EINTERNAL;
  }

  *snippet = s;
  IFJ_ERR err = format(snippet, literal->as.literal.type);
  CHECK_ERR_RET(err);
  return IFJ_SUCCESS;

cleanup:
  free(s);
  return IFJ_EINTERNAL;
}

static IFJ_ERR get_var(ast_node_t *var, char **snippet) {
  CHECK_ARG_NULL_RET(var);

  size_t len = strlen(var->as.var.id) + 1;
  *snippet = malloc(len);
  CHECK_MEM_ERR(*snippet);
  memcpy(*snippet, var->as.var.id, len); // tohle bude chtit refaktorizovat

  int n = snprintf(NULL, 0, "LF@%s", *snippet);
  if (n < 0)
    goto cleanup;

  char *s = malloc((size_t)n + 1);
  n = snprintf(s, (size_t)n + 1, "LF@%s", *snippet);

  if (n < 0)
    goto cleanup_all;

  free(*snippet);
  *snippet = s;

  return IFJ_SUCCESS;

cleanup_all:
  free(s);

cleanup:
  free(*snippet);
  return IFJ_EINTERNAL;
}

/**
 * @param snippet K předání textu, volající se stará o uvolnění.
 */
static IFJ_ERR get_snippet(ast_node_t *instruction, char **snippet) {
  CHECK_ARG_NULL_RET(instruction);

  switch (instruction->type) {
  case AST_VAR: // 6
    return get_var(instruction, snippet);

  case AST_LITERAL: // 7
    return get_literal(instruction, snippet);

  default:
    printf("\tINSTRUCTION: %d\n", instruction->type);
    LOG_ERR(IFJ_EINTERNAL, "Unknown instruction given to get_snippet");
    return IFJ_EINTERNAL;
  }
}

static IFJ_ERR generate_instruction(ast_node_t *instruction) {
  CHECK_ARG_NULL_RET(instruction);

  IFJ_ERR err;
  char *snippet;

  switch (instruction->type) {
  case AST_EXPR: // 5
    err = calc_expression(instruction);
    CHECK_ERR_RET(err);

    printf("\tPUSHS GF@temp\n");    // kvuli dosavadnimu reseni returnu
    break;

  case AST_VAR: // 6
    err = get_snippet(instruction, &snippet);
    CHECK_ERR_RET(err);

    printf("\tPUSHS %s\n", snippet);    // kvuli dosavadnimu reseni returnu
    free(snippet);
    break;

  case AST_LITERAL: // 7
    err = get_snippet(instruction, &snippet);
    CHECK_ERR_RET(err);

    printf("\tPUSHS %s\n", snippet);    // kvuli dosavadnimu reseni returnu
    free(snippet);
    break;

  case AST_DEF: // 8
    return define_var(instruction);

  case AST_ASSIGN: // 9
    return assign(instruction);

  case AST_IF: // 10
    return condition(instruction);

  case AST_FOR: // 12
    printf("\tfor cycle - TODO\n");
    break;

  case AST_WHILE: // 16
    printf("\twhile cycle - TODO\n");
    break;

  case AST_FUNC_RET: // 17
    printf("\treturn - TODO\n");
    break;

  case AST_FUNC_CALL: // 18
    err = call_func(instruction);
    CHECK_ERR_RET(err);
    // if (instruction->as.func_call)
    // printf("\tPOPS LF@%s\n", def->as.def.id); - potrebuju flag z ast_func ze
    // to ma neco vratit
    printf("\tCLEARS\n\n");
    break;

  default:
    printf("\tINSTRUCTION: %d\n", instruction->type);
    LOG_ERR(IFJ_EINTERNAL, "Unknown instruction given");
    return IFJ_EINTERNAL;
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR generate_body(ast_node_t *body) {
  CHECK_ARG_NULL_RET(body);

  if (body->type != AST_BLOCK) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Passed argument is not a block");
  }

  ast_node_t *instruction;
  IFJ_ERR err;

  // generuje každou instrukci
  for (size_t i = 0; i < body->as.block.commands_cnt; i++) {
    instruction = body->as.block.commands[i];
    err = generate_instruction(instruction);
    CHECK_ERR_RET(err);
  }

  return IFJ_SUCCESS;
}

/**
 * parametry jsou na data stack
 * pak jsou ale ihned vytvoreny kopie v local framu
 *
 * vraci se hodnota na vrcholu zasobniku - volajici to musi popnout
 */
static IFJ_ERR generate_function(ast_node_t *function) {
  CHECK_ARG_NULL_RET(function);

  if (function->type != AST_FUNC) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Passed argument is not a function");
  }

  printf("LABEL %s\n", function->as.func.id); // vytvoří návěští funkce

  // vytvoří lokální kopie parametrů
  printf("\t#Local copies of parameters\n");
  for (size_t i = function->as.func.param_lists_cnt; i > 0; i--) {
    ast_node_t *param_list = function->as.func.param_lists[i - 1];
    for (size_t j = param_list->as.param_list.params_cnt; j > 0; j--) {
      ast_node_t *param = param_list->as.param_list.params[j - 1];

      printf("\tDEFVAR LF@%s\n", param->as.param.id);
      printf("\tPOPS LF@%s\n", param->as.param.id);
    }
  }
  printf("\n");

  IFJ_ERR err = generate_body(function->as.func.body);
  CHECK_ERR_RET(err);

  printf("\tRETURN\n\n"); // az pridam reakci na func_ret tak to mozna bude
                          // redundantni (nebo to tam necham 2x abych to nemusel
                          // kontrolovat jestli to tam uz je) - TODO

  return IFJ_SUCCESS;
}

static IFJ_ERR generate_funcitons(ast_node_t *root) {
  CHECK_ARG_NULL_RET(root);

  for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
    IFJ_ERR err = generate_function(root->as.root.funcs[i]);
    CHECK_ERR_RET(err);
  }

  return IFJ_SUCCESS;
}

// ==================== EXPORTOVANÉ FUNKCE ====================
IFJ_ERR generate(ast_node_t *root) {
  CHECK_ARG_NULL_RET(root);

  if (root->type != AST_ROOT) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                      "Abstract syntax tree doesn't have a root");
  }

  IFJ_ERR err;

  err = start_code(root);
  CHECK_ERR_RET(err);

  err = generate_funcitons(root);
  CHECK_ERR_RET(err);

  return IFJ_SUCCESS;
}
