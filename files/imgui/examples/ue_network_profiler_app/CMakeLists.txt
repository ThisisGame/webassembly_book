cmake_minimum_required(VERSION 3.17)
project(example_profiler_app)

set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    add_compile_options(/bigobj)
    add_compile_options(/wd4819)#忽略保存为unicode警告
endif ()

include(CMakeLists.txt.glfw)
include(CMakeLists.txt.imgui)

#拷贝data文件
file(COPY "../../misc/fonts/" DESTINATION "./fonts/")

#头文件目录
include_directories("./")
include_directories("../../misc/depends/")

#这里使用GLOB_RECURSE会遍历子文件夹导致包含了cmake-build-debug\CMakeFiles\3.27.8\CompilerIdCXX\CMakeCXXCompilerId.cpp，导致main函数重复定义。
# file(GLOB_RECURSE example_cpp ./*.cpp)
file(GLOB example_cpp ./*.cpp)

add_executable(${PROJECT_NAME} ${glfw_sources} ${imgui_src} ${example_cpp})

if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_definitions(-D _DEBUG)
endif ()