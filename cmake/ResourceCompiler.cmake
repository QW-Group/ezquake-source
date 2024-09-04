# Generate C code with some content encoded as an array of unsigned char.
# See AddResources.cmake for more information.

set(input_file "${CMAKE_ARGV3}")
set(output_file "${CMAKE_ARGV4}")

get_filename_component(base_name "${input_file}" NAME)
string(REGEX REPLACE "[.]" "_" variable_name "${base_name}")

file(READ "${input_file}" content HEX)

string(LENGTH "${content}" content_length)
math(EXPR data_length "${content_length} / 2")

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," data "${content}")
file(WRITE "${output_file}" "const unsigned char ${variable_name}[] = {\n${data}\n};\nconst unsigned int ${variable_name}_len = ${data_length};\n")
