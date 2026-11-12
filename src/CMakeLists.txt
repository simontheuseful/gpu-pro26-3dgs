# Output locations

set(TEMPLATE_NAME "gpu-pro26-3dgs")
set(OUTPUT_DIR     ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME})          # executable
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${OUTPUT_DIR}/shaders)            # compiled .spv files
 
# Executable

# Every .cpp under src/ (main.cpp, vulkan/, renderers/, ...) is included
# GLOB_RECURSE searches in every folder recursively for files matching a pattern
# APP_SOURCES is the variable that stores the paths provided by GLOB_RECURSE
# CONFIGURE_DEPENDS re-runs the check on every build and not only once at configure time
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
 
# link everything to main
add_executable(${TEMPLATE_NAME} ${APP_SOURCES})
 
# The $<1:...> generator expression stops Visual Studio from adding
# a Debug/ or Release/ subfolder, so the output is always in build/bin/
#set_target_properties(${TEMPLATE_NAME} PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY $<1:${OUTPUT_DIR}>)
set_target_properties(${TEMPLATE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
 
# Allows #include "vulkan/Device.hpp" from any file in src/
target_include_directories(${TEMPLATE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
 
target_link_libraries(${TEMPLATE_NAME} PRIVATE
    Vulkan::cppm
    glfw
    glm::glm)
 
target_compile_definitions(${TEMPLATE_NAME} PRIVATE
    # Treat VK_ERROR_OUT_OF_DATE_KHR as a success code (from the tutorial)
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
    # Stop glfw3.h from including OpenGL headers (we only use Vulkan)
    GLFW_INCLUDE_NONE
    # Vulkan conventions for GLM: radians, depth range [0, 1]
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    # Absolute paths, so the program finds its files from any working directory
    SHADER_DIR="${SHADER_OUT_DIR}/"
    ASSET_DIR="${PROJECT_SOURCE_DIR}/assets/")
 
if(ENABLE_CPP20_MODULE)
    target_compile_definitions(${TEMPLATE_NAME} PRIVATE USE_CPP20_MODULES=1)
endif()
 
# For the shader files --> Shader files are in SLANG!
# some filtering can be added to take files out if necessary 
find_program(SLANGC_EXECUTABLE slangc REQUIRED
    HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin)

# CONFIGURE_DEPENDS recreates the GLOB every build (and not just only once) 
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS ${SHADER_SRC_DIR}/*.slang)

foreach(SHADER ${SHADER_FILES})
    file(RELATIVE_PATH SHADER_REL ${SHADER_SRC_DIR} ${SHADER})
    string(REGEX REPLACE "\\.slang$" ".spv" SPIRV_REL ${SHADER_REL})
    set(SPIRV ${SHADER_OUT_DIR}/${SPIRV_REL}) # check this, could this cause some /shader/shader dir creation in the output folder?
    get_filename_component(SPIRV_DIR ${SPIRV} DIRECTORY)

        add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR} # this should create the shader/<subfolder> dir in output
        COMMAND ${SLANGC_EXECUTABLE} ${SHADER}
                -target spirv -profile spirv_1_4 -emit-spirv-directly
                -fvk-use-entrypoint-name
                -I ${SHADER_SRC_DIR}#/shared
                -o ${SPIRV}
        DEPENDS ${SHADER} # depends only on itself
        COMMENT "Compiling shader ${SHADER_REL} current spv ${SPIRV_DIR}"
        VERBATIM)
 
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_FILES})
add_dependencies(${TEMPLATE_NAME} Shaders)

