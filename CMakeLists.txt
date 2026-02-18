cmake_minimum_required(VERSION 3.16)
project(rtspreceiver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── LiteRT / TFLite inference ──────────────────────────────────────────────
# Builds from source for true cross-compilation (x86_64 dev → ARM64 target).
# tflite/CMakeLists.txt transitively fetches tensorflow/tensorflow at v2.19.0
# for its headers, so #include "tensorflow/lite/..." resolves automatically.
# First configure will download ~1 GB; subsequent builds reuse the cache.
#
# Delegates added later via the same InferenceBackend interface:
#   GPU  → TFLITE_ENABLE_GPU ON   (OpenCL on Linux ARM, Metal on macOS)
#   NPU  → TFLITE_ENABLE_NNAPI ON (Android NNAPI / vendor delegate)
include(FetchContent)
set(TFLITE_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Pin FlatBuffers to v24.x: LiteRT v2.1.2 defaults to v25.9.23, but TF v2.19.0's
# schema_generated.h has a compile-time assert requiring FLATBUFFERS_VERSION_MAJOR == 24.
# OverridableFetchContent (LiteRT's dependency wrapper) reads this variable.
set(OVERRIDABLE_FETCH_CONTENT_flatbuffers_GIT_TAG "v24.12.23" CACHE STRING "" FORCE)

FetchContent_Declare(
    litert
    GIT_REPOSITORY https://github.com/google-ai-edge/LiteRT.git
    GIT_TAG        v2.1.2
    GIT_SHALLOW    TRUE
)
# Use EXCLUDE_FROM_ALL so that orphan tool targets (benchmark, profiling protos)
# that have broken --proto_path when built as FetchContent are not built by
# default; only tensorflow-lite and its actual transitive deps are built.
FetchContent_GetProperties(litert)
if(NOT litert_POPULATED)
    FetchContent_Populate(litert)
    add_subdirectory(${litert_SOURCE_DIR}/tflite ${litert_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

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
    src/cpu_backend.cpp
    src/inference_engine.cpp
)

target_link_libraries(rtspreceiver
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_RTSP_LIBRARIES}
    ${GSTREAMER_RTSP_SERVER_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    tensorflow-lite
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
