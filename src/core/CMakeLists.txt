# Header-only until T02 adds the first source file (then it becomes a STATIC library).
add_library(dv_core INTERFACE)
target_include_directories(dv_core INTERFACE "${PROJECT_SOURCE_DIR}/src")
