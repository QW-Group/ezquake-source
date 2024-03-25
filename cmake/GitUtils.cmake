find_package(Git QUIET)

function(git_refresh_submodules)
    if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if (GIT_SUBMODULE)
            message(STATUS "Submodule update")
            execute_process(
                    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT
                    OUTPUT_QUIET
            )
            if (NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
            endif()
        endif()
    endif()
endfunction()

# Will load the version from 'version.json' file on configuration time if it exists.
#
# The content of this file looks like this:
# {
#   "version": "3.6.5-92-g8e3875f40",
#   "revision": 7739,
#   "commit": "595806cd2449d4b17024b892c6e5b169512be5e0",
#   "date": "2024-08-18T16:53:29+02:00",
#   "vcpkg": "2024-02-14"
# }
function(git_extract_version target_var)
    if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
                OUTPUT_VARIABLE GIT_REVISION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                OUTPUT_VARIABLE GIT_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags --always
                OUTPUT_VARIABLE GIT_DESCRIBE
                RESULT_VARIABLE GIT_DESCRIBE_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/version.json")
        message("-- Loading version from 'version.json'")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" VERSION_CONTENT)
        string(JSON GIT_DESCRIBE GET "${VERSION_CONTENT}" "version")
        string(JSON GIT_REVISION GET "${VERSION_CONTENT}" "revision")
        string(JSON GIT_COMMIT_HASH GET "${VERSION_CONTENT}" "commit")
    endif()

    if (NOT GIT_DESCRIBE)
        set(GIT_DESCRIBE "0.0.0-0-g00000000")
    endif()

    if (NOT GIT_REVISION)
        set(GIT_REVISION "0")
    endif()

    if (NOT GIT_COMMIT_HASH)
        set(GIT_COMMIT_HASH "0000000000000000000000000000000000000000")
    endif()

    string(SUBSTRING ${GIT_COMMIT_HASH} 0 9 GIT_COMMIT_SHORT_HASH)

    add_library(${target_var} INTERFACE)
    target_compile_definitions(${target_var} INTERFACE
            REVISION=${GIT_REVISION}
            VERSION="${GIT_REVISION}~${GIT_COMMIT_SHORT_HASH}"
    )

    string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1;\\2;\\3" PARTS "${GIT_DESCRIBE}")
    list(LENGTH PARTS PARTS_SIZE)

    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)

    if (PARTS_SIZE GREATER 0)
        list(GET PARTS 0 VERSION_MAJOR)
    endif()

    if (PARTS_SIZE GREATER 1)
        list(GET PARTS 1 VERSION_MINOR)
    endif()

    if (PARTS_SIZE GREATER 2)
        list(GET PARTS 2 VERSION_PATCH)
    endif()

    set_target_properties(${target_var} PROPERTIES
            REVISION      "${GIT_REVISION}"
            VERSION       "${GIT_REVISION}~${GIT_COMMIT_SHORT_HASH}"
            GIT_DESCRIBE  "${GIT_DESCRIBE}"
            VERSION_MAJOR "${VERSION_MAJOR}"
            VERSION_MINOR "${VERSION_MINOR}"
            VERSION_PATCH "${VERSION_PATCH}"
    )

    message("-- Version: ${GIT_DESCRIBE} (${GIT_REVISION}~${GIT_COMMIT_SHORT_HASH})")
endfunction()