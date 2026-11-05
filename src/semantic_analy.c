/**
 * @file semantic_analy.c
 * @brief IFJ26 - Implementace sémantické analýzy.
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "semantic_analy.h"
#include "error.h"
#include "symtable.h"

/**
 * @brief Pomocná funkce pro kontrolu seznamů parametrů.
 * @param param_list_count Počet seznamů parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @return IFJ_EINTERNAL při chybě, jinak IFJ_SUCCESS.
 */
static IFJ_ERR check_parameter_lists(int param_list_count,
                                     param_list_t *const *param_lists) {
  if (param_list_count < 0) {
    LOG_ERR(IFJ_EINTERNAL, "Invalid parameter list count: %d",
            param_list_count);
    return IFJ_EINTERNAL;
  }

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

    if (param_lists[i]->count < 0) {
      LOG_ERR(IFJ_EINTERNAL, "Invalid parameter count in parameter list %d: %d",
              i, param_lists[i]->count);
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
 * @brief Pomocná funkce pro vytvoření nové funkce a její hlubokou kopii
 *        seznamů parametrů.
 *
 * Alokovaná data funkce jsou v případě úspěchu uložena do @p func_data a je
 * nutné je uvolnit pomocí free_symtable_data(). V případě chyby zůstává
 * obsah @p func_data nezměněn.
 *
 * @param name Název funkce.
 * @param return_type Návratový typ funkce.
 * @param param_list_count Počet seznamů parametrů.
 * @param param_lists Pole ukazatelů na seznamy parametrů.
 * @param func_data Ukazatel na novou funkci.
 * @return IFJ_EINTERNAL při chybě alokace paměti nebo poslání NULL pointeru.
 * @return IFJ_SUCCESS při úspěšném vytvoření funkce.
 */
static IFJ_ERR create_function(const char *name, const ifj26_type_t return_type,
                               const int param_list_count,
                               param_list_t *const *param_lists,
                               symtable_data_t *func_data) {
  CHECK_ARGS_NULL_RET(name, func_data);

  symtable_data_t data = {0};

  CHECK_ERR_RET(check_parameter_lists(param_list_count, param_lists));

  data.is_function = true;
  data.func.return_type = return_type;
  data.func.return_type_explicit = true;
  data.func.has_return_stmt = true;
  data.func.body = NULL;
  data.func.param_list_count = param_list_count;
  data.func.param_list = NULL;

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

  IFJ_ERR err = create_function(name, return_type, param_list_count,
                                param_lists, &func_data);
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
        has_main_function = true;
    }

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
                          (int)lists_cnt, lists, &func_data);
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
    CHECK_ERR_LOG_RET(IFJ_ESEM_UNDEF, "Missing 'main' function in user-defined functions.");
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
