/**
 * @file ast.h
 * @brief IFJ26 - Definice abstraktního syntaktického stromu
 *
 * @author xgleton00 Ondřej Glet
 */

#ifndef IFJ_AST_H
#define IFJ_AST_H

#include <stdint.h> // Kvuli int64_t
#include <stdlib.h> // Kvuli size_t

#include "error.h"
#include "scanner.h"
#include "symtable.h"

typedef enum {
  AST_ROOT,
  AST_FUNC,
  AST_PARAM,
  AST_PARAM_LIST,
  AST_BLOCK,
  AST_EXPR,
  AST_VAR,
  AST_LITERAL,
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
  AST_FUNC_CALL,
} ast_node_type_t;

typedef struct ast_node ast_node_t;

struct ast_node {
  ast_node_type_t type;
  ifj26_type_t
      inferred_type; /**< Odvozený typ výrazu, pokud je to relevantní */

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

    /**
     * @brief Reprezentace volání funkce, metody nebo indexace seznamu (např.
     * foo(1, 2), add(x)(y)).
     */
    struct {
      char *id; /**< Název funkce */
      ast_node_t **param_lists;  /**< Pole seznamů parametrů */
      size_t param_lists_cnt;    /**< Počet seznamů parametrů */
    } func_call;

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

    /**
     * @brief Reprezentace výrazu.
     */
    struct {
      bool is_unary; /**< Flag, true = unární operátor, false = binární operátor
                      */
      token_type_t
          operator_type; /**< Typ operátoru (např. TOK_ADD, TOK_SUB, ...) */
      union {
        /**
         * @brief Struktura pro binární operátory
         */
        struct {
          ast_node_t *lhs; /**< Levý operand (pro binární operátory) */
          ast_node_t *rhs; /**< Pravý operand (pro binární operátory) */
        } binary;

        /**
         * @brief Struktura pro unární operátory
         */
        struct {
          ast_node_t *operand; /**< Operand (pro unární operátory) */
        } unary;
      };
    } expr;

    /**
     * @brief Reprezentace proměnné.
     */
    struct {
      char *id; /**< Název proměnné */
    } var;

    /**
     * @brief Reprezentace literálu.
     */
    struct {
      data_type_t type; /**< Typ literálu (např. TYPE_INT, TYPE_FLOAT,
                           TYPE_STRING, ...) */
      bool is_folded; /**< Flag, true = literál je složený (např. výsledek
                         konstantní výrazu), false = literál je přímo zapsaný */
      union {
        int64_t int_value; /**< Hodnota literálu typu int */
        double double_value; /**< Hodnota literálu typu double */
        bool bool_value; /**< Hodnota literálu typu bool */
        char *str_value; /**< Hodnota literálu typu string (alokovaná paměť) */
      };
    } literal;

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

/**
 * @brief Přidá seznam argumentů do volání funkce.
 * @param func_call AST uzel volání funkce.
 * @param param_list AST uzel seznamu argumentů.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_func_call_add_param_list(ast_node_t *func_call,
                                     ast_node_t *param_list);
IFJ_ERR ast_param_list_add_param(ast_node_t *param_list, ast_node_t *param);

/**
 * @brief Přidá argument do seznamu argumentů volání funkce.
 * @param param_list AST uzel seznamu argumentů.
 * @param arg AST uzel argumentu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_func_call_param_list_add_arg(ast_node_t *param_list,
                                         ast_node_t *arg);
IFJ_ERR ast_if_add_else_if(ast_node_t *if_st, ast_node_t *else_if_st);
IFJ_ERR ast_for_add_guard(ast_node_t *for_st, ast_node_t *guard);

void ast_free(ast_node_t *root);

#endif
