import mge
import pytest


def test_block_registry():
    reg = mge.BlockRegistry()
    assert len(reg) == 1  # Air is pre-registered as id 0
    assert not reg.is_solid(mge.BlockRegistry.AIR)

    stone = reg.register_block("stone", mge.BlockDesc(is_solid=True, is_opaque=True))
    assert stone > 0
    assert reg.name(stone) == "stone"
    assert reg.is_solid(stone)
    assert reg.is_opaque(stone)
    assert reg.find("stone") == stone
    assert reg.find("nonexistent") is None


def test_voxel_world_and_generator():
    reg = mge.BlockRegistry()
    dirt = reg.register_block("dirt", mge.BlockDesc(is_solid=True, is_opaque=True))
    grass = reg.register_block("grass", mge.BlockDesc(is_solid=True, is_opaque=True))

    def terrain_gen(x, y, z):
        if y < -1:
            return dirt
        elif y == -1:
            return grass
        return mge.BlockRegistry.AIR

    world = mge.VoxelWorld(reg, terrain_gen)
    assert world.get_block(0, -1, 0) == grass
    assert world.get_block(0, -5, 0) == dirt
    assert world.get_block(0, 5, 0) == mge.BlockRegistry.AIR

    world.set_block(0, 5, 0, dirt)
    assert world.get_block(0, 5, 0) == dirt


def test_voxel_raycast():
    reg = mge.BlockRegistry()
    stone = reg.register_block("stone", mge.BlockDesc(is_solid=True, is_opaque=True))

    def terrain_gen(x, y, z):
        return stone if y < 0 else mge.BlockRegistry.AIR

    world = mge.VoxelWorld(reg, terrain_gen)

    # Cast from (0.5, 5.0, 0.5) downwards
    hit = world.raycast(mge.Vec3(0.5, 5.0, 0.5), mge.Vec3(0.0, -1.0, 0.0), max_dist=20.0)
    assert hit.hit
    assert hit.block_id == stone
    assert hit.block_pos == (0, -1, 0)
    assert hit.normal == (0, 1, 0)
    assert hit.distance == pytest.approx(5.0)

    # Cast upwards into air -> miss
    miss = world.raycast(mge.Vec3(0.5, 5.0, 0.5), mge.Vec3(0.0, 1.0, 0.0), max_dist=20.0)
    assert not miss.hit
