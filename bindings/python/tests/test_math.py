import math
import mge
import pytest

try:
    import numpy as np
    HAVE_NUMPY = True
except ImportError:
    HAVE_NUMPY = False


def test_vec2_operations():
    v1 = mge.Vec2(1.0, 2.0)
    v2 = mge.Vec2(3.0, 4.0)
    v3 = v1 + v2
    assert v3.x == pytest.approx(4.0)
    assert v3.y == pytest.approx(6.0)
    assert v3[0] == pytest.approx(4.0)
    assert v3[1] == pytest.approx(6.0)

    v3[0] = 10.0
    assert v3.x == pytest.approx(10.0)

    assert v1.dot(v2) == pytest.approx(11.0)
    assert v2.length() == pytest.approx(5.0)
    assert v2.norm() == pytest.approx(5.0)

    norm = v2.normalized()
    assert norm.length() == pytest.approx(1.0)


def test_vec3_operations():
    v1 = mge.Vec3(1.0, 2.0, 3.0)
    v2 = mge.Vec3(4.0, 5.0, 6.0)
    v3 = v1 + v2
    assert v3.x == pytest.approx(5.0)
    assert v3.y == pytest.approx(7.0)
    assert v3.z == pytest.approx(9.0)

    scaled = v1 * 2.0
    assert scaled.x == pytest.approx(2.0)
    assert scaled.y == pytest.approx(4.0)
    assert scaled.z == pytest.approx(6.0)

    cross = mge.Vec3(1.0, 0.0, 0.0).cross(mge.Vec3(0.0, 1.0, 0.0))
    assert cross.x == pytest.approx(0.0)
    assert cross.y == pytest.approx(0.0)
    assert cross.z == pytest.approx(1.0)


def test_vec4_operations():
    v = mge.Vec4(1.0, 2.0, 3.0, 4.0)
    assert v.x == pytest.approx(1.0)
    assert v.y == pytest.approx(2.0)
    assert v.z == pytest.approx(3.0)
    assert v.w == pytest.approx(4.0)
    assert len(v) == 4


def test_quat_operations():
    q = mge.Quat()
    assert q.w == pytest.approx(1.0)
    assert q.x == pytest.approx(0.0)
    assert q.y == pytest.approx(0.0)
    assert q.z == pytest.approx(0.0)

    axis = mge.Vec3(0.0, 1.0, 0.0)
    angle = math.pi / 2.0
    q_rot = mge.Quat.angle_axis(angle, axis)
    assert q_rot.length() == pytest.approx(1.0)


def test_mat4_operations():
    m = mge.Mat4.identity()
    pos = mge.Vec3(10.0, 20.0, 30.0)
    m_trans = mge.Mat4.translation(pos)
    m_scaled = mge.Mat4.scaling(mge.Vec3(2.0, 2.0, 2.0))

    combined = m_trans * m_scaled
    assert combined[0, 3] == pytest.approx(10.0)
    assert combined[1, 3] == pytest.approx(20.0)
    assert combined[2, 3] == pytest.approx(30.0)

    persp = mge.Mat4.perspective(math.radians(60.0), 16.0 / 9.0, 0.1, 100.0)
    assert persp[3, 2] == pytest.approx(-1.0)


def test_buffer_protocol():
    v = mge.Vec3(1.5, 2.5, 3.5)
    mv = memoryview(v)
    assert mv.format == 'f'
    assert mv.shape == (3,)
    assert mv[0] == pytest.approx(1.5)
    assert mv[1] == pytest.approx(2.5)
    assert mv[2] == pytest.approx(3.5)

    if HAVE_NUMPY:
        arr = np.asarray(v)
        assert arr.shape == (3,)
        assert arr.dtype == np.float32
        assert arr[0] == pytest.approx(1.5)

        m = mge.Mat4.identity()
        m_arr = np.asarray(m)
        assert m_arr.shape == (4, 4)
        assert m_arr.dtype == np.float32
        assert m_arr[0, 0] == pytest.approx(1.0)
