#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#define RES_NUM 3

/**
 * @brief Identifiers for the types of hardware resources managed by the node.
 */
typedef enum { CPU = 0, GPU = 1, RAM = 2 } resource_t;

// Aquí también podrías poner otros #define globales si los tuvieras
// #define BUFFER_SIZE 1024
// #define MAX_FDS 100

#endif