#include "test_c.h"

static mge_block_id c_terrain_generator(int32_t x, int32_t y, int32_t z, void* user_data) {
    (void)x;
    (void)z;
    (void)user_data;
    // Capa de piedra en y <= 3, aire en y > 3
    if (y <= 3) return 1;
    return MGE_BLOCK_AIR;
}

int test_c_voxel(void) {
    // 1. Registro de bloques
    mge_block_registry* reg = mge_block_registry_create(NULL);
    TEST_ASSERT(reg != NULL);

    mge_block_desc stoneDesc = {"Stone", true, true, {0.5f, 0.5f, 0.5f, 1.0f}};
    mge_block_id stoneId = mge_block_register(reg, &stoneDesc);
    TEST_ASSERT_EQ(stoneId, 1);

    mge_block_desc dirtDesc = {"Dirt", true, true, {0.4f, 0.2f, 0.1f, 1.0f}};
    mge_block_id dirtId = mge_block_register(reg, &dirtDesc);
    TEST_ASSERT_EQ(dirtId, 2);

    // 2. Mundo voxel con callback en C
    mge_voxel_world* world = mge_voxel_world_create(NULL, reg, c_terrain_generator, NULL);
    TEST_ASSERT(world != NULL);

    // Bloque en (0, 2, 0) debe ser Stone
    TEST_ASSERT_EQ(mge_voxel_world_get_block(world, 0, 2, 0), stoneId);

    // Bloque en (0, 8, 0) debe ser Aire
    TEST_ASSERT_EQ(mge_voxel_world_get_block(world, 0, 8, 0), MGE_BLOCK_AIR);

    // Modificar bloque
    TEST_ASSERT(mge_voxel_world_set_block(world, 0, 8, 0, dirtId));
    TEST_ASSERT_EQ(mge_voxel_world_get_block(world, 0, 8, 0), dirtId);

    // 3. Raycast: hacia abajo desde (0.5, 15.0, 0.5) hacia -Y
    float origin[3] = {0.5f, 15.0f, 0.5f};
    float dirDown[3] = {0.0f, -1.0f, 0.0f};
    mge_voxel_raycast_hit hit;

    bool didHit = mge_voxel_world_raycast(world, origin, dirDown, 20.0f, &hit);
    TEST_ASSERT(didHit);
    // Debe impactar el bloque Dirt en y = 8
    TEST_ASSERT_EQ(hit.block[0], 0);
    TEST_ASSERT_EQ(hit.block[1], 8);
    TEST_ASSERT_EQ(hit.block[2], 0);
    TEST_ASSERT_EQ(hit.block_id, dirtId);
    TEST_ASSERT_EQ(hit.normal[1], 1); // Cara superior (+Y)

    // Raycast hacia arriba (sin impacto)
    float dirUp[3] = {0.0f, 1.0f, 0.0f};
    bool hitAir = mge_voxel_world_raycast(world, origin, dirUp, 20.0f, &hit);
    TEST_ASSERT(!hitAir);

    mge_voxel_world_destroy(world);
    mge_block_registry_destroy(reg);

    return 0;
}
