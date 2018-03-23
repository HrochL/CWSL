// Alex Ranaldi  W2AXR   alexranaldi@gmail.com

// LICENSE: GNU General Public License v3.0
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.

#include <string>
#include <iostream>

#include <Windows.h>
#include <ipp.h>

#include "WinWave.hpp"

///////////////////////////////////////////////////////////////////////////////
// Construction
WinWave::WinWave() : 
    mInitialized(false),
    mFs(0),
    mPrintClipped(false) {
    mBuffers.resize(NUM_BUFFERS);
    mHeaders.resize(NUM_BUFFERS);
}

WinWave::~WinWave() {
    stop();
}

bool WinWave::initialize(const size_t devNum, const uint64_t Fs, const size_t bufferLen) {

    // if already initialized
    if (mInitialized) {
        return false;
    }

    // Sample rate in Hz
    mFs = Fs;
    // Device Id for the Wave Out device
    mDevNum = devNum;
    // Number of samples that will be passed per call to write()
    mBufferLen = bufferLen;

    WAVEFORMATEX format;

    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = NUM_CHANNELS;
    format.nSamplesPerSec = Fs;
    format.wBitsPerSample = BITS_PER_SAMPLE;
    // nBlockAlign = num chans * bits/sample / 8, per Microsoft doc
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8; 
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0; // ignored

    // Open the Wave Out audio device
    const MMRESULT res = waveOutOpen(&mWaveOut, devNum, &format, NULL, NULL, CALLBACK_NULL);
    mInitialized = res == MMSYSERR_NOERROR;
    // Was the device initialized?
    if (mInitialized) {
        // Initialization OK, so Allocate buffers
        for (size_t k = 0; k < NUM_BUFFERS; ++k) {
            mBuffers[k] = (int32_t*)malloc(sizeof(int32_t) * mBufferLen);
        }
        mBufferIndex = 0;
    }
    return mInitialized;
}

void WinWave::enablePrintClipped(){
    mPrintClipped = true;
}

bool WinWave::write(float* samples, const size_t numSamples, const float scaleFactor) {

    if (numSamples > mBufferLen) {
        return false;
    }

    const size_t nChannels = 1;

    WAVEHDR& header = mHeaders[mBufferIndex];
    int32_t* buffer = mBuffers[mBufferIndex];

    for (size_t k = 0; k < numSamples; ++k) {
        const float val = samples[k] * scaleFactor;
        if ( mPrintClipped && ((val >= MAX_VAL) || (val <= -MAX_VAL)) ) {
            std::cout << "Clip" << std::endl;
        }
        // float -> int32_t
        buffer[k] = static_cast<int32_t>(val);
    }

    header.lpData = (LPSTR)buffer;
    header.dwBufferLength = numSamples * sizeof(int32_t);
    header.dwFlags = 0;

    MMRESULT mmr = waveOutPrepareHeader(mWaveOut, &header, sizeof(WAVEHDR));
    bool ok = mmr == MMSYSERR_NOERROR;
    if (!ok) {
        return false;
    }
    mmr = waveOutWrite(mWaveOut, &header, sizeof(WAVEHDR));
    ok = mmr == MMSYSERR_NOERROR;
    if (ok) {
        // Go to next buffer
        mBufferIndex++;
        if (mBufferIndex == NUM_BUFFERS - 1) {
            mBufferIndex = 0;
        }
    }

    return ok;
}

std::string WinWave::getDeviceDescription(const size_t devNum) {
    WAVEOUTCAPS caps = {};
    const MMRESULT mmr = waveOutGetDevCaps(devNum, &caps, sizeof(caps));
    if (MMSYSERR_NOERROR != mmr) {
        return "";
    }
    return std::to_string ((long double)devNum) + " " + caps.szPname;
}

std::string WinWave::getDeviceName(const size_t devNum) {
    WAVEOUTCAPS caps = {};
    const MMRESULT mmr = waveOutGetDevCaps(devNum, &caps, sizeof(caps));
    if (MMSYSERR_NOERROR != mmr) {
        return "";
    }
    return std::string(caps.szPname);
}

bool WinWave::getDeviceByName(const std::string devName, size_t &devNum) {
    const size_t numDevices = getNumDevices();
    for (size_t k = 0; k < numDevices; ++k) {
        const std::string testName = getDeviceName(k);
        const int same = testName.compare(0, devName.length(), devName);
        // 0 means strings equal
        if (0 == same) {
            devNum = k;
            return true;
        }
    }
    return false;
}

size_t WinWave::getNumDevices() {
    return waveOutGetNumDevs();
}

void WinWave::stop() {
    if (mInitialized){
        // Free memory in each buffer
        for (size_t k = 0; k < NUM_BUFFERS; ++k) {
            free(mBuffers[k]);
        }
    }
    mInitialized = false;
}
