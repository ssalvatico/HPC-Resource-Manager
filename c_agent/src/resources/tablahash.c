#include "tablahash.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// la casilla almacena donde esta guardado el dato 

typedef struct {
  void * dato;
  unsigned fdelete;     
} CasillaHash;

// estructura principal que representa la tabla hash.
struct _TablaHash {
  CasillaHash * elems;        // puntero al primer dato del arreglo de casillas
  unsigned numElems;          // cantidad de elementos
  unsigned capacidad;         // cantidad maxima de elementos que puede contener
  FuncionCopiadora copia;
  FuncionComparadora comp;
  FuncionDestructora destr;
  FuncionHash hash;
};

// crea una nueva tabla hash vacia, con la capacidad dada.
TablaHash tablahash_crear(unsigned capacidad,
                          FuncionCopiadora copia,
                          FuncionComparadora comp,
                          FuncionDestructora destr,
                          FuncionHash hash){

  TablaHash tabla = malloc(sizeof(struct _TablaHash));    // se asigna el espacio de memoria necesario para la estructura
  assert(tabla != NULL);

  tabla->elems = malloc(sizeof(CasillaHash) * capacidad); // se asigna el espacio de memoria del array para los elementos
  assert(tabla->elems != NULL);

  tabla->numElems = 0;
  tabla->capacidad = capacidad;
  tabla->copia = copia;
  tabla->comp = comp;
  tabla->destr = destr;
  tabla->hash = hash;

  for (unsigned idx = 0; idx < capacidad; ++idx) { // Inicializamos las casillas con datos nulos.
    tabla->elems[idx].dato = NULL;
    tabla->elems[idx].fdelete = 0;
  }

  return tabla;
}

int tablahash_nelems(TablaHash tabla) { return tabla->numElems; }     // retorna el numero de elementos de la tabla.

int tablahash_capacidad(TablaHash tabla) { return tabla->capacidad; } // retorna la capacidad de la tabla.

void tablahash_destruir(TablaHash tabla) {                            // destruye la tabla
  for (unsigned idx = 0; idx < tabla->capacidad; ++idx)

    if (tabla->elems[idx].dato != NULL)
      tabla->destr(tabla->elems[idx].dato);

  free(tabla->elems);
  free(tabla);
  return;
}

// ***************************************************************************************************************************
// Insercion : recibe un dato, si está en la tabla lo reemplaza, si no está lo agrega, de excederse el indice, redimensiona

void noDest(void * dato){
  dato = dato;
  return;
}

void * noCopy(void * dato){
  return dato;
}

static void th_redimensionar(TablaHash tabla);

static void th_insertar(TablaHash tabla, void *dato,
                        FuncionComparadora  comp, 
                        FuncionDestructora  dest, 
                        FuncionCopiadora    copy){

  CasillaHash * elems = tabla -> elems;                 // guardamos el arreglo
  CasillaHash * elem;           

  unsigned cap = tabla -> capacidad;                    // guardamos la capacidad
  unsigned cond = 0;

  int idxprev = -1;                                     // posicion prevista para guardar el dato

  for(unsigned idx = tabla -> hash(dato) % cap, i = 0 ; i < cap && !cond ; idx = (idx + 1) % cap, i++){
    elem = elems + idx;

    if(elem -> dato == NULL && elem -> fdelete == 0){   // se encontró final del cluster
      if(idxprev == -1)
        idxprev = idx;                                  // de no haberse encontrado pos. eliminada dentro, se lo guarda en extremo
      cond = 1;
    }

    else if(elem -> fdelete == 0 && comp(elem -> dato, dato) == 0){
      idxprev = -1;                                     // se encontró el dato en tabla, se lo actualizará
      cond = 1;
      dest(elem -> dato);
      elem -> dato = copy(dato);
    }

    else if(elem -> fdelete == 1 && idxprev == -1){
      idxprev = idx;                                    // se encontró primera posicion eliminada, quedará como posicion prevista
    }
  }

  if(idxprev == -1)
    return;                                             // no se ingresa el dato

  elems[idxprev].dato = copy(dato);
  elems[idxprev].fdelete = 0;
  (tabla -> numElems)++;

  if(tabla -> numElems * 1.0 / tabla -> capacidad > 0.7)
    th_redimensionar(tabla);
}

static void th_redimensionar(TablaHash tabla){
  unsigned cap = tabla -> capacidad;
  CasillaHash * arr = tabla -> elems;

  tabla -> capacidad *= 2;
  tabla -> elems = malloc(sizeof(CasillaHash) * tabla -> capacidad);

  for(unsigned idx = 0; idx < tabla -> capacidad; idx++){
    tabla -> elems[idx].dato = NULL;
    tabla -> elems[idx].fdelete = 0;
  }

  tabla->numElems = 0;

  for(unsigned idx = 0; idx < cap; idx++){
    if(arr[idx].dato != NULL)
      th_insertar(tabla, arr[idx].dato, tabla -> comp, noDest, noCopy);
  }

  free(arr);
}

void tablahash_insertar(TablaHash tabla, void * dato){
  th_insertar(tabla, dato, tabla -> comp, tabla -> destr, tabla -> copia);
}

// ***************************************************************************************************************************
// Busqueda : retorna el dato de la tabla que coincida con el dato dado, o NULL si el dato buscado no se encuentra en la tabla

void * tablahash_buscar(TablaHash tabla, void *dato) {
  
  CasillaHash * elems = tabla -> elems;
  CasillaHash * elem;

  FuncionComparadora comp = tabla -> comp;
  
  unsigned cap = tabla -> capacidad;
  unsigned cond = 0;
  
  for(unsigned idx = tabla->hash(dato) % cap, i = 0 ; i < cap && !cond ; idx = (idx + 1) % cap, i++){
    elem = elems + idx;

    if(elem -> dato == NULL && elem -> fdelete == 0)
      cond = 1;                                  // Se ha llegado al final del Cluster

    else if(elem -> fdelete == 0 && comp(elem -> dato, dato) == 0)
      cond = 2;                                  // Se a encontrado elemento coincidente
  }

  if(cond == 2)
    return elem -> dato;
  return NULL;
  
}

// ***************************************************************************************************************************
// Eliminacion : Elimina el dato de la tabla que coincida con el dato dado.

static void th_eliminar(TablaHash tabla, void * dato,
                        FuncionComparadora  comp,  
                        FuncionDestructora  dest){

  CasillaHash * elems = tabla -> elems;
  CasillaHash * elem;

  unsigned cap = tabla -> capacidad;
  unsigned cond = 0;

  for(unsigned idx = tabla -> hash(dato) % cap, i = 0 ; i < cap && !cond ; idx = (idx + 1) % cap, i++){
    elem = elems + idx;

    if(elem -> dato == NULL && elem->fdelete == 0)
      cond = 1;                                   // Se ha llegado al final del Cluster

    else if(elem -> fdelete == 0 && comp(elem -> dato, dato) == 0)
      cond = 2;                                   // Se ha encontrado elemento coincidente
  }

  if(cond != 2)
    return;

  dest(elem -> dato);
  elem -> dato = NULL;
  elem -> fdelete = 1;
  (tabla -> numElems)--;
}

void tablahash_eliminar(TablaHash tabla, void *dato) {
  th_eliminar(tabla, dato, tabla -> comp, tabla -> destr);
}

void tablahash_visitar(TablaHash tabla, FuncionVisitante show){
  unsigned can = tabla -> capacidad;
  CasillaHash * elems = tabla -> elems;
  CasillaHash * elem;

  for(unsigned idx = 0 ; idx < can ; idx++){
    elem = elems + idx;
    
    if(elem -> dato != NULL){
      show(elem -> dato);
    }
  }
}
