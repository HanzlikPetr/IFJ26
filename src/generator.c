 /**
 * @file generator.c
 * @brief IFJ26 - Implementace generátoru
 *
 * @author xberanm00 Marek Beran
 */

// --- libraries ---
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- our files ---
#include "generator.h"
#include "printer_tree.h"

// ==================== STRUKTURY ====================
typedef struct {
  char **data;
  size_t size;
} local_ids_t;

local_ids_t local_ids;

// ==================== POMOCNÉ FUNKCE ====================

static IFJ_ERR local_ids_ctor(local_ids_t *local_ids) {
  // pole s hlavickou
  local_ids->data = malloc(sizeof(char *) * 1);
  CHECK_MEM_ERR(local_ids->data);

  local_ids->size = 1;
  local_ids->data[0] = NULL;

  return IFJ_SUCCESS;
}

static void local_ids_dtor(local_ids_t *local_ids) { free(local_ids->data); }

static IFJ_ERR get_snippet(ast_node_t *instruction, char **snippet);
static IFJ_ERR call_func(ast_node_t *func_call);
static IFJ_ERR generate_body(ast_node_t *body);
static IFJ_ERR generate_instruction(ast_node_t *instruction);

static IFJ_ERR start_code(ast_node_t *root) {
  // CHECK_ARG_NULL_RET(root); // redundant
  puts(".IFJcode26");

  printf("\n\t# Initialization of global variables\n");
  printf("\tDEFVAR GF@-temp\n");
  printf("\tDEFVAR GF@-tempLeft\n");
  printf("\tDEFVAR GF@-tempRight\n\n");

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

  printf("LABEL -error\n"
         "\tEXIT int@40\n");

  printf("\n\t# Built-in functions\n");

  printf("LABEL print\n"
         "\tDEFVAR LF@symb\n"
         "\tPOPS LF@symb\n"
         "\tTYPE GF@-temp LF@symb\n"
         "\tJUMPIFEQ -printNil GF@-temp string@nil\n"
         "\tWRITE LF@symb\n"
         "\tJUMP -endPrint\n"
         "\nLABEL -printNil\n"
         "\tWRITE string@()\n"
         "\nLABEL -endPrint\n"
         "\tRETURN\n\n");

  printf("LABEL readLine\n"
         "\tREAD GF@-temp string\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL readInt\n"
         "\tREAD GF@-temp int\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL readDouble\n"
         "\tREAD GF@-temp float\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL toStr\n"
         "\tDEFVAR LF@type\n"
         "\tPOPS GF@-temp\n"
         "\tTYPE LF@type GF@-temp\n"
         "\tJUMPIFEQ -toStrEnd LF@type string@string\n"
         "\tJUMPIFEQ -toStrInt LF@type string@int\n"
         "\tJUMPIFEQ -toStrFloat LF@type string@float\n"
         "\tJUMPIFEQ -toStrNil LF@type string@nil\n"
         // je to bool
         "\tJUMPIFEQ -toStrTrue GF@-temp bool@true\n"

         "\n\tMOVE GF@-temp string@false\n"
         "\tJUMP -toStrEnd\n"

         "\nLABEL -toStrTrue\n"
         "\tMOVE GF@-temp string@true\n"
         "\tJUMP -toStrEnd\n"

         "\nLABEL -toStrNil\n"
         "\tMOVE GF@-temp string@()\n"
         "\tJUMP -toStrEnd\n"

         "\nLABEL -toStrInt\n"
         "\tINT2STR GF@-temp GF@-temp\n"
         "\tJUMP -toStrEnd\n"

         "\nLABEL -toStrFloat\n"
         "\tFLOAT2STR GF@-temp GF@-temp\n"

         "\nLABEL -toStrEnd\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL toInt\n"
         "\tPOPS GF@-temp\n"
         "\tFLOAT2INT GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL strLength\n"
         "\tPOPS GF@-temp\n"
         "\tSTRLEN GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL subStr\n"
         "\t#Local copies of parameters\n"
         "\tDEFVAR LF@end$0\n"
         "\tPOPS LF@end$0\n"
         "\tDEFVAR LF@start$0\n"
         "\tPOPS LF@start$0\n"
         "\tDEFVAR LF@value$0\n"
         "\tPOPS LF@value$0\n"
         "\n\t# Definition of local variables\n"
         "\tDEFVAR LF@len$1\n"
         "\tDEFVAR LF@res$1\n"
         "\tDEFVAR LF@i$1\n"
         "\n\tCREATEFRAME\n"
         "\tPUSHS LF@value$0\n"
         "\tPUSHFRAME\n"
         "\tCALL strLength\n"
         "\tPOPFRAME\n"
         "\tPOPS LF@len$1\n"
         "\nLABEL -subStrConditions\n"
         "\tMOVE GF@-tempLeft LF@start$0\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight int@0\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tLT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tJUMPIFEQ -error GF@-temp bool@true\n"
         "\tMOVE GF@-tempLeft LF@end$0\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight int@0\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tLT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tJUMPIFEQ -error GF@-temp bool@true\n"
         "\tMOVE GF@-tempLeft LF@start$0\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight LF@end$0\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tGT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tJUMPIFEQ -error GF@-temp bool@true\n"
         "\tMOVE GF@-tempLeft LF@start$0\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight LF@len$1\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tPUSHS GF@-temp\n"
         "\tGT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\tOR GF@-temp GF@-temp GF@-tempLeft\n"
         "\tJUMPIFEQ -error GF@-temp bool@true\n"
         "\tMOVE GF@-tempLeft LF@end$0\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight LF@len$1\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tGT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tJUMPIFEQ -error GF@-temp bool@true\n"
         "\n\tMOVE LF@res$1 string@\n"
         "\tMOVE LF@i$1 LF@start$0\n"
         "\n\t# Definition of loop variables\n"
         "\tDEFVAR LF@c$2\n"
         "\nLABEL -subStrWhileStart\n"
         "\tMOVE GF@-tempLeft LF@i$1\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight LF@end$0\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tLT GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tJUMPIFNEQ -subStrWhileEnd GF@-temp bool@true\n"
         "\nLABEL -subStrWhileBody\n"
         "\tGETCHAR LF@c$2 LF@value$0 LF@i$1\n"
         "\tCONCAT LF@res$1 LF@res$1 LF@c$2\n"
         "\tMOVE GF@-tempLeft LF@i$1\n"
         "\tPUSHS GF@-tempLeft\n"
         "\n\tMOVE GF@-tempRight int@1\n"
         "\tPUSHS GF@-tempRight\n"
         "\n\tPOPS GF@-tempRight\n"
         "\tPOPS GF@-tempLeft\n"
         "\n\tADD GF@-temp GF@-tempLeft GF@-tempRight\n"
         "\tMOVE LF@i$1 GF@-temp\n"
         "\tJUMP -subStrWhileStart\n"
         "\nLABEL -subStrWhileEnd\n"
         "\tPUSHS LF@res$1\n"
         "\tRETURN\n\n");

  printf("LABEL ord\n"
         "\tDEFVAR LF@index\n"
         "\tPOPS LF@index\n"
         "\tDEFVAR LF@value\n"
         "\tPOPS LF@value\n"
         "\tDEFVAR LF@len\n"
         "\n\tJUMPIFEQ -ordErr LF@value string@\n"
         "\n\tCREATEFRAME\n"
         "\tPUSHS LF@value\n"
         "\tPUSHFRAME\n"
         "\tCALL strLength\n"
         "\tPOPFRAME\n"
         "\tPOPS LF@len\n"
         "\tLT GF@-temp LF@index LF@len\n"
         "\tJUMPIFEQ -ordErr GF@-temp bool@false\n"
         "\tLT GF@-temp LF@index int@0\n"
         "\tJUMPIFEQ -ordErr GF@-temp bool@true\n"
         "\n\tSTRI2INT GF@-temp LF@value LF@index\n"
         "\tPUSHS GF@-temp\n"
         "\tJUMP -ordSkip\n"
         "\nLABEL -ordErr\n"
         "\tPUSHS int@-1\n"
         "\nLABEL -ordSkip\n"
         "\tRETURN\n\n");

  printf("LABEL chr\n"
         "\tPOPS GF@-temp\n"
         "\tINT2CHAR GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  printf("LABEL readBoolean\n"
         "\tREAD GF@-temp bool\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

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
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);
    n = snprintf(s, (size_t)n + 1, "string@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_INT) {
    n = snprintf(NULL, 0, "int@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);
    n = snprintf(s, (size_t)n + 1, "int@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_DOUBLE) {
    n = snprintf(NULL, 0, "float@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);
    n = snprintf(s, (size_t)n + 1, "float@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_BOOL) {
    n = snprintf(NULL, 0, "bool@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);
    n = snprintf(s, (size_t)n + 1, "bool@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;
  }

  return IFJ_SUCCESS;

cleanup:
  free(*snippet);
  return IFJ_EINTERNAL;
}

static IFJ_ERR get_op(ast_node_t *expr, const char **snippet) {
  token_type_t type = expr->as.expr.operator_type;
  ast_node_t *left = expr->as.expr.binary.lhs;
  ast_node_t *right = expr->as.expr.binary.rhs;
  IFJ_ERR err = IFJ_SUCCESS;

  switch (type) {
  case TOK_EQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    *snippet = "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_NEQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    *snippet = "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tNOT GF@-temp\n";
    break;

  case TOK_LT:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    if (left->inferred_type.type ==
        TYPE_UNIT) { // datový typ nil se může porovnávat pouze pomocí
                            // operace EQ
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tLT GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_GT:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    if (left->inferred_type.type ==
        TYPE_UNIT) { // datový typ nil se může porovnávat pouze pomocí
                            // operace EQ
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tGT GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_LEQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    if (left->inferred_type.type == TYPE_UNIT) {
      // datový typ nil se může porovnávat pouze s nil
      *snippet = "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n";
      break;
    }

    *snippet =
        "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n\tLT "
        "GF@-temp GF@-tempLeft GF@-tempRight\n\tPOPS GF@-tempLeft\n\tOR "
        "GF@-temp GF@-temp GF@-tempLeft\n";
    break;

  case TOK_GEQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    if (left->inferred_type.type == TYPE_UNIT) {
      // datový typ nil se může porovnávat pouze s nil
      *snippet = "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n";
      break;
    }

    *snippet =
        "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n\tGT "
        "GF@-temp GF@-tempLeft GF@-tempRight\n\tPOPS GF@-tempLeft\n\tOR "
        "GF@-temp GF@-temp GF@-tempLeft\n";
    break;

  case TOK_ADD:
    if (left->inferred_type.type != right->inferred_type.type) {
      if ((left->inferred_type.type >= TYPE_STRING) !=
          (right->inferred_type.type >= TYPE_STRING)) { // logický XOR
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }
    switch (left->inferred_type.type) {
    case TYPE_INT:
      *snippet = "\tADD GF@-temp GF@-tempLeft GF@-tempRight\n";
      break;

    case TYPE_DOUBLE:
      *snippet = "\tADD GF@-temp GF@-tempLeft GF@-tempRight\n";
      break;

    case TYPE_STRING:
      *snippet = "\tCONCAT GF@-temp GF@-tempLeft GF@-tempRight\n";
      break;

    default:
      err = IFJ_EINTERNAL;
      break;
    }

    break;

  case TOK_SUB:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tSUB GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_MUL:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tMUL GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_DIV:
    if (left->inferred_type.type != right->inferred_type.type) {
      if (left->inferred_type.type + right->inferred_type.type > 1) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) {
      err = IFJ_EINTERNAL;
    }

    if (left->inferred_type.type == TYPE_DOUBLE) {
      *snippet = "\tDIV GF@-temp GF@-tempLeft GF@-tempRight\n";
    } else if (left->inferred_type.type == TYPE_INT) {
      *snippet = "\tIDIV GF@-temp GF@-tempLeft GF@-tempRight\n";
    } else {
      err = IFJ_EINTERNAL;
    }
    break;

  case TOK_AND:
    if (left->inferred_type.type != right->inferred_type.type ||
        left->inferred_type.type != TYPE_BOOL) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tAND GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_OR:
    if (left->inferred_type.type != right->inferred_type.type ||
        left->inferred_type.type != TYPE_BOOL) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tOR GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown operator type");
    return IFJ_EINTERNAL;
  }

  CHECK_ERR_LOG_RET(err, "Uncompatible operator type");

  return IFJ_SUCCESS;
}

static IFJ_ERR valmem_chain(ast_node_t *expr) {
  IFJ_ERR err;
  char *snippet;

  if (expr->as.expr.binary.lhs->type != AST_EXPR) {
    err = get_snippet(expr->as.expr.unary.operand, &snippet);
    CHECK_ERR_RET(err);

    if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toString") == 0) {
      printf("\tCREATEFRAME\n"
             "\tPUSHS %s\n"
             "\tPUSHFRAME\n"
             "\tCALL toStr\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n",
             snippet);
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toInt") == 0) {
      printf("\tCREATEFRAME\n"
             "\tPUSHS %s\n"
             "\tPUSHFRAME\n"
             "\tCALL toInt\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n",
             snippet);
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "length") == 0) {
      printf("\tCREATEFRAME\n"
             "\tPUSHS %s\n"
             "\tPUSHFRAME\n"
             "\tCALL strLength\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n",
             snippet);

    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "floor") == 0) {
      printf("\tCREATEFRAME\n"
             "\tPUSHS %s\n"
             "\tPUSHFRAME\n"
             "\tCALL toInt\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n"
             "\tINT2FLOAT GF@-temp GF@-temp\n",
             snippet);
    }
    free(snippet);

    return IFJ_SUCCESS;
  } else {
    err = valmem_chain(expr->as.expr.binary.lhs);
    CHECK_ERR_RET(err);

    printf("\tCREATEFRAME\n");
    printf("\tPUSHS GF@-temp\n");
    printf("\tPUSHFRAME\n");

    if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toString") == 0) {
      printf("\tCALL toStr\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toInt") == 0) {
      printf("\tCALL toInt\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "length") == 0) {
      printf("\tCALL strLength\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "floor") == 0) {
      printf("\tCALL toInt\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n"
             "\tINT2FLOAT GF@-temp GF@-temp\n");
    }

    printf("\tPOPFRAME\n");
    printf("\t# end of valmem\n\n");

    return IFJ_SUCCESS;
  }
}

static IFJ_ERR calc_expression(ast_node_t *expr) {
  CHECK_ARG_NULL_RET(expr);
  char *snippet;
  IFJ_ERR err;

  if (expr->as.expr.is_unary) {
    if (expr->as.expr.operator_type != TOK_NOT) { // jediný unární operand
      return IFJ_EINTERNAL;
    }

    if (expr->as.expr.unary.operand->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.unary.operand);
      CHECK_ERR_RET(err);
    } else if (expr->as.expr.unary.operand->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.unary.operand);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-temp\n");
    } else {
      err = get_snippet(expr->as.expr.unary.operand, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-temp %s\n", snippet);
      free(snippet);
    }

    printf("\tNOT GF@-temp\n");
  } else {
    if (expr->as.expr.operator_type == TOK_NOT) { // jediný nebinární operand
      return IFJ_EINTERNAL;
    }

    if (expr->as.expr.operator_type == TOK_DOT) {
      if (expr->as.expr.binary.lhs->type != AST_EXPR) {
        err = get_snippet(expr->as.expr.binary.lhs, &snippet);
        CHECK_ERR_RET(err);

        if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toString") == 0) {
          printf("\n\t# VALMEM\n"
                 "\tCREATEFRAME\n"
                 "\tPUSHS %s\n"
                 "\tPUSHFRAME\n"
                 "\tCALL toStr\n"
                 "\tPOPFRAME\n"
                 "\tPOPS GF@-temp\n",
                 snippet);
          free(snippet);
        } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toInt") == 0) {
          printf("\n\t# VALMEM\n"
                 "\tCREATEFRAME\n"
                 "\tPUSHS %s\n"
                 "\tPUSHFRAME\n"
                 "\tCALL toInt\n"
                 "\tPOPFRAME\n"
                 "\tPOPS GF@-temp\n",
                 snippet);
          free(snippet);
        } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "length") == 0) {
          printf("\n\t# VALMEM\n"
                 "\tCREATEFRAME\n"
                 "\tPUSHS %s\n"
                 "\tPUSHFRAME\n"
                 "\tCALL strLength\n"
                 "\tPOPFRAME\n"
                 "\tPOPS GF@-temp\n",
                 snippet);
          free(snippet);
        } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "floor") == 0) {
          printf("\n\t# VALMEM\n"
                 "\tCREATEFRAME\n"
                 "\tPUSHS %s\n"
                 "\tPUSHFRAME\n"
                 "\tCALL toInt\n"
                 "\tPOPFRAME\n"
                 "\tPOPS GF@-temp\n"
                 "\tINT2FLOAT GF@-temp GF@-temp\n",
                 snippet);
          free(snippet);
        }
      } else {
        err = valmem_chain(expr);
        CHECK_ERR_RET(err);
      }

      return IFJ_SUCCESS;
    } else if (expr->as.expr.binary.lhs->type ==
               AST_EXPR) { // získání levé strany
      err = calc_expression(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempLeft GF@-temp\n");
    } else if (expr->as.expr.binary.lhs->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-tempLeft\n");
    } else {
      err = get_snippet(expr->as.expr.binary.lhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempLeft %s\n", snippet);
      free(snippet);
    }

    printf("\tPUSHS GF@-tempLeft\n\n");

    // získání pravé strany
    if (expr->as.expr.binary.rhs->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.binary.rhs);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempRight GF@-temp\n");
    } else if (expr->as.expr.binary.rhs->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.binary.rhs);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-tempRight\n");
    } else {
      err = get_snippet(expr->as.expr.binary.rhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempRight %s\n", snippet);
      free(snippet);
    }

    printf("\tPUSHS GF@-tempRight\n\n");
    printf("\tPOPS GF@-tempRight\n");

    printf("\tPOPS GF@-tempLeft\n\n");

    err = get_op(expr, (const char **)&snippet);
    CHECK_ERR_RET(err);

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
        printf("\tPUSHS GF@-temp\n");
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

static IFJ_ERR assign(ast_node_t *assign) {
  CHECK_ARG_NULL_RET(assign);
  IFJ_ERR err;
  char *snippet;

  switch (assign->as.assign.rhs->type) {
  case AST_EXPR:
    err = calc_expression(assign->as.assign.rhs);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s GF@-temp\n", assign->as.assign.id);
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

  switch (elif->as.else_if_st.expr->type) {
  case AST_VAR:
    err = get_snippet(elif->as.else_if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(elif->as.else_if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_EXPR:
    err = calc_expression(elif->as.else_if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d GF@-temp bool@true\n", id);
    break;

  case AST_FUNC_CALL:
    err = call_func(elif->as.else_if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS GF@-temp\n");
    printf("\tJUMPIFEQ -then%d GF@-temp bool@true\n", id);
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

  printf("LABEL -start%d\n", id); // redundant?

  switch (if_st->as.if_st.expr->type) {
  case AST_VAR:
    err = get_snippet(if_st->as.if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(if_st->as.if_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_EXPR:
    err = calc_expression(if_st->as.if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFEQ -then%d GF@-temp bool@true\n", id);
    break;

  case AST_FUNC_CALL:
    err = call_func(if_st->as.if_st.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS GF@-temp\n");
    printf("\tJUMPIFEQ -then%d GF@-temp bool@true\n", id);
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown condition expression type");
    return IFJ_EINTERNAL;
  }

  int elif_ids[if_st->as.if_st.else_if_cnt];

  // vytvoření else if skoků
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    elif_ids[i] = ++last_id;
    err = else_condition_expr(if_st->as.if_st.else_if[i],
                              elif_ids[i]); // jelikoz to je jiny AST_TYPE
    CHECK_ERR_RET(err);
  }

  printf("\tJUMP -%s%d\n", if_st->as.if_st.else_st == NULL ? "end" : "else",
         id);

  // vytvoř else if bloků
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    printf("LABEL -then%d\n", elif_ids[i]);
    err =
        generate_body(if_st->as.if_st.else_if[i]
                          ->as.else_if_st.body); // jelikoz to je jiny AST_TYPE
    CHECK_ERR_RET(err);

    printf("\tJUMP -end%d\n", id);
  }

  if (if_st->as.if_st.else_st) { // else větev nemusí existovat
    printf("LABEL -else%d\n", id);
    err = generate_body(if_st->as.if_st.else_st);
    CHECK_ERR_RET(err);

    printf("\tJUMP -end%d\n", id);
  }

  printf("\nLABEL -then%d\n", id);
  err = generate_body(if_st->as.if_st.body);
  CHECK_ERR_RET(err);

  printf("LABEL -end%d\n\n", id);

  return IFJ_SUCCESS;
}

static IFJ_ERR define_var(ast_node_t *def) {
  CHECK_ARG_NULL_RET(def);
  char *snippet;
  IFJ_ERR err;

  for (size_t i = 1; i < local_ids.size; i++) {
    if (strcmp(def->as.def.id, local_ids.data[i]) == 0) {

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
        err = call_func(def->as.def.rhs);
        CHECK_ERR_RET(err);

        printf("\tPOPS LF@%s\n", def->as.def.id);
      } else if (def->as.def.rhs->type == AST_EXPR) {
        err = calc_expression(def->as.def.rhs);
        CHECK_ERR_RET(err);
        printf("\tMOVE LF@%s GF@-temp\n", def->as.def.id);
      }

      return IFJ_SUCCESS;
    }
  }

  printf("\tDEFVAR LF@%s\n", def->as.def.id);

  char **p = realloc(local_ids.data, (local_ids.size + 1) * sizeof *p);
  CHECK_MEM_ERR(p);
  local_ids.data = p;

  local_ids.data[local_ids.size] = def->as.def.id;
  local_ids.size = local_ids.size + 1;

  return IFJ_SUCCESS;
}

static IFJ_ERR define_local_vars(ast_node_t *body) {
  CHECK_ARG_NULL_RET(body);
  IFJ_ERR err;

  for (size_t i = 0; i < body->as.block.commands_cnt; i++) {
    ast_node_t *instruction = body->as.block.commands[i];

    if (instruction->type == AST_DEF) {
      err = define_var(instruction);
      CHECK_ERR_RET(err);
    }
  }

  printf("\n");

  return IFJ_SUCCESS;
}

static IFJ_ERR while_loop(ast_node_t *while_st) {
  CHECK_ARG_NULL_RET(while_loop);
  IFJ_ERR err;
  char *snippet;
  static int last_id = 0;
  int id = ++last_id;
  ast_node_t *body = while_st->as.while_st.body;

  // definování vnořených proměnných cyklů
  printf("\n\t# Definition of loop variables\n");
  err = define_local_vars(body);
  CHECK_ERR_RET(err);

  printf("\nLABEL -whileStart%d\n", id);

  switch (while_st->as.while_st.expr->type) {
  case AST_VAR:
    err = get_snippet(while_st->as.while_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFNEQ -whileEnd%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(while_st->as.while_st.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFNEQ -whileEnd%d %s bool@true\n", id, snippet);
    free(snippet);
    break;

  case AST_EXPR:
    err = calc_expression(while_st->as.while_st.expr);
    CHECK_ERR_RET(err);

    printf("\tJUMPIFNEQ -whileEnd%d GF@-temp bool@true\n", id);
    break;

  case AST_FUNC_CALL:
    err = call_func(while_st->as.while_st.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS GF@-temp\n");
    printf("\tJUMPIFNEQ -whileEnd%d GF@-temp bool@true\n", id);
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown condition expression type");
    return IFJ_EINTERNAL;
  }

  printf("\nLABEL -whileBody%d\n", id);

  // generuje každou instrukci
  for (size_t i = 0; i < body->as.block.commands_cnt; i++) {
    ast_node_t *instruction = body->as.block.commands[i];

    err = generate_instruction(instruction);
    CHECK_ERR_RET(err);
  }

  printf("\tJUMP -whileStart%d\n", id);

  printf("\nLABEL -whileEnd%d\n", id);

  return IFJ_SUCCESS;
}

static IFJ_ERR ret_st(ast_node_t *func_ret) {
  CHECK_ARG_NULL_RET(func_ret);
  IFJ_ERR err;
  char *snippet;

  if (func_ret->as.func_ret.rhs) {
    ast_node_type_t type = func_ret->as.func_ret.rhs->type;
    if (type == AST_FUNC_CALL) {
      generate_instruction(func_ret->as.func_ret.rhs);
    } else if (type == AST_EXPR) {
      generate_instruction(func_ret->as.func_ret.rhs);
      printf("\tPUSHS GF@-temp\n");
    } else {
      err = get_snippet(func_ret->as.func_ret.rhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tPUSHS %s\n", snippet);
      free(snippet);
    }
  }

  printf("\tRETURN\n");
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
    n = snprintf(s, (size_t)n + 1, "%a", literal->as.literal.double_value);
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

  case TYPE_UNIT:
    char *text = "nil@nil";
    size_t len = strlen(text) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);

    memcpy(s, text, len);
    break;

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

    printf("\tPUSHS GF@-temp\n");
    break;

  case AST_VAR: // 6

    err = get_snippet(instruction, &snippet);
    CHECK_ERR_RET(err);

    printf("\tPUSHS %s\n", snippet);
    free(snippet);
    break;

  case AST_DEF: // 8
    return define_var(instruction);

  case AST_ASSIGN: // 9
    return assign(instruction);

  case AST_IF: // 10
    return condition(instruction);

  case AST_FOR: // 12
    // return for_loop(instruction);ahoj"
    // printf("\tfor cycle - TODO\n");
    break;

  case AST_WHILE: // 16
    return while_loop(instruction);

  case AST_FUNC_RET: // 17
    return ret_st(instruction);

  case AST_FUNC_CALL: // 18
    err = call_func(instruction);
    CHECK_ERR_RET(err);
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown instruction given");
    return IFJ_EINTERNAL;
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR generate_body(ast_node_t *body) {
  CHECK_ARG_NULL_RET(body);
  ast_node_t *instruction;
  IFJ_ERR err;

  if (body->type != AST_BLOCK) {
    err = generate_instruction(body);
    return err;
  }

  // definování lokálních proměnných
  printf("\t# Definition of local variables\n");
  err = define_local_vars(body);
  CHECK_ERR_RET(err);

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

  IFJ_ERR err;

  err = local_ids_ctor(&local_ids);
  CHECK_ERR_RET(err);

  printf("LABEL %s\n", function->as.func.id); // vytvoří návěští funkce
  // vytvoří lokální kopie parametrů
  if (function->as.func.param_lists != NULL) {
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
  }

  err = generate_body(function->as.func.body);
  CHECK_ERR_GOTO(err, cleanup);

  printf("\tRETURN\n\n");

  local_ids_dtor(&local_ids);

  return IFJ_SUCCESS;

cleanup:
  local_ids_dtor(&local_ids);
  return IFJ_EINTERNAL;
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
  // ast_print(root);
  IFJ_ERR err;

  if (root->type != AST_ROOT) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                      "Abstract syntax tree doesn't have a root");
  }

  err = start_code(root);
  CHECK_ERR_RET(err);

  err = generate_funcitons(root);
  CHECK_ERR_RET(err);

  return IFJ_SUCCESS;
}
