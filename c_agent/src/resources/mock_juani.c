#include "../include/resources/mock_resource_manager.h"
#include <stdio.h>

void master_function(node_data_t NODE, const char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action) {
    (void)NODE; (void)SOCKET; 
    
    if (outbox_count != NULL) {
        *outbox_count = 0; // Prevenir envíos de basura
    }

    if (BUFFER != NULL) {
        printf("[MOCK JUANI] Acción: %d | IP %s dice: %s\n", action, SENDER_IP, BUFFER);
    } else {
        printf("[MOCK JUANI] Acción: %d | Evento de red con IP %s (Sin msj)\n", action, SENDER_IP);
    }
}