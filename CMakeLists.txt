cmake_minimum_required(VERSION 3.16)

# 设置 Qt MinGW 编译器
set(CMAKE_C_COMPILER "D:/Qt/Tools/mingw1310_64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "D:/Qt/Tools/mingw1310_64/bin/g++.exe")

# 设置 Qt 路径
set(CMAKE_PREFIX_PATH "D:/QT/6.9.0/mingw_64")
set(Qt6_DIR "D:/QT/6.9.0/mingw_64/lib/cmake/Qt6")

project(HardwareVisualizer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/mainwindow.h
    src/hardwaremodule.cpp
    src/hardwaremodule.h
    src/hardwarevisualizer.cpp
    src/hardwarevisualizer.h
    src/moduleinfodialog.cpp
    src/moduleinfodialog.h
)

# 设置资源文件
set(PROJECT_RESOURCES
    resources/setup.txt
    resources/statistic.txt
)

# 创建可执行文件
add_executable(${PROJECT_NAME} 
    ${PROJECT_SOURCES}
    ${PROJECT_RESOURCES}
)

# 设置资源文件的复制
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resources/setup.txt
        ${CMAKE_SOURCE_DIR}/resources/statistic.txt
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/
)

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
) 