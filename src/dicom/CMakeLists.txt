# Header-only until T06 adds the DICOM loader (then it becomes a STATIC library linked to DCMTK).
add_library(dv_dicom INTERFACE)
target_include_directories(dv_dicom INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(dv_dicom INTERFACE dv_core)
