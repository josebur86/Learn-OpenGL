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
static bool GlobalWindowHasFocus = false;
static RECT GlobalClipRectToRestore;
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

void Win32WarpCursor(HWND Window, int x, int y)
{
    POINT p;
    p.x = x;
    p.y = y;

    ClientToScreen(Window, &p);
    SetCursorPos(p.x, p.y);
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
        case WM_ACTIVATEAPP:
        {
            int Activate = (int)WParam;
            if (Activate)
            {
                GlobalWindowHasFocus = true;
                GetClipCursor(&GlobalClipRectToRestore);
                RECT ClipRect;
                GetWindowRect(Window, &ClipRect);
                ClipCursor(&ClipRect);
                ShowCursor(0);

                RECT WindowRect;
                GetClientRect(Window, &WindowRect);
                int x = (WindowRect.right - WindowRect.left) / 2;
                int y = (WindowRect.bottom - WindowRect.top) / 2;
                Win32WarpCursor(Window, x, y);
            }
            else
            {
                ClipCursor(&GlobalClipRectToRestore);
                ShowCursor(1);
                GlobalWindowHasFocus = false;
            }
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

struct camera
{
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
};
struct camera_angles
{
    float Pitch;
    float Yaw;
};
static void UpdateCamera(camera *Camera, camera_angles *CameraAngles, game_controller_input *Input, float CameraSpeed, int WindowCenterX, int WindowCenterY)
{
    if (Input->Up.IsDown)
    {
        Camera->Position += CameraSpeed * Camera->Front;
    }
    if (Input->Down.IsDown)
    {
        Camera->Position -= CameraSpeed * Camera->Front;
    }
    if (Input->Left.IsDown)
    {
        Camera->Position -= glm::normalize(glm::cross(Camera->Front, Camera->Up)) * CameraSpeed;
    }
    if (Input->Right.IsDown)
    {
        Camera->Position += glm::normalize(glm::cross(Camera->Front, Camera->Up)) * CameraSpeed;
    }

    float Sensitivity = 0.1f;
    float XOffset = (Input->Mouse.x - WindowCenterX) * Sensitivity;
    float YOffset = (WindowCenterY - Input->Mouse.y) * Sensitivity;

    CameraAngles->Yaw += XOffset;
    CameraAngles->Pitch += YOffset;
    if (CameraAngles->Pitch > 89.0f)
    {
        CameraAngles->Pitch = 89.0f;
    }
    if (CameraAngles->Pitch < -89.0f)
    {
        CameraAngles->Pitch = -89.0f;
    }

    glm::vec3 Front;
    Front.x = cos(DEG_TO_RAD(CameraAngles->Pitch)) * cos(DEG_TO_RAD(CameraAngles->Yaw));
    Front.y = sin(DEG_TO_RAD(CameraAngles->Pitch));
    Front.z = cos(DEG_TO_RAD(CameraAngles->Pitch)) * sin(DEG_TO_RAD(CameraAngles->Yaw));
    Camera->Front = glm::normalize(Front);
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
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glEnable(GL_DEPTH_TEST);

            // Initialize the cube.
            GLfloat Vertices[] = {
                // Positions           // Normals           // Texture Coords
                -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
                 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
                 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
                 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
                -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
                -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

                -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
                 0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
                 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
                -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
                -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

                -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
                -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
                -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
                -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
                -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
                -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

                 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
                 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
                 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
                 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
                 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

                -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
                 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
                 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
                 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
                -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
                -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

                -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
                 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
                 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
                -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
                -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
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

            glm::vec3 PointLightPositions[] = {
                glm::vec3( 0.7f,  0.2f,  2.0f),
                glm::vec3( 2.3f, -3.3f, -4.0f),
                glm::vec3(-4.0f,  2.0f, -12.0f),
                glm::vec3( 0.0f,  0.0f, -3.0f)
            };

            loaded_image DiffuseImage = DEBUGLoadImage("container2.png");
            GLuint DiffuseMap = Win32CreateTexture(DiffuseImage, GL_RGBA);

            loaded_image SpecularImage = DEBUGLoadImage("container2_specular.png");
            GLuint SpecularMap = Win32CreateTexture(SpecularImage, GL_RGBA);

            GLuint VAO;
            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);

            // VBO: Vertex Buffer Object
            GLuint VBO;
            glGenBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), 0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void *)(3*sizeof(GLfloat)));
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), (void *)(6*sizeof(GLfloat)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);

            GLuint LightVAO;
            glGenVertexArrays(1, &LightVAO);
            glBindVertexArray(LightVAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), 0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);

            GLuint VertexShader = Win32CompileShader(GL_VERTEX_SHADER, "lighting.vert");
            GLuint FragmentShader = Win32CompileShader(GL_FRAGMENT_SHADER, "lighting.frag");

            GLuint LightingShaders[] = { VertexShader, FragmentShader };
            GLuint LightingProgram = Win32CreateProgram(LightingShaders, ArrayCount(LightingShaders));

            GLuint LampVertexShader = Win32CompileShader(GL_VERTEX_SHADER, "lamp.vert");
            GLuint LampFragmentShader = Win32CompileShader(GL_FRAGMENT_SHADER, "lamp.frag");
            GLuint LampShaders[] = { LampVertexShader, LampFragmentShader };
            GLuint LampProgram = Win32CreateProgram(LampShaders, ArrayCount(LampShaders));

            LARGE_INTEGER StartTime = Win32GetClock();

            camera Camera = {};
            Camera.Position = glm::vec3(0.0f, 0.0f, 3.0f);
            Camera.Front = glm::vec3(0.0f, 0.0f, -1.0f);
            Camera.Up = glm::vec3(0.0f, 1.0f, 0.0f);

            camera_angles CameraAngles = {};
            CameraAngles.Pitch = 0.0f;
            CameraAngles.Yaw = -90.0f;

            float CameraSpeed = 0.05f;
            int WindowCenterX = ScreenWidth / 2;
            int WindowCenterY = ScreenHeight / 2;

            game_controller_input Input = {};
            GlobalRunning = OpenGLContext != 0;
            while(GlobalRunning)
            {
                Win32ProcessPendingMessages(&Input);

                if (GlobalWindowHasFocus)
                {
                    UpdateCamera(&Camera, &CameraAngles, &Input, CameraSpeed, WindowCenterX, WindowCenterY);
                    Win32WarpCursor(Window, WindowCenterX, WindowCenterY);
                }

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                float t = Win32GetElapsedSeconds(StartTime, Win32GetClock());

                glm::mat4 View;
                View = glm::lookAt(Camera.Position, Camera.Position + Camera.Front, Camera.Up);

                glm::mat4 Projection;
                Projection = glm::perspective(DEG_TO_RAD(45), (float)ScreenWidth/(float)ScreenHeight, 0.01f, 100.0f);

                float LampX = cos(DEG_TO_RAD(t*25.0f));
                float LampZ = sin(DEG_TO_RAD(t*25.0f));
#if 0
                glm::vec3 LightPos(LampX, 1.2f, LampZ);
#else
                glm::vec3 LightPos(1.2f, 1.0f, 2.0f);
#endif

                glUseProgram(LightingProgram);

                // Set the view location.
                GLint ViewPosLoc = glGetUniformLocation(LightingProgram, "viewPos");
                glUniform3f(ViewPosLoc, Camera.Position.x, Camera.Position.y, Camera.Position.z);

                glm::vec3 LightColor(1.0f, 1.0f, 1.0f);
                glm::vec3 DiffuseColor = LightColor * glm::vec3(0.5f); // Decrease the influence.
                glm::vec3 AmbientColor = DiffuseColor * glm::vec3(0.2f); // Low influence.

                // Set the direction light properties (ambient, diffuse, specular).
                glUniform3f(glGetUniformLocation(LightingProgram, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
                glUniform3f(glGetUniformLocation(LightingProgram, "dirLight.ambient"), AmbientColor.x, AmbientColor.y, AmbientColor.z);
                glUniform3f(glGetUniformLocation(LightingProgram, "dirLight.diffuse"), DiffuseColor.x, DiffuseColor.y, DiffuseColor.z);
                glUniform3f(glGetUniformLocation(LightingProgram, "dirLight.specular"), 1.0f, 1.0f, 1.0f);

                // Set the point light properties.
#define NR_POINT_LIGHTS 4
#define POINT_LIGHT_UNIFORM(Buffer, Index, Uniform) sprintf_s(Buffer, (sizeof(Buffer) / sizeof(Buffer[0])), "pointLights[%i].%s", (Index), (Uniform))
                for (int LightIndex = 0; LightIndex < NR_POINT_LIGHTS; ++LightIndex)
                {
                    char Buffer[64];

                    // Position
                    glm::vec3 *Position = PointLightPositions + LightIndex;
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "position");
                    glUniform3f(glGetUniformLocation(LightingProgram, Buffer), Position->x, Position->y, Position->z);

                    // Attenuation
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "constant");
                    glUniform1f(glGetUniformLocation(LightingProgram, Buffer), 1.0f);
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "linear");
                    glUniform1f(glGetUniformLocation(LightingProgram, Buffer), 0.09f);
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "quadratic");
                    glUniform1f(glGetUniformLocation(LightingProgram, Buffer), 0.032f);

                    // Light properties
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "ambient");
                    glUniform3f(glGetUniformLocation(LightingProgram, Buffer), AmbientColor.x, AmbientColor.y, AmbientColor.z);
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "diffuse");
                    glUniform3f(glGetUniformLocation(LightingProgram, Buffer), DiffuseColor.x, DiffuseColor.y, DiffuseColor.z);
                    POINT_LIGHT_UNIFORM(Buffer, LightIndex, "specular");
                    glUniform3f(glGetUniformLocation(LightingProgram, Buffer), 1.0f, 1.0f, 1.0f);
                }

#if 0
                // Spotlight
                GLint LightSpotDirLoc = glGetUniformLocation(LightingProgram, "light.direction");
                GLint LightSpotCutOffLoc = glGetUniformLocation(LightingProgram, "light.cutOff");
                GLint LightSpotOuterCutOffLoc = glGetUniformLocation(LightingProgram, "light.outerCutOff");

                glUniform3f(LightPosLoc, Camera.Position.x, Camera.Position.y, Camera.Position.z);
                glUniform3f(LightSpotDirLoc, Camera.Front.x, Camera.Front.y, Camera.Front.z);
                glUniform1f(LightSpotCutOffLoc, glm::cos(DEG_TO_RAD(12.5f)));
                glUniform1f(LightSpotOuterCutOffLoc, glm::cos(DEG_TO_RAD(17.5f)));
#endif

                // Set the material properties.
                glUniform1i(glGetUniformLocation(LightingProgram, "material.diffuse"),   0);
                glUniform1i(glGetUniformLocation(LightingProgram, "material.specular"),  1);
                glUniform1f(glGetUniformLocation(LightingProgram, "material.shininess"), 32.0f);

                GLuint ModelLoc = glGetUniformLocation(LightingProgram, "model");
                GLuint ViewLoc = glGetUniformLocation(LightingProgram, "view");
                GLuint ProjectionLoc = glGetUniformLocation(LightingProgram, "projection");

                glUniformMatrix4fv(ViewLoc, 1, GL_FALSE, glm::value_ptr(View));
                glUniformMatrix4fv(ProjectionLoc, 1, GL_FALSE, glm::value_ptr(Projection));

                glBindVertexArray(VAO);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, DiffuseMap);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, SpecularMap);

                glm::mat4 Model;
                for (int PositionIndex = 0; PositionIndex < ArrayCount(CubePositions); ++PositionIndex)
                {
                    Model = glm::mat4();
                    Model = glm::translate(Model, CubePositions[PositionIndex]);
                    GLfloat angle = 20.0f * PositionIndex;
                    Model = glm::rotate(Model, DEG_TO_RAD(angle), glm::vec3(1.0f, 0.3f, 0.5f));

                    glUniformMatrix4fv(ModelLoc, 1, GL_FALSE, glm::value_ptr(Model));
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }

#if 1
                glUseProgram(LampProgram);
                glBindVertexArray(LightVAO);

                glUniformMatrix4fv(glGetUniformLocation(LampProgram, "view"), 1, GL_FALSE, glm::value_ptr(View));
                glUniformMatrix4fv(glGetUniformLocation(LampProgram, "projection"), 1, GL_FALSE, glm::value_ptr(Projection));

                for (int PositionIndex = 0; PositionIndex < ArrayCount(PointLightPositions); ++PositionIndex)
                {
                    Model = glm::mat4();
                    Model = glm::translate(Model, PointLightPositions[PositionIndex]);
                    Model = glm::scale(Model, glm::vec3(0.2f));
                    glUniformMatrix4fv(glGetUniformLocation(LampProgram, "model"), 1, GL_FALSE, glm::value_ptr(Model));
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
#endif

                glBindVertexArray(0);
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
