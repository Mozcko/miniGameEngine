#include "test_c.h"

int main(int argc, char** argv) {
    const char* filter = (argc > 1) ? argv[1] : NULL;

    if (!filter || strcmp(filter, "lifecycle") == 0) {
        printf("Corriendo test: c:lifecycle: ciclo de vida headless y timers\n");
        if (test_c_lifecycle() != 0) return 1;
    }

    if (!filter || strcmp(filter, "ecs") == 0) {
        printf("Corriendo test: c:ecs: registro dinamico de componentes y sistemas\n");
        if (test_c_ecs() != 0) return 1;
    }

    if (!filter || strcmp(filter, "input") == 0) {
        printf("Corriendo test: c:input: mapeo de acciones y consultas\n");
        if (test_c_input() != 0) return 1;
    }

    if (!filter || strcmp(filter, "audio") == 0) {
        printf("Corriendo test: c:audio: render offline y generacion de sfx\n");
        if (test_c_audio() != 0) return 1;
    }

    if (!filter || strcmp(filter, "voxel") == 0) {
        printf("Corriendo test: c:voxel: mundo, generador c y raycast\n");
        if (test_c_voxel() != 0) return 1;
    }

    if (!filter || strcmp(filter, "safety") == 0) {
        printf("Corriendo test: c:safety: liberacion segura de recursos gpu en shutdown\n");
        if (test_c_safety() != 0) return 1;
    }

    printf("Todos los tests de C pasaron con éxito.\n");
    return 0;
}
