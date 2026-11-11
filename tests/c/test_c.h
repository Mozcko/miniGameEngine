#ifndef TEST_C_H
#define TEST_C_H

#include <mge/c/mge_c.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond)                                                                                    \
    do {                                                                                                     \
        if (!(cond)) {                                                                                       \
            fprintf(stderr, "FALLO en %s:%d: %s\n", __FILE__, __LINE__, #cond);                              \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

#define TEST_ASSERT_EQ(a, b)                                                                                 \
    do {                                                                                                     \
        if ((a) != (b)) {                                                                                    \
            fprintf(stderr, "FALLO en %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);                       \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b)                                                                             \
    do {                                                                                                     \
        if (strcmp((a), (b)) != 0) {                                                                         \
            fprintf(stderr, "FALLO en %s:%d: '%s' != '%s'\n", __FILE__, __LINE__, (a), (b));                 \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

int test_c_lifecycle(void);
int test_c_ecs(void);
int test_c_input(void);
int test_c_audio(void);
int test_c_voxel(void);
int test_c_safety(void);

#endif /* TEST_C_H */
