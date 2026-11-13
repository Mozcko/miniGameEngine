import array
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


def test_whole_chunk_generator_bytes_layout():
    reg = mge.BlockRegistry()
    stone = reg.register_block("stone", mge.BlockDesc(is_solid=True, is_opaque=True))
    grass = reg.register_block("grass", mge.BlockDesc(is_solid=True, is_opaque=True))
    calls = 0

    def generate_chunk(cx, cy, cz):
        nonlocal calls
        calls += 1
        buf = bytearray(4096)
        # ChunkData layout: index = (y * 16 + z) * 16 + x (x fastest, then z, then y)
        buf[(5 * 16 + 3) * 16 + 7] = stone
        if cy == 0:
            buf[(1 * 16 + 2) * 16 + 4] = grass
        return bytes(buf)

    world = mge.VoxelWorld(reg, generate_chunk)
    assert world.get_block(7, 5, 3) == stone
    assert world.get_block(4, 1, 2) == grass
    assert world.get_block(0, 0, 0) == mge.BlockRegistry.AIR
    assert calls == 1


def test_whole_chunk_generator_uint16_array_and_method():
    reg = mge.BlockRegistry()
    ids = [reg.register_block(f"b_{i}", mge.BlockDesc(is_solid=True, is_opaque=True)) for i in range(300)]
    high_id = ids[290]

    class ChunkGenObj:
        def __init__(self):
            self.calls = 0

        def generate_chunk(self, cx, cy, cz):
            self.calls += 1
            arr = array.array("H", [0] * 4096)
            arr[(2 * 16 + 4) * 16 + 9] = high_id
            return arr

    gen_obj = ChunkGenObj()
    world = mge.VoxelWorld(reg, gen_obj)
    assert world.get_block(9, 2, 4) == high_id
    assert gen_obj.calls == 1


def test_whole_chunk_generator_invalid_size_raises():
    reg = mge.BlockRegistry()

    def bad_gen(cx, cy, cz):
        return bytes(100)

    world = mge.VoxelWorld(reg, bad_gen)
    with pytest.raises(mge.EngineError):
        world.load_chunk(0, 0, 0)
