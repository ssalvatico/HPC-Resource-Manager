#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h> // Para la función sleep()

// IMPORTANTE: Ajusta esta ruta según tu proyecto
#include "../../include/resources/known_nodes.h"

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
    printf(" INICIANDO TEST EXHAUSTIVO DE KNOWN NODES (YELLOW PAGES)\n");
    printf("==================================================\n");

    known_nodes_t nodes = create_known_nodes();
    assert_cond(nodes != NULL, "1. Directorio de Nodos creado exitosamente");

    /* =========================================================================
       TEST A: INSERCIÓN BÁSICA Y CONSTRUCCIÓN DE PAYLOAD
       ========================================================================= */
    printf("\n--> TEST A: Insercion de nuevos nodos\n");
    
    // Simulamos que llegaron dos ANNOUNCE por UDP
    update_known_node(nodes, "192.168.1.10", 8000, 4, 8192, 1);
    update_known_node(nodes, "192.168.1.11", 8001, 2, 4096, 0);

    char payload[512];
    get_known_nodes_payload(nodes, payload, sizeof(payload));
    printf("    Payload generado: %s\n", payload);

    // Verificamos que ambos esten en el payload
    int ok_10 = (strstr(payload, "192.168.1.10:8000:cpu:4:mem:8192:gpu:1") != NULL);
    int ok_11 = (strstr(payload, "192.168.1.11:8001:cpu:2:mem:4096:gpu:0") != NULL);
    assert_cond(ok_10 && ok_11, "2. Ambos nodos se registraron y aparecen en el payload");

    /* =========================================================================
       TEST B: ACTUALIZACIÓN Y "PISADO" DE DATOS
       ========================================================================= */
    printf("\n--> TEST B: Pisado de datos por un nuevo ANNOUNCE\n");
    
    // El nodo .10 consumió recursos y nos manda un nuevo ANNOUNCE
    update_known_node(nodes, "192.168.1.10", 8000, 1, 1024, 0); 
    
    // Generamos el payload de nuevo
    get_known_nodes_payload(nodes, payload, sizeof(payload));
    printf("    Payload tras update: %s\n", payload);

    int update_ok = (strstr(payload, "192.168.1.10:8000:cpu:1:mem:1024:gpu:0") != NULL);
    int no_duplicado = (strstr(payload, "cpu:4:mem:8192") == NULL); // El dato viejo debe desaparecer

    assert_cond(update_ok, "3. Los datos del Nodo .10 se actualizaron correctamente");
    assert_cond(no_duplicado, "4. No se generaron entradas duplicadas en la tabla hash");

    /* =========================================================================
       TEST C: RECOLECTOR DE BASURA (GARBAGE COLLECTION CON TIMEOUT)
       ========================================================================= */
    printf("\n--> TEST C: Garbage Collector (Deteccion de Nodos Caidos)\n");

    char * dead_ips[10];
    unsigned dead_ports[10];

    // Prueba temprana: si corremos el GC ahora, nadie deberia ser eliminado
    unsigned eliminados_pre = remove_inactive_nodes(nodes, dead_ips, dead_ports, 10);
    assert_cond(eliminados_pre == 0, "5. Sin timeout, el GC no elimina a ningun nodo");

    // Simulamos el paso del tiempo...
    printf("\n    [ATENCION] Simulando silencio de red...\n");
    printf("    Esperando 16 segundos para que los timestamps caduquen (Timeout > 15s)...\n");
    printf("    (Aprovecha para prepararte un mate 🧉)...\n");
    sleep(16);

    // Tras los 16s, simulamos que aparece un nodo totalmente NUEVO y fresco
    update_known_node(nodes, "192.168.1.12", 8002, 8, 16000, 2);

    // Ahora corremos el GC real
    unsigned eliminados_post = remove_inactive_nodes(nodes, dead_ips, dead_ports, 10);
    assert_cond(eliminados_post == 2, "6. El GC detecto y elimino exactamente a los 2 nodos viejos");

    // Validamos que los eliminados sean el .10 y el .11
    int match_10 = (strcmp(dead_ips[0], "192.168.1.10") == 0 || strcmp(dead_ips[1], "192.168.1.10") == 0);
    int match_11 = (strcmp(dead_ips[0], "192.168.1.11") == 0 || strcmp(dead_ips[1], "192.168.1.11") == 0);
    assert_cond(match_10 && match_11, "7. El GC extrajo las IPs y Puertos correctos para notificar al Patcher");

    // Como somos ordenados, liberamos los strings que el GC nos regalo para no tener memory leaks
    free(dead_ips[0]);
    free(dead_ips[1]);

    // Verificamos como quedo la tabla final
    get_known_nodes_payload(nodes, payload, sizeof(payload));
    printf("    Payload Final: %s\n", payload);
    
    int final_ok = (strstr(payload, "192.168.1.12") != NULL && strstr(payload, "192.168.1.10") == NULL);
    assert_cond(final_ok, "8. El directorio final refleja solo a los nodos vivos");

    /* =========================================================================
       TEST D: LIMPIEZA FINAL
       ========================================================================= */
    printf("\n--> TEST D: Limpieza de Memoria\n");
    delete_known_nodes(nodes);
    printf("[EXITO] 9. Estructuras destruidas\n");

    printf("\n==================================================\n");
    if (errores_test == 0) {
        printf(" \033[0;32mRESULTADO: TODOS LOS TESTS PASARON (0 Errores)\033[0m\n");
    } else {
        printf(" \033[0;31mRESULTADO: FALLARON %d TESTS. REVISA EL CODIGO.\033[0m\n", errores_test);
    }
    printf("==================================================\n");

    return 0;
}