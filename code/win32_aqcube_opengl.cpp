static HGLRC Win32InitializeOpenGL(HDC DeviceContext)
{
    HGLRC Result = 0;

    PIXELFORMATDESCRIPTOR PixelFormat = {};
    PixelFormat.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    PixelFormat.nVersion = 1;
    PixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    PixelFormat.iPixelType = PFD_TYPE_RGBA;
    PixelFormat.cColorBits = 24;
    // PixelFormat.cDepthBits = ?;
    // PixelFormat.cStencilBits = ?;

    int PixelFormatIndex = ChoosePixelFormat(DeviceContext, &PixelFormat);
    if(SetPixelFormat(DeviceContext, PixelFormatIndex, &PixelFormat))
    {
        Result = wglCreateContext(DeviceContext);
    }


    return Result;
}

GLuint Win32CompileShader(GLenum ShaderType, char *ShaderFile)
{
    GLuint Shader = 0;

    const GLchar *ShaderSource = (GLchar *)ReadFile(ShaderFile);
    if (ShaderSource)
    {
        Shader = glCreateShader(ShaderType);
        glShaderSource(Shader, 1, &ShaderSource, 0);
        glCompileShader(Shader);

        GLint CompileStatus;
        glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);

        if (CompileStatus == GL_TRUE)
        {
            char Log[512];
            glGetShaderInfoLog(Shader, 512, 0, Log);
            OutputDebugStringA(Log);
        }
    }

    assert(Shader);
    return Shader;
}

GLuint Win32CreateProgram(GLuint *Shaders, int ShaderCount)
{
    GLuint ShaderProgram = glCreateProgram();

    if (ShaderProgram)
    {
        for (int ShaderIndex = 0; ShaderIndex < ShaderCount; ++ShaderIndex)
        {
            GLuint Shader = Shaders[ShaderIndex];
            glAttachShader(ShaderProgram, Shader);
        }
        glLinkProgram(ShaderProgram);

        GLint Success;
        glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
        if (!Success)
        {
            char Log[512];
            glGetProgramInfoLog(ShaderProgram, 512, NULL, Log);
            OutputDebugStringA(Log);
        }

        for (int ShaderIndex = 0; ShaderIndex < ShaderCount; ++ShaderIndex)
        {
            GLuint Shader = Shaders[ShaderIndex];
            glDeleteShader(Shader);
        }
    }

    return ShaderProgram;
}

// TODO(joe): Make it possible for the loaded_image to know the Source Pixel Format?
GLuint Win32CreateTexture(loaded_image Image, GLint SourcePixelFormat)
{
    GLuint Texture;
    glGenTextures(1, &Texture);
    //glActiveTexture(TextureUnit);
    glBindTexture(GL_TEXTURE_2D, Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Image.Width, Image.Height, 0, SourcePixelFormat, GL_UNSIGNED_BYTE, Image.Data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return Texture;
}


