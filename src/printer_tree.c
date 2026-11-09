/**
 * @file printer_tree.c
 * @brief IFJ26 - Implementace výpisu abstraktního syntaktického stromu (AST)
 *
 * @author xhanzlp00 Petr Hanzlík
 *
 * prompt: "create function that print AST based on ast.h and symtable.h"
 * Gemini Argon: 4.0
 *
 * Nahrazení pomocné funkce print_ifj_type() veřejnou funkcí ifj26_type2str()
 * ze souboru symtable.h bylo provedeno za pomoci modelu GLM-5.3 (via
 * opencode).
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "ast.h"

/**
 * @brief Pomocná funkce pro převod operátoru na text (uprav případně názvy
 * TOK_...)
 */
static const char *op_to_str(token_type_t op) {
  switch (op) {
  case TOK_DOT:
    return ".";
  case TOK_ADD:
    return "+";
  case TOK_SUB:
    return "-";
  case TOK_MUL:
    return "*";
  case TOK_DIV:
    return "/";
  case TOK_CONCAT:
    return "++";
  case TOK_LT:
    return "<";
  case TOK_LEQ:
    return "<=";
  case TOK_GT:
    return ">";
  case TOK_GEQ:
    return ">=";
  case TOK_EQ:
    return "==";
  case TOK_NEQ:
    return "!=";
  case TOK_AND:
    return "&&";
  case TOK_OR:
    return "||";
  case TOK_NOT:
    return "!";
  case TOK_TO:
    return "to";
  case TOK_UNTIL:
    return "until";
  default:
    return "?OP?";
  }
}

/**
 * @brief Rekurzivní výpis libovolného uzlu AST
 */
static void ast_print_node(const ast_node_t *node, const char *prefix,
                           bool is_last, const char *role) {
  printf("%s%s", prefix, is_last ? "└── " : "├── ");
  if (role != NULL) {
    printf("\033[0;90m%s:\033[0m ", role);
  }

  if (node == NULL) {
    printf("(NULL)\n");
    return;
  }

  char next_prefix[512];
  snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix,
           is_last ? "    " : "│   ");

  switch (node->type) {
  case AST_ROOT:
    printf("\033[1;34m[ROOT]\033[0m (funcs: %zu)\n", node->as.root.funcs_cnt);
    for (size_t i = 0; i < node->as.root.funcs_cnt; i++) {
      ast_print_node(node->as.root.funcs[i], next_prefix,
                     i == node->as.root.funcs_cnt - 1, NULL);
    }
    break;

  case AST_FUNC:
    printf("\033[1;34m[FUNC]\033[0m %s\"%s\" -> ",
           node->as.func.main ? "@main " : "",
           node->as.func.id ? node->as.func.id : "NULL");
    printf("%s", ifj26_type2str(&node->as.func.return_type));
    printf("\n");
    for (size_t i = 0; i < node->as.func.param_lists_cnt; i++) {
      ast_print_node(node->as.func.param_lists[i], next_prefix, false,
                     "param_list");
    }
    ast_print_node(node->as.func.body, next_prefix, true, "body");
    break;

  case AST_PARAM_LIST:
    printf("\033[0;36m[PARAM/ARG_LIST]\033[0m (count: %zu)\n",
           node->as.param_list.params_cnt);
    for (size_t i = 0; i < node->as.param_list.params_cnt; i++) {
      ast_print_node(node->as.param_list.params[i], next_prefix,
                     i == node->as.param_list.params_cnt - 1, NULL);
    }
    break;

  case AST_PARAM:
    printf("\033[0;36m[PARAM]\033[0m \"%s\" : ",
           node->as.param.id ? node->as.param.id : "NULL");
    printf("%s", ifj26_type2str(&node->as.param.param_type));
    printf("\n");
    break;

  case AST_BLOCK:
    printf("\033[1;34m[BLOCK]\033[0m (commands: %zu)\n",
           node->as.block.commands_cnt);
    for (size_t i = 0; i < node->as.block.commands_cnt; i++) {
      ast_print_node(node->as.block.commands[i], next_prefix,
                     i == node->as.block.commands_cnt - 1, NULL);
    }
    break;

  case AST_DEF:
    printf(
        "\033[1;32m[DEF]\033[0m %s \"%s\" : ", node->as.def.mut ? "var" : "val",
        node->as.def.id ? node->as.def.id : "NULL");
    printf("%s", ifj26_type2str(&node->as.def.type));
    printf("\n");
    ast_print_node(node->as.def.rhs, next_prefix, true, "rhs");
    break;

  case AST_ASSIGN:
    printf("\033[1;32m[ASSIGN]\033[0m \"%s\" =\n",
           node->as.assign.id ? node->as.assign.id : "NULL");
    ast_print_node(node->as.assign.rhs, next_prefix, true, "rhs");
    break;

  case AST_FUNC_RET:
    printf("\033[1;31m[RETURN]\033[0m\n");
    if (node->as.func_ret.rhs != NULL) {
      ast_print_node(node->as.func_ret.rhs, next_prefix, true, "value");
    }
    break;

  case AST_IF: {
    printf("\033[1;33m[IF]\033[0m\n");
    bool has_else_if = (node->as.if_st.else_if_cnt > 0);
    bool has_else = (node->as.if_st.else_st != NULL);
    ast_print_node(node->as.if_st.expr, next_prefix, false, "cond");
    ast_print_node(node->as.if_st.body, next_prefix, !has_else_if && !has_else,
                   "then");
    for (size_t i = 0; i < node->as.if_st.else_if_cnt; i++) {
      bool last_branch = (!has_else && i == node->as.if_st.else_if_cnt - 1);
      ast_print_node(node->as.if_st.else_if[i], next_prefix, last_branch,
                     "else_if");
    }
    if (has_else) {
      ast_print_node(node->as.if_st.else_st, next_prefix, true, "else");
    }
    break;
  }

  case AST_ELSE_IF:
    printf("\033[1;33m[ELSE_IF]\033[0m\n");
    ast_print_node(node->as.else_if_st.expr, next_prefix, false, "cond");
    ast_print_node(node->as.else_if_st.body, next_prefix, true, "then");
    break;

  case AST_WHILE:
    printf("\033[1;33m[WHILE]\033[0m\n");
    ast_print_node(node->as.while_st.expr, next_prefix, false, "cond");
    ast_print_node(node->as.while_st.body, next_prefix, true, "body");
    break;

  case AST_FOR:
    printf("\033[1;33m[FOR]\033[0m\n");
    ast_print_node(node->as.for_st.gen, next_prefix, false, "generator");
    for (size_t i = 0; i < node->as.for_st.guards_cnt; i++) {
      ast_print_node(node->as.for_st.guards[i], next_prefix, false, "guard");
    }
    ast_print_node(node->as.for_st.body, next_prefix, true, "body");
    break;

  case AST_FOR_GEN:
    printf("\033[0;33m[FOR_GEN]\033[0m \"%s\" <-\n",
           node->as.for_gen.id ? node->as.for_gen.id : "NULL");
    ast_print_node(node->as.for_gen.expr, next_prefix,
                   node->as.for_gen.range == NULL, "from/source");
    if (node->as.for_gen.range != NULL) {
      ast_print_node(node->as.for_gen.range, next_prefix, true, "range");
    }
    break;

  case AST_FOR_RANGE:
    printf("\033[0;33m[FOR_RANGE]\033[0m (%s)\n",
           node->as.for_range.until ? "until" : "to");
    ast_print_node(node->as.for_range.expr, next_prefix,
                   node->as.for_range.by_expr == NULL, "end");
    if (node->as.for_range.by_expr != NULL) {
      ast_print_node(node->as.for_range.by_expr, next_prefix, true, "by");
    }
    break;

  case AST_FOR_GUARD:
    printf("\033[0;33m[FOR_GUARD]\033[0m\n");
    ast_print_node(node->as.for_guard.expr, next_prefix, true, "cond");
    break;

  case AST_EXPR:
    if (node->as.expr.is_unary) {
      printf("\033[1;33m[UNARY_OP]\033[0m '%s'\n",
             op_to_str(node->as.expr.operator_type));
      ast_print_node(node->as.expr.unary.operand, next_prefix, true, NULL);
    } else {
      printf("\033[1;36m[BIN_OP]\033[0m '%s'\n",
             op_to_str(node->as.expr.operator_type));
      ast_print_node(node->as.expr.binary.lhs, next_prefix, false, "lhs");
      ast_print_node(node->as.expr.binary.rhs, next_prefix, true, "rhs");
    }
    break;

  case AST_VAR:
    printf("\033[1;32m[VAR]\033[0m id = \"%s\"\n",
           node->as.var.id ? node->as.var.id : "NULL");
    break;

  case AST_LITERAL:
    printf("\033[1;35m[LITERAL]\033[0m ");
    switch (node->as.literal.type) {
    case TYPE_INT:
      printf("Int: %" PRId64 "\n", (int64_t)node->as.literal.int_value);
      break;
    case TYPE_DOUBLE:
      printf("Double: %g\n", node->as.literal.double_value);
      break;
    case TYPE_BOOL:
      printf("Boolean: %s\n", node->as.literal.bool_value ? "true" : "false");
      break;
    case TYPE_STRING:
      printf("String: \"%s\"\n",
             node->as.literal.str_value ? node->as.literal.str_value : "");
      break;
    case TYPE_UNIT:
      printf("Unit: ()\n");
      break;
    default:
      printf("Unknown literal\n");
      break;
    }
    break;

  case AST_FUNC_CALL:
    printf("\033[1;35m[CALL]\033[0m id = \"%s\" (arg_lists: %zu)\n",
           node->as.func_call.id ? node->as.func_call.id : "NULL",
           node->as.func_call.param_lists_cnt);
    for (size_t i = 0; i < node->as.func_call.param_lists_cnt; i++) {
      ast_print_node(node->as.func_call.param_lists[i], next_prefix,
                     i == node->as.func_call.param_lists_cnt - 1, "args");
    }
    break;

  default:
    printf("[UNKNOWN_NODE type=%d]\n", node->type);
    break;
  }
}

/**
 * @brief Vypíše celý AST strom (od kořene programu nebo od kořene podvýrazu)
 */
void ast_print(const ast_node_t *root) {
  printf("\n================ AST ================\n");
  if (root == NULL) {
    printf("(NULL)\n");
  } else {
    ast_print_node(root, "", true, NULL);
  }
  printf("=====================================\n\n");
}
