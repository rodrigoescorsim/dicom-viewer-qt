# dv_add_test(<name> SOURCES <files...> LIBS <targets...>)
function(dv_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${name} PRIVATE DV_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")
    dv_set_project_options(${name})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    if(WIN32)
        # The tests are NOT deployed with windeployqt: they find Qt (including the offscreen platform
        # plugin) through PATH.
        set_property(TEST ${name} APPEND PROPERTY
                     ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DV_QT_BIN_DIR}")
    endif()
endfunction()

dv_add_test(tst_Voi SOURCES core/tst_Voi.cpp LIBS dv_core)
dv_add_test(tst_GrayLut SOURCES core/tst_GrayLut.cpp LIBS dv_core)
dv_add_test(tst_Raster SOURCES core/tst_Raster.cpp LIBS dv_core)
