#pragma once

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) ((Kilobytes(Value)) * 1024)
#define Gigabytes(Value) ((Megabytes(Value)) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// Note(joe): These are service to the game provided by the platform layer.
void *ReadFile(char *Filename);
// TODO(joe): WriteFile
void FreeMemory(void *Memory);

// Note(joe): These are service to the platform layer provided by the game.
struct game_memory
{
    uint64 PermanentStorageSize;
    void *PermanentStorage; // This should always be cleared to zero.
    uint64 TransientStorageSize;
    void *TransientStorage; // This should always be cleared to zero.

    bool IsInitialized;
};

struct game_state
{
    int OffsetX;
    int OffsetY;

    int ToneHz;
};

struct button_state
{
    bool IsDown;
};
struct game_controller_input
{
    button_state Up;
    button_state Down;
    button_state Left;
    button_state Right;
};
struct game_back_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};
struct game_sound_buffer
{
    int16 *Samples;
    int SampleCount;
    int16 ToneVolume;
    int16 SamplesPerSec;
};
void UpdateGameAndRender(game_memory *Memory, game_back_buffer *BackBuffer, game_sound_buffer *SoundBuffer, game_controller_input *Input);
void GetSoundSamples(game_sound_buffer *SoundBuffer, game_state* GameState);
