// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "../MIPS/MIPS.h"
#include "../Host.h"
#include "Core/CoreTiming.h"
#include "ChunkFile.h"

#include "sceAudio.h"
#include "__sceAudio.h"
#include "HLE.h"

void AudioChannel::DoState(PointerWrap &p)
{
	p.Do(reserved);
	p.Do(sampleAddress);
	p.Do(sampleCount);
	p.Do(leftVolume);
	p.Do(rightVolume);
	p.Do(format);
	p.Do(waitingThread);
	sampleQueue.DoState(p);
	p.DoMarker("AudioChannel");
}

// There's a second Audio api called Audio2 that only has one channel, I guess the 8 channel api was overkill.
// We simply map it to the first of the 8 channels.

AudioChannel chans[8];
int src; //not initialized and default 0 

// Enqueues the buffer pointer on the channel. If channel buffer queue is full (2 items?) will block until it isn't.
// For solid audio output we'll need a queue length of 2 buffers at least, we'll try that first.

// Not sure about the range of volume, I often see 0x800 so that might be either
// max or 50%?

u32 sceAudioOutputBlocking(u32 chan, u32 vol, u32 samplePtr) {
	if (vol > 0xFFFF) {
		ERROR_LOG(HLE, "sceAudioOutputBlocking() - invalid volume");
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	} else if (samplePtr == 0) {
		ERROR_LOG(HLE, "sceAudioOutputBlocking() - Sample pointer null");
		return 0;
	} else if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioOutputBlocking() - bad channel");
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioOutputBlocking() - channel not reserved");
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioOutputBlocking(%08x, %08x, %08x)", chan, vol, samplePtr);
		chans[chan].leftVolume = vol;
		chans[chan].rightVolume = vol;
		chans[chan].sampleAddress = samplePtr;
		return __AudioEnqueue(chans[chan], chan, true);
	}
}

u32 sceAudioOutputPannedBlocking(u32 chan, u32 leftvol, u32 rightvol, u32 samplePtr) {
	if (leftvol > 0xFFFF || rightvol > 0xFFFF) {
		ERROR_LOG(HLE, "sceAudioOutputPannedBlocking() - invalid volume");
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	} else if (samplePtr == 0) {
		ERROR_LOG(HLE, "sceAudioOutputPannedBlocking() - Sample pointer null");
		return 0;
	} else if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioOutputPannedBlocking() - bad channel");
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioOutputPannedBlocking() - channel not reserved");
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioOutputPannedBlocking(%08x, %08x, %08x, %08x)", chan, leftvol, rightvol, samplePtr);
		chans[chan].leftVolume = leftvol;
		chans[chan].rightVolume = rightvol;
		chans[chan].sampleAddress = samplePtr;
		return __AudioEnqueue(chans[chan], chan, true);
	}
}

u32 sceAudioOutput(u32 chan, u32 vol, u32 samplePtr) {
	if (vol > 0xFFFF) {
		ERROR_LOG(HLE, "sceAudioOutput() - invalid volume");
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	} else if (samplePtr == 0) {
		ERROR_LOG(HLE, "sceAudioOutput() - Sample pointer null");
		return 0;
	} else if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioOutput() - bad channel");
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved)	{
		ERROR_LOG(HLE,"sceAudioOutput(%08x, %08x, %08x) - channel not reserved", chan, vol, samplePtr);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioOutputPanned(%08x, %08x, %08x)", chan, vol, samplePtr);
		chans[chan].leftVolume = vol;
		chans[chan].rightVolume = vol;
		chans[chan].sampleAddress = samplePtr;
		return __AudioEnqueue(chans[chan], chan, false);
	}
}

u32 sceAudioOutputPanned(u32 chan, u32 leftvol, u32 rightvol, u32 samplePtr) {
	if (leftvol > 0xFFFF || rightvol > 0xFFFF) {
		ERROR_LOG(HLE, "sceAudioOutputPannedBlocking() - invalid volume");
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	} else if (samplePtr == 0) {
		ERROR_LOG(HLE, "sceAudioOutputPannedBlocking() - Sample pointer null");
		return 0;
	} else if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioOutputPanned() - bad channel");
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioOutputPanned(%08x, %08x, %08x, %08x) - channel not reserved", chan, leftvol, rightvol, samplePtr);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE,"sceAudioOutputPanned(%08x, %08x, %08x, %08x)", chan, leftvol, rightvol, samplePtr);
		chans[chan].leftVolume = leftvol;
		chans[chan].rightVolume = rightvol;
		chans[chan].sampleAddress = samplePtr;
		return __AudioEnqueue(chans[chan], chan, false);
	}
}

int sceAudioGetChannelRestLen(u32 chan) {
	if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE, "sceAudioGetChannelRestLen(%08x) - bad channel", chan);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	}
	DEBUG_LOG(HLE,"sceAudioGetChannelRestLen(%08x)", chan);
	return (int)chans[chan].sampleQueue.size() / 2;
}

int sceAudioGetChannelRestLength(u32 chan) {
	if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE, "sceAudioGetChannelRestLength(%08x) - bad channel", chan);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	}
	DEBUG_LOG(HLE,"sceAudioGetChannelRestLength(%08x)", chan);
	return (int)chans[chan].sampleQueue.size() / 2;
}

static u32 GetFreeChannel() {
	for (u32 i = 0; i < PSP_AUDIO_CHANNEL_MAX ; i++)
		if (!chans[i].reserved)
			return i;
	return -1;
}

u32 sceAudioChReserve(u32 chan, u32 sampleCount, u32 format) {
	if (chan == (u32)-1) {
		chan = GetFreeChannel();
	} 
	if (chan >= PSP_AUDIO_CHANNEL_MAX)	{
		ERROR_LOG(HLE ,"sceAudioChReserve(%08x, %08x, %08x) - bad channel", chan, sampleCount, format);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	}
	if (format != PSP_AUDIO_FORMAT_MONO && format != PSP_AUDIO_FORMAT_STEREO) {
		ERROR_LOG(HLE, "sceAudioChReserve(%08x, %08x, %08x) - invalid format", chan, sampleCount, format);
		return SCE_ERROR_AUDIO_INVALID_FORMAT;
	}
	if (chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioChReserve - reserve channel failed");
		return SCE_ERROR_AUDIO_NO_CHANNELS_AVAILABLE;
	}

	DEBUG_LOG(HLE, "sceAudioChReserve(%08x, %08x, %08x)", chan, sampleCount, format);
	chans[chan].sampleCount = sampleCount;
	chans[chan].format = format;
	chans[chan].reserved = true;
	return chan; 
}

u32 sceAudioChRelease(u32 chan) {
	if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE, "sceAudioChRelease(%i) - bad channel", chan);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	}

	if (!chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioChRelease(%i) - channel not reserved", chan);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	}
	DEBUG_LOG(HLE, "sceAudioChRelease(%i)", chan);
	chans[chan].clear();
	chans[chan].reserved = false;
	return 1;
}

u32 sceAudioSetChannelDataLen(u32 chan, u32 len) {
	if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioSetChannelDataLen(%08x, %08x) - bad channel", chan, len);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved)	{
		ERROR_LOG(HLE,"sceAudioSetChannelDataLen(%08x, %08x) - channel not reserved", chan, len);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioSetChannelDataLen(%08x, %08x)", chan, len);
		chans[chan].sampleCount = len;
		return 0;
	}
}

u32 sceAudioChangeChannelConfig(u32 chan, u32 format) {
	if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioChangeChannelConfig(%08x, %08x) - invalid channel number", chan, format);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved) {
		ERROR_LOG(HLE,"sceAudioChangeChannelConfig(%08x, %08x) - channel not reserved", chan, format);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioChangeChannelConfig(%08x, %08x)", chan, format);
		chans[chan].format = format;
		return 0;
	}
}

u32 sceAudioChangeChannelVolume(u32 chan, u32 leftvol, u32 rightvol) {
	if (leftvol > 0xFFFF || rightvol > 0xFFFF) {
		ERROR_LOG(HLE,"sceAudioChangeChannelVolume(%08x, %08x, %08x) - invalid volume", chan, leftvol, rightvol);
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	} else if (chan >= PSP_AUDIO_CHANNEL_MAX) {
		ERROR_LOG(HLE,"sceAudioChangeChannelVolume(%08x, %08x, %08x) - invalid channel number", chan, leftvol, rightvol);
		return SCE_ERROR_AUDIO_INVALID_CHANNEL;
	} else if (!chans[chan].reserved)	{
		ERROR_LOG(HLE,"sceAudioChangeChannelVolume(%08x, %08x, %08x) - channel not reserved", chan, leftvol, rightvol);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioChangeChannelVolume(%08x, %08x, %08x)", chan, leftvol, rightvol);
		chans[chan].leftVolume = leftvol;
		chans[chan].rightVolume = rightvol;
		return 0;
	}
}

u32 sceAudioInit(){
	DEBUG_LOG(HLE,"sceAudioInit()");
	// Don't need to do anything
	return 0;
}

u32 sceAudioEnd(){
	DEBUG_LOG(HLE,"sceAudioEnd()");
	// Don't need to do anything
	return 0;
}

u32 sceAudioOutput2Reserve(u32 sampleCount){
	DEBUG_LOG(HLE,"sceAudioOutput2Reserve(%08x)", sampleCount);
	chans[0].sampleCount = sampleCount;
	chans[0].reserved = true;
	return 0;
}

u32 sceAudioOutput2OutputBlocking(u32 vol, u32 dataPtr){
	DEBUG_LOG(HLE,"sceAudioOutput2OutputBlocking(%08x, %08x)", vol, dataPtr);
	chans[0].leftVolume = vol;
	chans[0].rightVolume = vol;
	chans[0].sampleAddress = dataPtr;
	return __AudioEnqueue(chans[0], 0, true);
}

u32 sceAudioOutput2ChangeLength(u32 sampleCount){
	DEBUG_LOG(HLE,"sceAudioOutput2ChangeLength(%08x)", sampleCount);
	if (!chans[0].reserved) {
		DEBUG_LOG(HLE,"sceAudioOutput2ChangeLength(%08x) - channel not reserved ", sampleCount);
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	}
	chans[0].sampleCount = sampleCount;
	return 0;
}

u32 sceAudioOutput2GetRestSample(){
	DEBUG_LOG(HLE,"sceAudioOutput2GetRestSample()");
	if (!chans[0].reserved) {
		DEBUG_LOG(HLE,"sceAudioOutput2GetRestSample() - channel not reserved ");
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	}
	return (u32) chans[0].sampleQueue.size() * 2;
}

u32 sceAudioOutput2Release(){
	DEBUG_LOG(HLE,"sceAudioOutput2Release()");
	chans[0].clear();
	chans[0].reserved = false;
	return 0;
}

u32 sceAudioSetFrequency(u32 freq) {
	if (freq == 44100 || freq == 48000) {
		INFO_LOG(HLE, "sceAudioSetFrequency(%08x)", freq);
		__AudioSetOutputFrequency(freq);
		return 0;
	} else {
		ERROR_LOG(HLE, "sceAudioSetFrequency(%08x) - invalid frequency (must be 44.1 or 48 khz)", freq);
		return SCE_ERROR_AUDIO_INVALID_FREQUENCY;
	}
}

u32 sceAudioSetVolumeOffset() {
	ERROR_LOG(HLE, "UNIMPL sceAudioSetVolumeOffset()");
	return 0;
}

u32 sceAudioSRCChReserve(u32 sampleCount, u32 freq, u32 format) {
	if (chans[src].reserved) {
		DEBUG_LOG(HLE, "sceAudioSRCChReserve(%08x, %08x, %08x) - channel already reserved ", sampleCount, freq, format);
		return SCE_ERROR_AUDIO_CHANNEL_ALREADY_RESERVED;
	} else {
		DEBUG_LOG(HLE, "sceAudioSRCChReserve(%08x, %08x, %08x)", sampleCount, freq, format);
		chans[src].reserved = true;
		chans[src].sampleCount = sampleCount;
		chans[src].format = format;
		__AudioSetOutputFrequency(freq);
	}
	return 0;
}

u32 sceAudioSRCChRelease() {
	DEBUG_LOG(HLE, "sceAudioSRCChRelease()");
	if (!chans[src].reserved) {
		DEBUG_LOG(HLE, "sceAudioSRCChRelease() - channel already reserved ");
		return SCE_ERROR_AUDIO_CHANNEL_NOT_RESERVED;
	}
	chans[src].clear();
	chans[src].reserved = false;
	return 0;
}

u32 sceAudioSRCOutputBlocking(u32 vol, u32 buf) {
	if (vol > 0xFFFF) {
		ERROR_LOG(HLE,"sceAudioSRCOutputBlocking(%08x, %08x) - invalid volume", vol, buf);
		return SCE_ERROR_AUDIO_INVALID_VOLUME;
	}
	DEBUG_LOG(HLE, "sceAudioSRCOutputBlocking(%08x, %08x)", vol, buf);
	chans[src].leftVolume = vol;
	chans[src].rightVolume = vol;
	chans[src].sampleAddress = buf;
	return __AudioEnqueue(chans[src], src, true);
}

const HLEFunction sceAudio[] = 
{
	// Newer simplified single channel audio output. Presumably for games that use Atrac3
	// directly from Sas instead of playing it on a separate audio channel.
	{0x01562ba3, WrapU_U<sceAudioOutput2Reserve>, "sceAudioOutput2Reserve"},
	{0x2d53f36e, WrapU_UU<sceAudioOutput2OutputBlocking>, "sceAudioOutput2OutputBlocking"},
	{0x63f2889c, WrapU_U<sceAudioOutput2ChangeLength>, "sceAudioOutput2ChangeLength"},
	{0x647cef33, WrapU_V<sceAudioOutput2GetRestSample>, "sceAudioOutput2GetRestSample"},	
	{0x43196845, WrapU_V<sceAudioOutput2Release>, "sceAudioOutput2Release"},
	{0x80F1F7E0, WrapU_V<sceAudioInit>, "sceAudioInit"},
	{0x210567F7, WrapU_V<sceAudioEnd>, "sceAudioEnd"},
	{0xA2BEAA6C, WrapU_U<sceAudioSetFrequency>, "sceAudioSetFrequency"},
	{0x927AC32B, WrapU_V<sceAudioSetVolumeOffset>, "sceAudioSetVolumeOffset"},
	{0x8c1009b2, WrapU_UUU<sceAudioOutput>, "sceAudioOutput"},
	{0x136CAF51, WrapU_UUU<sceAudioOutputBlocking>, "sceAudioOutputBlocking"},
	{0xE2D56B2D, WrapU_UUUU<sceAudioOutputPanned>, "sceAudioOutputPanned"},
	{0x13F592BC, WrapU_UUUU<sceAudioOutputPannedBlocking>, "sceAudioOutputPannedBlocking"}, 
	{0x5EC81C55, WrapU_UUU<sceAudioChReserve>, "sceAudioChReserve"}, 
	{0x6FC46853, WrapU_U<sceAudioChRelease>, "sceAudioChRelease"}, 
	{0xE9D97901, WrapI_U<sceAudioGetChannelRestLen>, "sceAudioGetChannelRestLen"},
	{0xB011922F, WrapI_U<sceAudioGetChannelRestLen>, "sceAudioGetChannelRestLength"},	
	{0xCB2E439E, WrapU_UU<sceAudioSetChannelDataLen>, "sceAudioSetChannelDataLen"}, 
	{0x95FD0C2D, WrapU_UU<sceAudioChangeChannelConfig>, "sceAudioChangeChannelConfig"},
	{0xB7E1D8E7, WrapU_UUU<sceAudioChangeChannelVolume>, "sceAudioChangeChannelVolume"},
	{0x38553111, WrapU_UUU<sceAudioSRCChReserve>, "sceAudioSRCChReserve"},
	{0x5C37C0AE, WrapU_V<sceAudioSRCChRelease>, "sceAudioSRCChRelease"},
	{0xE0727056, WrapU_UU<sceAudioSRCOutputBlocking>, "sceAudioSRCOutputBlocking"},

	// Never seen these used
	{0x41efade7, 0, "sceAudioOneshotOutput"},
	{0xB61595C0, 0, "sceAudioLoopbackTest"},

	// Microphone interface
	{0x7de61688, 0, "sceAudioInputInit"},
	{0xE926D3FB, 0, "sceAudioInputInitEx"},
	{0x6d4bec68, 0, "sceAudioInput"},
	{0x086e5895, 0, "sceAudioInputBlocking"},
	{0xa708c6a6, 0, "sceAudioGetInputLength"},
	{0xA633048E, 0, "sceAudioPollInputEnd"},
	{0x87b2e651, 0, "sceAudioWaitInputEnd"},
};

void Register_sceAudio()
{
	RegisterModule("sceAudio", ARRAY_SIZE(sceAudio), sceAudio);
}
