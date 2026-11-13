#include "Common.h"

#include <cstdio>
#include <string>

namespace mge::python {

    namespace {

        // Buffer protocol para Vec2
        int vec2GetBuffer(PyObject* exporter, Py_buffer* view, int flags) {
            if (!view) return -1;
            auto* v = nanobind::inst_ptr<glm::vec2>(exporter);
            view->buf = glm::value_ptr(*v);
            view->obj = exporter;
            Py_INCREF(exporter);
            view->len = sizeof(float) * 2;
            view->readonly = 0;
            view->itemsize = sizeof(float);
            view->format = (flags & PyBUF_FORMAT) ? const_cast<char*>("f") : nullptr;
            view->ndim = 1;
            static Py_ssize_t s_shape[1] = {2};
            static Py_ssize_t s_strides[1] = {sizeof(float)};
            view->shape = s_shape;
            view->strides = s_strides;
            view->suboffsets = nullptr;
            view->internal = nullptr;
            return 0;
        }

        void vec2ReleaseBuffer(PyObject*, Py_buffer*) {}

        PyType_Slot s_vec2Slots[] = {
            {Py_bf_getbuffer, reinterpret_cast<void*>(vec2GetBuffer)},
            {Py_bf_releasebuffer, reinterpret_cast<void*>(vec2ReleaseBuffer)},
            {0, nullptr},
        };

        // Buffer protocol para Vec3
        int vec3GetBuffer(PyObject* exporter, Py_buffer* view, int flags) {
            if (!view) return -1;
            auto* v = nanobind::inst_ptr<glm::vec3>(exporter);
            view->buf = glm::value_ptr(*v);
            view->obj = exporter;
            Py_INCREF(exporter);
            view->len = sizeof(float) * 3;
            view->readonly = 0;
            view->itemsize = sizeof(float);
            view->format = (flags & PyBUF_FORMAT) ? const_cast<char*>("f") : nullptr;
            view->ndim = 1;
            static Py_ssize_t s_shape[1] = {3};
            static Py_ssize_t s_strides[1] = {sizeof(float)};
            view->shape = s_shape;
            view->strides = s_strides;
            view->suboffsets = nullptr;
            view->internal = nullptr;
            return 0;
        }

        void vec3ReleaseBuffer(PyObject*, Py_buffer*) {}

        PyType_Slot s_vec3Slots[] = {
            {Py_bf_getbuffer, reinterpret_cast<void*>(vec3GetBuffer)},
            {Py_bf_releasebuffer, reinterpret_cast<void*>(vec3ReleaseBuffer)},
            {0, nullptr},
        };

        // Buffer protocol para Vec4
        int vec4GetBuffer(PyObject* exporter, Py_buffer* view, int flags) {
            if (!view) return -1;
            auto* v = nanobind::inst_ptr<glm::vec4>(exporter);
            view->buf = glm::value_ptr(*v);
            view->obj = exporter;
            Py_INCREF(exporter);
            view->len = sizeof(float) * 4;
            view->readonly = 0;
            view->itemsize = sizeof(float);
            view->format = (flags & PyBUF_FORMAT) ? const_cast<char*>("f") : nullptr;
            view->ndim = 1;
            static Py_ssize_t s_shape[1] = {4};
            static Py_ssize_t s_strides[1] = {sizeof(float)};
            view->shape = s_shape;
            view->strides = s_strides;
            view->suboffsets = nullptr;
            view->internal = nullptr;
            return 0;
        }

        void vec4ReleaseBuffer(PyObject*, Py_buffer*) {}

        PyType_Slot s_vec4Slots[] = {
            {Py_bf_getbuffer, reinterpret_cast<void*>(vec4GetBuffer)},
            {Py_bf_releasebuffer, reinterpret_cast<void*>(vec4ReleaseBuffer)},
            {0, nullptr},
        };

        // Buffer protocol para Quat
        int quatGetBuffer(PyObject* exporter, Py_buffer* view, int flags) {
            if (!view) return -1;
            auto* q = nanobind::inst_ptr<glm::quat>(exporter);
            view->buf = glm::value_ptr(*q);
            view->obj = exporter;
            Py_INCREF(exporter);
            view->len = sizeof(float) * 4;
            view->readonly = 0;
            view->itemsize = sizeof(float);
            view->format = (flags & PyBUF_FORMAT) ? const_cast<char*>("f") : nullptr;
            view->ndim = 1;
            static Py_ssize_t s_shape[1] = {4};
            static Py_ssize_t s_strides[1] = {sizeof(float)};
            view->shape = s_shape;
            view->strides = s_strides;
            view->suboffsets = nullptr;
            view->internal = nullptr;
            return 0;
        }

        void quatReleaseBuffer(PyObject*, Py_buffer*) {}

        PyType_Slot s_quatSlots[] = {
            {Py_bf_getbuffer, reinterpret_cast<void*>(quatGetBuffer)},
            {Py_bf_releasebuffer, reinterpret_cast<void*>(quatReleaseBuffer)},
            {0, nullptr},
        };

        // Buffer protocol para Mat4
        int mat4GetBuffer(PyObject* exporter, Py_buffer* view, int flags) {
            if (!view) return -1;
            auto* m = nanobind::inst_ptr<glm::mat4>(exporter);
            view->buf = glm::value_ptr(*m);
            view->obj = exporter;
            Py_INCREF(exporter);
            view->len = sizeof(float) * 16;
            view->readonly = 0;
            view->itemsize = sizeof(float);
            view->format = (flags & PyBUF_FORMAT) ? const_cast<char*>("f") : nullptr;
            view->ndim = 2;
            static Py_ssize_t s_shape[2] = {4, 4};
            static Py_ssize_t s_strides[2] = {sizeof(float) * 4, sizeof(float)};
            view->shape = s_shape;
            view->strides = s_strides;
            view->suboffsets = nullptr;
            view->internal = nullptr;
            return 0;
        }

        void mat4ReleaseBuffer(PyObject*, Py_buffer*) {}

        PyType_Slot s_mat4Slots[] = {
            {Py_bf_getbuffer, reinterpret_cast<void*>(mat4GetBuffer)},
            {Py_bf_releasebuffer, reinterpret_cast<void*>(mat4ReleaseBuffer)},
            {0, nullptr},
        };

    } // namespace

    void initMath(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- Vec2 ---
        nb::class_<glm::vec2>(m, "Vec2", nb::type_slots(s_vec2Slots))
            .def(nb::init<>())
            .def(nb::init<float, float>(), nb::arg("x"), nb::arg("y"))
            .def("__init__",
                 [](glm::vec2* self, nb::sequence seq) {
                     if (nb::len(seq) != 2) {
                         throw nb::value_error("Vec2 sequence must have length 2");
                     }
                     new (self) glm::vec2(nb::cast<float>(seq[0]), nb::cast<float>(seq[1]));
                 })
            .def_rw("x", &glm::vec2::x)
            .def_rw("y", &glm::vec2::y)
            .def(nb::self + nb::self)
            .def(nb::self - nb::self)
            .def(nb::self * nb::self)
            .def(nb::self / nb::self)
            .def(nb::self * float())
            .def(float() * nb::self)
            .def(nb::self / float())
            .def(-nb::self)
            .def(nb::self == nb::self)
            .def("__getitem__",
                 [](const glm::vec2& v, size_t i) {
                     if (i >= 2) throw nb::index_error();
                     return v[static_cast<glm::length_t>(i)];
                 })
            .def("__setitem__",
                 [](glm::vec2& v, size_t i, float val) {
                     if (i >= 2) throw nb::index_error();
                     v[static_cast<glm::length_t>(i)] = val;
                 })
            .def("__len__", [](const glm::vec2&) { return 2; })
            .def("__repr__",
                 [](const glm::vec2& v) {
                     char buf[64];
                     std::snprintf(buf, sizeof(buf), "Vec2(%.3f, %.3f)", static_cast<double>(v.x),
                                   static_cast<double>(v.y));
                     return std::string(buf);
                 })
            .def("length", [](const glm::vec2& v) { return glm::length(v); })
            .def("norm", [](const glm::vec2& v) { return glm::length(v); })
            .def("length_squared", [](const glm::vec2& v) { return glm::dot(v, v); })
            .def("normalized",
                 [](const glm::vec2& v) {
                     const float len = glm::length(v);
                     return len > 0.00001f ? (v / len) : glm::vec2(0.0f);
                 })
            .def("dot", [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); });

        // --- Vec3 ---
        nb::class_<glm::vec3>(m, "Vec3", nb::type_slots(s_vec3Slots))
            .def(nb::init<>())
            .def(nb::init<float, float, float>(), nb::arg("x"), nb::arg("y"), nb::arg("z"))
            .def("__init__",
                 [](glm::vec3* self, nb::sequence seq) {
                     if (nb::len(seq) != 3) {
                         throw nb::value_error("Vec3 sequence must have length 3");
                     }
                     new (self)
                         glm::vec3(nb::cast<float>(seq[0]), nb::cast<float>(seq[1]), nb::cast<float>(seq[2]));
                 })
            .def_rw("x", &glm::vec3::x)
            .def_rw("y", &glm::vec3::y)
            .def_rw("z", &glm::vec3::z)
            .def(nb::self + nb::self)
            .def(nb::self - nb::self)
            .def(nb::self * nb::self)
            .def(nb::self / nb::self)
            .def(nb::self * float())
            .def(float() * nb::self)
            .def(nb::self / float())
            .def(-nb::self)
            .def(nb::self == nb::self)
            .def("__getitem__",
                 [](const glm::vec3& v, size_t i) {
                     if (i >= 3) throw nb::index_error();
                     return v[static_cast<glm::length_t>(i)];
                 })
            .def("__setitem__",
                 [](glm::vec3& v, size_t i, float val) {
                     if (i >= 3) throw nb::index_error();
                     v[static_cast<glm::length_t>(i)] = val;
                 })
            .def("__len__", [](const glm::vec3&) { return 3; })
            .def("__repr__",
                 [](const glm::vec3& v) {
                     char buf[64];
                     std::snprintf(buf, sizeof(buf), "Vec3(%.3f, %.3f, %.3f)", static_cast<double>(v.x),
                                   static_cast<double>(v.y), static_cast<double>(v.z));
                     return std::string(buf);
                 })
            .def("length", [](const glm::vec3& v) { return glm::length(v); })
            .def("norm", [](const glm::vec3& v) { return glm::length(v); })
            .def("length_squared", [](const glm::vec3& v) { return glm::dot(v, v); })
            .def("normalized",
                 [](const glm::vec3& v) {
                     const float len = glm::length(v);
                     return len > 0.00001f ? (v / len) : glm::vec3(0.0f);
                 })
            .def("dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); })
            .def("cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); });

        // --- Vec4 ---
        nb::class_<glm::vec4>(m, "Vec4", nb::type_slots(s_vec4Slots))
            .def(nb::init<>())
            .def(nb::init<float, float, float, float>(), nb::arg("x"), nb::arg("y"), nb::arg("z"),
                 nb::arg("w"))
            .def("__init__",
                 [](glm::vec4* self, nb::sequence seq) {
                     if (nb::len(seq) != 4) {
                         throw nb::value_error("Vec4 sequence must have length 4");
                     }
                     new (self) glm::vec4(nb::cast<float>(seq[0]), nb::cast<float>(seq[1]),
                                          nb::cast<float>(seq[2]), nb::cast<float>(seq[3]));
                 })
            .def_rw("x", &glm::vec4::x)
            .def_rw("y", &glm::vec4::y)
            .def_rw("z", &glm::vec4::z)
            .def_rw("w", &glm::vec4::w)
            .def(nb::self + nb::self)
            .def(nb::self - nb::self)
            .def(nb::self * nb::self)
            .def(nb::self / nb::self)
            .def(nb::self * float())
            .def(float() * nb::self)
            .def(nb::self / float())
            .def(-nb::self)
            .def(nb::self == nb::self)
            .def("__getitem__",
                 [](const glm::vec4& v, size_t i) {
                     if (i >= 4) throw nb::index_error();
                     return v[static_cast<glm::length_t>(i)];
                 })
            .def("__setitem__",
                 [](glm::vec4& v, size_t i, float val) {
                     if (i >= 4) throw nb::index_error();
                     v[static_cast<glm::length_t>(i)] = val;
                 })
            .def("__len__", [](const glm::vec4&) { return 4; })
            .def("__repr__",
                 [](const glm::vec4& v) {
                     char buf[64];
                     std::snprintf(buf, sizeof(buf), "Vec4(%.3f, %.3f, %.3f, %.3f)", static_cast<double>(v.x),
                                   static_cast<double>(v.y), static_cast<double>(v.z),
                                   static_cast<double>(v.w));
                     return std::string(buf);
                 })
            .def("length", [](const glm::vec4& v) { return glm::length(v); })
            .def("norm", [](const glm::vec4& v) { return glm::length(v); })
            .def("normalized",
                 [](const glm::vec4& v) {
                     const float len = glm::length(v);
                     return len > 0.00001f ? (v / len) : glm::vec4(0.0f);
                 })
            .def("dot", [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); });

        // --- Quat ---
        nb::class_<glm::quat>(m, "Quat", nb::type_slots(s_quatSlots))
            .def(
                "__init__",
                [](glm::quat* self, float x, float y, float z, float w) { new (self) glm::quat(w, x, y, z); },
                nb::arg("x") = 0.0f, nb::arg("y") = 0.0f, nb::arg("z") = 0.0f, nb::arg("w") = 1.0f)
            .def("__init__",
                 [](glm::quat* self, nb::sequence seq) {
                     if (nb::len(seq) != 4) {
                         throw nb::value_error("Quat sequence must have length 4 [x, y, z, w]");
                     }
                     new (self) glm::quat(nb::cast<float>(seq[3]), nb::cast<float>(seq[0]),
                                          nb::cast<float>(seq[1]), nb::cast<float>(seq[2]));
                 })
            .def_rw("x", &glm::quat::x)
            .def_rw("y", &glm::quat::y)
            .def_rw("z", &glm::quat::z)
            .def_rw("w", &glm::quat::w)
            .def_static("identity", []() { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); })
            .def_static("from_axis_angle",
                        [](const glm::vec3& axis, float angleRad) {
                            return glm::angleAxis(angleRad, glm::normalize(axis));
                        })
            .def_static("angle_axis",
                        [](float angleRad, const glm::vec3& axis) {
                            return glm::angleAxis(angleRad, glm::normalize(axis));
                        })
            .def_static("from_euler", [](float pitch, float yaw,
                                         float roll) { return glm::quat(glm::vec3(pitch, yaw, roll)); })
            .def(nb::self * nb::self)
            .def(nb::self * glm::vec3())
            .def(nb::self == nb::self)
            .def("__getitem__",
                 [](const glm::quat& q, size_t i) {
                     if (i == 0) return q.x;
                     if (i == 1) return q.y;
                     if (i == 2) return q.z;
                     if (i == 3) return q.w;
                     throw nb::index_error();
                 })
            .def("__len__", [](const glm::quat&) { return 4; })
            .def("__repr__",
                 [](const glm::quat& q) {
                     char buf[64];
                     std::snprintf(buf, sizeof(buf), "Quat(x=%.3f, y=%.3f, z=%.3f, w=%.3f)",
                                   static_cast<double>(q.x), static_cast<double>(q.y),
                                   static_cast<double>(q.z), static_cast<double>(q.w));
                     return std::string(buf);
                 })
            .def("length", [](const glm::quat& q) { return glm::length(q); })
            .def("norm", [](const glm::quat& q) { return glm::length(q); })
            .def("normalized", [](const glm::quat& q) { return glm::normalize(q); })
            .def("inverse", [](const glm::quat& q) { return glm::inverse(q); })
            .def("conjugate", [](const glm::quat& q) { return glm::conjugate(q); })
            .def("to_euler", [](const glm::quat& q) { return glm::eulerAngles(q); });

        // --- Mat4 ---
        nb::class_<glm::mat4>(m, "Mat4", nb::type_slots(s_mat4Slots))
            .def(nb::init<>())
            .def("__init__",
                 [](glm::mat4* self, nb::sequence seq) {
                     if (nb::len(seq) != 16) {
                         throw nb::value_error("Mat4 sequence must have length 16");
                     }
                     float values[16];
                     for (size_t i = 0; i < 16; ++i) {
                         values[i] = nb::cast<float>(seq[i]);
                     }
                     new (self) glm::mat4(glm::make_mat4(values));
                 })
            .def_static("identity", []() { return glm::mat4(1.0f); })
            .def_static("translate",
                        [](const glm::vec3& pos) { return glm::translate(glm::mat4(1.0f), pos); })
            .def_static("translation",
                        [](const glm::vec3& pos) { return glm::translate(glm::mat4(1.0f), pos); })
            .def_static("scale", [](const glm::vec3& s) { return glm::scale(glm::mat4(1.0f), s); })
            .def_static("scaling", [](const glm::vec3& s) { return glm::scale(glm::mat4(1.0f), s); })
            .def_static("rotate",
                        [](float angleRad, const glm::vec3& axis) {
                            return glm::rotate(glm::mat4(1.0f), angleRad, glm::normalize(axis));
                        })
            .def_static("rotation",
                        [](float angleRad, const glm::vec3& axis) {
                            return glm::rotate(glm::mat4(1.0f), angleRad, glm::normalize(axis));
                        })
            .def_static("perspective",
                        [](float fovRad, float aspect, float nearPlane, float farPlane) {
                            return glm::perspective(fovRad, aspect, nearPlane, farPlane);
                        })
            .def_static("look_at", [](const glm::vec3& eye, const glm::vec3& center,
                                      const glm::vec3& up) { return glm::lookAt(eye, center, up); })
            .def(nb::self * nb::self)
            .def(nb::self * glm::vec4())
            .def("__matmul__", [](const glm::mat4& a, const glm::mat4& b) { return a * b; })
            .def("__matmul__", [](const glm::mat4& a, const glm::vec4& b) { return a * b; })
            .def(nb::self == nb::self)
            .def("__getitem__",
                 [](const glm::mat4& m4, nb::object key) -> float {
                     if (nb::isinstance<nb::tuple>(key)) {
                         auto tup = nb::cast<nb::tuple>(key);
                         if (nb::len(tup) != 2) throw nb::index_error();
                         size_t row = nb::cast<size_t>(tup[0]);
                         size_t col = nb::cast<size_t>(tup[1]);
                         if (row >= 4 || col >= 4) throw nb::index_error();
                         return m4[static_cast<glm::length_t>(col)][static_cast<glm::length_t>(row)];
                     }
                     size_t idx = nb::cast<size_t>(key);
                     if (idx >= 16) throw nb::index_error();
                     return glm::value_ptr(m4)[idx];
                 })
            .def("__repr__",
                 [](const glm::mat4& m4) {
                     const float* p = glm::value_ptr(m4);
                     char buf[256];
                     std::snprintf(
                         buf, sizeof(buf),
                         "Mat4([\n"
                         "  [%.2f, %.2f, %.2f, %.2f],\n"
                         "  [%.2f, %.2f, %.2f, %.2f],\n"
                         "  [%.2f, %.2f, %.2f, %.2f],\n"
                         "  [%.2f, %.2f, %.2f, %.2f]\n])",
                         static_cast<double>(p[0]), static_cast<double>(p[4]), static_cast<double>(p[8]),
                         static_cast<double>(p[12]), static_cast<double>(p[1]), static_cast<double>(p[5]),
                         static_cast<double>(p[9]), static_cast<double>(p[13]), static_cast<double>(p[2]),
                         static_cast<double>(p[6]), static_cast<double>(p[10]), static_cast<double>(p[14]),
                         static_cast<double>(p[3]), static_cast<double>(p[7]), static_cast<double>(p[11]),
                         static_cast<double>(p[15]));
                     return std::string(buf);
                 })
            .def("inverse", [](const glm::mat4& m4) { return glm::inverse(m4); })
            .def("transpose", [](const glm::mat4& m4) { return glm::transpose(m4); });
    }

} // namespace mge::python
