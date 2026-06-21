#include "../include/resources/mock_resource_manager.h"
#include "../include/comms/server_types.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// NOTA PARA JUANI: 
// Debes tener tus propios locks para proteger tus estructuras internas.
// La capa de red llama a esta función desde múltiples hilos a la vez.
// extern pthread_rwlock_t juani_internal_lock;

void master_function(
    node_data_t NODE, 
    const char * SENDER_IP, 
    unsigned SOCKET, 
    const char * BUFFER, 
    out_msg_t * outbox,       
    int * outbox_count,
    JuaniAction action
) {
    // 1. REGLA DE ORO: Siempre inicializar la bandeja de salida en 0 para evitar enviar memoria basura.
    if (outbox_count != NULL) {
        *outbox_count = 0; 
    }

    // 2. Evaluamos qué evento de red nos acaba de despertar
    switch (action) {

        // =================================================================================
        // EVENTO 1: LLEGÓ UN PAQUETE UDP DE UN NODO (Descubrimiento)
        // =================================================================================
        case ACTION_NEW_NODE_DISCOVERED:
            /* * CONTEXTO: Alguien gritó "¡Aquí estoy!" en la red local.
             * SENDER_IP: La IP de quien gritó.
             * SOCKET: Ignorar (-1).
             * BUFFER: Contiene el string "ANNOUNCE IP PUERTO cpu:X ram:Y"
             * * TAREAS PARA JUANI:
             * [ ] Extraer el puerto y los recursos del BUFFER usando sscanf().
             * [ ] pthread_rwlock_wrlock(&tu_lock).
             * [ ] Buscar si la IP ya está en tu tabla de nodos vivos.
             * [ ] Si NO ESTÁ -> AGREGAR NODO a la tabla.
             * [ ] Si YA ESTÁ -> ACTUALIZAR sus recursos disponibles.
             * [ ] ACTUALIZAR EL RELOJ: nodo.last_seen = time(NULL); (¡Crucial para el GC!)
             * [ ] pthread_rwlock_unlock(&tu_lock).
             */
            
            // Ejemplo de uso:
            // printf("[JUANI] Registrando/Actualizando nodo %s en tabla.\n", SENDER_IP);
            break;


        // =================================================================================
        // EVENTO 2: ES HORA DE ANUNCIAR NUESTROS RECURSOS (Timer UDP)
        // =================================================================================
        case ACTION_GET_RESOURCES:
            /*
             * CONTEXTO: El timer de 1 segundo sonó. Nos toca gritar a la red local.
             * * TAREAS PARA JUANI:
             * [ ] pthread_rwlock_rdlock(&tu_lock).
             * [ ] Leer cuántos recursos LOCALES tienes disponibles actualmente.
             * [ ] pthread_rwlock_unlock(&tu_lock).
             * [ ] Formatear un string con la norma: "ANNOUNCE IP PUERTO cpu:X ram:Y".
             * [ ] Guardar ese string en outbox[0].message.
             * [ ] *outbox_count = 1;
             */
            
            // Ejemplo:
            // sprintf(outbox[0].message, "ANNOUNCE %s %d cpu:4 ram:16\n", MI_IP, MI_PUERTO);
            // *outbox_count = 1;
            break;


        // =================================================================================
        // EVENTO 3: LIMPIAR LA BASURA (Timer del Garbage Collector)
        // =================================================================================
        case ACTION_CHECK_DEADNODES:
            /*
             * CONTEXTO: El timer de 5 segundos sonó. Hay que ver quién se murió en silencio.
             * * TAREAS PARA JUANI:
             * [ ] time_t now = time(NULL);
             * [ ] pthread_rwlock_wrlock(&tu_lock).
             * [ ] Iterar sobre toda la tabla de nodos conocidos.
             * [ ] Si difftime(now, nodo.last_seen) > 15.0 segundos -> ¡EL NODO MURIÓ!
             * [ ] ELIMINAR NODO de la tabla de vivos.
             * [ ] Revisar tabla de Jobs Activos: ¿Teníamos un job ejecutándose en ese nodo?
             * - Si es así: MARCAR JOB COMO FALLIDO. 
             * - (Opcional) Armar un "JOB_TIMEOUT" en el outbox para avisarle al FD de Erlang.
             * [ ] pthread_rwlock_unlock(&tu_lock).
             */
             break;


        // =================================================================================
        // EVENTO 4: ALGUIEN SE DESCONECTÓ DE FORMA BRUSCA (Fallo de Red TCP)
        // =================================================================================
        case ACTION_DISCONNECTED:
            /*
             * CONTEXTO: Estábamos hablando con un nodo/Erlang (o intentando conectar) y se cortó el cable.
             * SENDER_IP: La IP del nodo que se acaba de desconectar o fallar.
             * SOCKET: El FD que murió (muy útil si es el de Erlang).
             * * TAREAS PARA JUANI:
             * [ ] pthread_rwlock_wrlock(&tu_lock).
             * [ ] Si SENDER_IP era un nodo del cluster que tenía nuestros recursos:
             * - LIBERAR RECURSOS: Descontar lo que le habíamos prestado.
             * - SACAR DE COLA: Borrar cualquier petición pendiente que tuviera ese SOCKET.
             * [ ] Si el SOCKET era de Erlang:
             * - Cancelar las solicitudes de ese planificador.
             * [ ] pthread_rwlock_unlock(&tu_lock).
             */
             
            // printf("[JUANI] El FD %d (IP: %s) murió. Limpiando sus rastros...\n", SOCKET, SENDER_IP);
            break;


        // =================================================================================
        // EVENTO 5: LLEGÓ UN MENSAJE TCP (El núcleo de la lógica de recursos)
        // =================================================================================
        case ACTION_RESPOND:
            /*
             * CONTEXTO: Alguien (Erlang u otro Nodo) nos envió un comando por TCP.
             * BUFFER: Contiene el string exacto (Ej: "RESERVE job123 cpu 2", "RELEASE...", "GRANTED...")
             * SOCKET: El identificador único del tubo por el que nos hablan.
             * * TAREAS PARA JUANI:
             * [ ] Hacer un parsing del comando en BUFFER (ej. usando strncmp y sscanf).
             * * SUB-CASO A: "RESERVE ..." (Un nodo/Erlang nos pide recursos)
             * [ ] Lock()
             * [ ] ¿Hay recursos locales disponibles?
             * - SÍ: Restar recursos -> Armar "GRANTED" en outbox -> target_fd = SOCKET.
             * - NO: Guardar el pedido en tu COLA FIFO interna (guardar el SOCKET, job_id y cantidad).
             * No responder nada (*outbox_count = 0).
             * [ ] Unlock()
             * * SUB-CASO B: "RELEASE ..." (Un nodo/Erlang nos devuelve recursos)
             * [ ] Lock()
             * [ ] Sumar recursos locales (Liberarlos).
             * [ ] Mirar la COLA FIFO: ¿Alguien estaba esperando esto?
             * - SÍ: DESENCOLAR PEDIDO -> Restar recursos -> Armar "GRANTED" en outbox -> 
             * target_fd = [EL SOCKET QUE ESTABA GUARDADO EN LA COLA].
             * [ ] Unlock()
             * * SUB-CASO C: "JOB_REQUEST ..." (Erlang local pide que consigamos recursos del cluster)
             * [ ] Lock()
             * [ ] Mirar tabla de nodos vivos: ¿Quién tiene recursos suficientes?
             * [ ] Elegir un nodo (ej. IP 192.168.1.55, Puerto 8100).
             * [ ] Armar "RESERVE ..." en el outbox.
             * [ ] outbox[0].target_fd = -1; (Para que la capa de red abra una nueva conexión si hace falta).
             * [ ] strcpy(outbox[0].target_ip, "192.168.1.55");
             * [ ] outbox[0].target_port = 8100;
             * [ ] Unlock()
             * * SUB-CASO D: "GRANTED ..." / "DENIED ..." (Respuesta de otro nodo)
             * [ ] Actualizar el estado del trabajo local.
             * [ ] Informar a Erlang armando la respuesta en el outbox apuntando al FD de Erlang.
             */

            // --- EJEMPLO BÁSICO DE CÓMO LLENAR EL OUTBOX ---
            /*
            if (strncmp(BUFFER, "PING", 4) == 0) {
                // Queremos responder PONG por el mismo tubo al instante
                strcpy(outbox[*outbox_count].message, "PONG\n");
                outbox[*outbox_count].target_fd = SOCKET; // Respuesta directa
                (*outbox_count)++;
            }
            */
            break;

        case ACTION_NONE:
        default:
            break;
    }
}