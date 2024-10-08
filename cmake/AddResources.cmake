# Generate C source code that embeds arbitrary files via calling the
# ResourceCompiler CMake script using CMake itself.
#
# Declared like this:
# const unsigned char blabla[] = {
#   0x23,0x76,0x65,...,
# };
# const unsigned int blabla_len = 581;

find_program(JQ_EXECUTABLE jq)

macro(add_resources target_var)
    add_library(${target_var} OBJECT)
    set(RESOURCE_COMPILER "${PROJECT_SOURCE_DIR}/cmake/ResourceCompiler.cmake")

    set(generated_base_directory "${CMAKE_CURRENT_BINARY_DIR}/${target_var}.dir/resources")

    foreach(source_file ${ARGN})
        file(RELATIVE_PATH source_file_relative "${CMAKE_SOURCE_DIR}" "${source_file}")
        get_filename_component(source_file_dir "${source_file}" DIRECTORY)
        get_filename_component(source_file_dir_relative "${source_file_relative}" DIRECTORY)
        set(generated_directory "${generated_base_directory}/${source_file_dir_relative}")

        file(MAKE_DIRECTORY ${generated_directory})

        get_filename_component(source_file_name "${source_file}" NAME)
        set(generated_file_name "${generated_directory}/${source_file_name}.c")

        get_filename_component(source_ext "${source_file}" EXT)
        if (source_ext STREQUAL ".json" AND JQ_EXECUTABLE)
            set(validation_command ${JQ_EXECUTABLE} empty "${source_file}")
        endif()

        add_custom_command(
                OUTPUT ${generated_file_name}
                COMMAND ${validation_command}
                COMMAND ${CMAKE_COMMAND} -P ${RESOURCE_COMPILER} "${source_file}" "${generated_file_name}"
                WORKING_DIRECTORY "${source_file_dir}"
                DEPENDS ${source_file} ${RESOURCE_COMPILER}
                COMMENT "Generating C file from ${source_file_relative}"
                VERBATIM
        )
        target_sources(${target_var} PRIVATE ${generated_file_name} ${source_file})
        set_source_files_properties("${CMAKE_SOURCE_DIR}/${source_file}" PROPERTIES HEADER_FILE_ONLY TRUE)
        set_source_files_properties("${generated_file_name}" PROPERTIES GENERATED TRUE)

        source_group(TREE "${CMAKE_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_file})
        source_group(TREE "${generated_base_directory}" PREFIX "Generated Sources" FILES ${generated_file_name})
    endforeach()
endmacro()

