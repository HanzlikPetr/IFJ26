/**
 * @file stack.h
 * @brief IFJ26 - Definice zásobníku a jeho funkcí (stack)
 *
 * @author xberanm00 Marek Beran
 */

#ifndef STACK_H
#define STACK_H

/**
 * @brief Reprezentace prvku zásobníku.
 */
typedef struct stack_elem stack_elem_t;

/**
 * @brief Definice zásobníku.
 */
typedef struct stack {
  stack_elem_t *top;              /**< Vrchol zásobníku. */
} stack_t;

/**
 * @brief Inicializace prázdného zásobníku.
 *
 * @param stack Zásobník, který se má inicializovat.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšné inicializaci.
 */
IFJ_ERR stack_init(stack_t *stack);

/**
 * @brief Uvolnění zásobníku.
 *
 * @param stack Zásobník, který se má uvolnit.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšném uvolnění zásobníku.
 */
IFJ_ERR stack_dtor(stack_t *stack);

/**
 * @brief Kontrola prázdnosti stromu.
 *
 * @param stack Zásobník, který se kontroluje.
 * @param is_empty Proměnná, která zachycuje výsledek.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšné kontrole prázdnosti zásobníku.
 */
IFJ_ERR stack_is_empty(stack_t *stack, bool *is_empty);

/**
 * @brief Vložení prvku na vrchol zásobníku.
 *
 * @param stack Zásobník, na který se vkládá.
 * @param data Hodnota, která se zapisuje na zásobník.
 * @return IFJ_EINTERNAL při poslání ukazatelů NULL, nebo při chybě alokace
 * paměti.
 * @return IFJ_SUCCESS při úspěšném vložení prvku na zásobník.
 */
IFJ_ERR stack_push(stack_t *stack, void *data);

/**
 * @brief Ukázání vrcholu zásobníku.
 *
 * @param stack Zásobník, jehož vrchol se ukazuje.
 * @param data_out Hodnota, kam se vrchol zapisuje.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšném ukázání vrcholu zásobníku.
 */
IFJ_ERR stack_top(stack_t *stack, void **data_out);

/**
 * @brief Odstranění a vrácení vrcholu zásobníku.
 *
 * @param stack Zásobník, ze kterého se odstraňuje.
 * @param data_out Hodnota, která zachycuje odstraňovaný vrchol.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšném odstranění prvku ze zásobníku.
 */
IFJ_ERR stack_pop(stack_t *stack, void **data_out);

#endif // STACK_H
