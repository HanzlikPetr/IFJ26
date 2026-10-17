/**
 * @file stack.h
 * @brief IFJ26 - Definice zásobníku a jeho funkcí (stack)
 *
 * @author xberanm00 Marek Beran
 */

#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

#include "error.h"

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
 * @brief Pomocný datový typ callback funkce.
 */
typedef IFJ_ERR (*callback_t)(void *);

/**
 * @brief Uvolnění zásobníku.
 *
 * Uživatel ručí za uvolnění atributu data.
 *
 * @param stack Zásobník, který se má uvolnit.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL.
 * @return IFJ_SUCCESS při úspěšném uvolnění zásobníku.
 */
IFJ_ERR stack_dtor(stack_t *stack);

/**
 * @brief Hluboké uvolnění zásobníku.
 *
 * Funkce uvolní i atribut data.
 *
 * @param stack Zásobník, který se má uvolnit.
 * @param data_dtor Funkce, která uvolňuje atribut data.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL,
 * nebo při selhání uvolňování atributu data.
 * @return IFJ_SUCCESS při úspěšném uvolnění zásobníku.
 */
IFJ_ERR stack_deep_dtor(stack_t *stack, callback_t data_dtor);

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
 * @return IFJ_EINTERNAL při poslání ukazatele NULL,
 * nebo když je zásobník prázdný.
 * @return IFJ_SUCCESS při úspěšném ukázání vrcholu zásobníku.
 */
IFJ_ERR stack_top(stack_t *stack, void **data_out);

/**
 * @brief Odstranění a vrácení vrcholu zásobníku.
 *
 * @param stack Zásobník, ze kterého se odstraňuje.
 * @param data_out Hodnota, která zachycuje odstraňovaný vrchol.
 * @return IFJ_EINTERNAL při poslání ukazatele NULL místo ukazatele na zásobník.
 * @return IFJ_SUCCESS při úspěšném odstranění prvku ze zásobníku.
 */
IFJ_ERR stack_pop(stack_t *stack, void **data_out);

#endif // STACK_H
