add_library(dv_dicom STATIC
    CodecRegistry.hpp CodecRegistry.cpp
    DicomLoader.hpp DicomLoader.cpp
)
target_link_libraries(dv_dicom
    PUBLIC dv_core
    PRIVATE DCMTK::dcmimage DCMTK::dcmimgle DCMTK::dcmjpeg DCMTK::dcmjpls DCMTK::dcmdata)
set_target_properties(dv_dicom PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
dv_set_project_options(dv_dicom)
