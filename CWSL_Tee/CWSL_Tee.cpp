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

// Master mode flag
bool  gMaster;

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

// Center frequencies for receivers (Master mode)
int gL0[MAX_RX_COUNT];

// Number of current receivers
int gRxCnt = 0;

// Shared memories for teeing data
CSharedMemory gSM[MAX_RX_COUNT];

// Length of shared memories in blocks
int gSMLen = 64;

// Shared memories headers (Slave mode)
SM_HDR *gHdr[MAX_RX_COUNT];

// Data buffer (Slave mode)
CmplxA gData[MAX_RX_COUNT];

// Handle & ID of worker thread (Slave mode)
DWORD  idWrk = 0;
HANDLE hWrk = NULL;

// Stop flag (Slave mode)
volatile bool StopFlag = false;


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
// Send error
void Error(const char *Fmt, ...)
{va_list Args;
 char Line[0x1000];
 
 // for safety
 if (gSet.pErrorProc == NULL) return;
 
 // format text
 va_start(Args, Fmt);
 vsprintf(Line, (const char *)Fmt, Args);
 va_end(Args);   
 
 // save it into log
 Print("Error: %s", Line);

 // send it
 (*gSet.pErrorProc)(gSet.THandle, Line);
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
        
        // initialize Slave mode memory pointers
        for (int i = 0; i < MAX_RX_COUNT; i++) {gHdr[i] = NULL; gData[i] = NULL;}
        
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
// Detect working mode
void DetectMode(void)
{CSharedMemory SM;

 // try to open first shared memory buffer
 if (SM.Open("CWSL0Band"))
 {// success -> shared memory exist, so we run in Slave mode
  gMaster = false;
  SM.Close();
  Print("Slave mode detected");
 }
  else
 {// can't open -> shared memory still don't exist, so we run in Master mode
  gMaster = true;
  Print("Master mode detected");
 } 
}

///////////////////////////////////////////////////////////////////////////////
// Free alocated
void Free(void)
{int i;

 // free Salve mode data buffers
 for (i = 0; i < MAX_RX_COUNT; i++)
 {if (gData[i] != NULL) free(gData[i]);
  gData[i] = NULL;
 }
 
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

 // detect working mode 
 DetectMode();

 // decode sample rate
 if (gSet.RateID == RATE_48KHZ) gSampleRate = 48000;
  else
 if (gSet.RateID == RATE_96KHZ) gSampleRate = 96000;
  else
 if (gSet.RateID == RATE_192KHZ) gSampleRate = 192000;
  else
 {// unknown sample rate
  Error("Unknown sample rate (RateID=%d)", gSet.RateID);
  return(FALSE);
 } 

 // compute length of block in samples
 gBlockInSamples = (int)((float)gSampleRate / (float)BLOCKS_PER_SEC);
 
 // create/open shared memories
 for (i = 0; i < gRxCnt; i++)
 {// create name of memory
  sprintf(Name, "CWSL%dBand", i);
 
  // according to mode ...
  if (gMaster)
  {// Master -> try to create shared memory
   if (!gSM[i].Create(Name, gSMLen*gBlockInSamples*sizeof(Cmplx), TRUE))
   {// can't
    Error("Can't create shared memory buffer %d", i);
    return(FALSE);
   }
  
   // fill header
   pHdr = gSM[i].GetHeader();
   pHdr->SampleRate = gSampleRate;
   pHdr->BlockInSamples = gBlockInSamples;
   pHdr->L0 = 0;
  }
   else
  {// Slave -> try to open shared memory
   if (!gSM[i].Open(Name))
   {// can't
    Error("Can't open shared memory buffer %d", i);
    return(FALSE);
   }
  
   // check sample rate
   gHdr[i] = gSM[i].GetHeader();
   if (gHdr[i]->SampleRate != gSampleRate)
   {// can't
    Error("CWSL has different sample rate (%d/%d)", gHdr[i]->SampleRate, gSampleRate);
    return FALSE;
   }   
  } 
 }
 
 // in Slave mode ...
 if (!gMaster)
 {// ... allocate data buffers
  for (i = 0; i < MAX_RX_COUNT; i++)
  {// try to allocate memory 
   gData[i] = (CmplxA)calloc(gBlockInSamples, sizeof(Cmplx));
   if (gData[i] == NULL)
   {// can't
    Error("Can't allocate working memory");
    return FALSE;
   }
  } 
 }
  
 // success
 return(TRUE);
}

///////////////////////////////////////////////////////////////////////////////
// Our callback procedure (Master mode)
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
// Worker thread function (Slave mode)
DWORD WINAPI Worker(LPVOID lpParameter)
{bool incomplete = false;
 int i;
 
 // for sure ...
 if (gRxCnt < 1) return(0);
 
 // wait for new data on highest receiver
 gSM[gRxCnt - 1].WaitForNewData(100);

 // clear all rx buffers
 for (i = 0; i < gRxCnt; i++) gSM[i].ClearBytesToRead();
 
 // main loop
 while (!StopFlag)
 {// reset incomplete flag
  incomplete = false;
 
  // wait for new data on highest receiver
  gSM[gRxCnt - 1].WaitForNewData(100);

  // for every receiver ...
  for (i = 0; i < gRxCnt; i++)
  {// try to read data
   if (!gSM[i].Read((PBYTE)gData[i], gBlockInSamples*sizeof(Cmplx)))
   {// no data 
    incomplete = true; 
    break;
   }
  }   
 
  // have we all of data ?
  if (StopFlag || incomplete) {Print("Incomplete data !!!"); continue;}
   
  // send data to skimmer
  (*gSet.pIQProc)(gSet.THandle, gData);
 }
  
 // that's all
 return(0);
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

 // according to mode ...
 if (gMaster)
 {// Master -> redirect IQ callback routine
  pSettings->pIQProc = MyIQProc;

  // call lower function
  (*pStartRx)(pSettings);

  // bring back original callback routine
  pSettings->pIQProc = gSet.pIQProc;
 }
  else
 {// Slave -> start worker thread
  StopFlag = false;
  hWrk = CreateThread(NULL, 0, Worker, NULL, 0, &idWrk);
  if (hWrk == NULL)
  {// can't
   Error("Can't start worker thread");
   return;
  } 
 } 
 
 Print("StartRx stopping");
}

///////////////////////////////////////////////////////////////////////////////
// Stop of receiving
extern "C" CWSL_TEE_API void __stdcall StopRx(void)
{
 Print("StopRx starting");

 // according to mode ...
 if (gMaster)
 {// Master -> call lower function
  (*pStopRx)();
 }
  else
 {// Slave -> was worker thread started ?
  if (hWrk != NULL)
  {// set stop flag
   StopFlag = true;
  
   // wait for thread
   WaitForSingleObject(hWrk, 1000);

   // close thread handle 
   CloseHandle(hWrk);
   hWrk = NULL;
  }
 } 

 // free all
 Free();

 Print("StopRx stopping");
}

///////////////////////////////////////////////////////////////////////////////
// Set center frequency of Receiver-th receiver
extern "C" CWSL_TEE_API void __stdcall SetRxFrequency(int Frequency, int Receiver)
{SM_HDR *pHdr;

 Print("SetRxFrequency starting with Frequency=%d, Receiver=%d", Frequency, Receiver);

 // check receiver number
 if ((Receiver < 0) || (Receiver >= gRxCnt))
 {// bad receiver number
  Error("SetRxFrequency: Unknown receiver with number %d", Receiver);
  return;
 }
 
 // have this receiver created/opened shared memory ?
 if (!gSM[Receiver].IsOpen())
 {// no -> we can't set or check the frequency
  Error("SetRxFrequency: Receiver with number %d does not have %s shared memory", 
        Receiver, gMaster ? "created" : "opened"
       );
  return;
 }
 
 // according to mode ...
 if (gMaster)
 {// Master -> save it into our variable ...
  gL0[Receiver] = Frequency;
  
  // ... and into shared variable
  pHdr = gSM[Receiver].GetHeader();
  if (pHdr != NULL) pHdr->L0 = Frequency;
 
  // call lower function
  (*pSetRxFrequency)(Frequency, Receiver);
 }
  else
 {// Slave -> only check the frequency
  if (Frequency != gHdr[Receiver]->L0) 
  {// bad frequency
   Error("SetRxFrequency: can't change L0 from %d to %d for %d-th receiver", gHdr[Receiver]->L0, Frequency, Receiver);
   return;
  }
 }

 Print("SetRxFrequency stopping");
}

///////////////////////////////////////////////////////////////////////////////
//
extern "C" CWSL_TEE_API void __stdcall SetCtrlBits(unsigned char Bits)
{
 Print("SetCtrlBits starting with Bits=%d", Bits);

 // in Master mode ...
 if (gMaster)
 {// ... call lower function
  (*pSetCtrlBits)(Bits);
 }

 Print("SetCtrlBits stopping");
}

///////////////////////////////////////////////////////////////////////////////
//
extern "C" CWSL_TEE_API int __stdcall ReadPort(int PortNumber)
{int Res = 0;

 Print("ReadPort starting with PortNumber=%d", PortNumber);

 // in Master mode ...
 if (gMaster)
 {// ... call lower function
  Res = (*pReadPort)(PortNumber);
 }

 Print("ReadPort stopping");

 // return result
 return(Res);
}
