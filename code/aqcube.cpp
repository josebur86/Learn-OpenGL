#include "aqcube.h"

#include <cmath>

static void Render(game_back_buffer *BackBuffer, game_state *GameState)
{
    int32 *Pixel = (int32 *)BackBuffer->Memory;
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            uint8 b = GameState->OffsetX + XIndex;
            uint8 g = GameState->OffsetY + YIndex;
            uint8 r = GameState->OffsetX + XIndex + GameState->OffsetY + YIndex;
            *Pixel++ = (b << 0 | g << 8 | r << 16);
        }
    }
}

void UpdateGameAndRender(game_memory *Memory, game_back_buffer *BackBuffer, game_sound_buffer *SoundBuffer, game_controller_input *Input)
{
    game_state *GameState= (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->OffsetX = 0;
        GameState->OffsetY = 0;
        GameState->ToneHz = 256;

        Memory->IsInitialized = true;
    }
    if (Input->Up.IsDown)
    {
        GameState->OffsetY -= 10;
        GameState->ToneHz += 10;
    }
    if (Input->Down.IsDown)
    {
        GameState->OffsetY += 10;
        GameState->ToneHz -= 10;
    }
    if (Input->Left.IsDown)
    {
        GameState->OffsetX -= 10;
    }
    if (Input->Right.IsDown)
    {
        GameState->OffsetX += 10;
    }

    Render(BackBuffer, GameState);
    GetSoundSamples(SoundBuffer, GameState);
}

void GetSoundSamples(game_sound_buffer *SoundBuffer, game_state* GameState)
{
    // TODO(joe): This is pretty hacky but the real game won't be generating sounds
    // using sine anyway. Remove this when we can actually play sound files.
    static float tSine = 0.0f;

    float WavePeriod = (float)SoundBuffer->SamplesPerSec / GameState->ToneHz;

    int16 *Sample = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        uint16 ToneValue = (uint16)(SoundBuffer->ToneVolume*SineValue);

        *Sample++ = ToneValue; // Left
        *Sample++ = ToneValue; // Right

        tSine += 2*PI32 * 1.0f/WavePeriod;
    }
}
