#include <windows.h>
#include <windowsx.h>
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
#include "win32_aqcube_opengl.cpp"

struct win32_back_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
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
static LARGE_INTEGER GlobalPerfFrequencyCount;

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

int GlobalLastMouseX = -1;
int GlobalLastMouseY = -1;

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
            case WM_MOUSEMOVE:
                {
                    Input->Mouse.x = GET_X_LPARAM(Message.lParam);
                    Input->Mouse.y = GET_Y_LPARAM(Message.lParam);
                } break;
            default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
        }
    }
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

            RECT ClipRect;
            RECT PreviousClipRect;
            GetClipCursor(&PreviousClipRect);
            GetWindowRect(Window, &ClipRect);
            ClipCursor(&ClipRect);
            ShowCursor( 0 );

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

            glm::vec3 CameraPos(0.0f, 0.0f, 3.0f);
            glm::vec3 CameraFront(0.0f, 0.0f, -1.0f);
            glm::vec3 CameraUp(0.0f, 1.0f, 0.0f);

            float Pitch = 0.0f;
            float Yaw = -90.0f;

            while(GlobalRunning)
            {
                Win32ProcessPendingMessages(&Input);

                float CameraSpeed = 0.05f;
                if (Input.Up.IsDown)
                {
                    CameraPos += CameraSpeed * CameraFront;
                }
                if (Input.Down.IsDown)
                {
                    CameraPos -= CameraSpeed * CameraFront;
                }
                if (Input.Left.IsDown)
                {
                    CameraPos -= glm::normalize(glm::cross(CameraFront, CameraUp)) * CameraSpeed;
                }
                if (Input.Right.IsDown)
                {
                    CameraPos += glm::normalize(glm::cross(CameraFront, CameraUp)) * CameraSpeed;
                }

                if (GlobalLastMouseX < 0)
                    GlobalLastMouseX = Input.Mouse.x;
                if (GlobalLastMouseY < 0)
                    GlobalLastMouseY = Input.Mouse.y;

                float Sensitivity = 0.1f;
                float XOffset = (Input.Mouse.x - GlobalLastMouseX) * Sensitivity;
                float YOffset = (GlobalLastMouseY - Input.Mouse.y) * Sensitivity;
                GlobalLastMouseX = Input.Mouse.x;
                GlobalLastMouseY = Input.Mouse.y;

                Yaw += XOffset;
                Pitch += YOffset;
                if (Pitch > 89.0f)
                {
                    Pitch = 89.0f;
                }
                if (Pitch < -89.0f)
                {
                    Pitch = -89.0f;
                }

                glm::vec3 Front;
                Front.x = cos(DEG_TO_RAD(Pitch)) * cos(DEG_TO_RAD(Yaw));
                Front.y = sin(DEG_TO_RAD(Pitch));
                Front.z = cos(DEG_TO_RAD(Pitch)) * sin(DEG_TO_RAD(Yaw));
                CameraFront = glm::normalize(Front);

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                float t = Win32GetElapsedSeconds(StartTime, Win32GetClock());

                glm::mat4 View;
                View = glm::lookAt(CameraPos, CameraPos + CameraFront, CameraUp);

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

            ClipCursor(&PreviousClipRect);
        }
    }

    return 0;
}
