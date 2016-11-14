#pragma once

#include "glext.h"


// Buffers
typedef void (*GENBUFFERS)(GLsizei n, GLuint * buffers);
typedef void (*BINDBUFFER)(GLenum target, GLuint buffer);
typedef void (*BUFFERDATA)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
typedef void (*GENVERTEXARRAYS)(GLsizei n, GLuint *arrays);
typedef void (*BINDVERTEXARRAY)(GLuint array);

GENBUFFERS glGenBuffers;
BINDBUFFER glBindBuffer;
BUFFERDATA glBufferData;
GENVERTEXARRAYS glGenVertexArrays;
BINDVERTEXARRAY glBindVertexArray;

// Shaders
typedef GLuint (*CREATESHADER)(GLenum shaderType);
typedef void (*DELETESHADER)(GLuint shader);
typedef void (*SHADERSOURCE)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (*COMPILESHADER)(GLuint shader);
typedef void (*GETSHADERIV)(GLuint shader, GLenum pname, GLint *params);
typedef void (*GETSHADERINFOLOG)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (*BINDFRAGDATALOCATION)(GLuint program, GLuint colorNumber, const char * name);

CREATESHADER glCreateShader;
DELETESHADER glDeleteShader;
SHADERSOURCE glShaderSource;
COMPILESHADER glCompileShader;
GETSHADERIV glGetShaderiv;
GETSHADERINFOLOG glGetShaderInfoLog;
BINDFRAGDATALOCATION glBindFragDataLocation;

//Textures
typedef void (*GENERATEMIPMAP)(GLenum target);
GENERATEMIPMAP glGenerateMipmap;

// Program
typedef GLuint (*CREATEPROGRAM)(void);
typedef void (*ATTACHSHADER)(GLuint program, GLuint shader);
typedef void (*LINKPROGRAM)(GLuint program);
typedef void (*USEPROGRAM)(GLuint program);
typedef GLint (*GETATTRIBLOCATION)(GLuint program, const GLchar *name);
typedef GLint (*GETUNIFORMLOCATION)(GLuint program, const GLchar *name);
typedef void (*GETPROGRAM)(GLuint program, GLenum pname, GLint *params);
typedef void (*GETPROGRAMINFOLOG)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);

CREATEPROGRAM glCreateProgram;
ATTACHSHADER glAttachShader;
LINKPROGRAM glLinkProgram;
USEPROGRAM glUseProgram;
GETATTRIBLOCATION glGetAttribLocation;
GETUNIFORMLOCATION glGetUniformLocation;
GETPROGRAM glGetProgramiv;
GETPROGRAMINFOLOG glGetProgramInfoLog;

typedef void (*VERTEXATTRIBPOINTER)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
typedef void (*ENABLEVERTEXATTRIBARRAY)(GLuint index);

VERTEXATTRIBPOINTER glVertexAttribPointer;
ENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;

typedef void (*UNIFORM3F)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);

UNIFORM3F glUniform3f;

void InitOpenGLExtensions()
{
#define GET_FUNC(sig, name) name = (sig)wglGetProcAddress(#name)
    // Buffers
    GET_FUNC(GENBUFFERS, glGenBuffers);
    GET_FUNC(BINDBUFFER, glBindBuffer);
    GET_FUNC(BUFFERDATA, glBufferData);
    GET_FUNC(GENVERTEXARRAYS, glGenVertexArrays);
    GET_FUNC(BINDVERTEXARRAY, glBindVertexArray);

    // Shaders
    GET_FUNC(CREATESHADER, glCreateShader);
    GET_FUNC(DELETESHADER, glDeleteShader);
    GET_FUNC(SHADERSOURCE, glShaderSource);
    GET_FUNC(COMPILESHADER, glCompileShader);
    GET_FUNC(GETSHADERIV, glGetShaderiv);
    GET_FUNC(GETSHADERINFOLOG, glGetShaderInfoLog);
    GET_FUNC(BINDFRAGDATALOCATION, glBindFragDataLocation);

    // Textures
    GET_FUNC(GENERATEMIPMAP, glGenerateMipmap);

    // Program
    GET_FUNC(CREATEPROGRAM, glCreateProgram);
    GET_FUNC(ATTACHSHADER, glAttachShader);
    GET_FUNC(LINKPROGRAM, glLinkProgram);
    GET_FUNC(USEPROGRAM, glUseProgram);
    GET_FUNC(GETATTRIBLOCATION, glGetAttribLocation);
    GET_FUNC(GETUNIFORMLOCATION, glGetUniformLocation);
    GET_FUNC(GETPROGRAM, glGetProgramiv);
    GET_FUNC(GETPROGRAMINFOLOG, glGetProgramInfoLog);

    GET_FUNC(VERTEXATTRIBPOINTER, glVertexAttribPointer);
    GET_FUNC(ENABLEVERTEXATTRIBARRAY, glEnableVertexAttribArray);

    GET_FUNC(UNIFORM3F, glUniform3f);

#undef GET_FUNC
}
