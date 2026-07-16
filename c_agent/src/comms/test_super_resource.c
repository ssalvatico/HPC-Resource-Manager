#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ajusta las rutas según tu estructura de carpetas
#include "../../include/comms/resource_adapter.h"
#include "../../include/resources/node_structures.h"
#include "../../include/comms/server_types.h"

/* ========================================================================= */
/* 1. MOCKING DEL ENTORNO DE RED                                             */
/* ========================================================================= */

// Simulamos la tabla de puertos de event_handler.c
unsigned mock_ports[1024] = {0};

// Sobrescribimos las funciones que el adapter espera de event_handler.c
unsigned get_connection_port(int fd) {
    if (fd >= 0 && fd < 1024) return mock_ports[fd];
    return 0;
}

int find_fd_by_ip_port(const char* target_ip, unsigned target_port) {
    // En nuestro entorno simulado, buscamos qué FD tiene asignado ese puerto
    for (int i = 0; i < 1024; i++) {
        if (mock_ports[i] == target_port) return i;
    }
    return -1;
}

/* ========================================================================= */
/* 2. FRAMEWORK DE TESTING                                                   */
/* ========================================================================= */

int errores_test = 0;

void assert_cond(int condicion, const char* test_name) {
    if (condicion) {
        printf("  [\033[0;32mEXITO\033[0m] %s\n", test_name);
    } else {
        printf("  [\033[0;31mFALLO\033[0m] %s\n", test_name);
        errores_test++;
    }
}

// Imprime el outbox para ver qué intentó mandar el Patcher por la red
void debug_outbox(out_msg_t *outbox, int count) {
    for (int i = 0; i < count; i++) {
        printf("      -> [OUTBOX %d] FD: %2d | IP: %-15s | Port: %-5d | Msg: %s", 
               i, outbox[i].target_fd, outbox[i].target_ip, outbox[i].target_port, outbox[i].message);
    }
}

/* ========================================================================= */
/* 3. BUCLE PRINCIPAL Y TESTS EXHAUSTIVOS                                    */
/* ========================================================================= */

int main() {
    printf("==============================================================\n");
    printf(" INICIANDO SUPER TEST DE INTEGRACION: RESOURCE ADAPTER\n");
    printf("==============================================================\n");

    // Inicializamos un nodo con 4 CPU, 1 GPU y 8192 RAM
    ServerContext ctx;
    ctx.erlang_tcp_fd = 98; // FD falso para Erlang
    ctx.mynode = init_node(4, 1, 8192); 
    assert_cond(ctx.mynode != NULL, "Inicializacion del chasis (node_structures)");

    out_msg_t outbox[MAX_OUTBOX];
    int count = 0;

    /* -------------------------------------------------------------------------
       TEST A: EL CAMINO FELIZ (Creación y éxito total del Job)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST A: Camino Feliz (Erlang -> Red -> Erlang)\033[0m\n");
    
    // 1. Erlang pide recursos a dos nodos
    resource_adapter_patch(&ctx, "127.0.0.1", 99,
        "JOB_REQUEST 100 @192.168.1.10:8010:cpu:2 @192.168.1.11:8011:mem:1024", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 1 && strstr(outbox[0].message, "RESERVE 100 cpu 2"), "A1. Se envio el primer RESERVE");
    
    // 2. El primer nodo (192.168.1.10 en FD 10) dice que SI (GRANTED)
    mock_ports[10] = 8010; // Configuramos el mock
    resource_adapter_patch(&ctx, "192.168.1.10", 10, "GRANTED 100", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 1 && strstr(outbox[0].message, "RESERVE 100 mem 1024"), "A2. Recibe GRANTED parcial, envia RESERVE al 2do nodo");

    // 3. El segundo nodo (192.168.1.11 en FD 11) dice que SI (GRANTED)
    mock_ports[11] = 8011;
    resource_adapter_patch(&ctx, "192.168.1.11", 11, "GRANTED 100", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 1 && strstr(outbox[0].message, "JOB_GRANTED 100"), "A3. Job completo. Se avisa JOB_GRANTED a Erlang");
    assert_cond(outbox[0].target_fd == 99, "A4. El JOB_GRANTED se envio al FD correcto de Erlang");

    /* -------------------------------------------------------------------------
       TEST B: LIMPIEZA FINAL DEL CAMINO FELIZ (Erlang libera los recursos)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST B: Liberacion de un Job completado (JOB_RELEASE)\033[0m\n");
    
    resource_adapter_patch(&ctx, "127.0.0.1", 99, "JOB_RELEASE 100", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 2, "B1. Genero exactamente 2 mensajes de RELEASE");
    int rel_ok = (strstr(outbox[0].message, "RELEASE 100") && strstr(outbox[1].message, "RELEASE 100"));
    assert_cond(rel_ok, "B2. Los mensajes generados devuelven los recursos a los nodos");

    /* -------------------------------------------------------------------------
       TEST C: EL FALLO RUIDOSO (DENIED y Rollback Inmediato)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST C: Rechazo por DENIED y Rollback\033[0m\n");
    
    // Erlang pide el Job 200
    resource_adapter_patch(&ctx, "127.0.0.1", 99, "JOB_REQUEST 200 @192.168.1.10:8010:cpu:1 @192.168.1.12:8012:gpu:1", outbox, &count, ACTION_RESPOND);
    
    // Nodo 1 acepta
    resource_adapter_patch(&ctx, "192.168.1.10", 10, "GRANTED 200", outbox, &count, ACTION_RESPOND);
    
    // Nodo 2 RECHAZA (DENIED)
    mock_ports[12] = 8012;
    resource_adapter_patch(&ctx, "192.168.1.12", 12, "DENIED 200", outbox, &count, ACTION_RESPOND);
    
    debug_outbox(outbox, count);
    assert_cond(count == 2, "C1. Genero 2 mensajes (Rollback + Notificacion a Erlang)");
    assert_cond(strstr(outbox[0].message, "RELEASE 200 cpu 1"), "C2. Se genero el RELEASE para el nodo que habia dicho que si");
    assert_cond(strcmp(outbox[1].message, "JOB_DENIED 200\n") == 0 && outbox[1].target_fd == 99, "C3. Se informo JOB_DENIED a Erlang");

    /* -------------------------------------------------------------------------
       TEST D: EL FALLO SILENCIOSO TCP (Desconexión Instantánea)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST D: Tolerancia a caidas TCP (Rollback por Desconexion)\033[0m\n");
    
    // Erlang pide el Job 300
    resource_adapter_patch(&ctx, "127.0.0.1", 99, "JOB_REQUEST 300 @192.168.1.10:8010:cpu:1 @192.168.1.15:8015:ram:100", outbox, &count, ACTION_RESPOND);
    
    // Nodo 1 acepta
    resource_adapter_patch(&ctx, "192.168.1.10", 10, "GRANTED 300", outbox, &count, ACTION_RESPOND);
    
    // Nodo 2 (FD 15) SUFRE UN CORTE DE LUZ ANTES DE RESPONDER (Cae el TCP)
    mock_ports[15] = 8015;
    resource_adapter_patch(&ctx, "192.168.1.15", 15, NULL, outbox, &count, ACTION_DISCONNECTED);
    
    debug_outbox(outbox, count);
    assert_cond(count == 2, "D1. La desconexion TCP gatillo un Rollback instantaneo");
    assert_cond(strstr(outbox[0].message, "RELEASE 300 cpu 1"), "D2. Se devolvieron los recursos secuestrados al Nodo 1");

    /* -------------------------------------------------------------------------
       TEST E: PRESTAR RECURSOS LOCALES Y SISTEMA DE COLAS
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST E: Prestar recursos locales y manejo de colas\033[0m\n");
    
    // Nodo A (FD 20) pide 3 CPUs (Tenemos 4, asi que sobra)
    mock_ports[20] = 0; // Puerto 0 porque es una conexion entrante
    resource_adapter_patch(&ctx, "192.168.1.20", 20, "RESERVE 400 cpu 3", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 1 && strstr(outbox[0].message, "GRANTED 400"), "E1. Se presto 3 CPUs al Nodo A inmediatamente");

    // Nodo B (FD 21) pide 2 CPUs (Solo nos queda 1, asi que va a la COLA)
    mock_ports[21] = 0;
    resource_adapter_patch(&ctx, "192.168.1.21", 21, "RESERVE 401 cpu 2", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 0, "E2. No se envia respuesta al Nodo B (quedo encolado esperando recursos)");

    // Nodo A devuelve las 3 CPUs que tenia
    resource_adapter_patch(&ctx, "192.168.1.20", 20, "RELEASE 400 cpu 3", outbox, &count, ACTION_RESPOND);
    
    debug_outbox(outbox, count);
    assert_cond(count == 1 && strstr(outbox[0].message, "GRANTED 401"), "E3. Al liberar recursos, se desbloqueo al Nodo B de la cola automaticamente");
    assert_cond(outbox[0].target_fd == 21, "E4. El GRANTED se envio al FD del Nodo B");

    resource_adapter_patch(&ctx, "192.168.1.22", 22, "RESERVE 402 cpu 5", outbox, &count, ACTION_RESPOND);
    assert_cond(count == 1 && strcmp(outbox[0].message, "DENIED 402\n") == 0 && outbox[0].target_fd == 22, "E5. Una reserva imposible recibe DENIED");

    /* -------------------------------------------------------------------------
    TEST G: ERLANG CLIENT CRASH (Headless Node Rollback)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST G: Desconexion del Cliente Erlang (Nodo Huerfano)\033[0m\n");
    
    // 1. Un nuevo cliente Erlang (FD 99) pide el Job 500
    resource_adapter_patch(&ctx, "127.0.0.1", 99, 
        "JOB_REQUEST 500 @192.168.1.10:8010:cpu:2 @192.168.1.12:8012:ram:1024", outbox, &count, ACTION_RESPOND);
    
    // 2. El Nodo 1 (FD 10) responde rápido y acepta (GRANTED)
    mock_ports[10] = 8010;
    resource_adapter_patch(&ctx, "192.168.1.10", 10, "GRANTED 500", outbox, &count, ACTION_RESPOND);
    
    // 3. ERLANG CRASHEA (FD 99 se desconecta)
    resource_adapter_patch(&ctx, "127.0.0.1", 99, NULL, outbox, &count, ACTION_DISCONNECTED);
    
    assert_cond(count == 1, "G1. El crash de Erlang genero exactamente 1 mensaje de RELEASE");
    assert_cond(strstr(outbox[0].message, "RELEASE 500 cpu 2") && strcmp(outbox[0].target_ip, "192.168.1.10") == 0, "G2. Se libero correctamente la memoria secuestrada en el Nodo 1");

    unsigned owner_check = get_job_owner_socket(((node_data_t)ctx.mynode)->owned_jobs, 500);
    assert_cond(owner_check == 0, "G3. El Job 500 fue borrado completamente de la memoria interna (purga exitosa)");

    /* -------------------------------------------------------------------------
       TEST H: CRASH PREMATURO (Erlang muere sin tener recursos)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST H: Crash Prematuro (Erlang muere en peticion)\033[0m\n");
    
    // Erlang (FD 88) pide el Job 600
    resource_adapter_patch(&ctx, "127.0.0.1", 88, 
        "JOB_REQUEST 600 @192.168.1.10:8010:cpu:1", outbox, &count, ACTION_RESPOND);
    
    // Erlang crashea INMEDIATAMENTE ANTES de que el nodo .10 responda
    resource_adapter_patch(&ctx, "127.0.0.1", 88, NULL, outbox, &count, ACTION_DISCONNECTED);
    
    assert_cond(count == 0, "H1. Como no habia recursos secuestrados, no se envio ningun RELEASE a la red (0 trafico basura)");
    unsigned owner_check_600 = get_job_owner_socket(((node_data_t)ctx.mynode)->owned_jobs, 600);
    assert_cond(owner_check_600 == 0, "H2. El Job 600 fue purgado de la memoria para evitar deadlocks");

    /* -------------------------------------------------------------------------
       TEST I: MUERTE DEL PROVEEDOR (Erlang sigue vivo)
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST I: Muerte repentina de un nodo proveedor\033[0m\n");
    
    // Erlang (FD 77) pide el Job 700
    resource_adapter_patch(&ctx, "127.0.0.1", 77, 
        "JOB_REQUEST 700 @192.168.1.25:8025:gpu:1 @192.168.1.26:8026:ram:100", outbox, &count, ACTION_RESPOND);
    
    // El Nodo .25 (FD 25) acepta dar la GPU
    mock_ports[25] = 8025;
    resource_adapter_patch(&ctx, "192.168.1.25", 25, "GRANTED 700", outbox, &count, ACTION_RESPOND);
    
    // El Nodo .25 (FD 25) SE LE CORTA LA LUZ. Cae su TCP.
    resource_adapter_patch(&ctx, "192.168.1.25", 25, NULL, outbox, &count, ACTION_DISCONNECTED);
    
    assert_cond(count == 1, "I1. Se genero una accion compensatoria");
    assert_cond(strcmp(outbox[0].message, "JOB_DENIED 700\n") == 0 && outbox[0].target_fd == 77, "I2. Se informo JOB_DENIED al cliente Erlang para que sepa que la red fallo");

    /* -------------------------------------------------------------------------
       TEST F: LIMPIEZA TOTAL DE MEMORIA
       ------------------------------------------------------------------------- */
    printf("\n\033[1;36m>>> TEST F: Apagado del Servidor\033[0m\n");
    dest_node((node_data_t)ctx.mynode);
    assert_cond(1, "F1. Estructuras destruidas sin fugas de memoria");

    printf("\n==============================================================\n");
    if (errores_test == 0) {
        printf(" \033[0;32mEL ADAPTER ES A PRUEBA DE BALAS. TODOS LOS TESTS PASARON.\033[0m\n");
    } else {
        printf(" \033[0;31mFALLARON %d TESTS.\033[0m\n", errores_test);
    }
    printf("==============================================================\n");
    return 0;
}