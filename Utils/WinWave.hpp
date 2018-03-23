// Author: Alex Ranaldi  W2AXR
// alexranaldi@gmail.com

// LICENSE: GNU General Public License v3.0
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.

#include <vector>
#include <cstdint>

// Windows audio device API
#include <mmsystem.h>


// A simple class to encapsulate writing to Wave Out devices. This was
//  developed for writing demodulated audio from a Software Defined Radio 
//  receiver to Virtual Audio Cables.
class WinWave {

 // Public methods
 public:
      
    WinWave(void);

    virtual ~WinWave(void);
    
    // Returns the number of Wave Out audio devices 
    size_t getNumDevices();

    // Returns a string describing the given Wave Out audio device
    std::string getDeviceDescription(const size_t devNum);

    // Returns a string containing the given Wave Out audio device name
    std::string getDeviceName(const size_t devNum);

    // Gets the Wave Out audio device numeric id, from a string name
    bool getDeviceByName(const std::string devName, size_t &devNum);

    // Initializes the given Wave Out audio device
    bool initialize(const size_t devNum, const uint64_t Fs, const size_t bufferLen);
    
    // Writes audio samples to the Wave Out audio device
    bool write(float* samples, const size_t numSamples, const float scaleFactor);

    void stop();
    
    void enablePrintClipped();


protected:

private:

    // The device id for the Wave Out audio device
    size_t mDevNum;
    // Number of samples to use for buffers
    size_t mBufferLen;
    HWAVEOUT mWaveOut;
    // Sample rate in Hz
    uint64_t mFs;

    std::vector<WAVEHDR> mHeaders;
    std::vector<int32_t*> mBuffers;
    size_t mBufferIndex;

    bool mInitialized;
    bool mPrintClipped;
    
    // Number of internal buffers. Each buffer has length mBufferLen (in units of samples)
	static const size_t NUM_BUFFERS = 64;
    // Number of bits per audio sample
    static const size_t BITS_PER_SAMPLE = 32;
    // Number of channels. 1 for Mono
	static const size_t NUM_CHANNELS = 1;
    // Maximum value represented at Sample Rate=BITS_PER_SAMPLE
    // Minimum value is -MAX_VAL
	static const int32_t MAX_VAL = 2147483647; // 2^(32-1)-1

};
