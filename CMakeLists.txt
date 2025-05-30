cmake_minimum_required(VERSION 3.16)
project(HardwareVisualizer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加特定的编译选项
if(MSYS OR MINGW)
    add_compile_options(-fno-exceptions)
    add_compile_options(-fno-rtti)
endif()

# 设置Qt的自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 设置源文件
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/hardwaremodule.cpp
    src/dataparser.cpp
    src/hardwareview.cpp
    resources/resources.qrc
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
) 