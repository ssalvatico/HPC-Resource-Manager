#ifndef __TABLAHASH_H__
#define __TABLAHASH_H__

typedef void        *(*FuncionCopiadora)    (void *dato);                  // Retorna una copia fisica del dato
typedef int         (*FuncionComparadora)   (void *dato1, void *dato2);    // Retorna un entero negativo si dato1 < dato2, 0 si son iguales y un entero positivo si dato1 > dato2
typedef void        (*FuncionDestructora)   (void *dato);                  // Libera la memoria alocada para el dato
typedef unsigned    (*FuncionHash)          (void *dato);                  // Retorna un entero sin signo para el dato
typedef void        (*FuncionVisitante)     (void *dato);

typedef struct _TablaHash *TablaHash;

/**
 * @brief Creates and returns a new empty hash table.
 *
 * Allocates memory for the hash table and its internal array of slots.
 * All slots are initialized to NULL with their deletion flag cleared.
 *
 * @param capacidad Initial number of slots in the hash table.
 * @param copia     Function pointer to copy an element before insertion.
 * @param comp      Function pointer to compare two elements (0 = equal).
 * @param destr     Function pointer to free an element's memory.
 * @param hash      Function pointer returning an unsigned hash for an element.
 * @return A pointer to the newly created TablaHash.
 */
TablaHash tablahash_crear(unsigned capacidad, FuncionCopiadora copia,
                          FuncionComparadora comp, FuncionDestructora destr,
                          FuncionHash hash);

/**
 * @brief Returns the number of elements currently stored in the table.
 *
 * @param tabla The hash table to query.
 * @return The number of active elements.
 */
int tablahash_nelems(TablaHash tabla);

/**
 * @brief Returns the current capacity (number of slots) of the table.
 *
 * @param tabla The hash table to query.
 * @return The total number of slots allocated.
 */
int tablahash_capacidad(TablaHash tabla);

/**
 * @brief Frees all memory associated with the hash table and its elements.
 *
 * Calls the destructor on each active element before freeing the internal
 * array and the table structure itself.
 *
 * @param tabla The hash table to destroy.
 * @return Void.
 */
void tablahash_destruir(TablaHash tabla);

/**
 * @brief Inserts an element into the table, replacing it if already present.
 *
 * Uses open addressing with linear probing. If the load factor exceeds 0.7
 * after insertion, the table is automatically resized to double its capacity.
 *
 * @param tabla The hash table to insert into.
 * @param dato  Pointer to the element to insert.
 * @return Void.
 */
void tablahash_insertar(TablaHash tabla, void *dato);

/**
 * @brief Searches for an element in the table by value.
 *
 * Uses the hash and compare functions to locate the element.
 *
 * @param tabla The hash table to search.
 * @param dato  Pointer to the element to search for.
 * @return A pointer to the matching element, or NULL if not found.
 */
void *tablahash_buscar(TablaHash tabla, void *dato);

/**
 * @brief Removes an element from the table by value.
 *
 * Marks the slot as logically deleted using a tombstone flag, allowing
 * linear probing chains to remain intact for future searches.
 *
 * @param tabla The hash table to remove from.
 * @param dato  Pointer to the element to remove.
 * @return Void.
 */
void tablahash_eliminar(TablaHash tabla, void *dato);

/**
 * @brief Visits every active element in the table and applies a function.
 *
 * Iterates over all slots and calls the visitor function on each
 * non-null, non-deleted element. Order of traversal is not guaranteed.
 *
 * @param tabla The hash table to traverse.
 * @param show  Function pointer to apply to each active element.
 * @return Void.
 */
void tablahash_visitar(TablaHash tabla, FuncionVisitante show);

#endif /* __TABLAHASH_H__ */