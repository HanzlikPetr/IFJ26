/**
 * @file ast.h
 * @brief IFJ26 - Definice abstraktního syntaktického stromu
 *
 * @author xgleton00 Ondřej Glet
 */

#ifndef IFJ_AST_H
#define IFJ_AST_H

#include "error.h"
#include "symtable.h"

typedef enum {
  AST_ROOT,
  AST_FUNC,
  AST_PARAM,
  AST_PARAM_LIST,
  AST_BLOCK,
  AST_EXPR,
  AST_DEF,
  AST_ASSIGN,
  AST_IF,
  AST_ELSE_IF,
  AST_FOR,
  AST_FOR_GEN,
  AST_FOR_RANGE,
  AST_FOR_GUARD,
  AST_WHILE,
  AST_FUNC_RET,
} ast_node_type_t;

typedef struct ast_node ast_node_t;

struct ast_node {
  ast_node_type_t type;
  union {
    struct {
      ast_node_t **funcs;
      size_t funcs_cnt;
    } root;

    struct {
      bool main;
      char *id;
      ast_node_t **param_lists;
      size_t param_lists_cnt;
      ifj26_type_t *return_type;
      ast_node_t *body;
    } func;

    struct {
      ast_node_t *rhs;
    } func_ret;

    struct {
      ast_node_t **params;
      size_t params_cnt;
    } param_list;

    struct {
      char *id;
      ifj26_type_t *param_type;
    } param;

    struct {
      ast_node_t **commands;
      size_t commands_cnt;
    } block;

    struct {

    } expr;

    struct {
      bool mut;
      char *id;
      ifj26_type_t *type;
      ast_node_t *rhs;
    } def;

    struct {
      char *id;
      ast_node_t *rhs;
    } assign;

    struct {
      ast_node_t *expr;
      ast_node_t *body;
      ast_node_t **else_if;
      size_t else_if_cnt;
      ast_node_t *else_st;
    } if_st;

    struct {
      ast_node_t *expr;
      ast_node_t *body;
    } else_if_st;

    struct {
      ast_node_t *expr;
      ast_node_t *body;
    } while_st;

    struct {
      ast_node_t *gen;
      ast_node_t **guards;
      size_t guards_cnt;
      ast_node_t *body;
    } for_st;

    struct {
      char *id;
      ast_node_t *expr;
      ast_node_t *range;
    } for_gen;

    struct {
      ast_node_t *expr;
    } for_guard;

    struct {
      bool until;
      ast_node_t *expr;
      ast_node_t *by_expr;
    } for_range;
  } as;
};

IFJ_ERR ast_create_node(ast_node_t **node, ast_node_type_t type);

IFJ_ERR ast_root_add_func(ast_node_t *root, ast_node_t *func);
IFJ_ERR ast_block_add_command(ast_node_t *block, ast_node_t *command);
IFJ_ERR ast_func_add_param_list(ast_node_t *func, ast_node_t *param_list);
IFJ_ERR ast_param_list_add_param(ast_node_t *param_list, ast_node_t *param);
IFJ_ERR ast_if_add_else_if(ast_node_t *if_st, ast_node_t *else_if_st);
IFJ_ERR ast_for_add_guard(ast_node_t *for_st, ast_node_t *guard);

void ast_free(ast_node_t *root);

#endif
