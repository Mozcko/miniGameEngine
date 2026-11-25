#include "test_c.h"

int main(int argc, char** argv) {
    const char* filter = (argc > 1) ? argv[1] : NULL;

    if (!filter || strcmp(filter, "lifecycle") == 0) {
        printf("Running test: c:lifecycle: headless lifecycle and timers\n");
        if (test_c_lifecycle() != 0) return 1;
    }

    if (!filter || strcmp(filter, "ecs") == 0) {
        printf("Running test: c:ecs: dynamic component and system registration\n");
        if (test_c_ecs() != 0) return 1;
    }

    if (!filter || strcmp(filter, "input") == 0) {
        printf("Running test: c:input: action mapping and queries\n");
        if (test_c_input() != 0) return 1;
    }

    if (!filter || strcmp(filter, "audio") == 0) {
        printf("Running test: c:audio: offline rendering and SFX generation\n");
        if (test_c_audio() != 0) return 1;
    }

    if (!filter || strcmp(filter, "voxel") == 0) {
        printf("Running test: c:voxel: world, C generator, and raycast\n");
        if (test_c_voxel() != 0) return 1;
    }

    if (!filter || strcmp(filter, "safety") == 0) {
        printf("Running test: c:safety: safe GPU resource release on shutdown\n");
        if (test_c_safety() != 0) return 1;
    }

    printf("All C tests passed successfully.\n");
    return 0;
}
