/**
 * @file semantic_analy.c
 * @brief IFJ26 - Implementace sémantické analýzy.
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#define _POSIX_C_SOURCE 200809L // Kvuli strdup

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "scanner.h"
#include "semantic_analy.h"
#include "symtable.h"

typedef struct {
  symtable_t *global_symtable; // Globální tabulka funkcí
  stack_t local_scopes;        // Zásobník lokálních symtable_t* pro bloky
  int current_scope_level; // Aktuální úroveň zanoření bloků
  func_data_t *current_func;   // Právě analyzovaná funkce
  bool in_untyped_recursion; // Pomocný flag pro detekci nepřímé rekurze
} sem_ctx_t;

static IFJ_ERR check_funciton_body(sem_ctx_t *ctx, ast_node_t *block_node,
                                   ifj26_type_t *result_type);
static IFJ_ERR check_expr(sem_ctx_t *ctx, ast_node_t *expr_node,
                          ifj26_type_t *result_type);
static IFJ_ERR check_function(symtable_t *functions_table,
                              symtable_data_t *func_data);
static IFJ_ERR check_block(sem_ctx_t *ctx, ast_node_t *block_node,
                           ifj26_type_t *result_type, bool is_function_body);

/**
 * @brief Pomocná funkce pro kontrolu seznamů parametrů.
 * @param param_list_count Počet seznamů parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @return IFJ_EINTERNAL při chybě, jinak IFJ_SUCCESS.
 */
static IFJ_ERR check_parameter_lists(int param_list_count,
                                     param_list_t *const *param_lists) {
  if (param_list_count > 0 && param_lists == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid parameter lists: param_list_count > 0 and "
                           "param_lists is NULL");
    return IFJ_EINTERNAL;
  }

  for (int i = 0; i < param_list_count; i++) {
    if (param_lists[i] == NULL) {
      LOG_ERR(IFJ_EINTERNAL, "Invalid parameter list %d: NULL pointer", i);
      return IFJ_EINTERNAL;
    }

    if (param_lists[i]->count > 0 && param_lists[i]->types == NULL) {
      LOG_ERR(IFJ_EINTERNAL,
              "Invalid parameter types in parameter list %d: "
              "count > 0 and types is NULL",
              i);
      return IFJ_EINTERNAL;
    }
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro uvolnění pole ukazatelů na seznamy parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @param param_list_count Počet seznamů parametrů.
 */
static void free_param_lists(param_list_t **param_lists, int param_list_count) {
  if (param_lists == NULL) {
    return;
  }

  for (int i = 0; i < param_list_count; i++) {
    if (param_lists[i]) {
      free(param_lists[i]->types);
      free(param_lists[i]);
    }
  }

  free(param_lists);
}

/**
 * @brief Pomocná funkce pro uvolnění dat tabulky symbolů.
 * @param data Ukazatel na data.
 * @return IFJ_SUCCESS při úspěšném uvolnění dat.
 */
static IFJ_ERR free_symtable_data(symtable_data_t *data) {
  if (data == NULL) {
    return IFJ_SUCCESS;
  }

  if (data->is_function) {
    free_param_lists(data->func.param_list, data->func.param_list_count);
    data->func.param_list = NULL;
  } else {
    free(data->var.code_gen_name);
    data->var.code_gen_name = NULL;
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro vytvoření nového seznamu parametrů.
 *
 * Alokovaný seznam parametrů je v případě úspěchu uložen do @p param_list a je
 * nutné jej uvolnit pomocí free_param_lists(). V případě chyby zůstává
 * @p param_list nastaven na NULL.
 *
 * @param param_count Počet parametrů.
 * @param param_types Pole typů parametrů.
 * @param param_list Ukazatel na nový seznam parametrů.
 * @return IFJ_EINTERNAL při chybě alokace paměti nebo poslání NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném vytvoření seznamu parametrů.
 */
static IFJ_ERR create_param_list(int param_count,
                                 const ifj26_type_t *param_types,
                                 param_list_t **param_list) {
  CHECK_ARGS_NULL_RET(param_list);
  *param_list = NULL;

  if (param_count < 0) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid parameter count: %d", param_count);
    return IFJ_EINTERNAL;
  }

  if (param_count > 0 && param_types == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid parameter types: param_count > 0 and "
                           "param_types is NULL");
    return IFJ_EINTERNAL;
  }

  param_list_t *list = calloc(1, sizeof *list);
  CHECK_MEM_ERR(list);

  if (param_count > 0) {
    list->types = malloc(sizeof(ifj26_type_t) * (size_t)param_count);
    if (list->types == NULL) {
      free(list);
      LOG_ERR(IFJ_EINTERNAL, "Memory allocation failed");
      return IFJ_EINTERNAL;
    }

    memcpy(list->types, param_types,
           sizeof(ifj26_type_t) * (size_t)param_count);
  }

  list->count = param_count;
  *param_list = list;

  return IFJ_SUCCESS;
}

/**
 * @brief Druh vytvářené funkce.
 */
typedef enum {
  FUNC_BUILT_IN, /**< Vestavěná funkce */
  FUNC_USER_DEFINED, /**< Uživatelem definovaná funkce */
} func_kind_t;

/**
 * @brief Pomocná funkce pro vytvoření nové funkce a její hlubokou kopii
 *        seznamů parametrů.
 *
 * Alokovaná data funkce jsou v případě úspěchu uložena do @p func_data a je
 * nutné je uvolnit pomocí free_symtable_data(). V případě chyby zůstává
 * obsah @p func_data nezměněn.
 *
 * Flagy a AST uzel se nastavují podle druhu funkce:
 * - FUNC_BUILT_IN: všechny flagy true, node je NULL.
 * - FUNC_USER_DEFINED: return_type_explicit podle návratového typu,
 *   ostatní flagy false, node ukazuje na AST uzel funkce.
 *
 * @param name Název funkce.
 * @param return_type Návratový typ funkce.
 * @param kind Druh funkce (vestavěná nebo uživatelem definovaná).
 * @param param_list_count Počet seznamů parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @param node AST uzel funkce (použit pouze u FUNC_USER_DEFINED).
 * @param func_data Ukazatel na novou funkci.
 * @return IFJ_EINTERNAL při chybě alokace paměti nebo poslání NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném vytvoření funkce.
 */
static IFJ_ERR create_function(const char *name, const ifj26_type_t return_type,
                               const func_kind_t kind,
                               const int param_list_count,
                               param_list_t *const *param_lists,
                               ast_node_t *node, symtable_data_t *func_data) {
  CHECK_ARGS_NULL_RET(name, func_data);

  symtable_data_t data = {0};

  CHECK_ERR_RET(check_parameter_lists(param_list_count, param_lists));

  data.is_function = true;
  data.func.return_type = return_type;
  data.func.param_list_count = param_list_count;
  data.func.param_list = NULL;

  // Flagy a AST uzel se nastavují podle druhu funkce
  switch (kind) {
  case FUNC_BUILT_IN:
    // Vestavěné funkce jsou plně vyřešené a nemají tělo v AST
    data.func.return_type_explicit = true;
    data.func.is_resolving = true;
    data.func.is_checked = true;
    data.func.has_return_stmt = true;
    data.func.node = NULL;
    break;

  case FUNC_USER_DEFINED:
    // Uživatelem definované funkce se teprve budou analyzovat
    data.func.return_type_explicit = return_type.type != TYPE_UNRESOLVED;
    data.func.is_resolving = false;
    data.func.is_checked = false;
    data.func.has_return_stmt = false;
    data.func.node = node;
    break;

  default:
    LOG_ERR(IFJ_EINTERNAL, "Invalid function kind: %d", (int)kind);
    return IFJ_EINTERNAL;
  }

  if (param_list_count > 0) {
    // Alokace paměti pro pole ukazatelů na seznamy parametrů
    data.func.param_list =
        calloc((size_t)param_list_count, sizeof(param_list_t *));
    CHECK_MEM_ERR(data.func.param_list);

    // Hluboká kopie jednotlivých seznamů parametrů
    for (int i = 0; i < param_list_count; i++) {
      IFJ_ERR err =
          create_param_list(param_lists[i]->count, param_lists[i]->types,
                            &data.func.param_list[i]);
      if (CHECK_ERR(err)) {
        free_param_lists(data.func.param_list, param_list_count);
        data.func.param_list = NULL;
        return err;
      }
    }
  }

  *func_data = data;
  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro vložení vestavěné funkce do tabulky symbolů.
 * @param table Ukazatel na tabulku symbolů.
 * @param name Název funkce.
 * @param return_type Návratový typ funkce.
 * @param param_list_count Počet seznamů parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @return IFJ_EINTERNAL při poslání NULL pointeru nebo chybě alokace paměti.
 * @return IFJ_SUCCESS při úspěšném vložení.
 */
static IFJ_ERR insert_built_in_function(symtable_t *table, const char *name,
                                        ifj26_type_t return_type,
                                        int param_list_count,
                                        param_list_t *const *param_lists) {
  symtable_data_t func_data = {0};

  IFJ_ERR err =
      create_function(name, return_type, FUNC_BUILT_IN, param_list_count,
                      param_lists, NULL, &func_data);
  CHECK_ERR_LOG_RET(err, "Failed to create built-in function '%s'.", name);

  err = symtable_insert(table, name, &func_data);
  if (CHECK_ERR(err)) {
    LOG_ERR(err, "Failed to insert built-in function '%s' into symbol table.",
            name);
    free_symtable_data(&func_data);
    return err;
  }

  free_symtable_data(&func_data);
  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro naplnění tabulky symbolů vestavěnými funkcemi.
 * @param table Ukazatel na tabulku symbolů.
 * @return IFJ_EINTERNAL při poslání NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném naplnění.
 */
static IFJ_ERR symtable_fill_built_in_functions(symtable_t *table) {
  CHECK_ARGS_NULL_RET(table);
  IFJ_ERR err = IFJ_SUCCESS;

  // Pomocné pole ukazatelů na seznamy parametrů (max. 3 parametry)
  param_list_t *lists[3] = {NULL, NULL, NULL};

  // print(value: any): Unit
  lists[0] = &(param_list_t){
      .count = 1,
      .types = (ifj26_type_t[]){{.type = TYPE_ANY, .is_list = false}}};
  err = insert_built_in_function(
      table, "print", (ifj26_type_t){.type = TYPE_UNIT, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // readLine(): String
  lists[0] = &(param_list_t){.count = 0, .types = NULL};
  err = insert_built_in_function(
      table, "readLine", (ifj26_type_t){.type = TYPE_STRING, .is_list = false},
      1, lists);
  CHECK_ERR_RET(err);

  // readInt(): Int
  lists[0] = &(param_list_t){.count = 0, .types = NULL};
  err = insert_built_in_function(
      table, "readInt", (ifj26_type_t){.type = TYPE_INT, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // readDouble(): Double
  lists[0] = &(param_list_t){.count = 0, .types = NULL};
  err = insert_built_in_function(
      table, "readDouble",
      (ifj26_type_t){.type = TYPE_DOUBLE, .is_list = false}, 1, lists);
  CHECK_ERR_RET(err);

  // toStr(value: any): String
  lists[0] = &(param_list_t){
      .count = 1,
      .types = (ifj26_type_t[]){{.type = TYPE_ANY, .is_list = false}}};
  err = insert_built_in_function(
      table, "toStr", (ifj26_type_t){.type = TYPE_STRING, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // toInt(value: String): Int
  lists[0] = &(param_list_t){
      .count = 1,
      .types = (ifj26_type_t[]){{.type = TYPE_STRING, .is_list = false}}};
  err = insert_built_in_function(
      table, "toInt", (ifj26_type_t){.type = TYPE_INT, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // strLength(value: String): Int
  lists[0] = &(param_list_t){
      .count = 1,
      .types = (ifj26_type_t[]){{.type = TYPE_STRING, .is_list = false}}};
  err = insert_built_in_function(
      table, "strLength", (ifj26_type_t){.type = TYPE_INT, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // subStr(value: String, start: Int, end: Int): String
  lists[0] = &(param_list_t){
      .count = 3,
      .types = (ifj26_type_t[]){{.type = TYPE_STRING, .is_list = false},
                                {.type = TYPE_INT, .is_list = false},
                                {.type = TYPE_INT, .is_list = false}}};
  err = insert_built_in_function(
      table, "subStr", (ifj26_type_t){.type = TYPE_STRING, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // ord(value: String, index: Int): Int
  lists[0] = &(param_list_t){
      .count = 2,
      .types = (ifj26_type_t[]){{.type = TYPE_STRING, .is_list = false},
                                {.type = TYPE_INT, .is_list = false}}};
  err = insert_built_in_function(
      table, "ord", (ifj26_type_t){.type = TYPE_INT, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  // chr(code: Int): String
  lists[0] = &(param_list_t){
      .count = 1,
      .types = (ifj26_type_t[]){{.type = TYPE_INT, .is_list = false}}};
  err = insert_built_in_function(
      table, "chr", (ifj26_type_t){.type = TYPE_STRING, .is_list = false}, 1,
      lists);
  CHECK_ERR_RET(err);

  return err;
}

/**
 * @brief Pomocná funkce pro vytvoření seznamu parametrů z AST uzlu.
 * @param param_list_node AST uzel seznamu parametrů (AST_PARAM_LIST).
 * @param param_list Ukazatel na nový seznam parametrů.
 * @return IFJ_EINTERNAL při chybě alokace paměti nebo neplatném AST uzlu.
 * @return IFJ_SUCCESS při úspěšném vytvoření seznamu parametrů.
 */
static IFJ_ERR create_param_list_from_ast(const ast_node_t *param_list_node,
                                          param_list_t **param_list) {
  CHECK_ARGS_NULL_RET(param_list_node, param_list);

  if (param_list_node->type != AST_PARAM_LIST) {
    LOG_ERR(IFJ_EINTERNAL,
            "Invalid AST node type: %d (expected AST_PARAM_LIST)",
            param_list_node->type);
    return IFJ_EINTERNAL;
  }

  size_t params_cnt = param_list_node->as.param_list.params_cnt;
  ifj26_type_t *types = NULL;

  if (params_cnt > 0) {
    types = malloc(sizeof(ifj26_type_t) * params_cnt);
    CHECK_MEM_ERR(types);

    for (size_t i = 0; i < params_cnt; i++) {
      const ast_node_t *param = param_list_node->as.param_list.params[i];
      if (param == NULL || param->type != AST_PARAM) {
        free(types);
        LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type for parameter: %d",
                param ? (int)param->type : -1);
        return IFJ_EINTERNAL;
      }

      types[i] = param->as.param.param_type;
    }
  }

  IFJ_ERR err = create_param_list((int)params_cnt, types, param_list);
  free(types);

  return err;
}

/**
 * @brief Pomocná funkce pro kontrolu duplicitních názvů parametrů funkce.
 *
 * Kontroluje, zda mají všechny parametry funkce napříč všemi seznamy
 * parametrů unikátní názvy.
 *
 * @param func_node AST uzel funkce (AST_FUNC).
 * @return IFJ_ESEM_REDEF při duplicitním názvu parametru.
 * @return IFJ_EINTERNAL při poslání NULL pointeru nebo neplatném AST uzlu.
 * @return IFJ_SUCCESS při úspěšné kontrole.
 */
static IFJ_ERR check_param_names_unique(const ast_node_t *func_node) {
  CHECK_ARGS_NULL_RET(func_node);

  if (func_node->type != AST_FUNC) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type: %d (expected AST_FUNC)",
            func_node->type);
    return IFJ_EINTERNAL;
  }

  size_t lists_cnt = func_node->as.func.param_lists_cnt;

  for (size_t i = 0; i < lists_cnt; i++) {
    const ast_node_t *list_i = func_node->as.func.param_lists[i];

    if (list_i == NULL || list_i->type != AST_PARAM_LIST) {
      LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type for parameter list: %d",
              list_i ? (int)list_i->type : -1);
      return IFJ_EINTERNAL;
    }

    for (size_t j = 0; j < list_i->as.param_list.params_cnt; j++) {
      const ast_node_t *param_i = list_i->as.param_list.params[j];

      if (param_i == NULL || param_i->type != AST_PARAM ||
          param_i->as.param.id == NULL) {
        LOG_ERR(IFJ_EINTERNAL, "Invalid AST node for parameter");
        return IFJ_EINTERNAL;
      }

      // Porovnání se všemi předchozími parametry napříč seznamy
      for (size_t k = 0; k <= i; k++) {
        const ast_node_t *list_k = func_node->as.func.param_lists[k];
        size_t params_cnt_k = (k == i) ? j : list_k->as.param_list.params_cnt;

        for (size_t l = 0; l < params_cnt_k; l++) {
          const ast_node_t *param_k = list_k->as.param_list.params[l];

          if (strcmp(param_i->as.param.id, param_k->as.param.id) == 0) {
            LOG_ERR(IFJ_ESEM_REDEF,
                    "Duplicate parameter name '%s' in function '%s'.",
                    param_i->as.param.id, func_node->as.func.id);
            return IFJ_ESEM_REDEF;
          }
        }
      }
    }
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro naplnění tabulky symbolů uživatelskými funkcemi.
 * @param table Ukazatel na tabulku symbolů.
 * @param root Ukazatel na kořen AST stromu.
 * @return IFJ_EINTERNAL při poslání NULL pointeru nebo chybě alokace paměti.
 * @return IFJ_ESEM_REDEF při redefinici funkce.
 * @return IFJ_SUCCESS při úspěšném naplnění.
 */
static IFJ_ERR symtable_fill_user_defined_functions(symtable_t *table,
                                                    const ast_node_t *root) {
  CHECK_ARGS_NULL_RET(table, root);
  IFJ_ERR err = IFJ_SUCCESS;
  bool has_main_function = false;

  if (root->type != AST_ROOT) {
    // Pokud kořenový uzel AST není typu AST_ROOT, nevracíme chybu. Logujeme
    // upozornění a vracíme IFJ_SUCCESS, aby se program mohl dál zpracovávat.
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST root node type: %d", root->type);
    return IFJ_SUCCESS;
  }

  for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
    ast_node_t *func_node = root->as.root.funcs[i];

    if (func_node == NULL || func_node->type != AST_FUNC) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Invalid AST node type for function: %d",
                        func_node ? (int)func_node->type : -1);
    }

    if (func_node->as.func.main) {
      // Kontrola, zda již byla definována hlavní funkce
      if (has_main_function) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_REDEF,
                          "Multiple @main functions defined in user-defined "
                          "functions.");
      }

      // Nastavení flagu, že byla nalezena hlavní funkce
      has_main_function = true;

      if (func_node->as.func.return_type.type != TYPE_UNIT ||
          func_node->as.func.return_type.is_list) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                          "Main function must have return type Unit.");
      }

      if (func_node->as.func.param_lists_cnt != 1 ||
          func_node->as.func.param_lists[0]->as.param_list.params_cnt != 0) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_ARGS,
                          "Main function must have no parameters.");
      }
    }

    // Kontrola duplicitních názvů parametrů napříč všemi seznamy parametrů
    err = check_param_names_unique(func_node);
    CHECK_ERR_RET(err);

    // Převod seznamů parametrů z AST na pole ukazatelů na seznamy parametrů
    size_t lists_cnt = func_node->as.func.param_lists_cnt;
    param_list_t **lists = NULL;

    if (lists_cnt > 0) {
      lists = calloc(lists_cnt, sizeof(param_list_t *));
      CHECK_MEM_ERR(lists);

      for (size_t j = 0; j < lists_cnt; j++) {
        err = create_param_list_from_ast(func_node->as.func.param_lists[j],
                                         &lists[j]);
        if (CHECK_ERR(err)) {
          free_param_lists(lists, (int)lists_cnt);
          CHECK_ERR_LOG_RET(err,
                            "Failed to create parameter list for function "
                            "'%s'.",
                            func_node->as.func.id);
        }
      }
    }

    symtable_data_t func_data = {0};
    err = create_function(func_node->as.func.id, func_node->as.func.return_type,
                          FUNC_USER_DEFINED, (int)lists_cnt, lists, func_node,
                          &func_data);
    if (CHECK_ERR(err)) {
      free_param_lists(lists, (int)lists_cnt);
      CHECK_ERR_LOG_RET(err, "Failed to create user-defined function '%s'.",
                        func_node->as.func.id);
    }

    // Dočasné seznamy parametrů nejsou potřeba, create_function vytvořila
    // hlubokou kopii
    free_param_lists(lists, (int)lists_cnt);
    lists = NULL;

    err = symtable_insert(table, func_node->as.func.id, &func_data);
    if (CHECK_ERR(err)) {
      LOG_ERR(err,
              "Failed to insert user-defined function '%s' into symbol "
              "table.",
              func_node->as.func.id);
      free_symtable_data(&func_data);
      return err;
    }

    free_symtable_data(&func_data);
  }

  if (!has_main_function) {
    CHECK_ERR_LOG_RET(IFJ_ESEM_UNDEF,
                      "Missing 'main' function in user-defined functions.");
  }

  return err;
}

IFJ_ERR symtable_fill_funcitons(symtable_t *table, const ast_node_t *root) {
  CHECK_ARGS_NULL_RET(table, root);
  IFJ_ERR err;

  err = symtable_fill_built_in_functions(table);
  CHECK_ERR_LOG_RET(err, "Failed to fill symbol table with built-in functions");

  err = symtable_fill_user_defined_functions(table, root);
  CHECK_ERR_LOG_RET(err,
                    "Failed to fill symbol table with user-defined functions");

  return IFJ_SUCCESS;
}

static IFJ_ERR destroy_local_symtable_stack(void *data) {
  symtable_destroy((symtable_t *)data);
  free(data);
  return IFJ_SUCCESS;
}

static bool types_compatible(const ifj26_type_t *expected,
                             const ifj26_type_t *actual) {
  if (expected == NULL || actual == NULL) {
    return false;
  }

  // Přímá shoda typů a seznamů
  if (expected->type == actual->type && expected->is_list == actual->is_list) {
    return true;
  }

  if (expected->type == TYPE_ANY && expected->is_list == actual->is_list) {
    return true;
  }

  // Implicitní konverze z Int na Double (pokud nejsou seznamy)
  return !expected->is_list && !actual->is_list &&
         expected->type == TYPE_DOUBLE && actual->type == TYPE_INT;
}

static char *ifj26_type2str(const ifj26_type_t *type) {
  if (type == NULL) {
    return "NULL";
  }

  switch (type->type) {
  case TYPE_UNRESOLVED:
    return type->is_list ? "List<Unresolved>" : "Unresolved";
  case TYPE_UNIT:
    return type->is_list ? "List<Unit>" : "Unit";
  case TYPE_INT:
    return type->is_list ? "List<Int>" : "Int";
  case TYPE_DOUBLE:
    return type->is_list ? "List<Double>" : "Double";
  case TYPE_STRING:
    return type->is_list ? "List<String>" : "String";
  case TYPE_BOOL:
    return type->is_list ? "List<Bool>" : "Bool";
  case TYPE_ANY:
    return type->is_list ? "List<Any>" : "Any";
  default:
    return type->is_list ? "List<Unknown>" : "Unknown";
  }
}

static char *gen_code_gen_name(sem_ctx_t *ctx, const char *base_name) {
  if (ctx == NULL || base_name == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid arguments to gen_code_gen_name");
    return NULL;
  }

  size_t name_len = strlen(base_name);

  // délka původního jména + 1 (pro '$') + 3 (pro číslo) + 1 ('\0')
  // pokud někdo vyvori 999, blocku tak to stejně spadne asi uz driv
  size_t new_len = name_len + 1 + 3 + 1;

  char *code_gen_name = malloc(new_len);
  if (code_gen_name == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Memory allocation failed for code_gen_name");
    return NULL;
  }
  snprintf(code_gen_name, new_len, "%s$%d", base_name,
           ctx->current_scope_level); // Přidání úrovně zanoření pro unikátnost
  return code_gen_name;
}

static ifj26_type_t get_type_from_literal(ast_node_t *literal) {
  ifj26_type_t literal_type = {0};
  literal_type.type = literal->as.literal.type;
  literal_type.is_list = false; // List nemuže být literál (je to brano
                                // jako funkce implementační detail)
  return literal_type;
}

static void pop_local_symtable(sem_ctx_t *ctx) {
  if (ctx == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid argument to pop_local_symtable");
    return;
  }

  symtable_t *local_symtable = NULL;
  if (stack_pop(&ctx->local_scopes, (void **)&local_symtable) == IFJ_SUCCESS &&
      local_symtable != NULL) {
    symtable_destroy(local_symtable);
    free(local_symtable);
  }
  ctx->current_scope_level--;
}

static IFJ_ERR push_local_symtables(sem_ctx_t *ctx,
                                    symtable_t *local_symtables[], int count) {
  if (ctx == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid argument to push_local_symtable");
    return IFJ_EINTERNAL;
  }

  for (int i = 0; i < count; i++) {
    if (local_symtables[i] == NULL) {
      LOG_ERR(IFJ_EINTERNAL, "Invalid local_symtable at index %d", i);
      return IFJ_EINTERNAL;
    }
    IFJ_ERR err = stack_push(&ctx->local_scopes, local_symtables[i]);
    if (CHECK_ERR(err)) {
      LOG_ERR(err, "Failed to push local_symtable at index %d onto stack", i);
      return err;
    }
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR search_variable_in_stack(sem_ctx_t *ctx, const char *var_name,
                                        symtable_data_t *data) {
  CHECK_ARGS_NULL_RET(ctx, var_name, data);
  symtable_t **remove_from_stack_symt_arr = NULL;
  IFJ_ERR err = IFJ_SUCCESS;
  bool is_empty = false;
  int max_stack_depth = ctx->current_scope_level; // Maximální hloubka zásobníku
                                                  // pro lokální tabulky symbolů
  int current_stack_depth = 0;

  err = stack_is_empty(&ctx->local_scopes, &is_empty);
  CHECK_ERR_LOG_RET(err, "Failed to check if local scopes stack is empty.");
  if (is_empty) {
    free(remove_from_stack_symt_arr);
    return IFJ_EINTERNAL; // Stack is empty, variable not found
  }

  // Alokace paměti pro pole ukazatelů na symtable_t* (základně 8 úrovní
  // zásobníku)
  remove_from_stack_symt_arr =
      malloc(sizeof *remove_from_stack_symt_arr * max_stack_depth);
  CHECK_MEM_ERR(remove_from_stack_symt_arr);

  while (!is_empty) {
    symtable_t *current_symtable = NULL;
    symtable_data_t *current_data = NULL;

    err = stack_pop(&ctx->local_scopes, (void **)&current_symtable);
    CHECK_ERR_LOG_RET(err, "Failed to pop from local scopes stack.");

    if (current_symtable == NULL) {
      LOG_ERR(IFJ_EINTERNAL, "Popped NULL symtable from local scopes stack.");
      return IFJ_EINTERNAL;
    }

    remove_from_stack_symt_arr[current_stack_depth++] = current_symtable;
    err = symtable_find(current_symtable, var_name, &current_data);
    if (err == IFJ_SUCCESS) {
      // Found the variable, push back the popped symtables
      push_local_symtables(ctx, remove_from_stack_symt_arr,
                           current_stack_depth);
      free(remove_from_stack_symt_arr);
      *data = *current_data;
      return IFJ_SUCCESS;
    }

    err = stack_is_empty(&ctx->local_scopes, &is_empty);
    CHECK_ERR_LOG_RET(err, "Failed to check if local scopes stack is empty.");
  }

  push_local_symtables(ctx, remove_from_stack_symt_arr, current_stack_depth);
  free(remove_from_stack_symt_arr);
  return IFJ_ESEM_UNDEF; // Variable not found
}

static IFJ_ERR check_binary_operation_types(ast_node_t *expr_node,
                                            ifj26_type_t *lhs_type,
                                            ifj26_type_t *rhs_type,
                                            ifj26_type_t *result_type,
                                            char *dot_identifier) {
  CHECK_ARGS_NULL_RET(expr_node, lhs_type, rhs_type, result_type);

  if (expr_node->type != AST_EXPR || expr_node->as.expr.is_unary) {
    LOG_ERR(IFJ_EINTERNAL,
            "Invalid AST node type: %d (expected AST_EXPR binary)",
            expr_node->type);
    return IFJ_EINTERNAL;
  }

  if (lhs_type->type == TYPE_UNIT || rhs_type->type == TYPE_UNIT) {
    CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE, "Unit type in binary operation.");
  }

  if (expr_node->as.expr.operator_type == TOK_CONCAT) {
    if (!lhs_type->is_list || !rhs_type->is_list) {
      CHECK_ERR_LOG_RET(
          IFJ_ESEM_TYPE,
          "Concatenation operator requires both operands to be lists.");
    }

    if (lhs_type->type != rhs_type->type) {
      CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE, "Concatenation operator requires both "
                                       "operands to be of the same type.");
    }
  }

  if (lhs_type->is_list || rhs_type->is_list) {
    CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                      "Only the concatenation operator can be used with lists. "
                      "Other operators require non-list operands.");
  }

  switch (expr_node->as.expr.operator_type) {
  case TOK_ADD:
    if ((lhs_type->type == TYPE_INT && rhs_type->type == TYPE_DOUBLE) ||
        (lhs_type->type == TYPE_DOUBLE && rhs_type->type == TYPE_INT) ||
        (lhs_type->type == TYPE_DOUBLE && rhs_type->type == TYPE_DOUBLE)) {
      result_type->type = TYPE_DOUBLE;
    } else if (lhs_type->type == rhs_type->type) {
      result_type->type = lhs_type->type;
    } else {
      CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE, "Invalid types for addition: %s and %s.",
                        ifj26_type2str(lhs_type), ifj26_type2str(rhs_type));
    }
    break;
  case TOK_SUB:
  case TOK_MUL:
  case TOK_DIV:
    if ((lhs_type->type == TYPE_INT && rhs_type->type == TYPE_DOUBLE) ||
        (lhs_type->type == TYPE_DOUBLE && rhs_type->type == TYPE_INT) ||
        (lhs_type->type == TYPE_DOUBLE && rhs_type->type == TYPE_DOUBLE)) {
      result_type->type = TYPE_DOUBLE;
    } else if (lhs_type->type == TYPE_INT && rhs_type->type == TYPE_INT) {
      result_type->type = TYPE_INT;
    } else {
      CHECK_ERR_LOG_RET(
          IFJ_ESEM_TYPE,
          "Invalid types for multiplication/division/subtraction: %s and %s.",
          ifj26_type2str(lhs_type), ifj26_type2str(rhs_type));
    }
    break;
  case TOK_TO:
    if (lhs_type->type == TYPE_INT && rhs_type->type == TYPE_INT) {
      result_type->type = TYPE_INT;
    } else {
      CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                        "Invalid types for string slicing: %s and %s.",
                        ifj26_type2str(lhs_type), ifj26_type2str(rhs_type));
    }
    break;
  case TOK_EQ:
  case TOK_NEQ:
  case TOK_LT:
  case TOK_LEQ:
  case TOK_GT:
  case TOK_GEQ:
    if (lhs_type->type == rhs_type->type) {
      result_type->type = TYPE_BOOL;
    } else if ((lhs_type->type == TYPE_INT && rhs_type->type == TYPE_DOUBLE) ||
               (lhs_type->type == TYPE_DOUBLE && rhs_type->type == TYPE_INT)) {
      result_type->type = TYPE_BOOL;
    } else {
      CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                        "Invalid types for comparison: %s and %s.",
                        ifj26_type2str(lhs_type), ifj26_type2str(rhs_type));
    }
    break;
  case TOK_AND:
  case TOK_OR:
    if (lhs_type->type == TYPE_BOOL && rhs_type->type == TYPE_BOOL) {
      result_type->type = TYPE_BOOL;
    } else {
      CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                        "Invalid types for logical operation: %s and %s.",
                        ifj26_type2str(lhs_type), ifj26_type2str(rhs_type));
    }
    break;
  case TOK_DOT:
    if (dot_identifier == NULL) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                        "Dot operator requires a valid identifier.");
    }

    if (strcmp(dot_identifier, "length") == 0) {
      if (lhs_type->type != TYPE_STRING || lhs_type->is_list) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                          "The 'length' method can only be called on a "
                          "non-list String type.");
      }
      result_type->type = TYPE_INT;
    } else if (strcmp(dot_identifier, "toInt") == 0) {
      if (lhs_type->type != TYPE_DOUBLE || lhs_type->is_list) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                          "The 'toInt' method can only be called on a "
                          "non-list String type.");
      }
      result_type->type = TYPE_INT;
    } else if (strcmp(dot_identifier, "toString") == 0) {
      result_type->type = TYPE_STRING;
    } else if (strcmp(dot_identifier, "floor") == 0) {
      if (lhs_type->type != TYPE_DOUBLE || lhs_type->is_list) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_TYPE,
                          "The 'floor' method can only be called on a "
                          "non-list Double type.");
      }
      result_type->type = TYPE_DOUBLE;
    } else {
      CHECK_ERR_LOG_RET(IFJ_ESEM_UNDEF, "Unknown method '%s' for dot operator.",
                        dot_identifier);
    }
    break;
  default:
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Unknown binary operator type: %d",
                      expr_node->as.expr.operator_type);
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR check_if_expr_is_bool(sem_ctx_t *ctx, ast_node_t *if_expr) {
  CHECK_ARGS_NULL_RET(ctx, if_expr);

  if (if_expr->type != AST_EXPR && if_expr->type != AST_LITERAL &&
      if_expr->type != AST_FUNC_CALL && if_expr->type != AST_VAR) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid condition expression node type: %d",
            if_expr->type);
    return IFJ_EINTERNAL;
  }

  // Kontrola typu podmínky
  ifj26_type_t cond_type = {0};
  IFJ_ERR err = check_expr(ctx, if_expr, &cond_type);
  CHECK_ERR_LOG_RET(err,
                    "Failed to check condition expression in if statement.");

  // Kontrola, zda je typ podmínky Bool
  if (cond_type.type != TYPE_BOOL) {
    LOG_ERR(IFJ_ESEM_TYPE, "Condition expression must be of type Bool.");
    return IFJ_ESEM_TYPE;
  }

  return IFJ_SUCCESS;
}

static IFJ_ERR check_if_expr(sem_ctx_t *ctx, ast_node_t *if_node,
                             ifj26_type_t *result_type) {
  CHECK_ARGS_NULL_RET(ctx, if_node);
  IFJ_ERR err = IFJ_SUCCESS;

  if (if_node->type != AST_IF) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type: %d (expected AST_IF)",
            if_node->type);
    return IFJ_EINTERNAL;
  }

  if (if_node->as.if_st.body == NULL || if_node->as.if_st.else_st == NULL ||
      if_node->as.if_st.else_if_cnt != 0) {
    CHECK_ERR_LOG_RET(IFJ_ESEM_OTHER,
                      "Invalid AST node structure for if statement expr.");
  }

  // Kontrola typu podmínky
  err = check_if_expr_is_bool(ctx, if_node->as.if_st.expr);
  CHECK_ERR_LOG_RET(err, "Failed to check condition expression in if statement "
                         "or expresion is not type boolean.");

  // Kontrola bloků then a else
  ifj26_type_t types_array[2] = {
      {0}}; // Pole pro uložení typů bloků then a else

  err = check_expr(ctx, if_node->as.if_st.body, types_array);
  CHECK_ERR_LOG_RET(err, "Failed to check then block in if statement.");

  err = check_expr(ctx, if_node->as.if_st.else_st, types_array + 1);
  CHECK_ERR_LOG_RET(err, "Failed to check else block in if statement.");

  if (!types_compatible(&types_array[0], &types_array[1])) {
    CHECK_ERR_LOG_RET(
        IFJ_ESEM_TYPE,
        "Type mismatch between then and else blocks in if statement: "
        "then block type '%s', else block type '%s'.",
        ifj26_type2str(&types_array[0]), ifj26_type2str(&types_array[1]));
  }

  *result_type = types_array[0];
  return IFJ_SUCCESS;
}

static IFJ_ERR check_expr(sem_ctx_t *ctx, ast_node_t *expr_node,
                          ifj26_type_t *result_type) {
  CHECK_ARGS_NULL_RET(ctx, expr_node, result_type);

  if (expr_node->type != AST_EXPR && expr_node->type != AST_LITERAL &&
      expr_node->type != AST_FUNC_CALL && expr_node->type != AST_IF &&
      expr_node->type != AST_VAR) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type: %d (expected AST_EXPR)",
            expr_node->type);
    return IFJ_EINTERNAL;
  }

  if (expr_node->type == AST_LITERAL) {
    // Vyřešení typu literálu přímo z AST uzlu
    *result_type = get_type_from_literal(expr_node);
  } else if (expr_node->type == AST_EXPR) {
    if (expr_node->as.expr.is_unary) {
      CHECK_ERR_LOG_RET(
          check_expr(ctx, expr_node->as.expr.unary.operand, result_type),
          "Failed to check unary expression operand.");

    } else {
      ifj26_type_t lhs_type = {0};
      ifj26_type_t rhs_type = {0};

      CHECK_ERR_LOG_RET(
          check_expr(ctx, expr_node->as.expr.binary.lhs, &lhs_type),
          "Failed to check binary expression left-hand side.");
      CHECK_ERR_LOG_RET(
          check_expr(ctx, expr_node->as.expr.binary.rhs, &rhs_type),
          "Failed to check binary expression right-hand side.");

      CHECK_ERR_LOG_RET(check_binary_operation_types(
                            expr_node, &lhs_type, &rhs_type, result_type,
                            expr_node->as.expr.operator_type == TOK_DOT
                                ? expr_node->as.expr.binary.rhs->as.var.id
                                : NULL),
                        "Failed to check binary operation types.");
    }
  } else if (expr_node->type == AST_FUNC_CALL) {
    symtable_data_t *func_data = NULL;
    CHECK_ERR_LOG_RET(symtable_find(ctx->global_symtable,
                                    expr_node->as.func_call.id, &func_data),
                      "Failed to find function '%s' in global symbol table.",
                      expr_node->as.func_call.id);

    // Kontrola počtu seznamů parametrů při volání funkce
    if (expr_node->as.func_call.param_lists_cnt !=
        func_data->func.param_list_count) {
      CHECK_ERR_LOG_RET(IFJ_ESEM_ARGS,
                        "Function '%s' called with incorrect number of "
                        "paramlists. Expected %ld, got %ld.",
                        expr_node->as.func_call.id,
                        func_data->func.param_list_count,
                        expr_node->as.func_call.param_lists_cnt);
    }

    // Kontrola seznamů parametrů při volání funkce (typy a počet jednotlivých
    // seznamů)
    for (size_t i = 0; i < expr_node->as.func_call.param_lists_cnt; i++) {
      ast_node_t *param_list_node = expr_node->as.func_call.param_lists[i];
      param_list_t *expected_param_list = func_data->func.param_list[i];

      if (param_list_node->as.param_list.params_cnt !=
          expected_param_list->count) {
        CHECK_ERR_LOG_RET(IFJ_ESEM_ARGS,
                          "Function '%s' called with incorrect number of "
                          "parameters in paramlist %ld. Expected %ld, got %ld.",
                          expr_node->as.func_call.id, i,
                          expected_param_list->count,
                          param_list_node->as.param_list.params_cnt);
      }

      for (size_t j = 0; j < param_list_node->as.param_list.params_cnt; j++) {
        ifj26_type_t actual_param_type = {0};

        if (param_list_node->as.param_list.params[j] == NULL) {
          CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                            "Null parameter node in paramlist %ld for function "
                            "'%s'.",
                            i, expr_node->as.func_call.id);
        }
        CHECK_ERR_LOG_RET(
            check_expr(ctx, param_list_node->as.param_list.params[j],
                       &actual_param_type),
            "Failed to check parameter %ld in paramlist %ld for function '%s'.",
            j, i, expr_node->as.func_call.id);

        if (!types_compatible(&expected_param_list->types[j],
                              &actual_param_type)) {
          CHECK_ERR_LOG_RET(
              IFJ_ESEM_ARGS,
              "Type mismatch for parameter %ld in paramlist %ld for function "
              "'%s'. Expected type '%s', got type '%s'.",
              j, i, expr_node->as.func_call.id,
              ifj26_type2str(&expected_param_list->types[j]),
              ifj26_type2str(&actual_param_type));
        }
      }
    }

    if (func_data->func.return_type.type == TYPE_UNRESOLVED) {
      CHECK_ERR_LOG_RET(check_function(ctx->global_symtable, func_data),
                        "Failed to check function '%s'.",
                        expr_node->as.func_call.id);
    }

    *result_type = func_data->func.return_type;
  } else if (expr_node->type == AST_IF) {
    CHECK_ERR_LOG_RET(check_if_expr(ctx, expr_node, result_type),
                      "Failed to check if expression.");
  } else if (expr_node->type == AST_VAR) {
    if (expr_node->as.var.id == NULL) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Variable AST node has NULL id.");
    }

    if (strcmp(expr_node->as.var.id, "toString") == 0) {
      *result_type = (ifj26_type_t){.type = TYPE_STRING, .is_list = false};
    } else if (strcmp(expr_node->as.var.id, "length") == 0) {
      *result_type = (ifj26_type_t){.type = TYPE_INT, .is_list = false};
    } else if (strcmp(expr_node->as.var.id, "toInt") == 0) {
      *result_type = (ifj26_type_t){.type = TYPE_INT, .is_list = false};
    } else if (strcmp(expr_node->as.var.id, "floor") == 0) {
      *result_type = (ifj26_type_t){.type = TYPE_DOUBLE, .is_list = false};
    } else {
      symtable_data_t var_data = {0};
      CHECK_ERR_LOG_RET(
          search_variable_in_stack(ctx, expr_node->as.var.id, &var_data),
          "Failed to find variable '%s' in local scopes.",
          expr_node->as.var.id);

      *result_type = var_data.var.type;
      free(expr_node->as.var.id);
      expr_node->as.var.id = strdup(var_data.var.code_gen_name);
      CHECK_MEM_ERR(expr_node->as.var.id);
    }
  }
  expr_node->inferred_type =
      *result_type; // Uložení odvozeného typu do AST uzlu
  return IFJ_SUCCESS;
}

static IFJ_ERR check_if(sem_ctx_t *ctx, ast_node_t *if_node,
                        ifj26_type_t *result_type) {
  CHECK_ARGS_NULL_RET(ctx, if_node);
  IFJ_ERR err;

  if (if_node->type != AST_IF) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST node type: %d (expected AST_IF)",
            if_node->type);
    return IFJ_EINTERNAL;
  }

  if (if_node->as.if_st.body == NULL || !if_node->as.if_st.expr) {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                      "Invalid AST node structure for if statement expr.");
  }

  // Kontrola typu podmínky (if)
  err = check_if_expr_is_bool(ctx, if_node->as.if_st.expr);
  CHECK_ERR_LOG_RET(err, "Failed to check condition expression in if statement "
                         "or expresion is not type boolean.");

  // Kontrola typu podmímnky (else if)
  for (size_t i = 0; i < if_node->as.if_st.else_if_cnt; i++) {
    err = check_if_expr_is_bool(
        ctx, if_node->as.if_st.else_if[i]->as.else_if_st.expr);
    CHECK_ERR_LOG_RET(err, "Failed to check condition expression in else if "
                           "statement or expresion is not type boolean.");
  }

  CHECK_ERR_LOG_RET(
      check_block(ctx, if_node->as.if_st.body, result_type, false),
      "Failed to check then block in if statement.");

  *result_type = (ifj26_type_t){
      .type = TYPE_UNIT,
      .is_list = false}; // If vždy vrací Unit, protože je to příkaz, ne výraz

  return IFJ_SUCCESS;
}

static IFJ_ERR check_block(sem_ctx_t *ctx, ast_node_t *block_node,
                           ifj26_type_t *result_type, bool is_function_body) {
  CHECK_ARGS_NULL_RET(ctx, block_node, result_type);
  symtable_t *local_symtable = NULL;
  symtable_data_t data = {0};
  IFJ_ERR err = IFJ_SUCCESS;

  if (is_function_body) {
    // Tělo funkce sdílí tabulku symbolů s parametry funkce. Přímá definice
    // proměnné se stejným názvem jako parametr musí skončit chybou 4,
    // protože jde o stejný blok.
    err = stack_top(&ctx->local_scopes, (void **)&local_symtable);
    if (err != IFJ_SUCCESS) {
      LOG_ERR(err, "Failed to get function body symbol table from stack.");
      return err;
    }

    ctx->current_scope_level++;
  } else {
    // Vnořené bloky (např. if nebo cykly) dostanou vlastní tabulku symbolů,
    // takže mohou stínovat proměnné a parametry z nadřazených bloků.
    local_symtable = malloc(sizeof(symtable_t));
    CHECK_MEM_ERR(local_symtable);

    err = symtable_init(local_symtable);
    if (err != IFJ_SUCCESS) {
      LOG_ERR(err, "Failed to initialize local symbol table for block.");
      free(local_symtable);
      return err;
    }

    err = stack_push(&ctx->local_scopes, local_symtable);
    if (err != IFJ_SUCCESS) {
      LOG_ERR(err, "Failed to push local symbol table onto stack.");
      symtable_destroy(local_symtable);
      free(local_symtable);
      return err;
    }
    ctx->current_scope_level++;
  }

  if (block_node->type != AST_BLOCK) {
    err = IFJ_EINTERNAL;
    LOG_ERR(err, "Invalid AST node type: %d (expected AST_BLOCK)",
            block_node->type);
    goto cleanup;
  }

  for (size_t i = 0; i < block_node->as.block.commands_cnt; i++) {
    // Procházíme jednotlivé příkazy v bloku a kontrolujeme je
    ast_node_t *stmt = block_node->as.block.commands[i];
    if (stmt == NULL) {
      LOG_ERR(IFJ_EINTERNAL, "Null statement in block at index %ld", i);
      return IFJ_EINTERNAL;
    }

    if (stmt->type == AST_DEF) {
      if (stmt->as.def.rhs == NULL) {
        err = IFJ_EINTERNAL;
        LOG_ERR(err, "Null RHS in variable definition at index %ld", i);
        goto cleanup;
      }

      if (stmt->as.def.id == NULL) {
        err = IFJ_EINTERNAL;
        LOG_ERR(err, "Null identifier in variable definition at index %ld", i);
        goto cleanup;
      }

      // Kontrola typu proměnné podle výrazu na pravé straně přiřazení
      if (stmt->as.def.rhs->type == AST_LITERAL ||
          stmt->as.def.rhs->type == AST_EXPR ||
          stmt->as.def.rhs->type == AST_FUNC_CALL ||
          stmt->as.def.rhs->type == AST_IF ||
          stmt->as.def.rhs->type == AST_VAR) {
        err = check_expr(ctx, stmt->as.def.rhs, &data.var.type);
        CHECK_ERR_LOG_GOTO(
            err, cleanup,
            "Failed to check variable definition expression at index %ld", i);
      } else {
        err = IFJ_EINTERNAL;
        LOG_ERR(
            err,
            "Invalid RHS AST node type in variable definition at index %ld: "
            "%d",
            i, stmt->as.def.rhs->type);
        goto cleanup;
      }

      if (data.var.type.type == TYPE_UNRESOLVED) {
        err = IFJ_ESEM_TYPE;
        LOG_ERR(err, "Unresolved type in variable definition at index %ld", i);
        goto cleanup;
      }

      if (stmt->as.def.type.type != TYPE_UNRESOLVED) {
        // Pokud je typ explicitně určen, zkontrolujeme jeho kompatibilitu s
        // odvozeným typem
        if (!types_compatible(&stmt->as.def.type, &data.var.type)) {
          err = IFJ_ESEM_TYPE;
          CHECK_ERR_LOG_GOTO(
              err, cleanup,
              "Type mismatch in variable definition at index %ld: "
              "explicit type '%s' does not match inferred type '%s'",
              i, ifj26_type2str(&stmt->as.def.type),
              ifj26_type2str(&data.var.type));
        }
      }

      // Kontrola definice proměnné
      data.is_function = false;
      data.var.is_mutable = stmt->as.def.mut;
      data.var.code_gen_name = gen_code_gen_name(ctx, stmt->as.def.id);
      if (data.var.code_gen_name == NULL) {
        err = IFJ_EINTERNAL;
        LOG_ERR(err,
                "Failed to generate code generation name for variable '%s'",
                stmt->as.def.id);
        goto cleanup;
      }

      // Vložení proměnné do lokální tabulky symbolů
      err = symtable_insert(local_symtable, stmt->as.def.id, &data);
      if (CHECK_ERR(err)) {
        free(data.var.code_gen_name);
        LOG_ERR(err,
                "Failed to insert variable '%s' into local symbol table at "
                "index %ld",
                stmt->as.def.id, i);
        goto cleanup;
      }

      // Uložení odvozeného typu do AST uzlu příkazu a zapsaní code_gen_name
      stmt->as.def.type = data.var.type;
      free(stmt->as.def.id);
      stmt->as.def.id = strdup(data.var.code_gen_name);
      free(data.var.code_gen_name);
      CHECK_MEM_ERR_GOTO(stmt->as.def.id, cleanup);
    } else if (stmt->type == AST_ASSIGN) {
      // Kontrola přiřazení
      if (stmt->as.assign.id == NULL || stmt->as.assign.rhs == NULL) {
        err = IFJ_EINTERNAL;
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Null identifier or RHS in assignment at index %ld",
                           i);
      }

      // Vyhledání proměnné v zásobníku lokálních tabulek symbolů
      err = search_variable_in_stack(ctx, stmt->as.assign.id, &data);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to find variable '%s' in local scopes stack "
                         "for assignment at index %ld",
                         stmt->as.assign.id, i);

      if (!data.var.is_mutable) {
        err = IFJ_ESEM_IMMUT;
        CHECK_ERR_LOG_GOTO(
            err, cleanup,
            "Attempt to assign to immutable variable '%s' at index %ld",
            stmt->as.assign.id, i);
      }

      free(stmt->as.assign.id);
      stmt->as.assign.id = strdup(data.var.code_gen_name);
      CHECK_MEM_ERR_GOTO(stmt->as.assign.id, cleanup);

      err = check_expr(ctx, stmt->as.assign.rhs, result_type);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to check assignment expression for variable "
                         "'%s' at index %ld",
                         stmt->as.assign.id, i);

      // Kontrola kompatibility typů přiřazení
      if (!types_compatible(&data.var.type,
                            &stmt->as.assign.rhs->inferred_type)) {
        err = IFJ_ESEM_TYPE;
        CHECK_ERR_LOG_GOTO(
            err, cleanup,
            "Type mismatch in assignment to variable '%s' at index %ld: "
            "variable type '%s' does not match expression type '%s'",
            stmt->as.assign.id, i, ifj26_type2str(&data.var.type),
            ifj26_type2str(&stmt->as.assign.rhs->inferred_type));
      }
    } else if (stmt->type == AST_FUNC_RET) {
      ifj26_type_t return_type = {0};
      ctx->current_func->has_return_stmt = true;

      // Pokud má funkce return type, ale není explicitně určen, je to chyba
      if (!ctx->current_func->return_type_explicit) {
        err = IFJ_ESEM_TYPE;
        CHECK_ERR_LOG_GOTO(IFJ_ESEM_TYPE, cleanup,
                           "Function '%s' has a return type, but it is not "
                           "explicitly specified at index %ld",
                           ctx->current_func->node->as.func.id, i);
      }

      if (stmt->as.func_ret.rhs == NULL) {
        if (ctx->current_func->return_type.type == TYPE_UNIT) {
          // samotný return bez výrazu je v pořádku, pokud je návratový typ
          // Unit
          stmt->inferred_type.type = TYPE_UNIT;
          stmt->inferred_type.is_list = false;
          continue;
        } else {
          err = IFJ_ESEM_OTHER;
          CHECK_ERR_LOG_GOTO(err, cleanup,
                             "Return statement without expression in function "
                             "with non-Unit return type at index %ld",
                             i);
        }
      }

      if (stmt->as.func_ret.rhs->type == AST_LITERAL ||
          stmt->as.func_ret.rhs->type == AST_EXPR ||
          stmt->as.func_ret.rhs->type == AST_FUNC_CALL ||
          stmt->as.func_ret.rhs->type == AST_IF ||
          stmt->as.func_ret.rhs->type == AST_VAR) {
        err = check_expr(ctx, stmt->as.func_ret.rhs, &return_type);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to check return expression at index %ld", i);
      } else if (stmt->as.func_ret.rhs->type == AST_IF) {
        err = check_if_expr(ctx, stmt->as.func_ret.rhs, &return_type);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to check return if expression at index %ld",
                           i);
      } else {
        err = IFJ_EINTERNAL;
        CHECK_ERR_LOG_GOTO(
            err, cleanup,
            "Invalid AST node type for return expression at index %ld: %d", i,
            stmt->as.func_ret.rhs->type);
      }

      if (ctx->current_func->return_type.type == TYPE_UNIT) {
        continue; // return s výrazem je v pořádku, pokud je návratový typ
                  // Unit, vysledek se ignoruje
      }

      if (ctx->current_func->return_type.type == TYPE_UNRESOLVED) {
        *result_type = return_type;
        ctx->current_func->return_type = return_type;
      }
      if (!types_compatible(&ctx->current_func->return_type, &return_type)) {
        // Kontrola kompatibility typů návratového výrazu s deklarovaným
        // návratovým typem funkce
        err = IFJ_ESEM_TYPE;
        CHECK_ERR_LOG_GOTO(
            err, cleanup,
            "Type mismatch in return statement at index %ld: "
            "function return type '%s' does not match expression type '%s'",
            i, ifj26_type2str(&ctx->current_func->return_type),
            ifj26_type2str(&return_type));
      }
    } else if (stmt->type == AST_IF) {
      if (i == block_node->as.block.commands_cnt - 1) {
        // Poslední příkaz v bloku je if, zkontrolujeme jeho typ
        err = check_if_expr(ctx, stmt, result_type);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to check if statement at index %ld", i);
      } else {
        // Pokud if není poslední příkaz, jeho typ se nevyužívá
        err = check_if(ctx, stmt, result_type);
        CHECK_ERR_LOG_GOTO(err, cleanup,
                           "Failed to check if statement at index %ld", i);
      }
    } else if (stmt->type == AST_WHILE) {
      // Kontrola typu podmínky cyklu while
      err = check_if_expr_is_bool(ctx, stmt->as.while_st.expr);
      CHECK_ERR_LOG_GOTO(
          err, cleanup,
          "Failed to check condition expression in while loop at index %ld", i);

      err = check_block(ctx, stmt->as.while_st.body, result_type, false);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to check while loop body at index %ld", i);
      *result_type = (ifj26_type_t){.type = TYPE_UNIT, .is_list = false};
    } else if (stmt->type == AST_FOR) {
    } else if (stmt->type == AST_FUNC_CALL) {
      err = check_expr(ctx, stmt, result_type);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to check function call at index %ld", i);
    } else if (stmt->type == AST_EXPR) {
      err = check_expr(ctx, stmt, result_type);
      CHECK_ERR_LOG_GOTO(
          err, cleanup, "Failed to check expression statement at index %ld", i);
    } else if (stmt->type == AST_LITERAL) {
      err = check_expr(ctx, stmt, result_type);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to check literal statement at index %ld", i);
    } else {
      err = IFJ_EINTERNAL;
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Unsupported AST node type in block at index %ld: %d",
                         i, stmt->type);
    }

    // Zde by měla být implementována kontrola jednotlivých typů příkazů
    // Například:
    // else if (stmt->type == AST_WHILE) { ... }
    // else { ... }
  }

cleanup:
  if (is_function_body) {
    // Tabulka symbolů pro parametry a tělo funkce patří volající funkci
    // check_function, proto ji zde nesmíme uvolňovat.
    ctx->current_scope_level--;
  } else {
    // Odstranění lokální tabulky symbolů z kontextu a její uvolnění
    pop_local_symtable(ctx);
  }
  return err;
}

static IFJ_ERR check_funciton_body(sem_ctx_t *ctx, ast_node_t *func_body,
                                   ifj26_type_t *result_type) {
  CHECK_ARGS_NULL_RET(ctx, func_body, result_type);
  IFJ_ERR err = IFJ_SUCCESS;

  if (func_body->type == AST_BLOCK) {
    err = check_block(ctx, func_body, result_type, true);
    CHECK_ERR_LOG_RET(err, "Failed to check function body for function '%s'.",
                      ctx->current_func->node->as.func.id);
  } else if (func_body->type == AST_EXPR || func_body->type == AST_LITERAL ||
             func_body->type == AST_FUNC_CALL || func_body->type == AST_IF ||
             func_body->type == AST_VAR) {
    // Kontrola výrazu, který je tělem funkce
    err = check_expr(ctx, func_body, result_type);
    CHECK_ERR_LOG_RET(err,
                      "Failed to check function body expression for function "
                      "'%s'.",
                      ctx->current_func->node->as.func.id);
  } else {
    CHECK_ERR_LOG_RET(IFJ_EINTERNAL,
                      "Invalid AST node type for function body: %d",
                      func_body->type);
  }
  return err;
}

static IFJ_ERR check_function(symtable_t *functions_table,
                              symtable_data_t *func_data) {
  CHECK_ARGS_NULL_RET(functions_table, func_data);
  sem_ctx_t ctx = {0};
  symtable_t *local_symtable = NULL;
  IFJ_ERR err = IFJ_SUCCESS;
  ifj26_type_t result_type = {0};

  // Funkce již byla zkontrolována, přeskočíme ji
  if (func_data->func.is_checked) {
    return IFJ_SUCCESS;
  }

  // Detekce přímé rekurze: funkce je právě řešena a její návratový typ není
  // explicitně určen
  if (func_data->func.is_resolving &&
      func_data->func.return_type_explicit == false) {
    err = IFJ_ESEM_TYPE;
    CHECK_ERR_LOG_RET(err,
                      "Indirect recursion detected in function '%s' with "
                      "unresolved return type.",
                      func_data->func.node->as.func.id);
  }

  func_data->func.is_resolving = true;

  // Inicializace kontextu sémantické analýzy
  ctx.global_symtable = functions_table;
  ctx.current_func = &func_data->func;
  ctx.current_scope_level = 0;
  ctx.in_untyped_recursion = false;
  CHECK_ERR_LOG_RET(
      stack_init(&ctx.local_scopes),
      "Failed to initialize local scopes stack for function '%s'.",
      func_data->func.node->as.func.id);

  // Vytvoření lokální tabulky symbolů pro funkci (parametry a lokální
  // proměnné)
  local_symtable = malloc(sizeof(symtable_t));
  CHECK_MEM_ERR(local_symtable);

  err = symtable_init(local_symtable);
  CHECK_ERR_LOG_GOTO(
      err, cleanup,
      "Failed to initialize local symbol table for function '%s'.",
      func_data->func.node->as.func.id);

  // Vložení lokální tabulky symbolů do zásobníku lokálních tabulek
  CHECK_ERR_LOG_RET(
      stack_push(&ctx.local_scopes, local_symtable),
      "Failed to push local symbol table onto stack for function '%s'.",
      func_data->func.node->as.func.id);

  // Vložení parametrů funkce do lokální tabulky symbolů
  for (size_t i = 0; i < func_data->func.param_list_count; i++) {
    param_list_t *param_list = func_data->func.param_list[i];
    for (size_t j = 0; j < param_list->count; j++) {
      // Vložení parametru do lokální tabulky symbolů
      symtable_data_t param_data = {0};
      param_data.is_function = false;
      param_data.var.type = param_list->types[j];
      param_data.var.is_mutable = false; // Parametry jsou implicitně neměnné
      const char *param_id = func_data->func.node->as.func.param_lists[i]
                                 ->as.param_list.params[j]
                                 ->as.param.id;
      param_data.var.code_gen_name = gen_code_gen_name(&ctx, param_id);
      if (param_data.var.code_gen_name == NULL) {
        err = IFJ_EINTERNAL;
        goto cleanup;
      }

      err = symtable_insert(local_symtable, param_id, &param_data);
      if (err == IFJ_SUCCESS) {
        ast_node_t *param = func_data->func.node->as.func.param_lists[i]
                                ->as.param_list.params[j];
        free(param->as.param.id);
        param->as.param.id = strdup(param_data.var.code_gen_name);
        if (param->as.param.id == NULL) {
          err = IFJ_EINTERNAL;
        }
      }
      free_symtable_data(&param_data);
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Failed to insert parameter into local symbol table "
                         "for function '%s'.",
                         func_data->func.node->as.func.id);
    }
  }

  err = check_funciton_body(&ctx, func_data->func.node->as.func.body,
                            &result_type);
  CHECK_ERR_LOG_GOTO(err, cleanup,
                     "Failed to check function body for function '%s'.",
                     func_data->func.node->as.func.id);

  if (func_data->func.return_type.type == TYPE_UNRESOLVED) {
    // Pokud návratový typ není explicitně určen, odvodíme ho z těla funkce
    func_data->func.return_type = result_type;
  } else if (func_data->func.return_type.type != TYPE_UNIT) {
    // Kontrola kompatibility explicitně určeného návratového typu s odvozeným
    // typem
    if (!types_compatible(&func_data->func.return_type, &result_type)) {
      err = IFJ_ESEM_TYPE;
      CHECK_ERR_LOG_GOTO(err, cleanup,
                         "Type mismatch in function '%s': "
                         "explicit return type '%s' does not match inferred "
                         "return type '%s'.",
                         func_data->func.node->as.func.id,
                         ifj26_type2str(&func_data->func.return_type),
                         ifj26_type2str(&result_type));
    }
  }

  // Označení funkce jako zkontrolované
  func_data->func.is_checked = true;
  func_data->func.is_resolving = false;
  func_data->func.node->as.func.return_type =
      func_data->func.return_type; // Uložení odvozeného návratového typu do
                                   // AST uzlu funkce
cleanup:
  func_data->func.is_resolving = false;
  stack_deep_dtor(&ctx.local_scopes, destroy_local_symtable_stack);
  return err;
}

IFJ_ERR check_functions(const ast_node_t *root, symtable_t *functions_table) {
  CHECK_ARGS_NULL_RET(root, functions_table);
  symtable_data_t *func_data = NULL;

  if (root->type != AST_ROOT) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid AST root node type: %d", root->type);
    return IFJ_EINTERNAL;
  }

  for (size_t i = 0; i < root->as.root.funcs_cnt; i++) {
    ast_node_t *func_node = root->as.root.funcs[i];

    if (func_node == NULL || func_node->type != AST_FUNC) {
      CHECK_ERR_LOG_RET(IFJ_EINTERNAL, "Invalid AST node type for function: %d",
                        func_node ? (int)func_node->type : -1);
    }

    // Získání dat funkce z tabulky symbolů pro kontrolu
    CHECK_ERR_LOG_RET(
        symtable_find(functions_table, func_node->as.func.id, &func_data),
        "Function '%s' not found in symbol table.", func_node->as.func.id);

    if (func_data->func.is_checked) {
      // Funkce již byla zkontrolována, přeskočíme ji
      continue;
    }

    CHECK_ERR_LOG_RET(check_function(functions_table, func_data),
                      "Failed to check function '%s'.", func_node->as.func.id);
  }

  return IFJ_SUCCESS;
}
