//
// CWSL_Wave.cpp - export IQ data from shared memory to waveout device
//
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "../Utils/SharedMemory.h"
#include "../Utils/WaveOut.h"

// Maximum of CWSL bands
#define MAX_CWSL   32

///////////////////////////////////////////////////////////////////////////////
// Find the right band
int FindBand(int in)
{CSharedMemory SM;
 char Name[32];
 SM_HDR h;
 int i, f;

 // if input is less then maximum of CWSL bands, use it as band number
 if (in < MAX_CWSL) return(in);

 // convert input parameter into Hz
 f = 1000 * in;
 
 // try to find right band - for all possible bands ...
 for (i = 0; i < MAX_CWSL; i++)
 {// create name of shared memory
  sprintf(Name, "CWSL%dBand", i);

  // try to open shared memory
  if (SM.Open(Name))
  {// save data from header of this band
   memcpy(&h, SM.GetHeader(), sizeof(SM_HDR));
   
   // close shared memory
   SM.Close();
   
   // is frequeny into this band ?
   if ((h.SampleRate > 0) && (f >= h.L0 - h.SampleRate/2) && (f <= h.L0 + h.SampleRate/2))
   {// yes -> assign it and break the finding loop
    in = i;
    break;
   }
  }
 }

 // vrat co jsi nasel
 return(in);
}

///////////////////////////////////////////////////////////////////////////////
// Main function
void main(int argc, char **argv)
{CSharedMemory SM;
 SM_HDR *SHDR;
 CWaveOut WO;
 float Data[8192];
 char mName[32];
 int SR, BIS, SF = 16;
 int nMem = 0;
 int nWO = 0;

 // check number of input parameters
 if (argc < 3)
 {// print usage
  fprintf(stderr, "Usage: CWSL_Wave <BandNr|Frequency_in_band_in_kHz> <WaveOutNr> [Scale_factor]\n");
  
  // print the list of WaveOut devices
  fprintf(stderr, "\nWaveOut devices:\n----------------------------------------\n");
  WAVEINCAPS Caps;
  UINT D, Devs = waveInGetNumDevs();
  for (D = 0; D < Devs; D++)
  {// get device description
   waveInGetDevCaps(D, &Caps, sizeof(Caps)); 

   // print it
   fprintf(stderr, " %2i - %-32s\n", D, Caps.szPname);
  }
  
  // that's all
  return;
 }
 
 // read mandatory input parameters
 if ((sscanf(argv[1], "%d", &nMem) != 1) || (nMem < 0))
 {fprintf(stderr, "Bad Bad BandNr|Frequency_in_band_in_kHz (%s)\n", argv[1]); return;}
 if ((sscanf(argv[2], "%d", &nWO) != 1) || (nWO < -1) || (nWO > 255))
 {fprintf(stderr, "Bad WaveOutdNr (%s)\n", argv[2]); return;}

 // have we user defined scale factor ?
 if (argc > 2)
 {// yes -> try to parse it
  if ((sscanf(argv[2], "%d", &SF) != 1) || (SF < 0) || (SF > 24))
  {fprintf(stderr, "Bad Scale_factor (%s)\n", argv[2]); return;}
 }

 // find right band
 nMem = FindBand(nMem);

 // create name of shared memory
 sprintf(mName, "CWSL%dBand", nMem);

 // try to open shared memory
 if (!SM.Open(mName))
 {// can't
  fprintf(stderr, "Can't open shared memory for %d receiver\n", nMem);
  return;
 }

 // get info about channel
 SHDR = SM.GetHeader();
 SR = SHDR->SampleRate;
 BIS = SHDR->BlockInSamples;
 fprintf(stdout, "%d receiver : SampleRate=%d BlockInSamples=%d L0=%d\n", 
         nMem, SR, BIS, SHDR->L0
        );
 
 // try to open waveout device
 if (!WO.Start(nWO, SR, 2, 256, BIS))
 {// can't
  SM.Close();
  fprintf(stderr, "Can't open waveout device %d \n", nWO);
  return;
 }

 // print info
 fprintf(stdout, "%d waveout device\n", nWO);

 // main loop
 while(_kbhit() == 0)
 {// wait for new data
  SM.WaitForNewData();
  
  // read block of data
  if (!SM.Read((PBYTE)Data, 2*BIS*sizeof(float))) continue;
 
  // write it into waveout
  if (!WO.Output(Data, BIS, SF)) printf("#");
 }
  
 // cleanup
 WO.Stop();
 SM.Close();
}