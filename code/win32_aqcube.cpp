#include <windows.h>
#include <dsound.h>

#include <GL\gl.h>
#include "win32_aqcube_opengl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#define PI32 3.14159265359f

#define DEG_TO_RAD(VALUE) ((VALUE)*(PI32/180.0f))

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#include "aqcube.cpp"

struct win32_back_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_sound_output
{
    DWORD BytesPerSample;
    WORD SamplesPerSec;
    DWORD BufferSize;

    uint32 RunningSamples;
    int LatencySampleCount;
    int16 ToneVolume;
};

void *ReadFile(char *Filename)
{
    void *Result = 0;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            DWORD FileSize32 = (DWORD)FileSize.QuadPart; // TODO(joe): Safe truncation?
            Result = VirtualAlloc(0, FileSize32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (Result)
            {
                DWORD BytesRead = 0;
                ReadFile(FileHandle, Result, FileSize32, &BytesRead, 0);
                if (BytesRead == FileSize32)
                {
                    // Success
                }
                else
                {
                    FreeMemory(Result);
                }
            }
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

void FreeMemory(void *Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
        Memory = 0;
    }
}

struct loaded_image
{
    int Width;
    int Height;
    int PixelComponentCount;
    unsigned char *Data;
};
loaded_image DEBUGLoadImage(char *FileName, bool FlipVertically = false)
{
    loaded_image Result = {};

    if (FlipVertically)
    {
        stbi_set_flip_vertically_on_load(1);
    }
    Result.Data = stbi_load(FileName, &Result.Width, &Result.Height, &Result.PixelComponentCount, 0);
    if (FlipVertically)
    {
        stbi_set_flip_vertically_on_load(0);
    }
    if (!Result.Data)
    {
        OutputDebugStringA(stbi_failure_reason());
    }

    return Result;
};

void DEBUGFreeImage(loaded_image Image)
{
    if (Image.Data)
    {
        stbi_image_free(Image.Data);
        Image.Data = 0;
    }
}

static bool GlobalRunning = true;

static win32_back_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
static LARGE_INTEGER GlobalPerfFrequencyCount;

static void Win32ResizeBackBuffer(win32_back_buffer *Buffer, int Width, int Height)
{
    // TODO(joe): There is some concern that the VirtualAlloc could fail which
    // would leave us without a buffer. See if there's a better way to handle
    // this memory reallocation.
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    // TODO(joe): Treat the buffer as top-down for now. It might be better to
    // treat the back buffer as bottom up in the future.
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB; // Uncompressed: The value for blue is in the least significant 8 bits, followed by 8 bits each for green and red.
                                                   // BB GG RR XX
                                                   //
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    int BufferMemorySize = Buffer->Height * Buffer->Width * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BufferMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void Win32PaintBackBuffer(HDC DeviceContext, win32_back_buffer *BackBuffer)
{
    int OffsetX = 10;
    int OffsetY = 10;

    // Note(joe): For right now, I'm just going to blit the buffer as-is without any stretching.
    StretchDIBits(DeviceContext,
                  OffsetX, OffsetY, BackBuffer->Width, BackBuffer->Height, // Destination
                  0, 0, BackBuffer->Width, BackBuffer->Height, // Source
                  BackBuffer->Memory,
                  &BackBuffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

inline static LARGE_INTEGER Win32GetClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline static float Win32GetElapsedSeconds(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    float Result = ((float)(End.QuadPart - Start.QuadPart)) /
                    (float)GlobalPerfFrequencyCount.QuadPart;
    return Result;
}

static LRESULT CALLBACK Win32MainCallWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;
        case WM_PAINT:
        {
            OutputDebugStringA("Paint\n");

            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);

            // Clear the entire client window to black.
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int ClientWidth = ClientRect.right - ClientRect.left;
            int ClientHeight = ClientRect.bottom - ClientRect.top;
            PatBlt(DeviceContext, 0, 0, ClientWidth, ClientHeight, BLACKNESS);

            Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);

            EndPaint(Window, &Paint);
        } break;
        default:
        {
            // OutputDebugStringA("default\n");
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }


    return Result;
}

static void Win32ProcessButtonState(button_state *Button, bool IsDown)
{
    assert(Button->IsDown != IsDown);
    Button->IsDown = IsDown;
}

static void Win32ProcessPendingMessages(game_controller_input *Input)
{
    // Process the message pump.
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
                {
                    GlobalRunning = false;
                } break;
            case WM_KEYUP:
            case WM_KEYDOWN:
                {
                    uint32 KeyCode = (uint32)Message.wParam;
                    bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                    bool WasDown = ((Message.lParam & (1 << 30)) != 0);

                    if (IsDown != WasDown)
                    {
                        if (KeyCode == 'W')
                        {
                            Win32ProcessButtonState(&Input->Up, IsDown);
                        }
                        else if (KeyCode == 'S')
                        {
                            Win32ProcessButtonState(&Input->Down, IsDown);
                        }
                        else if (KeyCode == 'A')
                        {
                            Win32ProcessButtonState(&Input->Left, IsDown);
                        }
                        else if (KeyCode == 'D')
                        {
                            Win32ProcessButtonState(&Input->Right, IsDown);
                        }
                    }
                } break;
            default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
        }
    }
}

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

GLuint Win32CreateTexture(loaded_image Image, GLint SourcePixelFormat, GLenum TextureUnit)
{
    GLuint Texture;
    glGenTextures(1, &Texture);
    glActiveTexture(TextureUnit);
    glBindTexture(GL_TEXTURE_2D, Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Image.Width, Image.Height, 0, SourcePixelFormat, GL_UNSIGNED_BYTE, Image.Data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return Texture;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainCallWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = "AdequateCubeWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        int ScreenWidth = 800;
        int ScreenHeight = 600;
        HWND Window = CreateWindowEx( 0,
                                      WindowClass.lpszClassName,
                                      "Adequate Cube",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      ScreenWidth, //CW_USEDEFAULT,
                                      ScreenHeight, //CW_USEDEFAULT,
                                      0,
                                      0,
                                      Instance,
                                      0);
        if (Window)
        {
            QueryPerformanceFrequency(&GlobalPerfFrequencyCount);

            HDC DeviceContext = GetDC(Window);
            HGLRC OpenGLContext = 0;
            if (DeviceContext)
            {
                OpenGLContext = Win32InitializeOpenGL(DeviceContext);
                if (OpenGLContext)
                {
                    wglMakeCurrent(DeviceContext, OpenGLContext);
                    InitOpenGLExtensions();
                }
                else
                {
                    // TODO(joe): Log
                }
            }


            // Init
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glEnable(GL_DEPTH_TEST);

            GLfloat Vertices[] = {
                 -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
                  0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
                  0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
                  0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
                 -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
                 -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

                 -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
                  0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
                  0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
                  0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
                 -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
                 -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

                 -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
                 -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
                 -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
                 -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
                 -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
                 -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

                 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
                 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
                 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
                 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
                 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

                 -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
                  0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
                  0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
                  0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
                 -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
                 -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

                 -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
                  0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
                  0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
                  0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
                 -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
                 -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
            };
            glm::vec3 CubePositions[] = {
                glm::vec3( 0.0f,  0.0f,  0.0f),
                glm::vec3( 2.0f,  5.0f, -15.0f),
                glm::vec3(-1.5f, -2.2f, -2.5f),
                glm::vec3(-3.8f, -2.0f, -12.3f),
                glm::vec3( 2.4f, -0.4f, -3.5f),
                glm::vec3(-1.7f,  3.0f, -7.5f),
                glm::vec3( 1.3f, -2.0f, -2.5f),
                glm::vec3( 1.5f,  2.0f, -2.5f),
                glm::vec3( 1.5f,  0.2f, -1.5f),
                glm::vec3(-1.3f,  1.0f, -1.5f)
            };

            GLuint Indices[] = {
                0, 1, 3,    // First triangle
                1, 2, 3     // Second triangle
            };

            GLuint VAO;
            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);

            // VBO: Vertex Buffer Object
            GLuint VBO;
            glGenBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), 0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (GLvoid *)(3*sizeof(GLfloat)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            // VBO: Vertex Buffer Object
            //GLuint EBO;
            //glGenBuffers(1, &EBO);
            //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices), Indices, GL_STATIC_DRAW);

            // Texture
            loaded_image Container = DEBUGLoadImage("container.jpg");
            GLuint Texture1 = Win32CreateTexture(Container, GL_RGB, GL_TEXTURE0);
            DEBUGFreeImage(Container);

            loaded_image Face = DEBUGLoadImage("awesomeface.png", true);
            GLuint Texture2 = Win32CreateTexture(Face, GL_RGBA, GL_TEXTURE1);
            DEBUGFreeImage(Face);

            glBindVertexArray(0);

            GLuint VertexShader = Win32CompileShader(GL_VERTEX_SHADER, "triangle.vert");
            GLuint FragmentShader = Win32CompileShader(GL_FRAGMENT_SHADER, "triangle.frag");

            GLuint Shaders[] = { VertexShader, FragmentShader };
            GLuint ShaderProgram = Win32CreateProgram(Shaders, ArrayCount(Shaders));

            LARGE_INTEGER StartTime = Win32GetClock();

            game_controller_input Input = {};
            GlobalRunning = OpenGLContext != 0;

            float ViewX = 0.0f;
            float ViewZ = -3.0f;

            while(GlobalRunning)
            {
                Win32ProcessPendingMessages(&Input);
                if (Input.Left.IsDown)
                {
                    ViewX += 0.5f;
                }
                if (Input.Right.IsDown)
                {
                    ViewX -= 0.5f;
                }
                if (Input.Up.IsDown)
                {
                    ViewZ += 0.5f;
                }
                if (Input.Down.IsDown)
                {
                    ViewZ -= 0.5f;
                }

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                float t = Win32GetElapsedSeconds(StartTime, Win32GetClock());

                glm::mat4 View;
                View = glm::translate(View, glm::vec3(ViewX, 0.0f, ViewZ));

                glm::mat4 Projection;
                Projection = glm::perspective(DEG_TO_RAD(45), (float)ScreenWidth/(float)ScreenHeight, 0.01f, 100.0f);

                glUseProgram(ShaderProgram);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, Texture1);
                glUniform1i(glGetUniformLocation(ShaderProgram, "ourTexture1"), 0);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, Texture2);
                glUniform1i(glGetUniformLocation(ShaderProgram, "ourTexture2"), 1);

                GLuint ModelLoc = glGetUniformLocation(ShaderProgram, "model");
                GLuint ViewLoc = glGetUniformLocation(ShaderProgram, "view");
                GLuint ProjectionLoc = glGetUniformLocation(ShaderProgram, "projection");

                glUniformMatrix4fv(ViewLoc, 1, GL_FALSE, glm::value_ptr(View));
                glUniformMatrix4fv(ProjectionLoc, 1, GL_FALSE, glm::value_ptr(Projection));

                glBindVertexArray(VAO);
                for (int i = 0; i < ArrayCount(CubePositions); ++i)
                {
                    glm::mat4 Model;
                    Model = glm::translate(Model, CubePositions[i]);
                    float Angle = 20.0f * i;
                    if (i % 3 == 0)
                    {
                        Angle = t * 50.0f;
                    }
                    Model = glm::rotate(Model, DEG_TO_RAD(Angle), glm::vec3(1.0f, 0.3f, 0.5f));
                    glUniformMatrix4fv(ModelLoc, 1, GL_FALSE, glm::value_ptr(Model));

                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUseProgram(0);

                SwapBuffers(DeviceContext);
            }


            wglMakeCurrent(0, 0);
            wglDeleteContext(OpenGLContext);
            DeleteDC(DeviceContext);
        }
    }

    return 0;
}
