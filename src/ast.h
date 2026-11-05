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

/**
 * @brief Typ uzlu abstraktního syntaktického stromu.
 */
typedef enum {
  AST_ROOT,       /**< Kořen programu obsahující definice funkcí */
  AST_FUNC,       /**< Definice funkce podle pravidla <func> */
  AST_PARAM,      /**< Jeden formální parametr funkce */
  AST_PARAM_LIST, /**< Jeden seznam parametrů */
  AST_BLOCK,      /**< Odsazený blok příkazů */
  AST_EXPR,       /**< Unární nebo binární výraz */
  AST_VAR,        /**< Proměnná */
  AST_LITERAL,    /**< Literál (např. číslo, řetězec, true, false) */
  AST_DEF,        /**< Definice proměnné var nebo val */
  AST_ASSIGN,     /**< Přiřazení do proměnné */
  AST_IF,         /**< Podmíněná konstrukce if */
  AST_ELSE_IF,    /**< Větev else if */
  AST_FOR,        /**< Cyklus for */
  AST_FOR_GEN,    /**< Generátor cyklu for */
  AST_FOR_RANGE,  /**< Rozsah generátoru to, until nebo by */
  AST_FOR_GUARD,  /**< Stráž cyklu for */
  AST_WHILE,      /**< Cyklus while */
  AST_FUNC_RET,   /**< Příkaz návratu z funkce */
  AST_FUNC_CALL,  /**< Volání funkce */
} ast_node_type_t;

/**
 * @brief Uzel abstraktního syntaktického stromu.
 */
typedef struct ast_node ast_node_t;

struct ast_node {
  ast_node_type_t type; /**< Typ uzlu určující platnou část unionu as. */
  ifj26_type_t inferred_type; /**< Typ odvozený sémantickou analýzou, pokud je
                                 relevantní. */

  union {
    /**
     * @brief Data kořene programu podle pravidla <functions>.
     */
    struct {
      ast_node_t **funcs; /**< Pole definic funkcí v pořadí zdrojového kódu. */
      size_t funcs_cnt; /**< Počet funkcí v poli funcs. */
    } root;

    /**
     * @brief Data definice funkce podle pravidla <func>.
     */
    struct {
      bool main; /**< Příznak anotace @main. */
      char *id; /**< Identifikátor funkce. */
      ast_node_t **param_lists; /**< Seznamy formálních parametrů. */
      size_t param_lists_cnt; /**< Počet seznamů formálních parametrů. */
      ifj26_type_t return_type; /**< Návratový typ nebo TYPE_UNRESOLVED. */
      ast_node_t *body; /**< Tělo funkce. */
    } func;

    /**
     * @brief Data příkazu návratu z funkce.
     */
    struct {
      ast_node_t *rhs; /**< Volitelný výraz vracený funkcí. */
    } func_ret;

    /**
     * @brief Reprezentace volání funkce, metody nebo indexace seznamu (např.
     * foo(1, 2), add(x)(y)).
     */
    struct {
      char *id; /**< Název volané funkce nebo objektu. */
      ast_node_t **param_lists; /**< Pole seznamů argumentů. */
      size_t param_lists_cnt; /**< Počet seznamů argumentů. */
    } func_call;

    /**
     * @brief Data seznamu parametrů nebo argumentů.
     */
    struct {
      ast_node_t **params; /**< Prvky seznamu v pořadí zdrojového kódu. */
      size_t params_cnt; /**< Počet prvků seznamu. */
    } param_list;

    /**
     * @brief Data formálního parametru podle pravidla <param>.
     */
    struct {
      char *id; /**< Identifikátor parametru. */
      ifj26_type_t param_type; /**< Typ parametru. */
    } param;

    /**
     * @brief Data odsazeného bloku podle pravidla <block>.
     */
    struct {
      ast_node_t **commands; /**< Příkazy bloku v pořadí zdrojového kódu. */
      size_t commands_cnt; /**< Počet příkazů v bloku. */
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

    /**
     * @brief Data definice proměnné var nebo val.
     */
    struct {
      bool mut; /**< Zda jde o modifikovatelnou proměnnou var. */
      char *id; /**< Identifikátor definované proměnné. */
      ifj26_type_t type; /**< Explicitní typ nebo TYPE_UNRESOLVED. */
      ast_node_t *rhs; /**< Výraz inicializující proměnnou. */
    } def;

    /**
     * @brief Data přiřazení podle pravidla <assign_or_expr>.
     */
    struct {
      char *id; /**< Identifikátor cílové proměnné. */
      ast_node_t *rhs; /**< Výraz přiřazovaný do proměnné. */
    } assign;

    /**
     * @brief Data podmíněné konstrukce if.
     */
    struct {
      ast_node_t *expr; /**< Podmínkový výraz. */
      ast_node_t *body; /**< Tělo hlavní větve. */
      ast_node_t **else_if; /**< Pole větví else if. */
      size_t else_if_cnt; /**< Počet větví else if. */
      ast_node_t *else_st; /**< Volitelná větev else. */
    } if_st;

    /**
     * @brief Data větve else if.
     */
    struct {
      ast_node_t *expr; /**< Podmínkový výraz větve. */
      ast_node_t *body; /**< Tělo větve. */
    } else_if_st;

    /**
     * @brief Data cyklu while podle pravidla <while_st>.
     */
    struct {
      ast_node_t *expr; /**< Podmínka cyklu. */
      ast_node_t *body; /**< Tělo cyklu. */
    } while_st;

    /**
     * @brief Data cyklu for podle pravidla <for_st>.
     */
    struct {
      ast_node_t *gen; /**< Generátor cyklu. */
      ast_node_t **guards; /**< Pole stráží cyklu. */
      size_t guards_cnt; /**< Počet stráží cyklu. */
      ast_node_t *body; /**< Tělo cyklu. */
    } for_st;

    /**
     * @brief Data generátoru cyklu for podle pravidla <gen>.
     */
    struct {
      char *id; /**< Identifikátor řídicí proměnné. */
      ast_node_t *expr; /**< Počáteční nebo zdrojový výraz. */
      ast_node_t *range; /**< Volitelný rozsah generátoru. */
    } for_gen;

    /**
     * @brief Data stráže cyklu for podle pravidla <guards>.
     */
    struct {
      ast_node_t *expr; /**< Podmínkový výraz stráže. */
    } for_guard;

    /**
     * @brief Data rozsahu generátoru to, until nebo by.
     */
    struct {
      bool until; /**< Zda je horní mez výlučná. */
      ast_node_t *expr; /**< Výraz horní meze. */
      ast_node_t *by_expr; /**< Volitelný krok rozsahu. */
    } for_range;
  } as;
};

/**
 * @brief Vytvoří a inicializuje nový uzel AST.
 *
 * @param node Ukazatel, do kterého se uloží nový uzel.
 * @param type Typ vytvářeného uzlu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_create_node(ast_node_t **node, ast_node_type_t type);

/**
 * @brief Přidá funkci do kořenového uzlu programu.
 *
 * @param root Kořenový uzel typu AST_ROOT.
 * @param func Uzel funkce typu AST_FUNC.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_root_add_func(ast_node_t *root, ast_node_t *func);

/**
 * @brief Přidá příkaz na konec bloku.
 *
 * @param block Blok typu AST_BLOCK.
 * @param command Příkaz, který se má přidat.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_block_add_command(ast_node_t *block, ast_node_t *command);

/**
 * @brief Přidá seznam parametrů do definice funkce.
 *
 * @param func Funkce typu AST_FUNC.
 * @param param_list Seznam parametrů typu AST_PARAM_LIST.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_func_add_param_list(ast_node_t *func, ast_node_t *param_list);

/**
 * @brief Přidá seznam argumentů do volání funkce.
 * @param func_call AST uzel volání funkce.
 * @param param_list AST uzel seznamu argumentů.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_func_call_add_param_list(ast_node_t *func_call,
                                     ast_node_t *param_list);

/**
 * @brief Přidá formální parametr do seznamu parametrů.
 *
 * @param param_list Seznam parametrů typu AST_PARAM_LIST.
 * @param param Parametr typu AST_PARAM.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_param_list_add_param(ast_node_t *param_list, ast_node_t *param);

/**
 * @brief Přidá argument do seznamu argumentů volání funkce.
 * @param param_list AST uzel seznamu argumentů.
 * @param arg AST uzel argumentu.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_func_call_param_list_add_arg(ast_node_t *param_list,
                                         ast_node_t *arg);

/**
 * @brief Přidá větev else if do podmíněného uzlu.
 *
 * @param if_st Podmíněný uzel typu AST_IF.
 * @param else_if_st Větev typu AST_ELSE_IF.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_if_add_else_if(ast_node_t *if_st, ast_node_t *else_if_st);

/**
 * @brief Přidá stráž do cyklu for.
 *
 * @param for_st Cyklus typu AST_FOR.
 * @param guard Stráž typu AST_FOR_GUARD.
 * @return IFJ_SUCCESS při úspěchu nebo chybový kód.
 */
IFJ_ERR ast_for_add_guard(ast_node_t *for_st, ast_node_t *guard);

/**
 * @brief Rekurzivně uvolní uzel AST a všechny jeho potomky.
 *
 * @param root Kořen uvolňovaného podstromu; může být NULL.
 */
void ast_free(ast_node_t *root);

#endif
