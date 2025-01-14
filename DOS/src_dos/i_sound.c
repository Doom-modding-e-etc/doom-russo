//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 1993-2008 Raven Software
// Copyright (C) 2016-2017 Alexey Khokholov (Nuke.YKT)
// Copyright (C) 2017 Alexandre-Xavier Labonte-Lamoureux
// Copyright (C) 2017-2021 Julian Nechaevsky
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  System interface for sound.
//

#include <stdio.h>

#include "dmx.h"

#include "i_system.h"
#include "s_sound.h"
#include "i_sound.h"
#include "sounds.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomdef.h"
#include "doomstat.h"
#include "jn.h"



//
// I_StartupTimer
//

int tsm_ID = -1;

void I_StartupTimer(void)
{
#ifndef NOTIMER
    extern int I_TimerISR(void);

    printf("I_StartupTimer()\n");
    // installs master timer.  Must be done before StartupTimer()!
    TSM_Install(SND_TICRATE);
    tsm_ID = TSM_NewService(I_TimerISR, 35, 0, 0);
    if (tsm_ID == -1)
    {
        I_Error(english_language ?
                "Can't register 35 Hz timer w/ DMX library" :
                "���������� ��ॣ����஢��� 35 �� ⠩��� � ������⥪� DMX");
    }
#endif
}

void I_ShutdownTimer(void)
{
    TSM_DelService(tsm_ID);
    TSM_Remove();
}

//
// Sound header & data
//
const char snd_prefixen[]
= { 'P', 'P', 'A', 'S', 'S', 'S', 'M', 'M', 'M', 'S', 'S', 'S' };

int dmxCodes[NUM_SCARDS]; // the dmx code for a given card

int snd_SBport, snd_SBirq, snd_SBdma; // sound blaster variables
int snd_Mport; // midi variables

int snd_MusicVolume; // maximum volume for music
int snd_SfxVolume; // maximum volume for sound

int snd_SfxDevice; // current sfx card # (index to dmxCodes)
int snd_MusicDevice; // current music card # (index to dmxCodes)
int snd_DesiredSfxDevice;
int snd_DesiredMusicDevice;

void I_PauseSong(int handle)
{
    MUS_PauseSong(handle);
}

void I_ResumeSong(int handle)
{
    MUS_ResumeSong(handle);
}

void I_SetMusicVolume(int volume)
{
    MUS_SetMasterVolume(volume);
    snd_MusicVolume = volume;
}

void I_SetSfxVolume(int volume)
{
    snd_SfxVolume = volume;
}

//
// Song API
//

int I_RegisterSong(void *data)
{
    int rc = MUS_RegisterSong(data);
#ifdef SNDDEBUG
    if (rc<0) printf(english_language ?
                     "MUS_Reg() returned %d\n" :
                     "MUS_Reg() ������ ���祭��: %d\n", rc);
#endif
    return rc;
}

void I_UnRegisterSong(int handle)
{
    int rc = MUS_UnregisterSong(handle);
#ifdef SNDDEBUG
    if (rc < 0) printf(english_language ?
                       "MUS_Unreg() returned %d\n" :
                       "MUS_Unreg() ������ ���祭��: %d\n", rc);
#endif
    rc = false;
}

int I_QrySongPlaying(int handle)
{
    int rc = MUS_QrySongPlaying(handle);
#ifdef SNDDEBUG
    if (rc < 0) printf(english_language ?
                       "MUS_QrySP() returned %d\n" :
                       "MUS_QrySP() ������ ���祭��: %d\n", rc);
#endif
    return rc;
}
//
// Stops a song.  MUST be called before I_UnregisterSong().
//
void I_StopSong(int handle)
{
    int rc;
    rc = MUS_StopSong(handle);
#ifdef SNDDEBUG
    if (rc < 0) printf(english_language ?
                       "MUS_StopSong() returned %d\n" :
                       "MUS_StopSong() ������ ���祭��: %d\n", rc);
#endif
    // Fucking kluge pause
    {
        int s;
        extern volatile int ticcount;
        for (s = ticcount; ticcount - s < 10; );
    }
}

void I_PlaySong(int handle, boolean looping)
{
    int rc;
    rc = MUS_ChainSong(handle, looping ? handle : -1);
#ifdef SNDDEBUG
    if (rc < 0) printf(english_language ?
                       "MUS_ChainSong() returned %d\n" :
                       "MUS_ChainSong() ������ ���祭��: %d\n", rc);
#endif
    rc = MUS_PlaySong(handle, snd_MusicVolume);
#ifdef SNDDEBUG
    if (rc < 0) printf(english_language ?
                       "MUS_PlaySong() returned %d\n" :
                       "MUS_PlaySong() ������ ���祭��: %d\n", rc);
#endif
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];

    if (sfx->link)
    {
        sfx = sfx->link;
    }
    sprintf(namebuf, "d%c%s", snd_prefixen[snd_SfxDevice], sfx->name);
    return W_GetNumForName(namebuf);
}

int I_StartSound(int id, void *data, int vol, int sep, int pitch, int priority)
{
    // hacks out certain PC sounds
    if (snd_SfxDevice == snd_PC
    && (data == S_sfx[sfx_posact].data
     || data == S_sfx[sfx_bgact].data
     || data == S_sfx[sfx_dmact].data
     || data == S_sfx[sfx_dmpain].data
     || data == S_sfx[sfx_popain].data
     || data == S_sfx[sfx_sawidl].data))
    {
        return -1;
    }
    if (snd_pitchshift)
        return SFX_PlayPatch(data, pitch, sep, vol, 0, 100);
    return SFX_PlayPatch(data, sep, pitch, vol, 0, 100);
}

void I_StopSound(int handle)
{
    SFX_StopPatch(handle);
}

int I_SoundIsPlaying(int handle)
{
    return SFX_Playing(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    SFX_SetOrigin(handle, pitch, sep, vol);
}

//
// Sound startup stuff
//

void I_sndArbitrateCards(void)
{
    boolean gus, adlib, sb, midi, codec, ensoniq;
    int i, wait, dmxlump;

    snd_SfxVolume = 127;
    snd_SfxDevice = snd_DesiredSfxDevice;
    snd_MusicDevice = snd_DesiredMusicDevice;

    //
    // check command-line parameters- overrides config file
    //
    if (M_CheckParm("-nosound"))
    {
        snd_MusicDevice = snd_SfxDevice = snd_none;
    }
    if (M_CheckParm("-nosfx"))
    {
        snd_SfxDevice = snd_none;
    }
    if (M_CheckParm("-nomusic"))
    {
        snd_MusicDevice = snd_none;
    }

    if (snd_MusicDevice > snd_MPU && snd_MusicDevice <= snd_MPU3)
    {
        snd_MusicDevice = snd_MPU;
    }
    if (snd_MusicDevice == snd_SB)
    {
        snd_MusicDevice = snd_Adlib;
    }
    if (snd_MusicDevice == snd_PAS)
    {
        snd_MusicDevice = snd_Adlib;
    }

    //
    // figure out what i've got to initialize
    //
    gus = snd_MusicDevice == snd_GUS || snd_SfxDevice == snd_GUS;
    sb = snd_SfxDevice == snd_SB || snd_MusicDevice == snd_SB;
    ensoniq = snd_SfxDevice == snd_ENSONIQ;
    codec = snd_SfxDevice == snd_CODEC;
    adlib = snd_MusicDevice == snd_Adlib;
    midi = snd_MusicDevice == snd_MPU;

    //
    // initialize whatever i've got
    //
    if (ensoniq)
    {
        if (devparm)
        {
            printf("ENSONIQ\n");
        }
        if (ENS_Detect())
        {
            printf(english_language ?
                   "Dude.  The ENSONIQ ain't responding.\n" :
                   "��������! ���ன�⢮ ENSONIQ �� �⢥砥�.\n");
        }
    }
    if (codec)
    {
        if (devparm)
        {
            printf("CODEC p=0x%x, d=%d\n", snd_SBport, snd_SBdma);
        }
        if (CODEC_Detect(&snd_SBport, &snd_SBdma))
        {
            printf(english_language ?
                   "CODEC.  The CODEC ain't responding.\n" :
                   "��������! CODEC �� �⢥砥�.\n");
        }
    }
    if (gus)
    {
        if (devparm)
        {
            printf("GUS\n");
        }
        fprintf(stderr, "GUS1\n");
        if (GF1_Detect())
        {
            printf(english_language ?
                   "Dude.  The GUS ain't responding.\n" :
                   "��������! ���ன�⢮ GUS �� �⢥砥�.\n");
        }
        else
        {
            fprintf(stderr, "GUS2\n");
            if (commercial)
            {
                dmxlump = W_GetNumForName("dmxgusc");
            }
            else
            {
                dmxlump = W_GetNumForName("dmxgus");
            }
            GF1_SetMap(W_CacheLumpNum(dmxlump, PU_CACHE), lumpinfo[dmxlump].size);
        }

    }
    if (sb)
    {
        if (devparm)
        {
            printf("SB p=0x%x, i=%d, d=%d\n",
                   snd_SBport, snd_SBirq, snd_SBdma);
        }
        if (SB_Detect(&snd_SBport, &snd_SBirq, &snd_SBdma, 0))
        {
            printf(english_language ?
                   "SB isn't responding at p=0x%x, i=%d, d=%d\n" :
                   "SB �� �⢥砥� �� ������� p=0x%x, i=%d, d=%d\n",
                   snd_SBport, snd_SBirq, snd_SBdma);
            // [JN] Try to set again
            SB_SetCard(snd_SBport, snd_SBirq, snd_SBdma);
        }
        else
        {
            SB_SetCard(snd_SBport, snd_SBirq, snd_SBdma);
        }
        if (devparm)
        {
            printf(english_language ?
                   "SB_Detect returned p=0x%x,i=%d,d=%d\n" :
                   "SB_Detect: ������ ���祭�� p=0x%x,i=%d,d=%d\n",
                   snd_SBport, snd_SBirq, snd_SBdma);
        }
    }

    if (adlib)
    {
        if (devparm)
        {
            printf("Adlib\n");
        }
        if (AL_Detect(&wait, 0))
        {
            printf(english_language ?
                   "Dude.  The Adlib isn't responding.\n" :
                   "��������! ���ன�⢮ Adlib �� �⢥砥�.\n");
            // [JN] Try to set again
            AL_SetCard(wait, W_CacheLumpName("genmidi", PU_STATIC));
        }
        else
        {
            AL_SetCard(wait, W_CacheLumpName("genmidi", PU_STATIC));
        }
    }

    if (midi)
    {
        if (devparm)
        {
            printf("Midi\n");
        }
        if (devparm)
        {
            printf("cfg p=0x%x\n", snd_Mport);
        }
        if (MPU_Detect(&snd_Mport, &i))
        {
            printf(english_language ?
                   "The MPU-401 isn't reponding @ p=0x%x.\n" :
                   "���ன�⢮ MPU-401 �� �⢥砥� @ p=0x%x.\n", snd_Mport);
            // [JN] Try to set again
            MPU_SetCard(snd_Mport);
        }
        else
        {
            MPU_SetCard(snd_Mport);
        }
    }
}

//
// I_StartupSound
// Inits all sound stuff
//
void I_StartupSound(void)
{
    int rc;

    //
    // initialize dmxCodes[]
    //
    dmxCodes[0] = 0;
    dmxCodes[snd_PC] = AHW_PC_SPEAKER;
    dmxCodes[snd_Adlib] = AHW_ADLIB;
    dmxCodes[snd_SB] = AHW_SOUND_BLASTER;
    dmxCodes[snd_PAS] = AHW_MEDIA_VISION;
    dmxCodes[snd_GUS] = AHW_ULTRA_SOUND;
    dmxCodes[snd_MPU] = AHW_MPU_401;
    dmxCodes[snd_AWE] = AHW_AWE32;
    dmxCodes[snd_ENSONIQ] = AHW_ENSONIQ;
    dmxCodes[snd_CODEC] = AHW_CODEC;
    
    //
    // inits sound library timer stuff
    //
    I_StartupTimer();

    //
    // pick the sound cards i'm going to use
    //
    I_sndArbitrateCards();

    if (devparm)
    {
        printf(english_language ?
               "  Music device #%d & dmxCode=%d\n" :
               "  ��몠�쭮� ���ன�⢮ #%d & dmxCode=%d\n", snd_MusicDevice,
               dmxCodes[snd_MusicDevice]);
        printf(english_language ?
               "  Sfx device #%d & dmxCode=%d\n" :
               "  ��㪮��� ���ன�⢮ #%d & dmxCode=%d\n", snd_SfxDevice,
               dmxCodes[snd_SfxDevice]);
    }

    //
    // inits DMX sound library
    //
    printf(english_language ?
           "  calling DMX_Init\n" :
           "  �맮� DMX_Init\n");

    rc = DMX_Init(SND_TICRATE, SND_MAXSONGS, 
                  dmxCodes[snd_MusicDevice],
                  dmxCodes[snd_SfxDevice]);

    if (devparm)
    {
        printf(english_language ?
               "  DMX_Init() returned %d\n" :
               "  ������ ���祭�� DMX_Init: %d\n", rc);
    }
}
//
// I_ShutdownSound
// Shuts down all sound stuff
//
void I_ShutdownSound(void)
{
    int s;
    extern volatile int ticcount;
    S_PauseSound();
    s = ticcount + 30;
    //while (s != ticcount);
    DMX_DeInit();
}

void I_SetChannels(int channels)
{
    
    WAV_PlayMode(channels, vanilla ? 11025 : snd_samplerate);
}
