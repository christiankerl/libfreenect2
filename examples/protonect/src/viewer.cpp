/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2015 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <libfreenect2/config.h>
#include <libfreenect2/viewer.h>
#include <libfreenect2/resource.h>
#include "flextGL.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace libfreenect2
{

struct Vertex
{
  float x, y;
  float u, v;
};

struct Program
{
  GLuint id, tex_idx, scale_idx;
};

struct Texture
{
  bool created;
  GLuint id;
  int width, height;
  GLenum internal_format, format, type;
  float scale;
  Program *program;

  Texture() : created(false), id(0), width(0), height(0), scale(1.0f), program(0) {}
};

class ViewerImpl
{
public:
  std::string title_;
  bool initialized_;
  GLFWwindow* window_;
  OpenGLBindings *gl_;

  GLuint vertex_shader_rgb_, fragment_shader_rgb_, vertex_shader_float_, fragment_shader_float_, square_vbo_, square_vao_;

  Program rgb_program_, float_program_;

  Texture textures_[3];

  ViewerImpl(const std::string &title) :
    title_(title), initialized_(false),
    window_(0), gl_(0)
  {
    textures_[0].program = &rgb_program_;
    textures_[1].program = &float_program_;
    textures_[2].program = &float_program_;
  }

  ~ViewerImpl()
  {
    if(window_ != 0)
    {
      delete gl_;
      gl_ = 0;

      glfwDestroyWindow(window_);
      window_ = 0;
    }
  }

  void checkError(const std::string &name)
  {
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
      std::cerr << "ERROR: " << name << " " << err << std::endl;
    }
  }

  void initializeContext()
  {
    // init glfw - if already initialized nothing happens
    glfwInit();

    // setup context
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    #ifdef __APPLE__
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(512, 424, title_.c_str(), 0, 0);
    glfwMakeContextCurrent(window_);
    gl_ = new OpenGLBindings;

    flextInit(window_, gl_);
  }

  void initializeShader()
  {
    const unsigned char *data;
    size_t length = 0;

    const char *vs_src, *fs_rgb_src, *fs_float_src;
    GLint vs_length, fs_rgb_length, fs_float_length;

    loadResource("src/shader/default.vs", &data, &length);
    vs_src = reinterpret_cast<const char*>(data);
    vs_length = length;
    loadResource("src/shader/viewer_rgb.fs", &data, &length);
    fs_rgb_src = reinterpret_cast<const char*>(data);
    fs_rgb_length = length;
    loadResource("src/shader/viewer_float.fs", &data, &length);
    fs_float_src = reinterpret_cast<const char*>(data);
    fs_float_length = length;

    // rgb program
    vertex_shader_rgb_ = gl_->glCreateShader(GL_VERTEX_SHADER);
    gl_->glShaderSource(vertex_shader_rgb_, 1, &vs_src, &vs_length);
    gl_->glCompileShader(vertex_shader_rgb_);

    fragment_shader_rgb_ = gl_->glCreateShader(GL_FRAGMENT_SHADER);
    gl_->glShaderSource(fragment_shader_rgb_, 1, &fs_rgb_src, &fs_rgb_length);
    gl_->glCompileShader(fragment_shader_rgb_);

    rgb_program_.id = gl_->glCreateProgram();
    gl_->glAttachShader(rgb_program_.id, vertex_shader_rgb_);
    gl_->glAttachShader(rgb_program_.id, fragment_shader_rgb_);
    gl_->glLinkProgram(rgb_program_.id);

    gl_->glUseProgram(rgb_program_.id);
    rgb_program_.tex_idx = gl_->glGetUniformLocation(rgb_program_.id, "Tex");
    rgb_program_.scale_idx = gl_->glGetUniformLocation(rgb_program_.id, "Scale");

    // float program
    vertex_shader_float_ = gl_->glCreateShader(GL_VERTEX_SHADER);
    gl_->glShaderSource(vertex_shader_float_, 1, &vs_src, &vs_length);
    gl_->glCompileShader(vertex_shader_float_);

    fragment_shader_float_ = gl_->glCreateShader(GL_FRAGMENT_SHADER);
    gl_->glShaderSource(fragment_shader_float_, 1, &fs_float_src, &fs_float_length);
    gl_->glCompileShader(fragment_shader_float_);

    float_program_.id = gl_->glCreateProgram();
    gl_->glAttachShader(float_program_.id, vertex_shader_float_);
    gl_->glAttachShader(float_program_.id, fragment_shader_float_);
    gl_->glLinkProgram(float_program_.id);

    gl_->glUseProgram(float_program_.id);
    float_program_.tex_idx = gl_->glGetUniformLocation(float_program_.id, "Tex");
    float_program_.scale_idx = gl_->glGetUniformLocation(float_program_.id, "Scale");
  }

  void initializeGeometry()
  {
    Vertex bl = {-1.0f, -1.0f, 0.0f, 1.0f }, br = { 1.0f, -1.0f, 1.0f, 1.0f }, tl = {-1.0f, 1.0f, 0.0f, 0.0f }, tr = { 1.0f, 1.0f, 1.0f, 0.0f };
    Vertex vertices[] = {
        bl, tl, tr, tr, br, bl
    };
    gl_->glGenBuffers(1, &square_vbo_);
    gl_->glGenVertexArrays(1, &square_vao_);

    gl_->glBindVertexArray(square_vao_);
    gl_->glBindBuffer(GL_ARRAY_BUFFER, square_vbo_);
    gl_->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint position_attr = gl_->glGetAttribLocation(rgb_program_.id, "Position");
    gl_->glVertexAttribPointer(position_attr, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
    gl_->glEnableVertexAttribArray(position_attr);

    GLint texcoord_attr = gl_->glGetAttribLocation(rgb_program_.id, "TexCoord");
    gl_->glVertexAttribPointer(texcoord_attr, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(2 * sizeof(float)));
    gl_->glEnableVertexAttribArray(texcoord_attr);
  }

  void initialize()
  {
    if(initialized_) return;

    initializeContext();
    initializeShader();
    initializeGeometry();
    initialized_ = true;
  }

  void createTexture(Texture &tex, int width, int height, GLenum internal_format, GLenum format, GLenum type)
  {
    if(tex.created) return;

    tex.width = width;
    tex.height = height;
    tex.internal_format = internal_format;
    tex.format = format;
    tex.type = type;

    glGenTextures(1, &tex.id);
    gl_->glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, tex.internal_format, tex.width, tex.height, 0, tex.format, tex.type, 0);

    tex.created = true;
  }

  void updateTexture(Texture &tex, unsigned char *data)
  {
    gl_->glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexSubImage2D(GL_TEXTURE_2D, /*level*/0, /*xoffset*/0, /*yoffset*/0, tex.width, tex.height, tex.format, tex.type, data);
  }

  void draw()
  {
    int w, h;
    glfwGetWindowSize(window_, &w, &h);

    glfwMakeContextCurrent(window_);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    Texture &tex = textures_[0];

    gl_->glUseProgram(tex.program->id);

    gl_->glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    gl_->glUniform1i(tex.program->tex_idx, 1);
    gl_->glUniform1f(tex.program->scale_idx, textures_[1].scale);

    gl_->glBindVertexArray(square_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(window_);
  }

  bool isWindowClosed()
  {
    return glfwWindowShouldClose(window_) != 0;
  }
};

Viewer::Viewer(const std::string &title) : impl_(new ViewerImpl(title))
{
}

Viewer::~Viewer()
{
  delete impl_;
}

void Viewer::show(Frame::Type type, const Frame &frame, float scale)
{
  impl_->initialize();

  switch(type)
  {
  case Frame::Color:
    impl_->createTexture(impl_->textures_[0], frame.width, frame.height, GL_RGB8UI, GL_BGR_INTEGER, GL_UNSIGNED_BYTE);
    impl_->updateTexture(impl_->textures_[0], frame.data);
    impl_->textures_[0].scale = 1.0f / 255.0f;
    break;
  case Frame::Ir:
    impl_->createTexture(impl_->textures_[1], frame.width, frame.height, GL_R32F, GL_RED, GL_FLOAT);
    impl_->updateTexture(impl_->textures_[1], frame.data);
    impl_->textures_[1].scale = scale;
    break;
  case Frame::Depth:
    impl_->createTexture(impl_->textures_[2], frame.width, frame.height, GL_R32F, GL_RED, GL_FLOAT);
    impl_->updateTexture(impl_->textures_[2], frame.data);
    impl_->textures_[2].scale = scale;
    break;
  default:
    break;
  }
}

bool Viewer::update()
{
  impl_->initialize();

  if(!impl_->isWindowClosed())
  {
    impl_->draw();
    glfwPollEvents();
  }

  return !impl_->isWindowClosed();
}

}  /* libfreenect2 */

