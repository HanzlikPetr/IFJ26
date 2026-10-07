/**
 * @file error.h
 * @brief IFJ26 - Definice chybových kódů a makra pro kontrolu chyb
 *
 * @author xhanzlp00 Petr Hanzlík
 */

#ifndef IFJ_ERROR_H
#define IFJ_ERROR_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Typ pro chybové kódy v překladači IFJ26
 */
typedef enum {
  IFJ_SUCCESS = 0,     /**< Úspěšný průběh */
  IFJ_ELEX = 1,        /**< Chyba lexikální analýzy */
  IFJ_ESYNTAX = 2,     /**< Chyba syntaktické analýzy */
  IFJ_ESEM_UNDEF = 3, /**< Nedefinovaná funkce/proměnná/typ, chybějící @main */
  IFJ_ESEM_REDEF = 4, /**< Nepovolená redefinice funkce/proměnné/typu */
  IFJ_ESEM_ARGS = 5, /**< Špatný počet/typ argumentů, parametry @main */
  IFJ_ESEM_TYPE = 6, /**< Typová kompatibilita/odvození typu */
  IFJ_ESEM_IMMUT = 7, /**< Zápis do nemodifikovatelné proměnné/parametru */
  IFJ_ESEM_OTHER = 10, /**< Ostatní sémantické chyby */
  IFJ_EINTERNAL = 99 /**< Interní chyba překladače */
} IFJ_ERR;

/**
 * @brief Makro pro kontrolu chybového kódu.
 * @param err Chybový kód.
 */
#define CHECK_ERR(err) ((err) != IFJ_SUCCESS)

/**
 * @brief Makro pro kontrolu chybového kódu a návrat z funkce.
 * @param err Chybový kód.
 */
#define CHECK_ERR_RET(err)                                                     \
  do {                                                                         \
    if (CHECK_ERR(err)) {                                                      \
      return err;                                                              \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro kontrolu chybového kódu, logování a návrat z funkce.
 * @param err Chybový kód.
 * @param ... Formátovací řetězec zprávy a jeho volitelné argumenty.
 */
#define CHECK_ERR_LOG_RET(err, ...)                                            \
  do {                                                                         \
    if (CHECK_ERR(err)) {                                                      \
      LOG_ERR(err, __VA_ARGS__);                                               \
      return err;                                                              \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro kontrolu alokace paměti.
 * @param ptr Ukazatel na alokovanou paměť.
 */
#define CHECK_MEM_ERR(ptr)                                                     \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      LOG_ERR(IFJ_EINTERNAL, "Memory allocation failed");                      \
      return IFJ_EINTERNAL;                                                    \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro logování chybového kódu a zprávy.
 * @param err Chybový kód.
 * @param ... Formátovací řetězec zprávy a jeho volitelné argumenty.
 */
#define LOG_ERR(err, ...)                                                      \
  do {                                                                         \
    fprintf(stderr, "Error: %d in %s at line %d: ", (err), __FILE__,           \
            __LINE__);                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputc('\n', stderr);                                                       \
  } while (0)

/**
 * @brief Makro pro kontrolu chybového kódu a přechod na label.
 * @param err Chybový kód.
 * @param goto_label Label pro přechod.
 */
#define CHECK_ERR_GOTO(err, goto_label)                                        \
  do {                                                                         \
    if (CHECK_ERR(err)) {                                                      \
      goto goto_label;                                                         \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro kontrolu null hodnoty argumentu a návratovou hodnotu.
 * @param arg Argument k kontrolě.
 */
#define CHECK_ARG_NULL_RET(arg)                                                \
  do {                                                                         \
    if ((arg) == NULL) {                                                       \
      LOG_ERR(IFJ_EINTERNAL, "Argument cannot be null.");                      \
      return IFJ_EINTERNAL;                                                    \
    }                                                                          \
  } while (0)

#define EXPAND(x) x
#define GLUE(a, b) a##b

// Počítadlo pro 1 až 10 argumentů
#define GET_11TH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define COUNT_ARGS(...)                                                        \
  EXPAND(GET_11TH_ARG(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

// Expanze pro 1 až 10 argumentů
#define MAP_1(a) CHECK_ARG_NULL_RET(a);
#define MAP_2(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_1(__VA_ARGS__))
#define MAP_3(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_2(__VA_ARGS__))
#define MAP_4(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_3(__VA_ARGS__))
#define MAP_5(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_4(__VA_ARGS__))
#define MAP_6(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_5(__VA_ARGS__))
#define MAP_7(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_6(__VA_ARGS__))
#define MAP_8(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_7(__VA_ARGS__))
#define MAP_9(a, ...)                                                          \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_8(__VA_ARGS__))
#define MAP_10(a, ...)                                                         \
  CHECK_ARG_NULL_RET(a);                                                       \
  EXPAND(MAP_9(__VA_ARGS__))

#define DISPATCH(n) GLUE(MAP_, n)

/**
 * @brief Makro pro kontrolu null hodnoty argumentů a návratovou hodnotu.
 * @param ... Argumenty k kontrolě.
 */
#define CHECK_ARGS_NULL_RET(...)                                               \
  EXPAND(DISPATCH(COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__))

#endif // IFJ_ERROR_H
