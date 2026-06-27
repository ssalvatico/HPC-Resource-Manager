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
    add_new_owned_job(jobs, 100, 10);
    int p1 = append_petition_to_job(jobs, 100, "192.168.1.10", 8000, CPU, 2);
    int p2 = append_petition_to_job(jobs, 100, "192.168.1.11", 8001, RAM, 4096);
    int p3 = append_petition_to_job(jobs, 100, "192.168.1.12", 8002, GPU, 1);
    
    // Job 101 que necesita recursos de 2 nodos
    add_new_owned_job(jobs, 101, 11);
    append_petition_to_job(jobs, 101, "192.168.1.10", 8000, RAM, 1024); // Mismo nodo que el Job 100
    append_petition_to_job(jobs, 101, "192.168.1.13", 8003, CPU, 4);

    assert_cond(p1 && p2 && p3 && get_job_owner_socket(jobs, 100) == 10, "2. Job 100 creado con owner y peticiones");

    /* =========================================================================
       TEST B: EXTRACCION DE RESERVAS (CONSTRUCCION DEL OUTBOX)
       ========================================================================= */
    printf("\n--> TEST B: Iterador de envios (get_next_reserve)\n");
    
    char *ip; unsigned port, cantidad; resource_t tipo;
    
    // Extraemos la 1ra peticion (Deberia ser la de la IP .10)
    int r1 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    assert_cond(r1 == 1 && strcmp(ip, "192.168.1.10") == 0 && tipo == CPU, "3. Primera peticion extraida en orden estricto");
    
    /* =========================================================================
       TEST C: RECEPCION DE RESPUESTAS DE LA RED (GRANTED)
       ========================================================================= */
    printf("\n--> TEST C: Actualizacion de estado (mark_petition_as_granted)\n");
    
    int g1 = mark_petition_as_granted(jobs, 100, "192.168.1.10", 8000);
    int r2 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    int g2 = mark_petition_as_granted(jobs, 100, "192.168.1.11", 8001);
    int r3 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    int r4 = get_next_reserve(jobs, 100, &ip, &port, &tipo, &cantidad);
    assert_cond(r2 == 1 && r3 == 1 && r4 == 0, "4. El iterador mantiene el orden secuencial");
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
    assert_cond(check == -1, "8. El Job 100 fue eliminado correctamente de la memoria");

    /* =========================================================================
       TEST E: FINALIZACION EXITOSA DE UN JOB
       ========================================================================= */
    printf("\n--> TEST E: Finalizacion total de un Job\n");
    
    get_next_reserve(jobs, 101, &ip, &port, &tipo, &cantidad);
    mark_petition_as_granted(jobs, 101, "192.168.1.10", 8000);
    get_next_reserve(jobs, 101, &ip, &port, &tipo, &cantidad);
    int ok = mark_petition_as_granted(jobs, 101, "192.168.1.13", 8003);
    assert_cond(ok == 1, "9. mark_petition retorna 1 al completarse todas las peticiones del Job");

    int ok_dup = mark_petition_as_granted(jobs, 101, "192.168.1.13", 8003);
    assert_cond(ok_dup == -1, "10. No notifica duplicado si llega un GRANTED fantasma");

    add_new_owned_job(jobs, 102, 12);
    append_petition_to_job(jobs, 102, "192.168.1.20", 8020, CPU, 2);
    append_petition_to_job(jobs, 102, "192.168.1.20", 8020, RAM, 2048);
    append_petition_to_job(jobs, 102, "192.168.1.20", 8020, GPU, 1);

    get_next_reserve(jobs, 102, &ip, &port, &tipo, &cantidad);
    int same_node_cpu = mark_petition_as_granted(jobs, 102, "192.168.1.20", 8020);
    get_next_reserve(jobs, 102, &ip, &port, &tipo, &cantidad);
    int same_node_ram = mark_petition_as_granted(jobs, 102, "192.168.1.20", 8020);
    get_next_reserve(jobs, 102, &ip, &port, &tipo, &cantidad);
    int same_node_gpu = mark_petition_as_granted(jobs, 102, "192.168.1.20", 8020);
    assert_cond(same_node_cpu == 0 && same_node_ram == 0 && same_node_gpu == 1, "11. Concede secuencialmente recursos del mismo nodo");

    /* =========================================================================
       TEST F: TOLERANCIA A FALLOS - NODO CAIDO
       ========================================================================= */
    printf("\n--> TEST F: Desconexion abrupta de un nodo\n");
    
    add_new_owned_job(jobs, 200, 20);
    append_petition_to_job(jobs, 200, "192.168.5.5", 5555, CPU, 1);
    
    add_new_owned_job(jobs, 201, 21);
    append_petition_to_job(jobs, 201, "192.168.5.5", 5555, RAM, 100);
    append_petition_to_job(jobs, 201, "192.168.1.10", 8000, GPU, 1);
    
    add_new_owned_job(jobs, 202, 22); // Este no depende del nodo .5.5
    append_petition_to_job(jobs, 202, "192.168.1.10", 8000, RAM, 100);

    // El nodo 192.168.5.5 muere. Buscamos a los afectados.
    unsigned afectados[10];
    unsigned cant_afectados = get_jobs_affected_by_dead_node(jobs, "192.168.5.5", 5555, afectados, 10);
    
    assert_cond(cant_afectados == 2, "12. Encuentra exactamente 2 Jobs afectados por la muerte del nodo");
    
    // Verificamos que sean el 200 y el 201
    int match = ((afectados[0] == 200 && afectados[1] == 201) || (afectados[0] == 201 && afectados[1] == 200));
    assert_cond(match, "13. Los Jobs identificados son los correctos (200 y 201)");

    /* =========================================================================
       TEST G: DESTRUCCION FINAL (Memory Leaks Check)
       ========================================================================= */
    printf("\n--> TEST G: Limpieza de Memoria\n");
    delete_owned_jobs(jobs);
    printf("[EXITO] 14. Estructuras destruidas\n");

    printf("\n==================================================\n");
    if (errores_test == 0) {
        printf(" \033[0;32mRESULTADO: TODOS LOS TESTS PASARON (0 Errores)\033[0m\n");
    } else {
        printf(" \033[0;31mRESULTADO: FALLARON %d TESTS. REVISA EL CODIGO.\033[0m\n", errores_test);
    }
    printf("==================================================\n");

    return 0;
}
