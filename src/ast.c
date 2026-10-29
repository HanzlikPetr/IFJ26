/**
 * @file ast.c
 * @brief IFJ26 - Implementace abstraktního syntaktického stromu
 *
 * @author xgleton00 Ondřej Glet
 */

#include "ast.h"
#include "error.h"
#include <stdlib.h>

IFJ_ERR ast_create_node(ast_node_t **node, ast_node_type_t type) {
  CHECK_ARG_NULL_RET(node);

  *node = calloc(1, sizeof(ast_node_t));

  CHECK_MEM_ERR(*node);

  (*node)->type = type;

  return IFJ_SUCCESS;
}

static IFJ_ERR ast_array_append(ast_node_t ***arr, size_t *cnt,
                                ast_node_t *child) {
  ast_node_t **tmp = realloc(*arr, sizeof(ast_node_t *) * (*cnt + 1));
  CHECK_MEM_ERR(tmp);

  *arr = tmp;
  (*arr)[*cnt] = child;
  (*cnt)++;

  return IFJ_SUCCESS;
}

IFJ_ERR ast_root_add_func(ast_node_t *root, ast_node_t *func) {
  CHECK_ARG_NULL_RET(root);
  CHECK_ARG_NULL_RET(func);

  if (root->type != AST_ROOT || func->type != AST_FUNC) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&root->as.root.funcs, &root->as.root.funcs_cnt, func);
}

IFJ_ERR ast_func_add_param_list(ast_node_t *func, ast_node_t *param_list) {
  CHECK_ARG_NULL_RET(func);
  CHECK_ARG_NULL_RET(param_list);

  if (param_list->type != AST_PARAM_LIST || func->type != AST_FUNC) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&func->as.func.param_lists,
                          &func->as.func.param_lists_cnt, param_list);
}

IFJ_ERR ast_func_call_add_param_list(ast_node_t *func_call,
                                     ast_node_t *param_list) {
  CHECK_ARGS_NULL_RET(func_call, param_list);

  if (param_list->type != AST_PARAM_LIST || func_call->type != AST_FUNC_CALL) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&func_call->as.func_call.param_lists,
                          &func_call->as.func_call.param_lists_cnt, param_list);
}

IFJ_ERR ast_param_list_add_param(ast_node_t *param_list, ast_node_t *param) {
  CHECK_ARG_NULL_RET(param);
  CHECK_ARG_NULL_RET(param_list);

  if (param_list->type != AST_PARAM_LIST || param->type != AST_PARAM) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&param_list->as.param_list.params,
                          &param_list->as.param_list.params_cnt, param);
}

IFJ_ERR ast_func_call_param_list_add_arg(ast_node_t *param_list,
                                         ast_node_t *arg) {
  CHECK_ARGS_NULL_RET(param_list, arg);

  if (param_list->type != AST_PARAM_LIST) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&param_list->as.param_list.params,
                          &param_list->as.param_list.params_cnt, arg);
}

IFJ_ERR ast_block_add_command(ast_node_t *block, ast_node_t *command) {
  CHECK_ARG_NULL_RET(block);
  CHECK_ARG_NULL_RET(command);

  if (block->type != AST_BLOCK) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&block->as.block.commands,
                          &block->as.block.commands_cnt, command);
}

IFJ_ERR ast_if_add_else_if(ast_node_t *if_st, ast_node_t *else_if_st) {
  CHECK_ARG_NULL_RET(if_st);
  CHECK_ARG_NULL_RET(else_if_st);

  if (if_st->type != AST_IF || else_if_st->type != AST_ELSE_IF) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&if_st->as.if_st.else_if,
                          &if_st->as.if_st.else_if_cnt, else_if_st);
}

IFJ_ERR ast_for_add_guard(ast_node_t *for_st, ast_node_t *guard) {
  CHECK_ARG_NULL_RET(for_st);
  CHECK_ARG_NULL_RET(guard);

  if (for_st->type != AST_FOR || guard->type != AST_FOR_GUARD) {
    return IFJ_EINTERNAL;
  }

  return ast_array_append(&for_st->as.for_st.guards,
                          &for_st->as.for_st.guards_cnt, guard);
}

void ast_free(ast_node_t *root) {
  if (root == NULL) {
    return;
  }

  switch (root->type) {
  case AST_ROOT:
    for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
      ast_free(root->as.root.funcs[i]);
    }
    free(root->as.root.funcs);
    break;

  case AST_FUNC:
    free(root->as.func.id);
    for (size_t i = 0; i < root->as.func.param_lists_cnt; i++) {
      ast_free(root->as.func.param_lists[i]);
    }
    free(root->as.func.param_lists);
    free(root->as.func.return_type);
    ast_free(root->as.func.body);
    break;

  case AST_FUNC_RET:
    ast_free(root->as.func_ret.rhs);
    break;

  case AST_PARAM_LIST:
    for (size_t i = 0; i < root->as.param_list.params_cnt; i++) {
      ast_free(root->as.param_list.params[i]);
    }
    free(root->as.param_list.params);
    break;

  case AST_PARAM:
    free(root->as.param.id);
    free(root->as.param.param_type);
    break;

  case AST_BLOCK:
    for (size_t i = 0; i < root->as.block.commands_cnt; i++) {
      ast_free(root->as.block.commands[i]);
    }
    free(root->as.block.commands);
    break;

  case AST_DEF:
    free(root->as.def.id);
    free(root->as.def.type);
    ast_free(root->as.def.rhs);
    break;

  case AST_ASSIGN:
    free(root->as.assign.id);
    ast_free(root->as.assign.rhs);
    break;

  case AST_IF:
    ast_free(root->as.if_st.expr);
    ast_free(root->as.if_st.body);
    for (size_t i = 0; i < root->as.if_st.else_if_cnt; i++) {
      ast_free(root->as.if_st.else_if[i]);
    }
    free(root->as.if_st.else_if);
    ast_free(root->as.if_st.else_st);
    break;

  case AST_ELSE_IF:
    ast_free(root->as.else_if_st.expr);
    ast_free(root->as.else_if_st.body);
    break;

  case AST_WHILE:
    ast_free(root->as.while_st.expr);
    ast_free(root->as.while_st.body);
    break;

  case AST_FOR:
    ast_free(root->as.for_st.gen);
    for (size_t i = 0; i < root->as.for_st.guards_cnt; i++) {
      ast_free(root->as.for_st.guards[i]);
    }
    free(root->as.for_st.guards);
    ast_free(root->as.for_st.body);
    break;

  case AST_FOR_GEN:
    free(root->as.for_gen.id);
    ast_free(root->as.for_gen.expr);
    ast_free(root->as.for_gen.range);
    break;

  case AST_FOR_GUARD:
    ast_free(root->as.for_guard.expr);
    break;

  case AST_FOR_RANGE:
    ast_free(root->as.for_range.expr);
    ast_free(root->as.for_range.by_expr);
    break;

  case AST_EXPR:
    if (root->as.expr.is_unary) {
      ast_free(root->as.expr.unary.operand);
    } else {
      ast_free(root->as.expr.binary.lhs);
      ast_free(root->as.expr.binary.rhs);
    }
    break;

  case AST_VAR:
    free(root->as.var.id);
    break;

  case AST_LITERAL:
    if (root->as.literal.type == TYPE_STRING) {
      free(root->as.literal.str_value);
    }
    break;

  case AST_FUNC_CALL:
    free(root->as.func_call.id);
    for (size_t i = 0; i < root->as.func_call.param_lists_cnt; i++) {
      ast_free(root->as.func_call.param_lists[i]);
    }
    free(root->as.func_call.param_lists);
    break;

  default:
    break;
  }

  free(root);
}
