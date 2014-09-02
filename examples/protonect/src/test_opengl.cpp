/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2014 individual OpenKinect contributors. See the CONTRIB file
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

#include <libfreenect2/opengl.h>
#include <iostream>
#include <sstream>

bool checkOpenGLError(const std::string &message)
{
  // based on code from http://blog.nobel-joergensen.com/2013/01/29/debugging-opengl-using-glgeterror/
  GLenum err(glGetError());
  bool no_error = true;

  while(err != GL_NO_ERROR)
  {
    no_error = false;
    std::string error;

    switch(err)
    {
      case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
      case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
      case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
    }

    std::cerr << "GL_" << error << ": " << message << std::endl;
    err = glGetError();
  }

  return no_error;
}

template<typename DataT, int Channels>
struct TextureFormatTraits
{
};

#define DEFINE_TEXTURE_FORMAT(DATATYPE, CHANNELS, FORMAT, TYPE) \
template<> \
struct TextureFormatTraits<DATATYPE, CHANNELS> \
{ \
  static const GLenum Format = FORMAT; \
  static const GLenum Type = TYPE; \
  static const char* Name; \
}; \
const char *TextureFormatTraits<DATATYPE, CHANNELS>::Name = #DATATYPE "[" #CHANNELS "] = " #FORMAT " " #TYPE;

// 1 channel formats
DEFINE_TEXTURE_FORMAT(char, 1, GL_RED_INTEGER, GL_BYTE);
DEFINE_TEXTURE_FORMAT(unsigned char, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
DEFINE_TEXTURE_FORMAT(short, 1, GL_RED_INTEGER, GL_SHORT);
DEFINE_TEXTURE_FORMAT(unsigned short, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
DEFINE_TEXTURE_FORMAT(int, 1, GL_RED_INTEGER, GL_INT);
DEFINE_TEXTURE_FORMAT(unsigned int, 1, GL_RED_INTEGER, GL_UNSIGNED_INT);
DEFINE_TEXTURE_FORMAT(float, 1, GL_RED, GL_FLOAT);

// 2 channel formats
DEFINE_TEXTURE_FORMAT(char, 2, GL_RG_INTEGER, GL_BYTE);
DEFINE_TEXTURE_FORMAT(unsigned char, 2, GL_RG_INTEGER, GL_UNSIGNED_BYTE);
DEFINE_TEXTURE_FORMAT(short, 2, GL_RG_INTEGER, GL_SHORT);
DEFINE_TEXTURE_FORMAT(unsigned short, 2, GL_RG_INTEGER, GL_UNSIGNED_SHORT);
DEFINE_TEXTURE_FORMAT(int, 2, GL_RG_INTEGER, GL_INT);
DEFINE_TEXTURE_FORMAT(unsigned int, 2, GL_RG_INTEGER, GL_UNSIGNED_INT);
DEFINE_TEXTURE_FORMAT(float, 2, GL_RG, GL_FLOAT);

// 3 channel formats
DEFINE_TEXTURE_FORMAT(char, 3, GL_RGB_INTEGER, GL_BYTE);
DEFINE_TEXTURE_FORMAT(unsigned char, 3, GL_RGB_INTEGER, GL_UNSIGNED_BYTE);
DEFINE_TEXTURE_FORMAT(short, 3, GL_RGB_INTEGER, GL_SHORT);
DEFINE_TEXTURE_FORMAT(unsigned short, 3, GL_RGB_INTEGER, GL_UNSIGNED_SHORT);
DEFINE_TEXTURE_FORMAT(int, 3, GL_RGB_INTEGER, GL_INT);
DEFINE_TEXTURE_FORMAT(unsigned int, 3, GL_RGB_INTEGER, GL_UNSIGNED_INT);
DEFINE_TEXTURE_FORMAT(float, 3, GL_RGB, GL_FLOAT);

// 4 channel formats
DEFINE_TEXTURE_FORMAT(char, 4, GL_RGBA_INTEGER, GL_BYTE);
DEFINE_TEXTURE_FORMAT(unsigned char, 4, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);
DEFINE_TEXTURE_FORMAT(short, 4, GL_RGBA_INTEGER, GL_SHORT);
DEFINE_TEXTURE_FORMAT(unsigned short, 4, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT);
DEFINE_TEXTURE_FORMAT(int, 4, GL_RGBA_INTEGER, GL_INT);
DEFINE_TEXTURE_FORMAT(unsigned int, 4, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
DEFINE_TEXTURE_FORMAT(float, 4, GL_RGBA, GL_FLOAT);

#define IFN(FORMAT) case FORMAT: return #FORMAT

const char *internalFormatName(GLenum f)
{
  switch(f)
  {
  IFN(GL_R8I);
  IFN(GL_R8UI);
  IFN(GL_RG8I);
  IFN(GL_RG8UI);
  IFN(GL_RGB8I);
  IFN(GL_RGB8UI);
  IFN(GL_RGBA8UI);
  IFN(GL_R16I);
  IFN(GL_R16UI);
  IFN(GL_RG16I);
  IFN(GL_RG16UI);
  IFN(GL_RGB16I);
  IFN(GL_RGB16UI);
  IFN(GL_RGBA16UI);
  IFN(GL_R32I);
  IFN(GL_R32UI);
  IFN(GL_RG32I);
  IFN(GL_RG32UI);
  IFN(GL_RGB32I);
  IFN(GL_RGB32UI);
  IFN(GL_RGBA32UI);
  IFN(GL_R32F);
  IFN(GL_RG32F);
  IFN(GL_RGB32F);
  IFN(GL_RGBA32F);
  default: return "unknown";
  }
}

template<typename DataT>
void generateTextureData(DataT *data, int n)
{
  for(int i = 0; i < n; ++i)
    data[i] = DataT(i);
}

template<typename DataT>
bool allElementsEqual(DataT *expected, DataT *actual, int n)
{
  for(int i = 0; i < n; ++i)
    if(expected[i] != actual[i]) return false;

  return true;
}

template<typename DataT, int Channels>
void testOpenGLTextureFormat(GLenum internal_format)
{
  typedef TextureFormatTraits<DataT, Channels> Traits;

  int width = 32, height = 32, n = width * height * Channels;
  DataT *data = new DataT[n], *data_download = new DataT[n];
  generateTextureData(data, n);

  GLuint tex;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, tex);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, internal_format, width, height, 0, Traits::Format, Traits::Type, data);

  std::stringstream error1, error2;
  error1 << "failed to create texture from data " << Traits::Name << " with internal format " << internalFormatName(internal_format) << "!";

  if(!checkOpenGLError(error1.str())) return;

  glGetTexImage(GL_TEXTURE_RECTANGLE, 0, Traits::Format, Traits::Type, data_download);

  error2 << "failed to download data from texture " << Traits::Name << " with internal format " << internalFormatName(internal_format) << "!";
  if(!checkOpenGLError(error2.str())) return;

  if(!allElementsEqual(data, data_download, n))
  {
    std::cerr << "uploaded and downloaded texture data  not equal for " << Traits::Name << " with internal format " << internalFormatName(internal_format) << "!";
  }

  glDeleteTextures(1, &tex);
  delete[] data;
  delete[] data_download;
}

void testOpenGLTextureFormats()
{
  checkOpenGLError("this error occurred during OpenGL setup - ignore it for now");

  // test input format == internal format textures
  std::cout << "testing available input formats with corresponding internal formats" << std::endl;
  testOpenGLTextureFormat<char, 1>(GL_R8I);
  testOpenGLTextureFormat<unsigned char, 1>(GL_R8UI);
  testOpenGLTextureFormat<short, 1>(GL_R16I);
  testOpenGLTextureFormat<unsigned short, 1>(GL_R16UI);
  testOpenGLTextureFormat<int, 1>(GL_R32I);
  testOpenGLTextureFormat<unsigned int, 1>(GL_R32UI);
  testOpenGLTextureFormat<float, 1>(GL_R32F);

  testOpenGLTextureFormat<char, 2>(GL_RG8I);
  testOpenGLTextureFormat<unsigned char, 2>(GL_RG8UI);
  testOpenGLTextureFormat<short, 2>(GL_RG16I);
  testOpenGLTextureFormat<unsigned short, 2>(GL_RG16UI);
  testOpenGLTextureFormat<int, 2>(GL_RG32I);
  testOpenGLTextureFormat<unsigned int, 2>(GL_RG32UI);
  testOpenGLTextureFormat<float, 2>(GL_RG32F);

  testOpenGLTextureFormat<char, 3>(GL_RGB8I);
  testOpenGLTextureFormat<unsigned char, 3>(GL_RGB8UI);
  testOpenGLTextureFormat<short, 3>(GL_RGB16I);
  testOpenGLTextureFormat<unsigned short, 3>(GL_RGB16UI);
  testOpenGLTextureFormat<int, 3>(GL_RGB32I);
  testOpenGLTextureFormat<unsigned int, 3>(GL_RGB32UI);
  testOpenGLTextureFormat<float, 3>(GL_RGB32F);

  testOpenGLTextureFormat<char, 4>(GL_RGBA8I);
  testOpenGLTextureFormat<unsigned char, 4>(GL_RGBA8UI);
  testOpenGLTextureFormat<short, 4>(GL_RGBA16I);
  testOpenGLTextureFormat<unsigned short, 4>(GL_RGBA16UI);
  testOpenGLTextureFormat<int, 4>(GL_RGBA32I);
  testOpenGLTextureFormat<unsigned int, 4>(GL_RGBA32UI);
  testOpenGLTextureFormat<float, 4>(GL_RGBA32F);

  // test input format -> float
  std::cout << "testing available input formats with conversion to float internal formats" << std::endl;
  testOpenGLTextureFormat<char, 1>(GL_R32F);
  testOpenGLTextureFormat<unsigned char, 1>(GL_R32F);
  testOpenGLTextureFormat<short, 1>(GL_R32F);
  testOpenGLTextureFormat<unsigned short, 1>(GL_R32F);
  testOpenGLTextureFormat<int, 1>(GL_R32F);
  testOpenGLTextureFormat<unsigned int, 1>(GL_R32F);

  testOpenGLTextureFormat<char, 2>(GL_RG32F);
  testOpenGLTextureFormat<unsigned char, 2>(GL_RG32F);
  testOpenGLTextureFormat<short, 2>(GL_RG32F);
  testOpenGLTextureFormat<unsigned short, 2>(GL_RG32F);
  testOpenGLTextureFormat<int, 2>(GL_RG32F);
  testOpenGLTextureFormat<unsigned int, 2>(GL_RG32F);

  testOpenGLTextureFormat<char, 3>(GL_RGB32F);
  testOpenGLTextureFormat<unsigned char, 3>(GL_RGB32F);
  testOpenGLTextureFormat<short, 3>(GL_RGB32F);
  testOpenGLTextureFormat<unsigned short, 3>(GL_RGB32F);
  testOpenGLTextureFormat<int, 3>(GL_RGB32F);
  testOpenGLTextureFormat<unsigned int, 3>(GL_RGB32F);

  testOpenGLTextureFormat<char, 4>(GL_RGBA32F);
  testOpenGLTextureFormat<unsigned char, 4>(GL_RGBA32F);
  testOpenGLTextureFormat<short, 4>(GL_RGBA32F);
  testOpenGLTextureFormat<unsigned short, 4>(GL_RGBA32F);
  testOpenGLTextureFormat<int, 4>(GL_RGBA32F);
  testOpenGLTextureFormat<unsigned int, 4>(GL_RGBA32F);
}

int main(int argc, char **argv) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  GLFWwindow* window = glfwCreateWindow(512, 424, "OpenGL", 0, 0); // Windowed
  libfreenect2::OpenGLContext opengl_ctx(window);
  opengl_ctx.makeCurrent();

  testOpenGLTextureFormats();

  return 0;
}
