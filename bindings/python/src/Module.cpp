#include "Common.h"

namespace mge::python {

    void registerEngine(nanobind::module_& m) {
        namespace nb = nanobind;

        // Excepción de runtime del motor
        nb::exception<EngineError>(m, "EngineError", PyExc_RuntimeError);

        initMath(m);
        initEcs(m);
        initRender(m);
        initAudio(m);
        initVoxel(m);
        initApp(m);
    }

} // namespace mge::python

NB_MODULE(_mge, m) {
    m.doc() = "miniGameEngine Python bindings via nanobind";
    mge::python::registerEngine(m);
}
