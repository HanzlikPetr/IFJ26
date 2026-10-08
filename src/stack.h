/**
 * @file stack.h
 * @brief IFJ26 - Implementace zásobníku (stack)
 *
 * @author xberanm00 Marek Beran
 */

#ifndef IFJ_STACK_H
#define IFJ_STACK_H

#include "error.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Reprezentace prvku zásobníku.
 */
typedef struct stack_elem_t {
  void *data;                    // Hodnota uložená v prvku.
  struct stack_elem_t *next;      // Další prvek zásobníku.
} stack_elem_t;

/**
 * @brief Implementace zásobníku.
 */
typedef struct {
  stack_elem_t *top;              // Vrchol zásobníku.
} stack_t;

/**
 * @brief Inicializace prázdného zásobníku.
 *
 * @param stack Zásobník, který se má inicializovat.
 */
void stack_init(stack_t *stack) { stack->top = NULL; }

/**
 * @brief Kontrola prázdnosti stromu.
 *
 * @param stack Zásobník, který se kontroluje.
 * @param is_empty Proměnná, která zachycuje výsledek.
 */
void stack_is_empty(stack_t *stack, bool *is_empty) {
  *is_empty = (stack->top == NULL);
}

/**
 * @brief Vložení prvku na vrchol zásobníku.
 *
 * @param stack Zásobník, na který se vkládá.
 * @param data Hodnota, která se zapisuje na zásobník.
 */
IFJ_ERR stack_push(stack_t *stack, void *data) {
  stack_elem_t *new_elem_ptr = (stack_elem_t *)malloc(sizeof(stack_elem_t));
  CHECK_MEM_ERR(new_elem_ptr);

  new_elem_ptr->data = data;
  new_elem_ptr->next = stack->top;
  stack->top = new_elem_ptr;

  return IFJ_SUCCESS;
}

/**
 * @brief Ukázání vrcholu zásobníku.
 *
 * @param stack Zásobník, jehož vrchol se ukazuje.
 * @param data_out Hodnota, kam se vrchol zapisuje.
 */
IFJ_ERR stack_top(stack_t *stack, void **data_out) {
  if (stack->top == NULL) {
    return IFJ_ESYNTAX;     // co vracet + je nejake makro?
  }

  *data_out = stack->top->data;
  return IFJ_SUCCESS;
}

/**
 * @brief Odstranění a vrácení vrcholu zásobníku.
 *
 * @param stack Zásobník, ze kterého se odstraňuje.
 * @param data_out Hodnota, která zachycuje odstraňovaný vrchol.
 */
IFJ_ERR stack_pop(stack_t *stack, void **data_out) {
  if (stack->top == NULL) {
    return IFJ_ESYNTAX;     // co vracet + je nejake makro?
  }

  stack_elem_t *elem_ptr = stack->top;
  stack->top = elem_ptr->next;
  *data_out = elem_ptr;
  free(elem_ptr);

  return IFJ_SUCCESS;
}

#endif
