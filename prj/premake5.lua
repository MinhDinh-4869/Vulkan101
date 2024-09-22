workspace "Vulkan101"
configurations {"Debug", "Release"}
location "vs2022"

project "Vulkan101"
    kind "ConsoleApp"
    language "C++"
    targetdir "../Release"
    location "vs2022"
    files {"../src/*"}
        
    includedirs {"../Externals/glm-1.0.1/glm", "../Externals/glfw-3.4.bin.WIN64/include", "C:\\VulkanSDK\\1.3.275.0\\Include"}
    libdirs {"../Externals/glfw-3.4.bin.WIN64/lib-vc2022", "C:\\VulkanSDK\\1.3.275.0\\Lib"}
    links {"vulkan-1.lib", "glfw3.lib"}

    cppdialect "C++17"

    clr "On"

    architecture "x64"