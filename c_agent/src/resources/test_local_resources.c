#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
// IMPORTANTE: Ajusta esta ruta si es diferente en tu máquina
#include "../../include/resources/local_resource.h"

// Variable global para contar errores en el test
int errores_test = 0;

void print_status(local_resources_t *res, const char* etapa) {
    unsigned cpu, gpu, ram;
    get_local_resources(res, &cpu, &gpu, &ram);
    printf("\n--- ESTADO [%s] ---\n", etapa);
    printf("Disponibilidad -> CPU: %u | GPU: %u | RAM: %u\n", cpu, gpu, ram);
    printf("------------------------\n");
}

void assert_recursos(local_resources_t *res, unsigned esp_cpu, unsigned esp_gpu, unsigned esp_ram, const char* test_name) {
    unsigned cpu, gpu, ram;
    get_local_resources(res, &cpu, &gpu, &ram);
    if (cpu == esp_cpu && gpu == esp_gpu && ram == esp_ram) {
        printf("[EXITO] %s\n", test_name);
    } else {
        printf("[FALLO] %s (Esperado: %u,%u,%u | Obtenido: %u,%u,%u)\n", test_name, esp_cpu, esp_gpu, esp_ram, cpu, gpu, ram);
        errores_test++;
    }
}

int main() {
    printf("==================================================\n");
    printf(" INICIANDO TEST EXHAUSTIVO DE RECURSOS LOCALES\n");
    printf("==================================================\n");

    // Inicializamos con: 4 CPU, 1 GPU, 8192 RAM
    local_resources_t *res = create_local_resource(4, 1, 8192);
    active_jobs_t jobs = create_active_jobs_table();

    assert_recursos(res, 4, 1, 8192, "1. Inicializacion correcta");

    /* =========================================================================
       TEST A: INDEPENDENCIA DE IDENTIDAD (Mismo JOB_ID, Distinto Socket)
       ========================================================================= */
    printf("\n--> TEST A: Independencia de Identidad\n");
    
    // Socket 5 pide Job 100
    int r1 = new_job_request(res, jobs, 100, 5, 1, CPU);
    // Socket 8 pide Job 100 (El cliente Erlang remoto generó el mismo ID por azar)
    int r2 = new_job_request(res, jobs, 100, 8, 1, CPU);
    // Socket 5 pide Job 101 (El mismo nodo pide otra cosa)
    int r3 = new_job_request(res, jobs, 101, 5, 1, CPU);

    if (r1 == 1 && r2 == 1 && r3 == 1) printf("[EXITO] Multiples asignaciones validas\n");
    else { printf("[FALLO] Asignaciones validas retornaron error\n"); errores_test++; }
    
    assert_recursos(res, 1, 1, 8192, "2. Recursos descontados correctamente (quedan 1 CPU)");

    /* =========================================================================
       TEST B: LIMITES FISICOS (DENIED)
       ========================================================================= */
    printf("\n--> TEST B: Limites y Rechazos\n");
    
    // Socket 9 pide 5 CPUs (La maquina solo tiene 4 en total) -> Debe dar -1 (DENIED)
    int r4 = new_job_request(res, jobs, 200, 9, 5, CPU);
    if (r4 == -1) printf("[EXITO] Peticion imposible rechazada con -1 (DENIED)\n");
    else { printf("[FALLO] Peticion imposible no fue rechazada\n"); errores_test++; }

    /* =========================================================================
       TEST C: ENCOLAMIENTO (WAIT)
       ========================================================================= */
    printf("\n--> TEST C: Encolamiento y Cola de Espera\n");

    // Socket 9 pide 2 CPUs. Solo queda 1. Se debe encolar -> Debe dar 0 (WAIT)
    int r5 = new_job_request(res, jobs, 201, 9, 2, CPU);
    if (r5 == 0) printf("[EXITO] Peticion sin recursos actuales encolada con 0 (WAIT)\n");
    else { printf("[FALLO] No se encolo correctamente\n"); errores_test++; }

    // Socket 10 pide 1 GPU (Queda 1) -> GRANTED
    new_job_request(res, jobs, 300, 10, 1, GPU);
    // Socket 11 pide 1 GPU (Quedan 0) -> WAIT
    new_job_request(res, jobs, 300, 11, 1, GPU);

    assert_recursos(res, 1, 0, 8192, "3. Estado tras encolamientos (1 CPU, 0 GPU, 8192 RAM)");

    /* =========================================================================
       TEST D: LIBERACION EXACTA Y AVANCE DE COLA (RELEASE normal)
       ========================================================================= */
    printf("\n--> TEST D: Liberacion especifica y chk_job_request\n");

    // Liberamos el Job 100 del Socket 5. OJO: El Job 100 del Socket 8 NO DEBE BORRARSE
    del_active_job(res, jobs, 100, 5); 
    assert_recursos(res, 2, 0, 8192, "4. Se recupero 1 CPU exacta del socket 5");

    // Revisamos la cola (Deberia darle las 2 CPUs al socket 9, Job 201 que estaba esperando)
    char buffer[256];
    unsigned socket_notificado = 0;
    int job_desencolado = chk_job_request(res, jobs, buffer, sizeof(buffer), &socket_notificado);
    
    if (job_desencolado == 201 && socket_notificado == 9) {
        printf("[EXITO] La cola avanzo y otorgo el recurso a Socket 9 (Job 201)\n");
    } else { 
        printf("[FALLO] La cola no avanzo o dio el recurso a alguien erroneo\n"); errores_test++; 
    }
    assert_recursos(res, 0, 0, 8192, "5. Recursos consumidos por la cola (0 CPU, 0 GPU)");

    /* =========================================================================
       TEST E: ABORTO DE COLA (del_pending_job_requests)
       ========================================================================= */
    printf("\n--> TEST E: Aborto de peticiones encoladas\n");
    
    // El Socket 11 cancela su peticion del Job 300 (GPU) que estaba encolada
    del_pending_job_requests(res, 300, 11);
    
    // Si la borro, liberar una GPU NO deberia otorgarsela al Socket 11 ahora
    del_active_job(res, jobs, 300, 10); // Socket 10 libera la GPU
    int check_vacio = chk_job_request(res, jobs, buffer, sizeof(buffer), &socket_notificado);
    
    if (check_vacio == -1) printf("[EXITO] La peticion pendiente fue eliminada exitosamente\n");
    else { printf("[FALLO] Se desencolo un trabajo que debio ser abortado\n"); errores_test++; }

    /* =========================================================================
       TEST F: PURGA TOTAL POR MUERTE DE NODO (free_all_resources_from_socket)
       ========================================================================= */
    printf("\n--> TEST F: Purga de Desconexion Abrupta (ACTION_DISCONNECTED)\n");
    
    // Estado actual antes de la purga:
    // Activos: Socket 8/Job 100 (1 CPU), Socket 5/Job 101 (1 CPU), Socket 9/Job 201 (2 CPUs)
    // Vamos a agregar cosas a la cola del Socket 5 para ver si se borran tambien
    new_job_request(res, jobs, 102, 5, 4000, RAM); // GRANTED
    new_job_request(res, jobs, 103, 5, 6000, RAM); // WAIT (Solo quedan 4192)
    
    print_status(res, "Antes de matar al Socket 5");

    // ¡MATAMOS AL SOCKET 5! (Debe devolver 1 CPU del Job 101, 4000 RAM del Job 102 y borrar el Job 103 de la cola)
    free_all_resources_from_socket(res, jobs, 5);

    assert_recursos(res, 1, 1, 8192, "6. Purga exitosa: Se devolvieron exactamente los recursos del Socket 5");

    // Verificamos que el Job 103 realmente desaparecio de la cola
    del_active_job(res, jobs, 100, 8); // Liberamos la CPU del Socket 8
    int check_cola_limpia = chk_job_request(res, jobs, buffer, sizeof(buffer), &socket_notificado);
    if (check_cola_limpia == -1) printf("[EXITO] Las colas del Socket 5 fueron purgadas correctamente\n");
    else { printf("[FALLO] Quedaron fantasmas en la cola del socket desconectado\n"); errores_test++; }

    /* =========================================================================
       TEST G: DESTRUCCION FINAL (Memory Leaks Check)
       ========================================================================= */
    printf("\n--> TEST G: Limpieza de Memoria\n");
    delete_local_resource(res);
    tablahash_destruir(jobs);
    printf("[EXITO] Estructuras destruidas\n");

    printf("\n==================================================\n");
    if (errores_test == 0) {
        printf(" \033[0;32mRESULTADO: TODOS LOS TESTS PASARON (0 Errores)\033[0m\n");
    } else {
        printf(" \033[0;31mRESULTADO: FALLARON %d TESTS. REVISA EL CODIGO.\033[0m\n", errores_test);
    }
    printf("==================================================\n");

    return 0;
}