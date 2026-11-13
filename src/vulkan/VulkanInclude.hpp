#pragma once

#if defined(__INTELISENSE__) || !defined(USE_CPP20_MODULES)
    #include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif