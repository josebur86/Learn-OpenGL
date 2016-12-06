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

#include <vector>
using namespace std;

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

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

GLint Win32TextureFromFile(char* FileName)
{
    GLint Result = -1;
    loaded_image Image = DEBUGLoadImage(FileName);
    if (Image.Data)
    {
        Result = Win32CreateTexture(Image, Image.PixelComponentCount == 4 ? GL_RGBA : GL_RGB);
        DEBUGFreeImage(Image);
    }

    return Result;
}

//
// Mesh
//

struct vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct texture
{
    GLuint Id;
    const char *Type;
    aiString Path;
};

class Mesh
{
    public:
        vector<vertex> Vertices;
        vector<GLuint> Indices;
        vector<texture> Textures;

        Mesh(vector<vertex> Vertices, vector<GLuint> Indices, vector<texture> Textures);
        void Draw(GLuint Program);

    private:
        GLuint VAO, VBO, EBO; // Render Data
        void SetupMesh();
};

Mesh::Mesh(vector<vertex> Vertices, vector<GLuint> Indices, vector<texture> Textures) :
    Vertices(Vertices),
    Indices(Indices),
    Textures(Textures)
{
    SetupMesh();
}

void Mesh::SetupMesh()
{
    // Generate the buffers.
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Bind the vertex buffer.
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(vertex), &Vertices[0], GL_STATIC_DRAW);

    // Bind the index buffer.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(GLuint), &Indices[0], GL_STATIC_DRAW);

    // Vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);

    // Vertex Normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)offsetof(vertex, Normal));

    // Vertex Texture Coordinates
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)offsetof(vertex, TexCoords));

    glBindVertexArray(0);
}

void Mesh::Draw(GLuint Program)
{
    //#define UNIFORM(NAME, NUMBER) sprintf_s(Uniform, sizeof(Uniform)/sizeof(Uniform[0]), "material.%s%i", (NAME), (NUMBER))
    #define UNIFORM(NAME, NUMBER) sprintf_s(Uniform, sizeof(Uniform)/sizeof(Uniform[0]), "%s%i", (NAME), (NUMBER))

    GLuint DiffuseNum = 1;
    GLuint SpecularNum = 1;

    char Uniform[64];

    for (GLuint i = 0; i < Textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);

        int number = 0;
        if (strcmp(Textures[i].Type, "texture_diffuse") == 0)
        {
            number = DiffuseNum++;
        }
        else if (strcmp(Textures[i].Type, "texture_specular") == 0)
        {
            number = SpecularNum++;
        }

        glBindTexture(GL_TEXTURE_2D, Textures[i].Id);
        UNIFORM(Textures[i].Type, number);
        glUniform1i(glGetUniformLocation(Program, Uniform), i);
    }
    glActiveTexture(GL_TEXTURE0);

    // Draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, Indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

class Model
{
    public:
        Model(GLchar *Path) { memset(&Directory, 0, 256); LoadModel(Path); }

        void Draw(GLuint Program);

    private:
        vector<Mesh> Meshes;
        char Directory[256];

        void LoadModel(const char *Path);
        void ProcessNode(aiNode *Node, const aiScene *Scene);
        Mesh ProcessMesh(aiMesh *Mesh, const aiScene *Scene);
        vector<texture> LoadMaterialTextures(aiMaterial *Material, aiTextureType Type, const char *TypeName);

        vector<texture> LoadedTextures;
};

void Model::Draw(GLuint Program)
{
    for (GLuint i = 0; i < Meshes.size(); ++i)
    {
        Meshes[i].Draw(Program);
    }
}

void Model::LoadModel(const char *Path)
{
    Assimp::Importer Import;
    const aiScene *Scene = Import.ReadFile(Path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!Scene || Scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        char ErrorString[256];
        sprintf_s(ErrorString, 256, "Error::Assimp:: %s\n", Import.GetErrorString());
        OutputDebugStringA(ErrorString);
    }

    // Set the path to the directory.
    // TODO(joe): Debug this!
    const char *Last = strrchr(Path, '/');
    int Count = Last-Path+1;
    memcpy_s(Directory, 256, Path, Count);

    ProcessNode(Scene->mRootNode, Scene);
}

void Model::ProcessNode(aiNode *Node, const aiScene *Scene)
{
    // Process all the node's meshes (if any)
    for (GLuint i = 0; i < Node->mNumMeshes; ++i)
    {
        aiMesh *Mesh = Scene->mMeshes[Node->mMeshes[i]];
        Meshes.push_back(ProcessMesh(Mesh, Scene));
    }

    // Do the same for each of its children
    for (GLuint i = 0; i < Node->mNumChildren; ++i)
    {
        ProcessNode(Node->mChildren[i], Scene);
    }
}

Mesh Model::ProcessMesh(aiMesh *Mesh, const aiScene *Scene)
{
    vector<vertex> Vertices;
    vector<GLuint> Indices;
    vector<texture> Textures;

    // Vertices
    for (GLuint i = 0; i < Mesh->mNumVertices; ++i)
    {
        vertex Vertex;

        // Position
        Vertex.Position.x = Mesh->mVertices[i].x;
        Vertex.Position.y = Mesh->mVertices[i].y;
        Vertex.Position.z = Mesh->mVertices[i].z;

        // Normal
        Vertex.Normal.x = Mesh->mNormals[i].x;
        Vertex.Normal.y = Mesh->mNormals[i].y;
        Vertex.Normal.z = Mesh->mNormals[i].z;

        // Texture Coordinates
        if (Mesh->mTextureCoords[0])
        {
            Vertex.TexCoords.x = Mesh->mTextureCoords[0][i].x;
            Vertex.TexCoords.y = Mesh->mTextureCoords[0][i].y;
        }
        else
        {
            Vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }

        Vertices.push_back(Vertex);
    }

    // Indices
    for (GLuint i = 0; i < Mesh->mNumFaces; ++i)
    {
        aiFace Face = Mesh->mFaces[i];
        for (GLuint j = 0; j < Face.mNumIndices; ++j)
        {
            Indices.push_back(Face.mIndices[j]);
        }
    }

    // Materials
    if (Mesh->mMaterialIndex >= 0)
    {
        aiMaterial *Material = Scene->mMaterials[Mesh->mMaterialIndex];

        vector<texture> DiffuseMaps = LoadMaterialTextures(Material, aiTextureType_DIFFUSE, "texture_diffuse");
        Textures.insert(Textures.end(), DiffuseMaps.begin(), DiffuseMaps.end());

        vector<texture> SpecularMaps = LoadMaterialTextures(Material, aiTextureType_SPECULAR, "texture_specular");
        Textures.insert(Textures.end(), SpecularMaps.begin(), SpecularMaps.end());
    }

    return Mesh::Mesh(Vertices, Indices, Textures);
}

vector<texture> Model::LoadMaterialTextures(aiMaterial *Material, aiTextureType Type, const char *TypeName)
{
    vector<texture> Textures;
    for (GLuint i = 0; i < Material->GetTextureCount(Type); ++i)
    {
        aiString str;
        Material->GetTexture(Type, i, &str);

        bool LoadTexture = true;
        for (size_t j = 0; j < LoadedTextures.size(); ++j)
        {
            if (LoadedTextures[j].Path == str)
            {
                Textures.push_back(LoadedTextures[i]);
                LoadTexture = false;
                break;
            }
        }
        if (LoadTexture)
        {
            char TextureFilePath[256];
            sprintf_s(TextureFilePath, 256, "%s\\%s", Directory, str.C_Str());

            texture Texture;
            Texture.Id = Win32TextureFromFile(TextureFilePath);
            Texture.Type = TypeName;
            Texture.Path = str;

            Textures.push_back(Texture);

            LoadedTextures.push_back(Texture);
        }
    }

    return Textures;
}
//
//
//

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

            glm::vec3 PointLightPositions[] = {
                glm::vec3( 0.7f,  0.2f,  2.0f),
                glm::vec3( 2.3f, -3.3f, -4.0f),
                glm::vec3(-4.0f,  2.0f, -12.0f),
                glm::vec3( 0.0f,  0.0f, -3.0f)
            };

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

            GLuint VertexShader = Win32CompileShader(GL_VERTEX_SHADER, "model.vert");
            GLuint FragmentShader = Win32CompileShader(GL_FRAGMENT_SHADER, "model.frag");
            GLuint Shaders[] = { VertexShader, FragmentShader };
            GLuint ModelProgram = Win32CreateProgram(Shaders, 2);

            Model TestModel("nanosuit/nanosuit.obj");

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

                glUseProgram(ModelProgram);

                // Set the view location.
                GLint ViewPosLoc = glGetUniformLocation(ModelProgram, "viewPos");
                glUniform3f(ViewPosLoc, Camera.Position.x, Camera.Position.y, Camera.Position.z);

                GLuint ModelLoc = glGetUniformLocation(ModelProgram, "model");
                GLuint ViewLoc = glGetUniformLocation(ModelProgram, "view");
                GLuint ProjectionLoc = glGetUniformLocation(ModelProgram, "projection");

                glUniformMatrix4fv(ViewLoc, 1, GL_FALSE, glm::value_ptr(View));
                glUniformMatrix4fv(ProjectionLoc, 1, GL_FALSE, glm::value_ptr(Projection));

                glm::mat4 Model;
                Model = glm::translate(Model, glm::vec3(0.0, -3.0f, 0.0));
                Model = glm::scale(Model, glm::vec3(0.25f, 0.25f, 0.25f));
                glUniformMatrix4fv(ModelLoc, 1, GL_FALSE, glm::value_ptr(Model));

                TestModel.Draw(ModelProgram);
#if 0
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
