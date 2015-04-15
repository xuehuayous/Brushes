
#ifndef _GLDEF_H_
#define _GLDEF_H_

#include "../Macro.h"

#if USE_OPENGL

 //#include <gl/GL.h>
 //#include <gl/GLU.h>
 #include "gl/glew.h"
 #include "gl/glut.h"

 #define GL_GEN_VERTEXARRAY(n,arr)	glGenVertexArrays(n, arr)
 #define GL_BIND_VERTEXARRAY(id)	glBindVertexArray(id)
 #define GL_DEL_VERTEXARRAY(n,arr)	glDeleteVertexArrays(n,arr)

#elif USE_OPENGL_ES

 #include <OpenGLES/EAGL.h>
 #include <OpenGLES/ES2/gl.h>
 #include <OpenGLES/ES2/glext.h>

 #define GL_GEN_VERTEXARRAY(n,arr)	glGenVertexArraysOES(n, arr)
 #define GL_BIND_VERTEXARRAY(id)	glBindVertexArrayOES(id)
 #define GL_DEL_VERTEXARRAY(n,arr)	glDeleteVertexArraysOES(n,arr)

#endif

#endif