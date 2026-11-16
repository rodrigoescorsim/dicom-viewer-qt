# Compiler options for the project's own targets.
function(dv_set_project_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
        if(DV_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        # Designated initializers may omit members that have default values.
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
                                                 -Wno-missing-field-initializers)
        if(DV_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
