cmake_minimum_required(VERSION 3.29)
set(TEMPLATE_NAME "gpu-pro26-3dgs")

project(${TEMPLATE_NAME} LANGUAGES C CXX) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

option(ENABLE_CPP20_MODULE "Use the Vulkan-Hpp C++20 module instead of headers" OFF)
 
# C++20 module scanning is only needed when the module is enabled
if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
else()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()


find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)


# ---------------------------------------------------------------------------
# Copied from the vulkan tutorial
set(VULKAN_HPP_DEFINITIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
 
if(ENABLE_CPP20_MODULE)
    add_library(VulkanCppModule)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)
 
    target_compile_definitions(VulkanCppModule PUBLIC ${VULKAN_HPP_DEFINITIONS})
    target_include_directories(VulkanCppModule PUBLIC "${Vulkan_INCLUDE_DIR}")
    target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)
 
    if(MSVC)
        target_compile_options(VulkanCppModule PRIVATE
            /std:c++latest
            /permissive-
            /Zc:__cplusplus
            /EHsc
            /Zc:preprocessor
            /translateInclude)
    endif()
 
    target_sources(VulkanCppModule
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS "${Vulkan_INCLUDE_DIR}"
            FILES     "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm")
else()
  # Create a dummy interface library when C++ 20 module is disabled
    add_library(VulkanCppModule INTERFACE)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)
 
    target_compile_definitions(VulkanCppModule INTERFACE ${VULKAN_HPP_DEFINITIONS})
    target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
endif()
 


add_subdirectory(src)