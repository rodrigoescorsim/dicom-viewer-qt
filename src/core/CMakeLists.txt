add_library(dv_core STATIC
    Voi.hpp Voi.cpp
    GrayLut.hpp GrayLut.cpp
    # T03: Raster.hpp
    # T04: Orientation.hpp Orientation.cpp
    # T05: LoadError.hpp ImageDocument.hpp
)
target_include_directories(dv_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
set_target_properties(dv_core PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
dv_set_project_options(dv_core)
