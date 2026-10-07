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

/**
 * @brief Struktura pro uzel AVL stromu.
 */
struct avl_node {
  char *key;              /**< Klíč uzlu */
  symtable_data_t data;   /**< Data uzlu */
  int height;             /**< Výška uzlu */
  struct avl_node *right; /**< Pravý potomek uzlu */
  struct avl_node *left;  /**< Levý potomek uzlu */
};

IFJ_ERR symtable_init(symtable_t *table) {
  // Kontrola proti NULL pointeru
  if (table == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table address cannot be null.");
    return IFJ_EINTERNAL;
  }

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
static int height(avl_node_t *node) {
  if (!node) {
    return 0;
  }

  return max(height(node->left), height(node->right)) + 1;
}

/**
 * @brief Pomocná funkce pro rekurzivní vložení uzlu do AVL stromu.
 * @param root Ukazatel na kořenový uzel stromu nebo podstromu.
 * @param key Klíč uzlu.
 * @param value Hodnota uzlu.
 * @return IFJ_EINTERNAL při chybě alokace paměti.
 * @return IFJ_SUCCESS při úspěšném vložení.
 */
static IFJ_ERR avl_insert(avl_node_t **root, const char *key,
                          const symtable_data_t *value) {
  avl_node_t *new_node;
  IFJ_ERR err;

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
    LOG_ERR(IFJ_EINTERNAL, "Key already exists: %s", key);
    return IFJ_EINTERNAL;
  }

  // Aktualizace vyvážení uzlu
  (*root)->height = height(*root);
  return IFJ_SUCCESS;
}

IFJ_ERR symtable_insert(symtable_t *table, const char *key,
                        const symtable_data_t *value) {
  IFJ_ERR err;

  // Kontrola proti NULL pointeru
  if (table == NULL || key == NULL || value == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table, key or value address cannot be null.");
    return IFJ_EINTERNAL;
  }

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
 * @return IFJ_EINTERNAL pokud prvek neexistuje.
 * @return IFJ_SUCCESS při úspěšném vyhledání.
 */
static IFJ_ERR avl_find(avl_node_t *root, const char *key,
                        symtable_data_t **value) {
  // Pokud je strom prázdný, vrátíme chybu
  if (!root) {
    return IFJ_EINTERNAL;
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

  return IFJ_EINTERNAL;
}

IFJ_ERR symtable_find(symtable_t *table, const char *key,
                      symtable_data_t **value) {
  IFJ_ERR err;

  // Kontrola proti NULL pointeru
  if (table == NULL || key == NULL || value == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table, key or value address cannot be null.");
    return IFJ_EINTERNAL;
  }

  // Pokud je strom prázdný, vrátíme chybu
  if (!table->root) {
    return IFJ_EINTERNAL;
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
  if (table == NULL) {
    LOG_ERR(IFJ_EINTERNAL, "Table address cannot be null.");
    return IFJ_EINTERNAL;
  }

  // Uvolnění stromu
  avl_destroy(table->root);

  // Nastavení ukazatele na NULL
  table->root = NULL;

  return IFJ_SUCCESS;
}

#ifdef DEBUG

static void print_tree_keys(avl_node_t *root) {
  if (root) {
    printf("%s", root->key);
    print_tree_keys(root->left);
    print_tree_keys(root->right);
  }
}

static void print_tree_data(avl_node_t *root) {
  if (root) {
    printf("%d", root->data.cislo);
    print_tree_data(root->left);
    print_tree_data(root->right);
  }
}

int main() {
  IFJ_ERR err;
  symtable_t *table = NULL;
  int cisla[] = {1, 2, 3, 4, 5};
  char *keys[] = {"g", "e", "a", "f", "j"};
  symtable_data_t data, *found_data;

  err = symtable_init(&table);
  CHECK_ERR_RET(err);

  for (int i = 0; i < 5; i++) {
    data.cislo = cisla[i];
    err = symtable_insert(table, keys[i], &data);
    CHECK_ERR_RET(err);
    print_tree_keys(table->root);
    printf("\n");
  }

  print_tree_data(table->root);
  printf("\n");

  symtable_find(table, "a", &found_data);
  printf("Key: %s, data: %d\n", "a", found_data->cislo);
  found_data->cislo = 10;

  symtable_find(table, "b", &found_data);
  printf("Key: %s, data: %d\n", "b", found_data->cislo);

  err = symtable_destroy(&table);
  return err;
}

#endif
