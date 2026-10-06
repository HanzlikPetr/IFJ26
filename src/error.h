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
 * @brief Makro pro kontrolu chybového kódu
 * @param err Chybový kód
 */
#define CHECK_ERR(err) ((err) != IFJ_SUCCESS)

/**
 * @brief Makro pro kontrolu chybového kódu a návrat z funkce
 * @param err Chybový kód
 */
#define CHECK_ERR_RET(err)                                                     \
  do {                                                                         \
    if (CHECK_ERR(err)) {                                                      \
      return err;                                                              \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro kontrolu chybového kódu, logování a návrat z funkce
 * @param err Chybový kód
 * @param ... Formátovací řetězec zprávy a jeho volitelné argumenty
 */
#define CHECK_ERR_LOG_RET(err, ...)                                            \
  do {                                                                         \
    if (CHECK_ERR(err)) {                                                      \
      LOG_ERR(err, __VA_ARGS__);                                               \
      return err;                                                              \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro kontrolu alokace paměti
 * @param ptr Ukazatel na alokovanou paměť
 */
#define CHECK_MEM_ERR(ptr)                                                     \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      LOG_ERR(IFJ_EINTERNAL, "Memory allocation failed");                      \
      return IFJ_EINTERNAL;                                                    \
    }                                                                          \
  } while (0)

/**
 * @brief Makro pro logování chybového kódu a zprávy
 * @param err Chybový kód
 * @param ... Formátovací řetězec zprávy a jeho volitelné argumenty
 */
#define LOG_ERR(err, ...)                                                      \
  do {                                                                         \
    fprintf(stderr, "Error: %d in %s at line %d: ", (err), __FILE__,           \
            __LINE__);                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputc('\n', stderr);                                                       \
  } while (0)

#endif
