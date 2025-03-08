cmake_minimum_required(VERSION 3.10)
project(video_stabilizer)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${OpenCV_INCLUDE_DIRS} src)

add_executable(${PROJECT_NAME}
    src/Main.cpp
    src/GrayscaleConverter.cpp
    src/Logger.cpp
)

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
