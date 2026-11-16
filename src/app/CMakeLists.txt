qt_add_library(dv_app STATIC
    ErrorMessages.hpp ErrorMessages.cpp
    DocumentLoader.hpp DocumentLoader.cpp
    ImageViewport.hpp ImageViewport.cpp
    MainWindow.hpp MainWindow.cpp
    WindowLevelControls.hpp WindowLevelControls.cpp
)
target_link_libraries(dv_app PUBLIC Qt6::Widgets Qt6::Concurrent dv_dicom)
dv_set_project_options(dv_app)

qt_add_executable(dicom-viewer WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(dicom-viewer PRIVATE dv_app)
target_compile_definitions(dicom-viewer PRIVATE DV_VERSION="${PROJECT_VERSION}")
dv_set_project_options(dicom-viewer)
dv_deploy_qt(dicom-viewer)
