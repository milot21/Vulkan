cmake_minimum_required(VERSION 3.24)
project(VulkanTest)
include(CMakePrintHelpers)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(lib)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

#==============================================================================
# COMPILE SHADERS
#

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

file(GLOB SHADERS
        ${SHADER_SOURCE_DIR}/*.vert
        ${SHADER_SOURCE_DIR}/*.frag
        ${SHADER_SOURCE_DIR}/*.comp
        ${SHADER_SOURCE_DIR}/*.geom
        ${SHADER_SOURCE_DIR}/*.tesc
        ${SHADER_SOURCE_DIR}/*.tese
        ${SHADER_SOURCE_DIR}/*.mesh
        ${SHADER_SOURCE_DIR}/*.task
        ${SHADER_SOURCE_DIR}/*.rgen
        ${SHADER_SOURCE_DIR}/*.rchit
        ${SHADER_SOURCE_DIR}/*.rmiss)

foreach(source IN LISTS SHADERS)
    get_filename_component(FILENAME ${source} NAME)
    add_custom_command(
            COMMAND
            ${glslc_executable}
            -o ${SHADER_BINARY_DIR}/${FILENAME}.spv
            ${source}
            OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.spv
            DEPENDS ${source} ${SHADER_BINARY_DIR}
            COMMENT "Compiling ${FILENAME}"
    )
    list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.spv)
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})

### END SHADER COMPILATION

add_library(glfw STATIC IMPORTED lve/lve_camera.hpp lve/lve_utils.hpp lve/lve_game_object.cpp lve/lve_frame_info.hpp lve/lve_descriptor.hpp lve/lve_descriptor.cpp systems/point_light_system.hpp systems/point_light_system.hpp systems/point_light_system.cpp)

set_target_properties(glfw PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/lib-mingw-w64/libglfw3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/include")

add_executable(VulkanTest main.cpp lve/lve_window.hpp lve/lve_window.cpp fileReader.cpp fileReader.hpp systems/simple_render_system.cpp systems/simple_render_system.hpp first_app.hpp first_app.cpp lve/lve_pipeline.hpp lve/lve_pipeline.cpp lve/lve_device.hpp lve/lve_device.cpp lve/lve_swap_chain.cpp
        ../shaders/simple_shader.frag.spv ../shaders/simple_shader.vert.spv lve/lve_model.hpp lve/lve_model.cpp lve/lve_game_object.hpp lve/lve_game_object.cpp lve/lve_renderer.cpp lve/lve_renderer.hpp lve/lve_camera.cpp lve/lve_camera.hpp movement_controller.cpp movement_controller.hpp
        lve/lve_buffer.cpp lve/lve_buffer.hpp lve/lve_frame_info.hpp lve/lve_descriptor.hpp lve/lve_descriptor.cpp systems/point_light_system.hpp systems/point_light_system.cpp)

target_sources(VulkanTest PRIVATE main.cpp)
target_link_libraries(VulkanTest PRIVATE Vulkan::Vulkan glm glfw tinyobjloader)


