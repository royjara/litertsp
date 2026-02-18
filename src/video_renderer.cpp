// On macOS, GLFW defaults to the legacy OpenGL/gl.h.
// GLFW_INCLUDE_GLCOREARB pulls in OpenGL/gl3.h which has core-profile functions
// (VAOs, glGenVertexArrays, glDrawElements, etc.).
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "video_renderer.h"

#include <cmath>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

static const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* kFragmentShader = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D tex;
void main() {
    FragColor = texture(tex, TexCoord);
}
)";

// Fullscreen quad: xy + uv. V-axis flipped (image top-left → GL bottom-left).
static const float kQuadVertices[] = {
    -1.0f, -1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 1.0f,
     1.0f,  1.0f,   1.0f, 0.0f,
    -1.0f,  1.0f,   0.0f, 0.0f,
};
static const unsigned int kQuadIndices[] = { 0, 1, 2,  0, 2, 3 };

// ---------------------------------------------------------------------------
// Per-stream slot (each has its own texture + pending-frame buffer + mutex)
// std::mutex is not movable, so slots live on the heap via unique_ptr.
// ---------------------------------------------------------------------------

struct StreamSlot {
    GLuint texture    = 0;
    int    tex_width  = 0;
    int    tex_height = 0;

    std::mutex           mutex;
    std::vector<uint8_t> pending_frame;
    int                  pending_width  = 0;
    int                  pending_height = 0;
    bool                 frame_dirty    = false;
};

// ---------------------------------------------------------------------------
// Implementation struct
// ---------------------------------------------------------------------------

struct VideoRendererImpl {
    GLFWwindow* window         = nullptr;
    GLuint      shader_program = 0;
    GLuint      vao            = 0;
    GLuint      vbo            = 0;
    GLuint      ebo            = 0;

    std::vector<std::unique_ptr<StreamSlot>> slots;

    // Grid dimensions computed from slot count
    int grid_cols = 1;
    int grid_rows = 1;

    void init_shaders();
    void init_quad();
    void init_textures();
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
    }
    return shader;
}

void VideoRendererImpl::init_shaders() {
    GLuint vert = compile_shader(GL_VERTEX_SHADER,   kVertexShader);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vert);
    glAttachShader(shader_program, frag);
    glLinkProgram(shader_program);
    GLint ok = 0;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(shader_program, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << "\n";
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
}

void VideoRendererImpl::init_quad() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kQuadIndices), kQuadIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void VideoRendererImpl::init_textures() {
    for (auto& slot : slots) {
        glGenTextures(1, &slot->texture);
        glBindTexture(GL_TEXTURE_2D, slot->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// VideoRenderer
// ---------------------------------------------------------------------------

VideoRenderer::VideoRenderer(int num_streams, const std::string& title)
    : impl_(std::make_unique<VideoRendererImpl>())
{
    // Build per-stream slots
    for (int i = 0; i < num_streams; ++i)
        impl_->slots.push_back(std::make_unique<StreamSlot>());

    // Grid: prefer wider layout (more cols than rows)
    impl_->grid_cols = (int)std::ceil(std::sqrt((double)num_streams));
    impl_->grid_rows = (int)std::ceil((double)num_streams / impl_->grid_cols);

    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    impl_->window = glfwCreateWindow(1280, 720, title.c_str(), nullptr, nullptr);
    if (!impl_->window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(impl_->window);
    glfwSwapInterval(1);

    impl_->init_shaders();
    impl_->init_quad();
    impl_->init_textures();
}

VideoRenderer::~VideoRenderer() {
    for (auto& slot : impl_->slots)
        if (slot->texture) glDeleteTextures(1, &slot->texture);
    if (impl_->vao)            glDeleteVertexArrays(1, &impl_->vao);
    if (impl_->vbo)            glDeleteBuffers(1, &impl_->vbo);
    if (impl_->ebo)            glDeleteBuffers(1, &impl_->ebo);
    if (impl_->shader_program) glDeleteProgram(impl_->shader_program);
    if (impl_->window)         glfwDestroyWindow(impl_->window);
    glfwTerminate();
}

void VideoRenderer::push_frame(int slot, const uint8_t* data, int width, int height) {
    if (slot < 0 || slot >= (int)impl_->slots.size()) return;
    auto& s = *impl_->slots[slot];
    std::lock_guard<std::mutex> lock(s.mutex);
    s.pending_frame.assign(data, data + width * height * 3);
    s.pending_width  = width;
    s.pending_height = height;
    s.frame_dirty    = true;
}

bool VideoRenderer::should_close() const {
    return glfwWindowShouldClose(impl_->window);
}

void VideoRenderer::render() {
    int fb_w, fb_h;
    glfwGetFramebufferSize(impl_->window, &fb_w, &fb_h);

    // Clear the whole window once
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const int cols = impl_->grid_cols;
    const int rows = impl_->grid_rows;
    const int cell_w = fb_w / cols;
    const int cell_h = fb_h / rows;

    glUseProgram(impl_->shader_program);
    glUniform1i(glGetUniformLocation(impl_->shader_program, "tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int i = 0; i < (int)impl_->slots.size(); ++i) {
        auto& s = *impl_->slots[i];

        // Upload new frame if available (brief lock, no GL inside the lock)
        std::vector<uint8_t> upload_buf;
        int upload_w = 0, upload_h = 0;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            if (s.frame_dirty) {
                upload_buf  = s.pending_frame; // copy out
                upload_w    = s.pending_width;
                upload_h    = s.pending_height;
                s.frame_dirty = false;
            }
        }
        if (!upload_buf.empty()) {
            glBindTexture(GL_TEXTURE_2D, s.texture);
            if (upload_w != s.tex_width || upload_h != s.tex_height) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, upload_w, upload_h, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, upload_buf.data());
                s.tex_width  = upload_w;
                s.tex_height = upload_h;
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, upload_w, upload_h,
                                GL_RGB, GL_UNSIGNED_BYTE, upload_buf.data());
            }
        }

        if (s.tex_width == 0) continue; // no frame received yet

        // Grid position: row 0 is top of the window.
        // OpenGL viewport Y=0 is the bottom, so row 0 → highest Y.
        int col = i % cols;
        int row = i / cols;
        int vp_x = col * cell_w;
        int vp_y = (rows - 1 - row) * cell_h;

        glViewport(vp_x, vp_y, cell_w, cell_h);
        glBindTexture(GL_TEXTURE_2D, s.texture);
        glBindVertexArray(impl_->vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    glfwSwapBuffers(impl_->window);
}

void VideoRenderer::poll_events() {
    glfwPollEvents();
}
