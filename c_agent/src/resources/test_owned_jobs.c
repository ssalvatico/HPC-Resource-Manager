#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// IMPORTANTE: Ajusta esta ruta si es diferente en tu máquina
#include "../../include/resources/owned_jobs.h"

int errores_test = 0;

void assert_cond(int condicion, const char* test_name) {
    if (condicion) {
        printf("[EXITO] %s\n", test_name);
    } else {
        printf("[FALLO] %s\n", test_name);
        errores_test++;
    }
}

int main() {
    printf("==================================================\n");
    printf(" INICIANDO TEST EXHAUSTIVO DE OWNED JOBS\n");
    printf("==================================================\n");

    owned_jobs_t jobs = create_owned_jobs();
    assert_cond(jobs != NULL, "1. Tabla de Owned Jobs creada exitosamente");

    /* =========================================================================
       TEST A: CREACION Y AGREGADO DE PETICIONES (JOB REQUEST)
       ========================================================================= */
    printf("\n--> TEST A: Creacion y Peticiones\n");
    
    // Erlang pide el Job 100 que necesita recursos de 3 nodos distintos
    add_new_owned_job(jobs, 100);
    int p1 = append_petition_to_job(jobs, 100, "192.168.1.10", 8000, CPU, 2);
    int p2 = append_petition_to_job(jobs, 100, "192.168.1.11", 8001, RAM, 4096);
    int p3 = append_petition_to_job(jobs, 100, "192.168.1.12", 8002, GPU, 1);
    
    // Job 101 que necesita recursos de 2 nodos
    add_new_owned_job(jobs, 101);
    append_petition_to_job(jobs, 101, "192.168.1.10", 8000, RAM, 1024); // Mismo nodo que el Job 100
    append_petition_to_job(jobs, 101, "192.168.1.13", 8003, CPU, 4);

    assert_cond(p1 && p2 && p3, "2. Peticiones anexadas correctamente al Job 100");

    /* =========================================================================
       TEST B: EXTRACCION DE RESERVAS (CONSTRUCCION DEL OUTBOX)
       ========================================================================= */
    printf("\n--> TEST B: Iterador de envios (get_next_reserve)\n");
    
    char *ip; unsigned port, cantidad; resource_t tipo;
    
    // Extraemos la 1ra peticion (Deberia ser la de la IP .10)
    int r1 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    assert_cond(r1 == 1 && strcmp(ip, "192.168.1.10") == 0 && tipo == CPU, "3. Primera peticion extraida en orden estricto");
    
    // Extraemos la 2da y 3ra
    get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad); // La .11
    get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad); // La .12
    
    // Intentamos extraer una 4ta (Ya no hay mas)
    int r4 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    assert_cond(r4 == 0, "4. El iterador retorna 0 cuando no hay mas peticiones por enviar");

    /* =========================================================================
       TEST C: RECEPCION DE RESPUESTAS DE LA RED (GRANTED)
       ========================================================================= */
    printf("\n--> TEST C: Actualizacion de estado (mark_petition_as_granted)\n");
    
    // El nodo .10 y .11 nos dicen que SÍ (GRANTED)
    int g1 = mark_petition_as_granted(jobs, 100, "192.168.1.10", 8000);
    int g2 = mark_petition_as_granted(jobs, 100, "192.168.1.11", 8001);
    assert_cond(g1 == 0 && g2 == 0, "5. El Job 100 sigue pendiente (retorna 0) porque falta el nodo .12");

    /* =========================================================================
       TEST D: EXTRACCION PARA ROLLBACKS (get_granted_resources)
       ========================================================================= */
    printf("\n--> TEST D: Extraccion de Rollback ante DENIED\n");
    
    // Supongamos que el nodo .12 nos manda DENIED. Tenemos que abortar el Job 100.
    // Necesitamos extraer SOLO las peticiones que ya nos habian dado GRANTED para devolverlas.
    char *ips[10]; unsigned ports[10], cantidades[10]; resource_t tipos[10];
    
    unsigned concedidos = get_granted_resources(jobs, 100, ips, ports, tipos, cantidades, 10);
    assert_cond(concedidos == 2, "6. Extrae exactamente 2 recursos que deben ser devueltos");
    assert_cond(strcmp(ips[0], "192.168.1.10") == 0 && strcmp(ips[1], "192.168.1.11") == 0, "7. IPs extraidas coinciden con los nodos que dieron GRANTED");

    // Borramos el Job 100 porque fallo
    remove_owned_job(jobs, 100);
    int check = mark_petition_as_granted(jobs, 100, "192.168.1.10", 8000);
    assert_cond(check == 0, "8. El Job 100 fue eliminado correctamente de la memoria");

    /* =========================================================================
       TEST E: FINALIZACION EXITOSA DE UN JOB
       ========================================================================= */
    printf("\n--> TEST E: Finalizacion total de un Job\n");
    
    // Avanzamos el iterador del Job 101 para "enviar" los reserves
    get_next_reserve(jobs, 101, &ip, &port, &tipo, &cantidad);
    get_next_reserve(jobs, 101, &ip, &port, &tipo, &cantidad);
    
    // Respondemos GRANTED a las dos peticiones del Job 101
    mark_petition_as_granted(jobs, 101, "192.168.1.10", 8000);
    int ok = mark_petition_as_granted(jobs, 101, "192.168.1.13", 8003);
    assert_cond(ok == 1, "9. mark_petition retorna 1 al completarse todas las peticiones del Job");

    int ok_dup = mark_petition_as_granted(jobs, 101, "192.168.1.13", 8003);
    assert_cond(ok_dup == 0, "10. No notifica duplicado si llega un GRANTED fantasma");

    /* =========================================================================
       TEST F: TOLERANCIA A FALLOS - NODO CAIDO
       ========================================================================= */
    printf("\n--> TEST F: Desconexion abrupta de un nodo\n");
    
    add_new_owned_job(jobs, 200);
    append_petition_to_job(jobs, 200, "192.168.5.5", 5555, CPU, 1);
    
    add_new_owned_job(jobs, 201);
    append_petition_to_job(jobs, 201, "192.168.5.5", 5555, RAM, 100);
    append_petition_to_job(jobs, 201, "192.168.1.10", 8000, GPU, 1);
    
    add_new_owned_job(jobs, 202); // Este no depende del nodo .5.5
    append_petition_to_job(jobs, 202, "192.168.1.10", 8000, RAM, 100);

    // El nodo 192.168.5.5 muere. Buscamos a los afectados.
    unsigned afectados[10];
    unsigned cant_afectados = get_jobs_affected_by_dead_node(jobs, "192.168.5.5", 5555, afectados, 10);
    
    assert_cond(cant_afectados == 2, "11. Encuentra exactamente 2 Jobs afectados por la muerte del nodo");
    
    // Verificamos que sean el 200 y el 201
    int match = ((afectados[0] == 200 && afectados[1] == 201) || (afectados[0] == 201 && afectados[1] == 200));
    assert_cond(match, "12. Los Jobs identificados son los correctos (200 y 201)");

    /* =========================================================================
       TEST G: DESTRUCCION FINAL (Memory Leaks Check)
       ========================================================================= */
    printf("\n--> TEST G: Limpieza de Memoria\n");
    delete_owned_jobs(jobs);
    printf("[EXITO] 13. Estructuras destruidas\n");

    printf("\n==================================================\n");
    if (errores_test == 0) {
        printf(" \033[0;32mRESULTADO: TODOS LOS TESTS PASARON (0 Errores)\033[0m\n");
    } else {
        printf(" \033[0;31mRESULTADO: FALLARON %d TESTS. REVISA EL CODIGO.\033[0m\n", errores_test);
    }
    printf("==================================================\n");

    return 0;
}