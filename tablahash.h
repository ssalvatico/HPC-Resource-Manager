#ifndef __TABLAHASH_H__
#define __TABLAHASH_H__

typedef void        *(*FuncionCopiadora)    (void *dato);                  // Retorna una copia fisica del dato
typedef int         (*FuncionComparadora)   (void *dato1, void *dato2);    // Retorna un entero negativo si dato1 < dato2, 0 si son iguales y un entero positivo si dato1 > dato2
typedef void        (*FuncionDestructora)   (void *dato);                  // Libera la memoria alocada para el dato
typedef unsigned    (*FuncionHash)          (void *dato);                  // Retorna un entero sin signo para el dato
typedef void        (*FuncionVisitante)     (void *dato);

typedef struct _TablaHash *TablaHash;

TablaHash tablahash_crear(  unsigned capacidad, 
                            FuncionCopiadora copia,
                            FuncionComparadora comp, 
                            FuncionDestructora destr,
                            FuncionHash hash);

int tablahash_nelems(TablaHash tabla);                          // Retorna el numero de elementos de la tabla.

int tablahash_capacidad(TablaHash tabla);                       // Retorna la capacidad de la tabla.

void tablahash_destruir(TablaHash tabla);                       // Destruye la tabla.

void tablahash_insertar(TablaHash tabla, void *dato);           // Inserta un dato en la tabla, o lo reemplaza si ya se encontraba.

void *tablahash_buscar(TablaHash tabla, void *dato);            // Retorna el dato de la tabla que coincida con el dato dado, o NULL si el dato no es encontrado

void tablahash_eliminar(TablaHash tabla, void *dato);           // Elimina el dato de la tabla que coincida con el dato dado.

void tablahash_visitar(TablaHash tabla, FuncionVisitante show);

#endif /* __TABLAHASH_H__ */