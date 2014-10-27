//
// CWSL_Tee.cpp - DLL Interface for CW Skimmer Server Listener Tee
//
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "CWSL_Tee.h"
#include "../Utils/SharedMemory.h"

///////////////////////////////////////////////////////////////////////////////
// Global variables

// Name of lower library
char nLib[_MAX_PATH];

// Instance of lower library
HINSTANCE hLib = NULL;

// Types and pointers to functions exported by lower library
typedef void (__stdcall *tGetSdrInfo)(PSdrInfo pInfo);
typedef void (__stdcall *tStartRx)(PSdrSettings pSettings);
typedef void (__stdcall *tStopRx)(void);
typedef void (__stdcall *tSetRxFrequency)(int Frequency, int Receiver);
typedef void (__stdcall *tSetCtrlBits)(unsigned char Bits);
typedef int  (__stdcall *tReadPort)(int PortNumber);
tGetSdrInfo pGetSdrInfo = NULL;
tStartRx pStartRx = NULL;
tStopRx pStopRx = NULL;
tSetRxFrequency pSetRxFrequency = NULL;
tSetCtrlBits pSetCtrlBits = NULL;
tReadPort pReadPort = NULL;

// String with our descritpion
char gDesc[256];

// Info for Skimmer server
SdrInfo gInfo;

// Settings from Skimmer server
SdrSettings gSet;

// Sample rate of Skimmer server
int gSampleRate = 0;

// Length of block for one call of IQProc
int gBlockInSamples = 0;

// Center frequencies for receivers
int gL0[MAX_RX_COUNT];

// Number of current receivers
int gRxCnt = 0;

// Shared memories for teeing data
CSharedMemory gSM[MAX_RX_COUNT];

// Length of shared memories in blocks
int gSMLen = 64;

///////////////////////////////////////////////////////////////////////////////
// Printing to debug log
void Print(const char *Fmt, ...)
{va_list Args;
 time_t now;
 struct tm *ptm;
 char Line[0x1000];
 FILE *fp;
 
 // create timestamp
 time(&now);
 ptm = localtime(&now);
 strftime(Line, 100, "%d/%m %H:%M:%S ", ptm);

 // format text
 va_start(Args, Fmt);
 vsprintf(Line + strlen(Line), (const char *)Fmt, Args);
 va_end(Args);   
 
 // add new line character
 strcat(Line, "\n");

 // add to log file
 fp = fopen("CWSL_Tee.log", "at");
 if (fp != NULL)
 {// write to file an close it
  fputs(Line, fp);
  fclose(fp);
 }
}

///////////////////////////////////////////////////////////////////////////////
// Load configuration from file
void LoadConfig(void)
{FILE *fp;
 char ln[_MAX_PATH], *pc;
 int i;
 
 // initialize variables to default value
 strcpy(nLib, "Qs1rIntf");
 gSMLen = 64;

 // try to read name from config file
 fp = fopen("CWSL_Tee.cfg", "rt");
 if (fp != NULL)
 {// try to read first line (name of lower library)
  if (fgets(ln, _MAX_PATH - 1, fp))
  {// cut strange chars
   for (pc = ln; *pc != '\0'; pc++)
    if (*pc < ' ') {*pc = '\0'; break;}
    
   // copy name into global variable
   strcpy(nLib, ln);
  
   // try to read second line (length of circular buffers)
   if (fgets(ln, _MAX_PATH - 1, fp))
   {// convert it into integer
    i = atoi(ln);
    
    // if this value is reasonable, assign it
    if ((i > 2) && (i < 1024)) gSMLen = i;
   }
  }
  
  // close file
  fclose(fp);
 }

 // print configuration variables
 Print("Lower library is \"%s\"", nLib);
 Print("Length of shared memories is %d blocks", gSMLen);
}

///////////////////////////////////////////////////////////////////////////////
// DllMain function
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        Print("DLL_PROCESS_ATTACH");
        
        // load configruation from file
        LoadConfig();
        
        // try to load lower library
        hLib = ::LoadLibrary(nLib);
        if (hLib != NULL)
        {// try to find functions exported by lower library
         pGetSdrInfo     = (tGetSdrInfo)    ::GetProcAddress(hLib, "GetSdrInfo");
         pStartRx        = (tStartRx)       ::GetProcAddress(hLib, "StartRx");
         pStopRx         = (tStopRx)        ::GetProcAddress(hLib, "StopRx");
         pSetRxFrequency = (tSetRxFrequency)::GetProcAddress(hLib, "SetRxFrequency");
         pSetCtrlBits    = (tSetCtrlBits)   ::GetProcAddress(hLib, "SetCtrlBits");
         pReadPort       = (tReadPort)      ::GetProcAddress(hLib, "ReadPort");
        }
        break;
        
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	case DLL_PROCESS_DETACH:
	     Print("DLL_PROCESS_DETTACH");
	     if (hLib != NULL) ::FreeLibrary(hLib);
	     hLib = NULL;
		break;
	}
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Free alocated
void Free(void)
{int i;
 
 // close shared memories
 for (i = 0; i < MAX_RX_COUNT; i++) gSM[i].Close();
} 

///////////////////////////////////////////////////////////////////////////////
// Allocate working buffers
BOOL Alloc(void)
{char Name[32];
 SM_HDR *pHdr;
 int i;
 
 // for safety ...
 Free();

 // decode sample rate
 if (gSet.RateID == RATE_48KHZ) gSampleRate = 48000;
  else
 if (gSet.RateID == RATE_96KHZ) gSampleRate = 96000;
  else
 if (gSet.RateID == RATE_192KHZ) gSampleRate = 192000;
  else
 {// unknown sample rate
  if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Unknown sample rate");
  return(FALSE);
 } 

 // compute length of block in samples
 gBlockInSamples = (int)((float)gSampleRate / (float)BLOCKS_PER_SEC);
 
 // open shared memories
 for (i = 0; i < gRxCnt; i++)
 {// create name of memory
  sprintf(Name, "CWSL%dBand", i);
  if (!gSM[i].Create(Name, gSMLen*gBlockInSamples*sizeof(Cmplx), TRUE))
  {// can't
   if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Can't create shared memory buffer");
   return(FALSE);
  }
  
  // fill header
  pHdr = gSM[i].GetHeader();
  pHdr->SampleRate = gSampleRate;
  pHdr->BlockInSamples = gBlockInSamples;
  pHdr->L0 = 0;
 }
  
 // success
 return(TRUE);
}

///////////////////////////////////////////////////////////////////////////////
// Our callback procedure
extern "C" void __stdcall MyIQProc(int RxHandle, CmplxAA Data)
{int i;
 
 // write IQ samples to shared memories
 for (i = 0; i < gRxCnt; i++) 
 {
  gSM[i].Write((PBYTE)Data[i], gBlockInSamples*sizeof(Cmplx));
 }
  
 // call lower callback function
 (*gSet.pIQProc)(RxHandle, Data);
}

///////////////////////////////////////////////////////////////////////////////
// Get info about driver
extern "C" CWSL_TEE_API void __stdcall GetSdrInfo(PSdrInfo pInfo)
{
 Print("GetSdrInfo starting with pInfo = %08X", pInfo);
 
 // have we info ?
 if (pInfo == NULL) return;
 
 // have we loaded lower library ?
 if ((hLib != NULL) && 
     (pGetSdrInfo != NULL) && (pStartRx != NULL) && (pStopRx != NULL) &&
     (pSetRxFrequency != NULL) && (pSetCtrlBits != NULL) && (pReadPort != NULL)
    )
 {// yes -> call GetSdrInfo of lower library
  (*pGetSdrInfo)(pInfo);
 
  // modify device name
  if (pInfo->DeviceName != NULL) sprintf(gDesc, "CWSL_Tee on %s", pInfo->DeviceName);
                            else sprintf(gDesc, "CWSL_Tee on Unnamed device");
  pInfo->DeviceName = gDesc;
 
  // do local copy of SdrInfo
  memcpy(&gInfo, pInfo, sizeof(SdrInfo));
 }
  else
 {// no - report it
  pInfo->DeviceName = "CWSL_Tee without lower library";
  pInfo->MaxRecvCount = 0;
 } 

 Print("GetSdrInfo stopping");
}


///////////////////////////////////////////////////////////////////////////////
// Start of receiving
extern "C" CWSL_TEE_API void __stdcall StartRx(PSdrSettings pSettings)
{
 Print("StartRx starting with THandle=%d, RecvCount=%d, RateID=%08X, LowLatency=%d, "
       "pIQProc=%08X, pAudioProc=%08X, pStatusBitProc=%08X, pLoadProgressProc=%08X, pErrorProc=%08X",
       pSettings->THandle, pSettings->RecvCount, pSettings->RateID, pSettings->LowLatency,
       pSettings->pIQProc, pSettings->pAudioProc, pSettings->pStatusBitProc, 
       pSettings->pLoadProgressProc, pSettings->pErrorProc
      );

 // have we settings ?
 if (pSettings == NULL) return;
 
 // make a copy of SDR settings
 memcpy(&gSet, pSettings, sizeof(gSet));
 
 // from skimmer server version 1.1 in high bytes is something strange
 gSet.RateID &= 0xFF;

 // save number of receivers
 gRxCnt = gSet.RecvCount;

 // allocate buffers 
 if (!Alloc()) 
 {// something wrong ...
  Free();
  return;
 }

 // redirect IQ callback routine
 pSettings->pIQProc = MyIQProc;

 // call lower function
 (*pStartRx)(pSettings);

 // bring back original callback routine
 pSettings->pIQProc = gSet.pIQProc;
 
 Print("StartRx stopping");
}

///////////////////////////////////////////////////////////////////////////////
// Stop of receiving
extern "C" CWSL_TEE_API void __stdcall StopRx(void)
{
 Print("StopRx starting");

 // call lower function
 (*pStopRx)();

 // free all
 Free();

 Print("StopRx stopping");
}

///////////////////////////////////////////////////////////////////////////////
// Set center frequency of Receiver-th receiver
extern "C" CWSL_TEE_API void __stdcall SetRxFrequency(int Frequency, int Receiver)
{SM_HDR *pHdr;

 Print("SetRxFrequency starting with Frequency=%d, Receiver=%d", Frequency, Receiver);

 // save it
 if ((Receiver > -1) && (Receiver < gRxCnt)) 
 {// into our variable
  gL0[Receiver] = Frequency;
  
  // into shared variable
  pHdr = gSM[Receiver].GetHeader();
  if (pHdr != NULL) pHdr->L0 = Frequency;
 }
 
 // call lower function
 (*pSetRxFrequency)(Frequency, Receiver);

 Print("SetRxFrequency stopping");
}

///////////////////////////////////////////////////////////////////////////////
//
extern "C" CWSL_TEE_API void __stdcall SetCtrlBits(unsigned char Bits)
{
 Print("SetCtrlBits starting with Bits=%d", Bits);

 // call lower function
 (*pSetCtrlBits)(Bits);

 Print("SetCtrlBits stopping");
}

///////////////////////////////////////////////////////////////////////////////
//
extern "C" CWSL_TEE_API int __stdcall ReadPort(int PortNumber)
{int Res;

 Print("ReadPort starting with PortNumber=%d", PortNumber);

 // call lower function
 Res = (*pReadPort)(PortNumber);

 Print("ReadPort stopping");

 // return result
 return(Res);
}
