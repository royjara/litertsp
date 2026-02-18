cmake_minimum_required(VERSION 3.16)
project(rtspreceiver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)

pkg_check_modules(GSTREAMER        REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP    REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO  REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_RTSP   REQUIRED gstreamer-rtsp-1.0)
pkg_check_modules(GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)
pkg_check_modules(GLFW             REQUIRED glfw3)

include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GSTREAMER_RTSP_INCLUDE_DIRS}
    ${GSTREAMER_RTSP_SERVER_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIRS}
    src
)

link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_APP_LIBRARY_DIRS}
    ${GSTREAMER_VIDEO_LIBRARY_DIRS}
    ${GSTREAMER_RTSP_LIBRARY_DIRS}
    ${GSTREAMER_RTSP_SERVER_LIBRARY_DIRS}
    ${GLFW_LIBRARY_DIRS}
)

add_executable(rtspreceiver
    src/main.cpp
    src/rtsp_stream_manager.cpp
    src/stream_discovery.cpp
    src/gstreamer_pipeline.cpp
    src/video_renderer.cpp
)

target_link_libraries(rtspreceiver
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_RTSP_LIBRARIES}
    ${GSTREAMER_RTSP_SERVER_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

target_compile_options(rtspreceiver PRIVATE
    ${GSTREAMER_CFLAGS_OTHER}
    ${GSTREAMER_APP_CFLAGS_OTHER}
    ${GSTREAMER_VIDEO_CFLAGS_OTHER}
    ${GSTREAMER_RTSP_CFLAGS_OTHER}
    ${GSTREAMER_RTSP_SERVER_CFLAGS_OTHER}
    ${GLFW_CFLAGS_OTHER}
)

# suppress macOS OpenGL deprecation warnings project-wide
target_compile_definitions(rtspreceiver PRIVATE GL_SILENCE_DEPRECATION)
