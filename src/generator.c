 /**
 * @file generator.c
 * @brief IFJ26 - Implementace generátoru
 *
 * @author xberanm00 Marek Beran
 */

// --- knihovny ---
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- naše soubory ---
#include "generator.h"

// ==================== STRUKTURY ====================
/**
 * @brief Struktura pro uchování jmen lokálních proměnných.
 */
typedef struct {
  char **data;  /**< Pole jmen proměnných. */
  size_t size;  /**< Velikost pole jmen. */
} local_ids_t;

local_ids_t local_ids;

// ==================== POMOCNÉ FUNKCE ====================
// Inicializace funkcí pro rekurzivní volání.
static IFJ_ERR get_snippet(ast_node_t *instruction, char **snippet);
static IFJ_ERR call_func(ast_node_t *func_call);
static IFJ_ERR generate_body(ast_node_t *body);
static IFJ_ERR generate_instruction(ast_node_t *instruction);
static IFJ_ERR define_var(ast_node_t *def);
static IFJ_ERR condition(ast_node_t *if_st);
static IFJ_ERR calc_expression(ast_node_t *expr);

/**
 * @brief Konstruktor pro pole jmen lokálních proměnných.
 *
 * @param local_ids Ukazatel na pole jmen lokálních proměnných.
 * @return IFJ_SUCCESS při úspěchu, nebo chybový kód.
 */
static IFJ_ERR local_ids_ctor(local_ids_t *local_ids) {
  CHECK_ARG_NULL_RET(local_ids);

  // Pole s hlavičkou.
  local_ids->data = malloc(sizeof(char *) * 1);
  CHECK_MEM_ERR(local_ids->data);

  local_ids->size = 1;
  local_ids->data[0] = NULL;

  return IFJ_SUCCESS;
}

/**
 * @brief Destruktor pro pole jmen lokálních proměnných.
 *
 * @param local_ids Ukazatel na pole jmen lokálních proměnných.
 */
static void local_ids_dtor(local_ids_t *local_ids) { free(local_ids->data); }

/**
 * @brief Vygeneruje kód, který bude součástí každého programu.
 *
 * @param root Ukazatel na kořen abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak IFJ_EINTERNAL.
 */
static IFJ_ERR start_code(ast_node_t *root) {
  CHECK_ARG_NULL_RET(root);

  puts(".IFJcode26");

  printf("\n\t# Initialization of global variables\n");
  printf("\tDEFVAR GF@-temp\n");
  printf("\tDEFVAR GF@-tempLeft\n");
  printf("\tDEFVAR GF@-tempRight\n\n");

  // Najde main funkci a zavolá jí.
  ast_node_t *function;
  for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
    function = root->as.root.funcs[i];

    if (function->type != AST_FUNC) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Passed node is not a function");
    }

    if (function->as.func.main) {
      // Zavolání main funkce.
      printf("\t# Run main\n");
      printf("\tCREATEFRAME\n");
      printf("\tPUSHFRAME\n");
      printf("\tCALL %s\n", function->as.func.id);
      printf("\tEXIT int@0\n\n");   // Úspěšné ukončení programu.
    }
  }

  // Návěští pro chybový stav.
  printf("LABEL -error\n"
         "\tEXIT int@40\n");

  // Vestavěné funkce.
  printf("\n\t# Built-in functions\n");

  // Funkce print.
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

  // Funkce readLine.
  printf("LABEL readLine\n"
         "\tREAD GF@-temp string\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  // Funkce readInt.
  printf("LABEL readInt\n"
         "\tDEFVAR LF@value\n"
         "\tREAD LF@value int\n"
         "\tTYPE GF@-temp LF@value\n"
         "\tJUMPIFNEQ -readIntSkip GF@-temp string@nil\n"
         "\tMOVE LF@value int@0\n"
         "\nLABEL -readIntSkip\n"
         "\tPUSHS LF@value\n"
         "\tRETURN\n\n");

  // Funkce readDouble.
  printf("LABEL readDouble\n"
         "\tDEFVAR LF@value\n"
         "\tREAD LF@value float\n"
         "\tTYPE GF@-temp LF@value\n"
         "\tJUMPIFNEQ -readDoubleSkip GF@-temp string@nil\n"
         "\tMOVE LF@value float@0x0p+0\n"
         "\nLABEL -readDoubleSkip\n"
         "\tPUSHS LF@value\n"
         "\tRETURN\n\n");

  // Funkce toStr
  printf("LABEL toStr\n"
         "\tDEFVAR LF@type\n"
         "\tPOPS GF@-temp\n"
         "\tTYPE LF@type GF@-temp\n"

         // Zjištění typu parametru.
         "\tJUMPIFEQ -toStrEnd LF@type string@string\n"
         "\tJUMPIFEQ -toStrInt LF@type string@int\n"
         "\tJUMPIFEQ -toStrFloat LF@type string@float\n"
         "\tJUMPIFEQ -toStrNil LF@type string@nil\n"

         // Parametr je type Bool.
         "\tJUMPIFEQ -toStrTrue GF@-temp bool@true\n"

         // Přetypování parametru na typ String.
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

          // Vrácení z funkce.
         "\nLABEL -toStrEnd\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  // Funkce toInt.
  printf("LABEL toInt\n"
         "\tPOPS GF@-temp\n"
         "\tFLOAT2INT GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  // Funkce strLenght.
  printf("LABEL strLength\n"
         "\tPOPS GF@-temp\n"
         "\tSTRLEN GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  // Funkce subStr.
  printf("LABEL subStr\n"
          // Vytvoření lokálních kopií parametrů.
         "\t#Local copies of parameters\n"
         "\tDEFVAR LF@end$0\n"
         "\tPOPS LF@end$0\n"
         "\tDEFVAR LF@start$0\n"
         "\tPOPS LF@start$0\n"
         "\tDEFVAR LF@value$0\n"
         "\tPOPS LF@value$0\n"

         // Definice lokálních proměnných.
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

         // Podmínky funkce.
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

         // Procházení řetězce.
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

         // Vytváření podřetězce
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

         // Vrácení z funkce.
         "\tPUSHS LF@res$1\n"
         "\tRETURN\n\n");

  // Funkce ord.
  printf("LABEL ord\n"
         "\tDEFVAR LF@index\n"
         "\tPOPS LF@index\n"
         "\tDEFVAR LF@value\n"
         "\tPOPS LF@value\n"
         "\tDEFVAR LF@len\n"

         // Podmínky funkce.
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

         // Počítání hodnoty znaku.
         "\n\tSTRI2INT GF@-temp LF@value LF@index\n"
         "\tPUSHS GF@-temp\n"
         "\tJUMP -ordSkip\n"

         // Chybový stav funnkce.
         "\nLABEL -ordErr\n"
         "\tPUSHS int@-1\n"

         // Vrácení z funkce.
         "\nLABEL -ordSkip\n"
         "\tRETURN\n\n");

  // Funkce chr.
  printf("LABEL chr\n"
         "\tPOPS GF@-temp\n"
         "\tINT2CHAR GF@-temp GF@-temp\n"
         "\tPUSHS GF@-temp\n"
         "\tRETURN\n\n");

  // Funkce readBoolean
  printf("LABEL readBoolean\n"
         "\tDEFVAR LF@value\n"
         "\tREAD LF@value bool\n"
         "\tTYPE GF@-temp LF@value\n"
         "\tJUMPIFNEQ -readBooleanSkip GF@-temp string@nil\n"
         "\tMOVE LF@value bool@false\n"
         "\nLABEL -readBooleanSkip\n"
         "\tPUSHS LF@value\n"
         "\tRETURN\n\n");

  // Pomocná funkce -floorHelp
  printf("LABEL -floorHelp\n"
         "\tDEFVAR LF@number\n"
         "\tPOPS LF@number\n"
         "\tLT GF@-temp LF@number int@0\n"
         "\tJUMPIFEQ -floorHelpSkip GF@-temp bool@false\n"

         // Pokud to je potřeba, správně zaokrouhlí záporná čísla.
         "\tSUB LF@number LF@number int@1\n"
         "\nLABEL -floorHelpSkip\n"
         "\tPUSHS LF@number\n"
         "\tRETURN\n\n");

  printf("\t# User defined functions\n");
  return IFJ_SUCCESS;
}

/**
 * @brief Formátuje řetězec podle specifikace jazyka IFJcode26.
 *
 * @param value Ukazatel na Řetězec, který se má formátovat.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR format_str(char **value) {
  CHECK_ARG_NULL_RET(value);

  // Podmínka přetečení.
  size_t len = strlen(*value);
  if (len > (SIZE_MAX - 1) / 4) {
    LOG_ERR(IFJ_EINTERNAL, "String is too long");
    return IFJ_EINTERNAL;
  }

  size_t cap = len * 4 + 1;

  char *s = malloc(cap);
  CHECK_MEM_ERR(s);

  // Formátování znak po znaku.
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

/**
 * @brief Formátuje hodnotu podle specifikace jazyka IFJcode26.
 *
 * @param snippet Ukazatel na hodnotu, která se má formátovat. O jeho uvolnění
 * se stará tato funkce.
 * @param type Datový typ hodnoty.
 * @return IFJ_SUCCESS při úspěchu, jinak IFJ_EINTERNAL.
 */
static IFJ_ERR format(char **snippet, data_type_t type) {
  CHECK_ARG_NULL_RET(snippet);
  IFJ_ERR err;
  int n;
  char *s;

  if (type == TYPE_STRING) {
    err = format_str(snippet);
    CHECK_ERR_GOTO(err, cleanup);

    // Změří, kolik znaků je potřeba alokovat.
    n = snprintf(NULL, 0, "string@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);

    // Uložení formátované hodnoty.
    n = snprintf(s, (size_t)n + 1, "string@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_INT) {
    // Změří, kolik znaků je potřeba alokovat.
    n = snprintf(NULL, 0, "int@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);

    // Uložení formátované hodnoty.
    n = snprintf(s, (size_t)n + 1, "int@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_DOUBLE) {
    // Změří, kolik znaků je potřeba alokovat.
    n = snprintf(NULL, 0, "float@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);

    // Uložení formátované hodnoty.
    n = snprintf(s, (size_t)n + 1, "float@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;

  } else if (type == TYPE_BOOL) {
    // Změří, kolik znaků je potřeba alokovat.
    n = snprintf(NULL, 0, "bool@%s", *snippet);
    if (n < 0)
      goto cleanup;

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR_GOTO(s, cleanup);

    // Uložení formátované hodnoty.
    n = snprintf(s, (size_t)n + 1, "bool@%s", *snippet);

    if (n < 0)
      goto cleanup;

    free(*snippet);
    *snippet = s;
  }

  return IFJ_SUCCESS;

cleanup:
  free(*snippet);
  LOG_ERR(IFJ_EINTERNAL, "Couldn't format literal");
  return IFJ_EINTERNAL;
}

/**
 * @brief Vygeneruje operaci.
 *
 * @param expr Ukazatel na uzel abstraktního syntaktického stromu.
 * @param snippet Ukazatel, do kterého se vloží vybraná operace.
 * @return IFJ_SUCCESS při úspěchu, jinak IFJ_EINTERNAL.
 */
static IFJ_ERR get_op(ast_node_t *expr, const char **snippet) {
  CHECK_ARGS_NULL_RET(expr, snippet);

  token_type_t type = expr->as.expr.operator_type;
  ast_node_t *left = expr->as.expr.binary.lhs;
  ast_node_t *right = expr->as.expr.binary.rhs;
  IFJ_ERR err = IFJ_SUCCESS;

  switch (type) {
  case TOK_EQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
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
      // Podmínka operace.
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

    *snippet =
        "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tNOT GF@-temp GF@-temp\n";
    break;

  case TOK_LT:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
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

    // Datový typ nil se může porovnávat pouze pomocí operace EQ.
    if (left->inferred_type.type == TYPE_UNIT) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tLT GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_GT:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
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

    // Datový typ nil se může porovnávat pouze pomocí operace EQ.
    if (left->inferred_type.type == TYPE_UNIT) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tGT GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_LEQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
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

    *snippet =
        "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n\tLT "
        "GF@-temp GF@-tempLeft GF@-tempRight\n\tPOPS GF@-tempLeft\n\tOR "
        "GF@-temp GF@-temp GF@-tempLeft\n";
    break;

  case TOK_GEQ:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
      if (!((left->inferred_type.type == TYPE_INT &&
             right->inferred_type.type == TYPE_DOUBLE) ||
            (left->inferred_type.type == TYPE_DOUBLE &&
             right->inferred_type.type == TYPE_INT))) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    *snippet =
        "\tEQ GF@-temp GF@-tempLeft GF@-tempRight\n\tPUSHS GF@-temp\n\tGT "
        "GF@-temp GF@-tempLeft GF@-tempRight\n\tPOPS GF@-tempLeft\n\tOR "
        "GF@-temp GF@-temp GF@-tempLeft\n";
    break;

  case TOK_ADD:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka operace.
      if ((left->inferred_type.type >= TYPE_STRING) !=
          (right->inferred_type.type >= TYPE_STRING)) { // Logický XOR.
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    }

    // Operace na základě datového typu operandů.
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
      // Podmínka operace.
      if (!((left->inferred_type.type == TYPE_INT &&
             right->inferred_type.type == TYPE_DOUBLE) ||
            (left->inferred_type.type == TYPE_DOUBLE &&
             right->inferred_type.type == TYPE_INT))) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) { // Podmínka operace.
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tSUB GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_MUL:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka ooperace.
      if (!((left->inferred_type.type == TYPE_INT &&
             right->inferred_type.type == TYPE_DOUBLE) ||
            (left->inferred_type.type == TYPE_DOUBLE &&
             right->inferred_type.type == TYPE_INT))) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) { // Podmínka operace.
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tMUL GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_DIV:
    if (left->inferred_type.type != right->inferred_type.type) {
      // Podmínka ooperace.
      if (!((left->inferred_type.type == TYPE_INT &&
             right->inferred_type.type == TYPE_DOUBLE) ||
            (left->inferred_type.type == TYPE_DOUBLE &&
             right->inferred_type.type == TYPE_INT))) {
        err = IFJ_EINTERNAL;
        break;
      }

      if (left->inferred_type.type == TYPE_INT) {
        printf("\tINT2FLOAT GF@-tempLeft GF@-tempLeft\n");
      } else {
        printf("\tINT2FLOAT GF@-tempRight GF@-tempRight\n");
      }
    } else if ((left->inferred_type.type != TYPE_INT) &&
               (left->inferred_type.type != TYPE_DOUBLE)) { // Podmínka operace.
      err = IFJ_EINTERNAL;
    }

    // Operace na základě datového typu operandů.
    if (left->inferred_type.type == TYPE_DOUBLE) {
      *snippet = "\tDIV GF@-temp GF@-tempLeft GF@-tempRight\n";
    } else {
      *snippet = "\tIDIV GF@-temp GF@-tempLeft GF@-tempRight\n";
    }

    break;

  case TOK_AND:
    // Podmínka operace.
    if (left->inferred_type.type != right->inferred_type.type ||
        left->inferred_type.type != TYPE_BOOL) {
      err = IFJ_EINTERNAL;
    }

    *snippet = "\tAND GF@-temp GF@-tempLeft GF@-tempRight\n";
    break;

  case TOK_OR:
    // Podmínka operace.
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

/**
 * @brief Generuje rozšíření VALMEM.
 *
 * Po zavolání se výsledek nachází v GF@-temp.
 *
 * @param expr Ukazatel na uzel abstraktního syntaktcikého stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR valmem(ast_node_t *expr) {
  CHECK_ARG_NULL_RET(expr);
  IFJ_ERR err;
  char *snippet;

  // Podmínka ukončující rekurzi.
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
      printf("\tCREATEFRAME\n"
             "\tPUSHS %s\n"
             "\tPUSHFRAME\n"
             "\tCALL toInt\n"
             "\tPOPFRAME\n"
             "\tCREATEFRAME\n"
             "\tPUSHFRAME\n"
             "\tCALL -floorHelp\n"
             "\tPOPFRAME\n"
             "\tPOPS GF@-temp\n"
             "\tINT2FLOAT GF@-temp GF@-temp\n",
             snippet);
      free(snippet);
    }

    return IFJ_SUCCESS;
  } else {
    // Vypočítání levé strany.
    if (expr->as.expr.binary.lhs->as.expr.operator_type != TOK_DOT) {
      err = calc_expression(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

    } else {
      // Zřetězení VALMEM.
      err = valmem(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);
    }

    printf("\tCREATEFRAME\n");
    printf("\tPUSHS GF@-temp\n");
    printf("\tPUSHFRAME\n");

    if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toString") == 0) {
      printf("\tCALL toStr\n");
      printf("\tPOPS GF@-temp\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "toInt") == 0) {
      printf("\tCALL toInt\n");
      printf("\tPOPS GF@-temp\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "length") == 0) {
      printf("\tCALL strLength\n");
      printf("\tPOPS GF@-temp\n");
    } else if (strcmp(expr->as.expr.binary.rhs->as.var.id, "floor") == 0) {
      printf("\tCALL toInt\n"
             "\tPOPS GF@-temp\n"
             "\tINT2FLOAT GF@-temp GF@-temp\n");
    }

    printf("\tPOPFRAME\n");
    printf("\t# end of valmem chain\n\n");

    return IFJ_SUCCESS;
  }
}

/**
 * @brief Generuje výpočet výrazu.
 *
 * Po zavolání se výsledek nachází v GF@-temp.
 *
 * @param expr Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR calc_expression(ast_node_t *expr) {
  CHECK_ARG_NULL_RET(expr);
  char *snippet;
  IFJ_ERR err;

  if (expr->as.expr.is_unary) {
    if (expr->as.expr.operator_type != TOK_NOT &&
        expr->as.expr.operator_type != TOK_SUB) {
      LOG_ERR(IFJ_EINTERNAL, "Unknown unary operator: %d",
              expr->as.expr.operator_type);
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

    if (expr->as.expr.operator_type == TOK_NOT) {
      printf("\tNOT GF@-temp GF@-temp\n");
    } else if (expr->as.expr.operator_type == TOK_SUB) {
      if (expr->as.expr.unary.operand->inferred_type.type == TYPE_INT) {
        printf("\tMUL GF@-temp GF@-temp int@-1\n");
      } else if (expr->as.expr.unary.operand->inferred_type.type ==
                 TYPE_DOUBLE) {
        printf("\tMUL GF@-temp GF@-temp float@-1.0\n");
      }
    }

  } else {
    // Jediný nebinární operand.
    if (expr->as.expr.operator_type == TOK_NOT) {
      LOG_ERR(IFJ_EINTERNAL, "NOT is non-binary operator type");
      return IFJ_EINTERNAL;
    }

    // Získání levé strany.
    if (expr->as.expr.operator_type == TOK_DOT) {
      err = valmem(expr);
      CHECK_ERR_RET(err);

      return IFJ_SUCCESS;
    } else if (expr->as.expr.binary.lhs->type == AST_EXPR) {
      err = calc_expression(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempLeft GF@-temp\n");
    } else if (expr->as.expr.binary.lhs->type == AST_FUNC_CALL) {
      err = call_func(expr->as.expr.binary.lhs);
      CHECK_ERR_RET(err);
      printf("\tCREATEFRAME\n");

      printf("\tPOPS GF@-tempLeft\n");
    } else {
      err = get_snippet(expr->as.expr.binary.lhs, &snippet);
      CHECK_ERR_RET(err);

      printf("\tMOVE GF@-tempLeft %s\n", snippet);
      free(snippet);
    }

    printf("\tPUSHS GF@-tempLeft\n\n");

    // Získání pravé strany.
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

    // Spočítání výrazu.
    err = get_op(expr, (const char **)&snippet);
    CHECK_ERR_RET(err);

    printf("%s", snippet);
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Uložení parametrů funkce na datový zísobník.
 *
 * @param func_call Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR push_params(ast_node_t *func_call) {
  CHECK_ARG_NULL_RET(func_call);
  IFJ_ERR err;
  char *snippet;

  for (size_t i = 0; i < func_call->as.func_call.param_lists_cnt; i++) {
    ast_node_t *param_list = func_call->as.func_call.param_lists[i];

    for (size_t j = 0; j < param_list->as.param_list.params_cnt; j++) {
      ast_node_t *param = param_list->as.param_list.params[j];

      if (param->type == AST_VAR) {
        printf("\tPUSHS LF@%s\n", param->as.var.id);
      } else if (param->type == AST_LITERAL) {
        err = get_snippet(param, &snippet);
        CHECK_ERR_RET(err);

        printf("\tPUSHS %s\n", snippet);
        free(snippet);
      } else if (param->type == AST_EXPR) {
        err = calc_expression(param);
        CHECK_ERR_RET(err);
        printf("\tPUSHS GF@-temp\n");
      } else if (param->type == AST_FUNC_CALL) {
        err = call_func(param);
        CHECK_ERR_RET(err);
      } else {
        LOG_ERR(IFJ_EINTERNAL, "Unknown parameter type");
        return IFJ_EINTERNAL;
      }
    }
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje volání funkce.
 *
 * @param func_call Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR call_func(ast_node_t *func_call) {
  CHECK_ARG_NULL_RET(func_call);
  IFJ_ERR err;

  err = push_params(func_call);
  CHECK_ERR_RET(err);

  printf("\tCREATEFRAME\n");
  printf("\tPUSHFRAME\n");
  printf("\tCALL %s\n", func_call->as.func_call.id);
  printf("\tPOPFRAME\n");
  return IFJ_SUCCESS;
}

/**
 * @brief Generuje přiřazení hodnoty.
 *
 * @param assign Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
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

  case AST_IF:
    err = condition(assign->as.assign.rhs);
    CHECK_ERR_RET(err);
    printf("\tPOPS LF@%s\n", assign->as.assign.id);
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

/**
 * @brief Generuje else if skoky podmínky.
 *
 * @param elif Ukazatel na uzel abstraktního syntaktického stromu.
 * @param id Identifikátor větve podmínky.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
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
 * @brief Generuje podmínky.
 *
 * @param if_st Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR condition(ast_node_t *if_st) {
  CHECK_ARG_NULL_RET(if_st);
  IFJ_ERR err;
  char *snippet;
  static int last_id = 0;
  int id = ++last_id;

  printf("# Condition %d\n", id);

  // Vytvoření podmínkového skoku.
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

  // Generování else if skoků.
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    elif_ids[i] = ++last_id;
    err = else_condition_expr(if_st->as.if_st.else_if[i], elif_ids[i]);
    CHECK_ERR_RET(err);
  }

  printf("\tJUMP -%s%d\n", if_st->as.if_st.else_st == NULL ? "end" : "else",
         id);

  // Generování else if těl.
  for (size_t i = 0; i < if_st->as.if_st.else_if_cnt; i++) {
    printf("LABEL -then%d\n", elif_ids[i]);
    err = generate_body(if_st->as.if_st.else_if[i]->as.else_if_st.body);
    CHECK_ERR_RET(err);

    printf("\tJUMP -end%d\n", id);
  }

  // Pokud exituje else větev, generuje její tělo.
  if (if_st->as.if_st.else_st) {
    printf("LABEL -else%d\n", id);
    err = generate_body(if_st->as.if_st.else_st);
    CHECK_ERR_RET(err);

    printf("\tJUMP -end%d\n", id);
  }

  // Generování podmínkového těla.
  printf("\nLABEL -then%d\n", id);
  err = generate_body(if_st->as.if_st.body);
  CHECK_ERR_RET(err);

  printf("LABEL -end%d\n\n", id);

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje definici proměnné.
 *
 * Proměnné se definují na začátku funkce.
 * Přiřazení hodnoty probíhá až v průběhu funkce.
 *
 * @param def Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR define_var(ast_node_t *def) {
  CHECK_ARG_NULL_RET(def);
  char *snippet;
  IFJ_ERR err;

  // Pokud už je proměnná definovaná, přiřadí jí hodnotu.
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
      } else if (def->as.def.rhs->type == AST_IF) {
        err = condition(def->as.def.rhs);
        CHECK_ERR_RET(err);
        printf("\tPOPS LF@%s\n", def->as.def.id);
      }

      return IFJ_SUCCESS;
    }
  }

  // Pokud proměnná není definovaná, definuje jí.
  printf("\tDEFVAR LF@%s\n", def->as.def.id);

  char **p = realloc(local_ids.data, (local_ids.size + 1) * sizeof *p);
  CHECK_MEM_ERR(p);
  local_ids.data = p;

  local_ids.data[local_ids.size] = def->as.def.id;
  local_ids.size = local_ids.size + 1;

  return IFJ_SUCCESS;
}

/**
 * @brief Definuje všechny lokální proměnné.
 *
 * @param body Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
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

/**
 * @brief Generuje cylkus while.
 *
 * @param while_st Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR while_loop(ast_node_t *while_st) {
  CHECK_ARG_NULL_RET(while_st);
  IFJ_ERR err;
  char *snippet;
  static int last_id = 0;
  int id = ++last_id;
  ast_node_t *body = while_st->as.while_st.body;

  // Definice vnořených proměnných cyklu.
  printf("\n\t# Definition of loop variables\n");
  err = define_local_vars(body);
  CHECK_ERR_RET(err);

  printf("\nLABEL -whileStart%d\n", id);

  // Podmínky cyklu.
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

  // Generuje tělo cyklu.
  for (size_t i = 0; i < body->as.block.commands_cnt; i++) {
    ast_node_t *instruction = body->as.block.commands[i];

    err = generate_instruction(instruction);
    CHECK_ERR_RET(err);
  }

  // Cyklení.
  printf("\tJUMP -whileStart%d\n", id);

  printf("\nLABEL -whileEnd%d\n", id);

  return IFJ_SUCCESS;
}

/**
 * @brief Definuje řídící proměnnou for cyklu.
 *
 * @param gen Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR define_loop_var(ast_node_t *gen) {
  CHECK_ARG_NULL_RET(gen);
  IFJ_ERR err;
  char *snippet;

  // Pokud proměnná je již definovaná, přiřadí jí startovní hodnotu.
  for (size_t i = 1; i < local_ids.size; i++) {
    if (strcmp(gen->as.for_gen.id, local_ids.data[i]) == 0) {
      if (gen->as.for_gen.range != NULL) {
        if (gen->as.for_gen.expr->type == AST_LITERAL) {
          err = get_snippet(gen->as.for_gen.expr, &snippet);
          CHECK_ERR_RET(err);

          printf("\tMOVE LF@%s %s\n", gen->as.for_gen.id, snippet);
          free(snippet);
        } else if (gen->as.for_gen.expr->type == AST_VAR) {
          err = get_snippet(gen->as.for_gen.expr, &snippet);
          CHECK_ERR_RET(err);

          printf("\tMOVE LF@%s %s\n", gen->as.for_gen.id, snippet);
          free(snippet);
        } else if (gen->as.for_gen.expr->type == AST_FUNC_CALL) {
          err = call_func(gen->as.for_gen.expr);
          CHECK_ERR_RET(err);

          printf("\tPOPS LF@%s\n", gen->as.for_gen.id);
        } else if (gen->as.for_gen.expr->type == AST_EXPR) {
          err = calc_expression(gen->as.for_gen.expr);
          CHECK_ERR_RET(err);
          printf("\tMOVE LF@%s GF@-temp\n", gen->as.for_gen.id);
        } else if (gen->as.for_gen.expr->type == AST_IF) {
          err = condition(gen->as.for_gen.expr);
          CHECK_ERR_RET(err);
          printf("\tPOPS LF@%s\n", gen->as.for_gen.id);
        }
      }

      return IFJ_SUCCESS;
    }
  }

  // Pokud proměnná není definovaná, definuje jí.
  printf("\tDEFVAR LF@%s\n", gen->as.for_gen.id);

  // Přiřazení startovní hodnoty.
  switch (gen->as.for_gen.expr->type) {
  case AST_EXPR:
    err = calc_expression(gen->as.for_gen.expr);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s GF@-temp\n", gen->as.for_gen.id);
    break;

  case AST_VAR:
    err = get_snippet(gen->as.for_gen.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", gen->as.for_gen.id, snippet);
    free(snippet);
    break;

  case AST_LITERAL:
    err = get_snippet(gen->as.for_gen.expr, &snippet);
    CHECK_ERR_RET(err);

    printf("\tMOVE LF@%s %s\n", gen->as.for_gen.id, snippet);
    free(snippet);
    break;

  case AST_IF:
    err = condition(gen->as.for_gen.expr);
    CHECK_ERR_RET(err);
    printf("\tPOPS LF@%s\n", gen->as.for_gen.id);
    break;

  case AST_FUNC_CALL:
    err = call_func(gen->as.for_gen.expr);
    CHECK_ERR_RET(err);

    printf("\tPOPS LF@%s\n", gen->as.for_gen.id);
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown expression in for loop");
    return IFJ_EINTERNAL;
  }

  char **p = realloc(local_ids.data, (local_ids.size + 1) * sizeof *p);
  CHECK_MEM_ERR(p);
  local_ids.data = p;

  local_ids.data[local_ids.size] = gen->as.for_gen.id;
  local_ids.size = local_ids.size + 1;

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje rozsah řídící proměnné for cyklu.
 *
 * @param range Ukazatel na uzel abstraktního syntaktického stromu.
 * @param loop_var Název řídící proměnné.
 * @param id Identifikátor for cyklu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR for_loop_range(ast_node_t *range, char *loop_var, int id) {
  CHECK_ARG_NULL_RET(range);
  IFJ_ERR err;
  char *snippet;

  printf("\t# Condition\n");
  switch (range->as.for_range.expr->type) {
  case AST_LITERAL:
    err = get_snippet(range->as.for_range.expr, &snippet);
    CHECK_ERR_RET(err);
    // Definice hraniční proměnné.
    printf("\tDEFVAR LF@-forHelp%d\n", id);
    if (!range->as.for_range.until) {
      printf("\tADD LF@-forHelp%d %s int@1\n", id, snippet);
      printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
      printf("\tJUMPIFEQ -forDescendingBefore%d GF@-temp bool@true\n", id);
    } else {
      printf("\tMOVE LF@-forHelp%d %s\n", id, snippet);
    }
    // Podmínky cyklu.
    printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@true\n", id);
    free(snippet);
    printf("\nLABEL -for%d\n", id);
    printf("\tLT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    break;
  case AST_VAR:
    err = get_snippet(range->as.for_range.expr, &snippet);
    CHECK_ERR_RET(err);
    // Definice hraniční proměnné.
    printf("\tDEFVAR LF@-forHelp%d\n", id);
    if (!range->as.for_range.until) {
      printf("\tADD LF@-forHelp%d %s int@1\n", id, snippet);
    } else {
      printf("\tMOVE LF@-forHelp%d %s\n", id, snippet);
    }
    // Podmínky cyklu.
    printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@true\n", id);
    free(snippet);
    printf("\nLABEL -for%d\n", id);
    printf("\tLT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    break;
  case AST_EXPR:
    err = calc_expression(range->as.for_range.expr);
    CHECK_ERR_RET(err);
    // Definice hraniční proměnné.
    printf("\tDEFVAR LF@-forHelp%d\n", id);
    if (!range->as.for_range.until) {
      printf("\tADD LF@-forHelp%d GF@-temp int@1\n", id);
    } else {
      printf("\tMOVE LF@-forHelp%d GF@-temp\n", id);
    }
    // Podmínky cyklu.
    printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@true\n", id);
    printf("\nLABEL -for%d\n", id);
    printf("\tLT GF@-temp LF@%s GF@-temp\n", loop_var);
    break;
  case AST_FUNC_CALL:
    err = call_func(range->as.for_range.expr);
    CHECK_ERR_RET(err);
    printf("\tPOPS GF@-temp\n");
    // Definice hraniční proměnné.
    printf("\tDEFVAR LF@-forHelp%d\n", id);
    if (!range->as.for_range.until) {
      printf("\tADD LF@-forHelp%d GF@-temp int@1\n", id);
    } else {
      printf("\tMOVE LF@-forHelp%d GF@-temp\n", id);
    }
    // Podmínky cyklu.
    printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@true\n", id);
    printf("\nLABEL -for%d\n", id);
    printf("\tLT GF@-temp LF@%s GF@-temp\n", loop_var);
    break;
  case AST_IF:
    err = condition(range->as.for_range.expr);
    CHECK_ERR_RET(err);
    printf("\tPOPS GF@-temp\n");
    // Definice hraniční proměnné.
    printf("\tDEFVAR LF@-forHelp%d\n", id);
    if (!range->as.for_range.until) {
      printf("\tADD LF@-forHelp%d GF@-temp int@1\n", id);
    } else {
      printf("\tMOVE LF@-forHelp%d GF@-temp\n", id);
    }
    // Podmínky cyklu.
    printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
    printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@true\n", id);
    printf("\nLABEL -for%d\n", id);
    printf("\tLT GF@-temp LF@%s GF@-temp\n", loop_var);
    break;
  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown expression in for loop");
    return IFJ_EINTERNAL;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje stráž for cyklu.
 *
 * @param for_loop Ukazatel na uzel abstraktního syntaktického stromu.
 * @param id Identifikátor for cyklu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR for_loop_guards(ast_node_t *for_loop, int id) {
  CHECK_ARG_NULL_RET(for_loop);
  char *snippet;
  IFJ_ERR err;

  printf("\t# Guards\n");
  printf("\nLABEL -forGuards%d\n", id);
  for (size_t i = 0; i < for_loop->as.for_st.guards_cnt; i++) {
    ast_node_t *guard = for_loop->as.for_st.guards[i];

    switch (guard->as.for_guard.expr->type) {
    case AST_LITERAL:
      err = get_snippet(guard->as.for_guard.expr, &snippet);
      CHECK_ERR_RET(err);

      printf("\n\tJUMPIFEQ -forSkip%d %s bool@false\n", id, snippet);
      free(snippet);
      break;

    case AST_VAR:
      err = get_snippet(guard->as.for_guard.expr, &snippet);
      CHECK_ERR_RET(err);

      printf("\n\tJUMPIFEQ -forSkip%d %s bool@false\n", id, snippet);
      free(snippet);
      break;

    case AST_EXPR:
      err = calc_expression(guard->as.for_guard.expr);
      CHECK_ERR_RET(err);

      printf("\n\tJUMPIFEQ -forSkip%d GF@-temp bool@false\n", id);
      break;

    case AST_FUNC_CALL:
      err = call_func(guard->as.for_guard.expr);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-temp\n");
      printf("\n\tJUMPIFEQ -forSkip%d GF@-temp bool@false\n", id);
      break;

    case AST_IF:
      err = condition(guard->as.for_guard.expr);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-temp\n");
      printf("\n\tJUMPIFEQ -forSkip%d GF@-temp bool@false\n", id);
      break;

    default:
      LOG_ERR(IFJ_EINTERNAL, "Unknown expression in for loop");
      return IFJ_EINTERNAL;
    }
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Přičte krok ve for cyklu.
 *
 * @param range Ukazatel na uzel abstraktního syntaktického stromu.
 * @param loop_var Jmého řídící proměnné for cyklu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR for_loop_step(ast_node_t *range, char *loop_var) {
  CHECK_ARG_NULL_RET(range);
  IFJ_ERR err;
  char *snippet;

  if (range->as.for_range.by_expr) {
    switch (range->as.for_range.by_expr->type) {
    case AST_LITERAL:
      err = get_snippet(range->as.for_range.by_expr, &snippet);
      CHECK_ERR_RET(err);

      printf("\n\tADD LF@%s LF@%s %s\n", loop_var, loop_var, snippet);
      free(snippet);
      break;

    case AST_VAR:
      err = get_snippet(range->as.for_range.by_expr, &snippet);
      CHECK_ERR_RET(err);

      printf("\n\tADD LF@%s LF@%s %s\n", loop_var, loop_var, snippet);
      free(snippet);
      break;

    case AST_EXPR:
      err = calc_expression(range->as.for_range.by_expr);
      CHECK_ERR_RET(err);

      printf("\n\tADD LF@%s LF@%s GF@-temp\n", loop_var, loop_var);
      break;

    case AST_FUNC_CALL:
      err = call_func(range->as.for_range.by_expr);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-temp\n");
      printf("\n\tADD LF@%s LF@%s GF@-temp\n", loop_var, loop_var);
      break;

    case AST_IF:
      err = condition(range->as.for_range.by_expr);
      CHECK_ERR_RET(err);

      printf("\tPOPS GF@-temp\n");
      printf("\n\tADD LF@%s LF@%s GF@-temp\n", loop_var, loop_var);
      break;

    default:
      LOG_ERR(IFJ_EINTERNAL, "Unknown expression in for loop");
      return IFJ_EINTERNAL;
    }
  } else {
    printf("\n\tADD LF@%s LF@%s int@1\n", loop_var, loop_var);
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje for cyklus.
 *
 * @param for_loop Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR for_loop(ast_node_t *for_loop) {
  CHECK_ARG_NULL_RET(for_loop);
  IFJ_ERR err;
  static int last_id = 0;
  int id = ++last_id;
  ast_node_t *body = for_loop->as.for_st.body;
  ast_node_t *gen = for_loop->as.for_st.gen;
  ast_node_t *range = gen->as.for_gen.range;

  // Definování proměnných cyklu.
  printf("\n\t# Definition of loop variables\n");
  err = define_loop_var(gen);
  CHECK_ERR_RET(err);
  err = define_local_vars(body);
  CHECK_ERR_RET(err);

  printf("\tDEFVAR LF@-forHelp2-%d\n", id);

  char *loop_var = gen->as.for_gen.id;

  if (range != NULL) {
    // Rozsah řídící proměnné.
    err = for_loop_range(range, loop_var, id);
    CHECK_ERR_RET(err);
  }

  printf("\tJUMPIFEQ -forEnd%d GF@-temp bool@false\n", id);
  printf("\tJUMP -forGuards%d\n", id);

  // Podmínky cyklu v sestupném pořadí.
  printf("\nLABEL -forDescendingBefore%d\n", id);
  printf("\tSUB LF@-forHelp%d LF@-forHelp%d int@2\n", id, id);
  printf("\nLABEL -forDescending%d\n", id);
  printf("\tGT GF@-temp LF@%s LF@-forHelp%d\n", loop_var, id);
  printf("\tJUMPIFEQ -forEnd%d GF@-temp bool@false\n", id);

  // Stráž cyklu.
  err = for_loop_guards(for_loop, id);
  CHECK_ERR_RET(err);

  // Tělo cyklu.
  err = generate_body(body);
  CHECK_ERR_RET(err);

  printf("\nLABEL -forSkip%d\n", id);

  printf("\tMOVE LF@-forHelp2-%d LF@%s\n", id, loop_var);

  // Přičítání skoku.
  err = for_loop_step(range, loop_var);
  CHECK_ERR_RET(err);

  // Cyklení.
  printf("\tJUMPIFEQ -error LF@%s LF@-forHelp2-%d\n", loop_var, id);

  printf("\tGT GF@-temp LF@%s LF@-forHelp2-%d\n", loop_var, id);
  printf("\tJUMPIFEQ -forDescending%d GF@-temp bool@false\n", id);

  printf("\tJUMP -for%d\n", id);

  printf("\nLABEL -forEnd%d\n\n", id);

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje explicitní příkaz return jazyka IFJ26.
 *
 * @param func_ret Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR ret_st(ast_node_t *func_ret) {
  CHECK_ARG_NULL_RET(func_ret);
  IFJ_ERR err;
  char *snippet;

  if (func_ret->as.func_ret.rhs) {
    ast_node_type_t type = func_ret->as.func_ret.rhs->type;
    if (type == AST_FUNC_CALL) {
      err = generate_instruction(func_ret->as.func_ret.rhs);
      CHECK_ERR_RET(err);
    } else if (type == AST_EXPR) {
      err = generate_instruction(func_ret->as.func_ret.rhs);
      CHECK_ERR_RET(err);
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

/**
 * @brief Pomocná funkce pro get_snippet, nahraje hodnotu literálu.
 *
 * Při chybě uvolňuje alokovanou paměť.
 * Při úspěchu paměť uvolňuje volající.
 *
 * @param literal Ukazatel na uzel abstraktního syntaktického stromu.
 * @param snippet Ukazatel, kam se má nahrát hodnota literálu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR get_literal(ast_node_t *literal, char **snippet) {
  CHECK_ARG_NULL_RET(literal);

  char *s = NULL;
  int n;

  switch (literal->as.literal.type) {
  case TYPE_INT:
    // Kolik znaků se má alokovat.
    n = snprintf(NULL, 0, "%" PRId64, literal->as.literal.int_value);
    if (n < 0) {
      LOG_ERR(IFJ_EINTERNAL, "Couldn't get literal");
      return IFJ_EINTERNAL;
    }

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR(s);

    // Uložení hodnoty literálu.
    n = snprintf(s, (size_t)n + 1, "%" PRId64, literal->as.literal.int_value);
    if (n < 0)
      goto cleanup;
    break;

  case TYPE_DOUBLE:
    // Kolik znaků se má alokovat.
    n = snprintf(NULL, 0, "%a", literal->as.literal.double_value);
    if (n < 0) {
      LOG_ERR(IFJ_EINTERNAL, "Couldn't get literal");
      return IFJ_EINTERNAL;
    }

    s = malloc((size_t)n + 1);
    CHECK_MEM_ERR(s);

    // Uložení hodnoty literálu.
    n = snprintf(s, (size_t)n + 1, "%a", literal->as.literal.double_value);
    if (n < 0)
      goto cleanup;
    break;

  case TYPE_BOOL: {
    const char *val = literal->as.literal.bool_value ? "true" : "false";
    size_t len = strlen(val) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);

    // Uložení hodnoty literálu.
    memcpy(s, val, len);
    break;
  }
  case TYPE_STRING: {
    size_t len = strlen(literal->as.literal.str_value) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);

    // Uložení hodnoty literálu.
    memcpy(s, literal->as.literal.str_value, len);
    break;
  }

  case TYPE_UNIT:
    char *text = "nil@nil";
    size_t len = strlen(text) + 1;
    s = malloc(len);
    CHECK_MEM_ERR(s);

    // Uložení hodnoty literálu.
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
  LOG_ERR(IFJ_EINTERNAL, "Couldn't get literal");
  return IFJ_EINTERNAL;
}

/**
 * @brief Pomocná funkce pro get_snippet, nahraje lokální proměnnou.
 *
 * Při chybě uvolňuje alokovanou paměť.
 * Při úspěchu paměť uvolňuje volající.
 *
 * @param var Ukazatel na uzel abstraktního syntaktického stromu.
 * @param snippet Ukazatel, kam se má nahrát lokální proměnná.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR get_var(ast_node_t *var, char **snippet) {
  CHECK_ARG_NULL_RET(var);

  // Kolik znaků se má alokovat.
  int n = snprintf(NULL, 0, "LF@%s", var->as.var.id);
  if (n < 0) {
    LOG_ERR(IFJ_EINTERNAL, "Couldn't get variable");
    return IFJ_EINTERNAL;
  }

  char *s = malloc((size_t)n + 1);

  // Uložení lokální proměnné.
  n = snprintf(s, (size_t)n + 1, "LF@%s", var->as.var.id);

  if (n < 0)
    goto cleanup;

  *snippet = s;

  return IFJ_SUCCESS;

cleanup:
  free(s);
  LOG_ERR(IFJ_EINTERNAL, "Couldn't get variable");
  return IFJ_EINTERNAL;
}

/**
 * @brief Pomocná funkce, která předá lokální proměnnou nebo literál.
 *
 * @param instruction Ukazatel na uzel abstraktního syntaktického stromu.
 * @param snippet Ukazatel k předání textu, volající se stará o uvolnění.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR get_snippet(ast_node_t *instruction, char **snippet) {
  CHECK_ARG_NULL_RET(instruction);

  switch (instruction->type) {
  case AST_VAR:
    return get_var(instruction, snippet);

  case AST_LITERAL:
    return get_literal(instruction, snippet);

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown instruction given to get_snippet");
    return IFJ_EINTERNAL;
  }
}

/**
 * @brief Generuje kód instrukce.
 *
 * @param intruction Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR generate_instruction(ast_node_t *instruction) {
  CHECK_ARG_NULL_RET(instruction);

  IFJ_ERR err;
  char *snippet;

  switch (instruction->type) {
  case AST_EXPR:
    err = calc_expression(instruction);
    CHECK_ERR_RET(err);

    printf("\tPUSHS GF@-temp\n");
    break;

  case AST_VAR:

    err = get_snippet(instruction, &snippet);
    CHECK_ERR_RET(err);

    printf("\tPUSHS %s\n", snippet);
    free(snippet);
    break;

  case AST_LITERAL:

    err = get_snippet(instruction, &snippet);
    CHECK_ERR_RET(err);

    printf("\tPUSHS %s\n", snippet);
    free(snippet);
    break;

  case AST_DEF:
    return define_var(instruction);

  case AST_ASSIGN:
    return assign(instruction);

  case AST_IF:
    return condition(instruction);

  case AST_FOR:
    return for_loop(instruction);

  case AST_WHILE:
    return while_loop(instruction);

  case AST_FUNC_RET:
    return ret_st(instruction);

  case AST_FUNC_CALL:
    err = call_func(instruction);
    CHECK_ERR_RET(err);
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Unknown instruction given");
    return IFJ_EINTERNAL;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje blok instrukcí.
 *
 * @param body Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR generate_body(ast_node_t *body) {
  CHECK_ARG_NULL_RET(body);
  ast_node_t *instruction;
  IFJ_ERR err;

  if (body->type != AST_BLOCK) {
    err = generate_instruction(body);
    return err;
  }

  // Definice lokálních proměnných na začátku bloku.
  printf("\t# Definition of local variables\n");
  err = define_local_vars(body);
  CHECK_ERR_RET(err);

  // Generuje každou instrukci.
  for (size_t i = 0; i < body->as.block.commands_cnt; i++) {
    instruction = body->as.block.commands[i];
    err = generate_instruction(instruction);
    CHECK_ERR_RET(err);
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Generuje funkci.
 *
 * Parametry se předávají pomocí datového zásobníku.
 * Výsledná hodnota se vrací na vrcholu zásobníku.
 *
 * @param function Ukazatel na uzel abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
static IFJ_ERR generate_function(ast_node_t *function) {
  CHECK_ARG_NULL_RET(function);

  if (function->type != AST_FUNC) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Passed argument is not a function");
  }

  IFJ_ERR err;

  // Inicializuje pole jmen lokálních proměnných.
  err = local_ids_ctor(&local_ids);
  CHECK_ERR_RET(err);

  printf("LABEL %s\n", function->as.func.id);

  // Lokální kopie parametrů.
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

  // Tělo funkce.
  err = generate_body(function->as.func.body);
  CHECK_ERR_GOTO(err, cleanup);

  printf("\tRETURN\n\n");

  // Destrukce pole jmen lokálních proměnných.
  local_ids_dtor(&local_ids);

  return IFJ_SUCCESS;

cleanup:
  local_ids_dtor(&local_ids);
  return IFJ_EINTERNAL;
}

/**
 * @brief Generuje všechny funkce programu.
 *
 * @param root Ukazatel na kořen abstraktního syntaktického stromu.
 * @return IFJ_SUCCESS při úspěchu, jinak chybový kód.
 */
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
