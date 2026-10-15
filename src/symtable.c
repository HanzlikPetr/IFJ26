/**
 * @file symtable.c
 * @brief IFJ26 - Implementace tabulky symbolů a AVL stromu
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#define _POSIX_C_SOURCE 200809L // Kvuli strdup

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "symtable.h"

struct avl_node {
  char *key;              /**< Klíč uzlu */
  symtable_data_t data;   /**< Data uzlu */
  int height;             /**< Výška uzlu */
  struct avl_node *right; /**< Pravý potomek uzlu */
  struct avl_node *left;  /**< Levý potomek uzlu */
};

IFJ_ERR symtable_init(symtable_t *table) {
  // Kontrola proti NULL pointeru
  CHECK_ARGS_NULL_RET(table);

  // Nulovani stromu
  table->root = NULL;

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro vytvoření nového uzlu AVL stromu.
 * @param key Klíč uzlu.
 * @param value Hodnota uzlu.
 * @param new Ukazatel na nový uzel.
 * @return IFJ_EINTERNAL při chybě alokace paměti.
 * @return IFJ_SUCCESS při úspěšném vytvoření uzlu.
 */
static IFJ_ERR create_node(const char *key, const symtable_data_t *value,
                           avl_node_t **new) {
  avl_node_t *new_node = malloc(sizeof(avl_node_t));
  CHECK_MEM_ERR(new_node);

  // Inicializace nového uzlu
  new_node->key = strdup(key);
  if (new_node->key == NULL) {
    free(new_node);
    LOG_ERR(IFJ_EINTERNAL, "Memory allocation failed for key.");
    return IFJ_EINTERNAL;
  }

  new_node->data = *value;
  new_node->height = 1;
  new_node->right = NULL;
  new_node->left = NULL;

  // Nastavení ukazatele na nový uzel
  *new = new_node;

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funcke pro zjisteneni maxima.
 * @param a Vyska prvniho uzlu.
 * @param b Vyska druheho uzlu.
 * @return Vyska vyssiho uzlu.
 */
static int max(int a, int b) { return (a > b) ? a : b; }

/**
 * @brief Pomocná funkce pro získání výšky uzlu.
 * @param node Ukazatel na uzel.
 * @return Výška uzlu, nebo 0 pokud je uzel NULL.
 */
static int get_height(avl_node_t *node) {
  if (!node) {
    return 0;
  }
  return node->height;
}

/**
 * @brief Pomocná funkce pro pravou rotaci uzlu.
 * @param crit_node Ukazatel na kritický uzel.
 * @return Ukazatel na nový kořen podstromu po rotaci.
 */
static avl_node_t *rotate_right(avl_node_t *crit_node) {
  avl_node_t *new = crit_node->left;
  crit_node->left = new->right;
  new->right = crit_node;

  crit_node->height =
      max(get_height(crit_node->left), get_height(crit_node->right)) + 1;
  new->height = max(get_height(new->left), get_height(new->right)) + 1;

  return new;
}

/**
 * @brief Pomocná funkce pro levou rotaci uzlu.
 * @param crit_node Ukazatel na kritický uzel.
 * @return Ukazatel na nový kořen podstromu po rotaci.
 */
static avl_node_t *rotate_left(avl_node_t *crit_node) {
  avl_node_t *new = crit_node->right;
  crit_node->right = new->left;
  new->left = crit_node;

  crit_node->height =
      max(get_height(crit_node->left), get_height(crit_node->right)) + 1;
  new->height = max(get_height(new->left), get_height(new->right)) + 1;

  return new;
}

/**
 * @brief Pomocná funkce pro rekurzivní vložení uzlu do AVL stromu.
 * @param root Ukazatel na kořenový uzel stromu nebo podstromu.
 * @param key Klíč uzlu.
 * @param value Hodnota uzlu.
 * @return IFJ_EINTERNAL při chybě alokace paměti.
 * @return IFJ_ESEM_REDEF pokud klíč již existuje v tabulce.
 * @return IFJ_SUCCESS při úspěšném vložení.
 */
static IFJ_ERR avl_insert(avl_node_t **root, const char *key,
                          const symtable_data_t *value) {
  avl_node_t *new_node;
  IFJ_ERR err;
  int balance;

  // Pokud je strom prázdný, vytvoříme nový uzel a nastavíme ho jako kořen
  if (!*root) {
    err = create_node(key, value, &new_node);
    CHECK_ERR_RET(err);
    *root = new_node;
    return IFJ_SUCCESS;
  }

  // Pokud je klíč menší než klíč kořene, vložíme do levého podstromu, jinak do
  // pravého podstromu
  if (strcmp((*root)->key, key) > 0) {
    // Rekurzivní volání pro vložení do levého podstromu
    err = avl_insert(&(*root)->left, key, value);
    CHECK_ERR_RET(err);
  } else if (strcmp((*root)->key, key) < 0) {
    // Rekurzivní volání pro vložení do pravého podstromu
    err = avl_insert(&(*root)->right, key, value);
    CHECK_ERR_RET(err);
  } else {
    // Pokud klíč již existuje, vrátíme chybu
    LOG_ERR(IFJ_ESEM_REDEF, "Key already exists: %s", key);
    return IFJ_ESEM_REDEF;
  }

  // Aktualizace vyvážení uzlu
  (*root)->height =
      1 + max(get_height((*root)->left), get_height((*root)->right));

  // Získání vyvážení uzlu
  balance = get_height((*root)->right) - get_height((*root)->left);

  // Kontrola vyvážení uzlu a případná rotace pro vyvážení stromu
  if (balance > 1) {
    if (strcmp((*root)->right->key, key) < 0) {
      // Pravý-pravý případ
      *root = rotate_left(*root);
    } else {
      // Pravý-levý případ
      (*root)->right = rotate_right((*root)->right);
      *root = rotate_left(*root);
    }
  } else if (balance < -1) {
    if (strcmp((*root)->left->key, key) > 0) {
      // Levý-levý případ
      *root = rotate_right(*root);
    } else {
      // Levý-pravý případ
      (*root)->left = rotate_left((*root)->left);
      *root = rotate_right(*root);
    }
  }
  return IFJ_SUCCESS;
}

IFJ_ERR symtable_insert(symtable_t *table, const char *key,
                        const symtable_data_t *value) {
  IFJ_ERR err;

  // Kontrola proti NULL pointeru
  CHECK_ARGS_NULL_RET(table, key, value);

  // Vložení prvku do stromu
  err = avl_insert(&table->root, key, value);
  CHECK_ERR_LOG_RET(err, "Failed to insert node with key: %s", key);

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro rekurzivní vyhledání uzlu v AVL stromu.
 * @param root Ukazatel na kořenový uzel stromu nebo podstromu.
 * @param key Klíč uzlu.
 * @param value Ukazatel na hodnotu uzlu.
 * @return IFJ_ESEM_UNDEF pokud prvek neexistuje.
 * @return IFJ_SUCCESS při úspěšném vyhledání.
 */
static IFJ_ERR avl_find(avl_node_t *root, const char *key,
                        symtable_data_t **value) {
  // Pokud je strom prázdný, vrátíme chybu
  if (!root) {
    return IFJ_ESEM_UNDEF;
  }

  // Porovnání klíče s klíčem kořene a rekurzivní vyhledání v levém nebo pravém
  // podstromu
  if (strcmp(root->key, key) > 0) {
    if (root->left) {
      return avl_find(root->left, key, value);
    }
  } else if (strcmp(root->key, key) < 0) {
    if (root->right) {
      return avl_find(root->right, key, value);
    }
  } else {
    *value = &root->data;
    return IFJ_SUCCESS;
  }

  return IFJ_ESEM_UNDEF;
}

IFJ_ERR symtable_find(symtable_t *table, const char *key,
                      symtable_data_t **value) {
  IFJ_ERR err;

  // Kontrola proti NULL pointeru
  CHECK_ARGS_NULL_RET(table, key, value);

  // Pokud je strom prázdný, vrátíme chybu
  if (!table->root) {
    return IFJ_ESEM_UNDEF;
  }

  err = avl_find(table->root, key, value);
  CHECK_ERR_LOG_RET(err, "Key not found: %s", key);

  return IFJ_SUCCESS;
}

/**
 * @brief Pomocná funkce pro rekurzivní uvolnění AVL stromu.
 * @param root Ukazatel na kořenový uzel stromu.
 */
static void avl_destroy(avl_node_t *root) {
  if (root) {
    avl_destroy(root->left);
    avl_destroy(root->right);
    free(root->key);
    free(root);
  }
}

IFJ_ERR symtable_destroy(symtable_t *table) {
  // Kontrola proti NULL pointeru
  CHECK_ARGS_NULL_RET(table);

  // Uvolnění stromu
  avl_destroy(table->root);

  // Nastavení ukazatele na NULL
  table->root = NULL;

  return IFJ_SUCCESS;
}
