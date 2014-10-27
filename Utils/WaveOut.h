//
// WaveOut.h - definition of class for sending data to waveout device
//
#include <mmsystem.h>

///////////////////////////////////////////////////////////////////////////////
// Class for sending data to waveout device
class CWaveOut {

 // Public methods
 public:
      
   // Construction & Destruction
   CWaveOut(void);
   ~CWaveOut(void) {Stop();}

   // Open device
   bool Start(int DevID, int SmplRate, int Channels, int nBlock, int MaxBlockSamples);
     
   // Close device
   void Stop(void);
     
   // Data output
   bool Output(float *pData, int nSamples, int scaleFactor);
   
   // Get number of output blocks that are not in the output queue
   int BlocksToFill(void);
  
    
 // Data members
 protected:

   // DeviceID
   int  m_DevID;
     
   // Handle to waveout device
   HWAVEOUT  m_Dev;

   // Audio data format
   WAVEFORMATEX  m_Fmt;
     
   // Array of output blocks
   WAVEHDR  *m_B;
   int  m_nBlock;

   // Maximum number of samples and bytes per block
   int  m_MaxBlockSamples;
   int  m_MaxBlockBytes;

   // Index of the current block
   int  m_Idx;     
   
 // Internal methods
 protected:

   // Function called when device is finished with a data block
   virtual void OnBlockFinished(WAVEHDR *pBlock) {}

   // Real callback function for messages from device
   friend void CALLBACK CWaveOut_CB(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2);
    
};
