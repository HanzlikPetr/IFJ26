/**
 * @file stack.c
 * @brief IFJ26 - Implementace zásobníku a jeho funkcí (stack)
 *
 * @author xberanm00 Marek Beran
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "stack.h"

/**
 * @brief Reprezentace prvku zásobníku.
 */
struct stack_elem {
  void *data;                    // Hodnota uložená v prvku.
  struct stack_elem *next;      // Další prvek zásobníku.
};

/**
 * @brief Implementace zásobníku.
 */
struct stack {
  stack_elem_t *top;              // Vrchol zásobníku.
};

/**
 * @brief Inicializace prázdného zásobníku.
 *
 * @param stack Zásobník, který se má inicializovat.
 */
IFJ_ERR stack_init(stack_t *stack) {
  CHECK_ARG_NULL_RET(stack);

  stack->top = NULL;

  return IFJ_SUCCESS;
}

/**
 * @brief Uvolnění zásobníku.
 *
 * @param stack Zásobník, který se má uvolnit.
 */
IFJ_ERR stack_dtor(stack_t *stack) {
  CHECK_ARG_NULL_RET(stack);

  stack_elem_t *top;

  while (stack->top) {
    top = stack->top;
    stack->top = top->next;
    free(top);
  }

  return IFJ_SUCCESS;
}

/**
 * @brief Kontrola prázdnosti stromu.
 *
 * @param stack Zásobník, který se kontroluje.
 * @param is_empty Proměnná, která zachycuje výsledek.
 */
IFJ_ERR stack_is_empty(stack_t *stack, bool *is_empty) {
  CHECK_ARGS_NULL_RET(stack, is_empty);
  *is_empty = (stack->top == NULL);

  return IFJ_SUCCESS;
}

/**
 * @brief Vložení prvku na vrchol zásobníku.
 *
 * @param stack Zásobník, na který se vkládá.
 * @param data Hodnota, která se zapisuje na zásobník.
 */
IFJ_ERR stack_push(stack_t *stack, void *data) {
  CHECK_ARGS_NULL_RET(stack, data);

  stack_elem_t *new_elem_ptr = malloc(sizeof(stack_elem_t));
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
  CHECK_ARGS_NULL_RET(stack, data_out);

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
  IFJ_ERR err = stack_top(stack, data_out);
  CHECK_ERR_RET(err);

  stack_elem_t *temp = stack->top;
  stack->top = temp->next;
  free(temp);

  return IFJ_SUCCESS;
}
