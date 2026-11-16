# Copies the Qt runtime next to an executable after each build (Windows only), so it can be started
# from Explorer without touching PATH. The DCMTK DLLs are copied by the vcpkg toolchain.
function(dv_deploy_qt target)
    if(NOT WIN32)
        return()
    endif()
    find_program(DV_WINDEPLOYQT windeployqt HINTS "${DV_QT_BIN_DIR}" REQUIRED)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${DV_WINDEPLOYQT}" --translations pt_BR --no-system-d3d-compiler --no-opengl-sw
                --no-compiler-runtime "$<TARGET_FILE:${target}>"
        COMMENT "Deploying the Qt runtime for ${target}"
        VERBATIM)
endfunction()
