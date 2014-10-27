//
// WaveOut.cpp - implementation of class for sending data to waveout device
//
#include <Windows.h>
#include <ipp.h>
#include "WaveOut.h"

///////////////////////////////////////////////////////////////////////////////
// Construction
CWaveOut::CWaveOut(void)
{
 // inicialize data members
 m_DevID = 0;
 m_Dev = NULL; 
 m_B = NULL;
 m_nBlock = 0;
 m_MaxBlockSamples = 0;
 m_MaxBlockBytes = 0;
 m_Idx = 0;
 
 // set some format
 memset(&m_Fmt, 0, sizeof(m_Fmt));
 m_Fmt.wFormatTag = WAVE_FORMAT_PCM;
 m_Fmt.nChannels = 2;
 m_Fmt.nSamplesPerSec = 48000;
 m_Fmt.wBitsPerSample = 16;
 m_Fmt.nBlockAlign = m_Fmt.nChannels * m_Fmt.wBitsPerSample / 8;
 m_Fmt.nAvgBytesPerSec = m_Fmt.nSamplesPerSec * m_Fmt.nBlockAlign;
 m_Fmt.cbSize = 0;
}

///////////////////////////////////////////////////////////////////////////////
// Open device
bool CWaveOut::Start(int DevID, int SmplRate, int Channels, int nBlock, int MaxBlockSamples)
{int i;

 // if we are already started, stop 
 if (m_Dev != NULL) Stop();
 
 // update audio format
 m_Fmt.nSamplesPerSec = SmplRate;
 m_Fmt.nChannels = Channels;
 m_Fmt.nBlockAlign = m_Fmt.nChannels * m_Fmt.wBitsPerSample / 8;
 m_Fmt.nAvgBytesPerSec = m_Fmt.nSamplesPerSec * m_Fmt.nBlockAlign;

 // save max length of block in samples and comute max block length in bytes
 m_MaxBlockSamples = MaxBlockSamples;
 m_MaxBlockBytes = m_Fmt.nBlockAlign * MaxBlockSamples;

 // try to open waveout device
 m_DevID = DevID;
 if (waveOutOpen(&m_Dev, m_DevID, &m_Fmt, (DWORD_PTR)CWaveOut_CB, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) 
 {m_Dev = NULL; return(false);}
 
 // allocate memory for array of output blocks 
 m_nBlock = nBlock;
 if (m_nBlock < 2) m_nBlock = 2;
 m_B = (WAVEHDR *)ippsMalloc_8u(m_nBlock * sizeof(WAVEHDR));
 if (m_B == NULL) goto err;
 
 // allocate memory for output blocks
 for (i = 0; i < m_nBlock; i++)
 {// for safety ...
  memset(m_B+i, 0, sizeof(m_B[i])); 
 
  // allocate memory
  m_B[i].lpData = (LPSTR)ippsMalloc_8u(m_MaxBlockBytes);
  if (m_B[i].lpData == NULL) goto err;
  m_B[i].dwBufferLength = m_MaxBlockBytes;
  
  // clear it
  memset(m_B[i].lpData, 0, m_B[i].dwBufferLength);
  
  // prepare it
  if (waveOutPrepareHeader(m_Dev, m_B+i, sizeof(m_B[i])) != MMSYSERR_NOERROR) goto err;
  
  // send it into the device queue
  if (waveOutWrite(m_Dev, m_B+i, sizeof(m_B[i])) != MMSYSERR_NOERROR) goto err;
 }
 
 // we start from the first block
 m_Idx = 0;
 
 // success
 return(true);
 
 // error 
 err:
 Stop();
 return(false);
}
     
///////////////////////////////////////////////////////////////////////////////
// Close device
void CWaveOut::Stop(void)
{int i;

 // are we started ?
 if (m_Dev != NULL) return;

 // remove all blocks from queue
 waveOutReset(m_Dev);

 // free output blocks
 if (m_B != NULL)
 {for (i = 0; i < m_nBlock; i++)
  {// if block is prepared, unprepare it
   if ((m_B[i].dwFlags & WHDR_PREPARED) == WHDR_PREPARED)
    waveOutUnprepareHeader(m_Dev, m_B+i, sizeof(m_B[i]));
  
   // if block has data, free it
   if (m_B[i].lpData != NULL) ippsFree(m_B[i].lpData);
  }
 
  // free array
  ippsFree(m_B);
  m_B = NULL;
 }
 
 // close device
 waveOutClose(m_Dev);
 m_Dev = NULL;
}
     
//////////////////////////////////////////////////////////////////////////////
// Data output
bool CWaveOut::Output(float *pData, int nSamples, int scaleFactor)
{
 // routine check
 if ((pData == NULL)|| (nSamples < 1)) return(false);
 
 // have we started ?
 if (m_Dev == NULL) return(false);
 
 // have we free block ?
 if ((m_B[m_Idx].dwFlags & WHDR_INQUEUE) == WHDR_INQUEUE) return(false);
 
 // if we have too much samples, cut it
 if (nSamples > m_MaxBlockSamples) nSamples = m_MaxBlockSamples;
 
 // convert data into shorts
 ippsConvert_32f16s_Sfs((Ipp32f *)pData, (Ipp16s *)m_B[m_Idx].lpData, nSamples*m_Fmt.nChannels, ippRndNear, scaleFactor);

 // set right data length
 m_B[m_Idx].dwBufferLength = m_Fmt.nBlockAlign * nSamples;
 
 // schedule it to play
 if (waveOutWrite(m_Dev, m_B+m_Idx, sizeof(m_B[m_Idx])) != MMSYSERR_NOERROR) return(false);
 
 // goto to the next block
 m_Idx = (m_Idx + 1) % m_nBlock;
 
 // success
 return(true);
}

//////////////////////////////////////////////////////////////////////////////
// Get number of output blocks that are not in the output queue
int CWaveOut::BlocksToFill(void)
{int Ret = 0;
 int i;

 // for each block ...
 for (i = 0; i < m_nBlock; i++)
  if ((m_B[i].dwFlags & WHDR_INQUEUE) != WHDR_INQUEUE) Ret++;
  
 // return result
 return(Ret); 
}

//////////////////////////////////////////////////////////////////////////////
// Real callback function for messages from device
void CALLBACK CWaveOut_CB(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{CWaveOut *pInst = (CWaveOut *)dwInstance;
 
 // check instance pointer and message
 if ((pInst == NULL) || (uMsg != WOM_DONE)) return;

 // call right function
 pInst->OnBlockFinished((WAVEHDR *)dwParam1);
}
