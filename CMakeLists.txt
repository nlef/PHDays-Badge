# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(badge-esp32)


add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/${PROJECT_BIN} ${build_dir}/rls/${PROJECT_BIN}
    COMMENT "Copying esp32 binary output to SomeFolder"
)

add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/phy_init_data.bin ${build_dir}/rls/phy_init_data.bin
    COMMENT "Copying esp32 binary output to SomeFolder"
)
add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/bootloader/bootloader.bin ${build_dir}/rls/bootloader.bin
    COMMENT "Copying esp32 binary output to SomeFolder"
)
add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/partition_table/partition-table.bin ${build_dir}/rls/partition-table.bin
    COMMENT "Copying esp32 binary output to SomeFolder"
)
