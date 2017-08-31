//
// CWSL_File.cpp - recording IQ data from shared memory to wav file
//
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <mmreg.h>
#include <ipp.h>
#include "../Utils/SharedMemory.h"

// Maximum of CWSL bands
#define MAX_CWSL   32

// Our KSDATAFORMAT_SUBTYPE_PCM ...
unsigned char X_SUBTYPE_PCM[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

///////////////////////////////////////////////////////////////////////////////
#pragma pack(push, 1)

// Winrad wav chunk (from Winrad sources)
struct auxihdr     // used by SpectraVue in WAV files
{
//	char  	   chunkID[4];	          // ="auxi" (chunk rfspace)
//	long  	   chunkSize;	          // lunghezza del chunk
	SYSTEMTIME StartTime;
	SYSTEMTIME StopTime;
	DWORD CenterFreq; //receiver center frequency
	DWORD ADFrequency; //A/D sample frequency before downsampling
	DWORD IFFrequency; //IF freq if an external down converter is used
	DWORD Bandwidth; //displayable BW if you want to limit the display to less than Nyquist band
	DWORD IQOffset; //DC offset of the I and Q channels in 1/1000's of a count
	DWORD Unused2;
	DWORD Unused3;
	DWORD Unused4;
	DWORD Unused5;
	char  nextfilename[96];
};

// Complete WAV file header
struct WavHdr 
{
    char  _RIFF[4]; // "RIFF"
    DWORD FileLen;  // length of all data after this (FileLength - 8)
    
    char _WAVE[4];  // "WAVE"
    
    char _fmt[4];        // "fmt "
    DWORD FmtLen;        // length of the next item (sizeof(WAVEFORMATEX))
    WAVEFORMATEX Format; // wave format description   
    
    char _auxi[4];       // "auxi"    
    DWORD AuxiLen;       // length of the next item (sizeof(struct auxihdr))
    struct auxihdr Auxi; // RF setup description

    char  _data[4];  // "data"
    DWORD DataLen;   // length of the next data (FileLength - sizeof(struct WavHdr))
    
};

// Complete WAV file header for 24-bit files
struct WavHdr24 
{
    char  _RIFF[4]; // "RIFF"
    DWORD FileLen;  // length of all data after this (FileLength - 8)
    
    char _WAVE[4];  // "WAVE"
    
    char _fmt[4];        // "fmt "
    DWORD FmtLen;        // length of the next item (sizeof(WAVEFORMATEXTENSIBLE))
    WAVEFORMATEXTENSIBLE Format; // wave format description   
    
    char _auxi[4];       // "auxi"    
    DWORD AuxiLen;       // length of the next item (sizeof(struct auxihdr))
    struct auxihdr Auxi; // RF setup description

    char  _data[4];  // "data"
    DWORD DataLen;   // length of the next data (FileLength - sizeof(struct WavHdr24) - ListBufLen)
    
};

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////////
// Global variables

// Prefix and suffix for the shared memories names
char gPreSM[128], gPostSM[128];

// Band number
int nMem = 0;

// Wave file headers
struct WavHdr Hdr;
struct WavHdr24 Hdr24;

// Buffer for LIST INFO tag
char ListBuf[4096];
int ListBufLen = 0;

// File handle
HANDLE File = INVALID_HANDLE_VALUE;

// Current time
struct tm *Time;
int LastSecond = 0;
int LastMinute = 0;

// Flag for filename in UTC
BOOL FNameUTC = TRUE;

// Flag for File-Per-Hour
BOOL FilePerHour = TRUE;

// Flag for File-Per-Half
BOOL FilePerHalf = FALSE;

// Flag for 24-bit files
BOOL BPS24 = FALSE;

// Flag for alternative format of 24-bit files
BOOL BPS24Alt = FALSE;

// Flag for closing the application
BOOL  StopEvent = FALSE;

//////////////////////////////////////////////////////////////////////////////////////////////
// Console control handler
BOOL WINAPI ControlHandler(DWORD CtrlType)
{
 switch(CtrlType)
 {case CTRL_BREAK_EVENT: 
   fprintf(stdout, "\nCtrl+Break event!\n");
   StopEvent = TRUE;
   return(TRUE);
   break;

  case CTRL_C_EVENT:   
   fprintf(stdout, "\nCtrl+C event!\n");
   StopEvent = TRUE;
   return(TRUE);
   break;

  case CTRL_CLOSE_EVENT:  
   fprintf(stdout, "\nConsole closing event!\n");
   StopEvent = TRUE;
   return(TRUE);
   break;
 }
 return(FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Init LIST INFO tag
void ListInit(void)
{DWORD *pListLen = (DWORD *)(ListBuf + 4);
 
 // tag descriptors
 ListBuf[ 0] = 'L'; ListBuf[ 1] = 'I'; ListBuf[ 2] = 'S'; ListBuf[ 3] = 'T';
 ListBuf[ 8] = 'I'; ListBuf[ 9] = 'N'; ListBuf[10] = 'F'; ListBuf[11] = 'O';

 // lengths
 ListBufLen = 12;
 *pListLen = ListBufLen - 8;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Add tag into LIST INFO
void ListAdd(const char *Name, const char *Value)
{DWORD *pListLen = (DWORD *)(ListBuf + 4);
 DWORD *pTagLen = (DWORD *)(ListBuf + ListBufLen + 4);

 // add tag
 strcpy(ListBuf + ListBufLen, Name);
 *pTagLen = strlen(Value);
 strcpy(ListBuf + ListBufLen + 8, Value);
 
 // add lengths
 ListBufLen += 8 + (*pTagLen) + 1;
 *pListLen = ListBufLen - 8;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Open wav file
BOOL wavOpen(void)
{char FileName[64];
 LPCVOID h;
 DWORD hl, l;
 time_t tt;
 struct tm *ptm;
 char str[128];

 // if file is open, nothing to do
 if (File != INVALID_HANDLE_VALUE) return(TRUE);

 // create name for file
 sprintf(FileName,"B%d_%04i%02i%02i_%02i%02i%02i_%dkHz.wav", nMem, 
         1900 + Time->tm_year, Time->tm_mon + 1, Time->tm_mday, 
         Time->tm_hour, Time->tm_min, Time->tm_sec,
         BPS24 ? (Hdr24.Auxi.CenterFreq / 1000) : (Hdr.Auxi.CenterFreq / 1000)
        );
 printf("\n%s ", FileName);

 // open the file
 File = ::CreateFile(FileName, GENERIC_WRITE | GENERIC_READ, 
                        FILE_SHARE_READ, NULL, CREATE_ALWAYS, 
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, 
                        NULL
                    );
 if (File == INVALID_HANDLE_VALUE) return(FALSE);

 // fill rest of header
 if (BPS24)
 {// 24-bit version
  Hdr24.FileLen = sizeof(Hdr24) - 8;
  ::GetSystemTime(&(Hdr24.Auxi.StartTime));
  Hdr24.DataLen = 0;
  
  // initialize ListInfo
  ListInit();
  
  // add ListInfo tags
  time(&tt);
  ptm = gmtime(&tt);
  strftime(str, 127, "%Y-%m-%d %H:%M:%S UTC", ptm);
  ListAdd("QTR ", str);
  sprintf(str, "%d", Hdr24.Auxi.CenterFreq);
  ListAdd("QRG ", str);
  ListAdd("ISFT", "CWSL_File");
  ListAdd("IWEB", "https://github.com/HrochL/CWSL");
  ListAdd("CHAN", "I/Q");
  
  // use Hdr24
  h = &Hdr24;
  hl = sizeof(Hdr24);
 }
  else
 {// standard version 
  Hdr.FileLen = sizeof(Hdr) - 8;
  ::GetSystemTime(&(Hdr.Auxi.StartTime));
  Hdr.DataLen = 0;
  
  // use Hdr
  h = &Hdr;
  hl = sizeof(Hdr);
 }
 
 // write header to file
 if ((!::WriteFile(File, h, hl, &l, NULL)) || (l != hl)) 
 {::CloseHandle(File); File = INVALID_HANDLE_VALUE; return(FALSE);}

 // success
 return(TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Close wav file
void wavClose(void)
{DWORD FLen, hl, l;
 LPCVOID h;

 // if file is closed, nothing to do ?
 if (File == INVALID_HANDLE_VALUE) return;

 // get length of file
 FLen = ::SetFilePointer(File, 0, NULL, FILE_CURRENT);

 // update header data
 if (BPS24)
 {// 24-bit version
  Hdr24.FileLen = FLen + ListBufLen - 8;
  ::GetSystemTime(&(Hdr24.Auxi.StopTime));
  Hdr24.DataLen = FLen - sizeof(Hdr24);

  // use Hdr24
  h = &Hdr24;
  hl = sizeof(Hdr24);
 }
  else
 {// standard version
  Hdr.FileLen = FLen - 8;
  ::GetSystemTime(&(Hdr.Auxi.StopTime));
  Hdr.DataLen = FLen - sizeof(Hdr);
  
  // use Hdr
  h = &Hdr;
  hl = sizeof(Hdr);
 }

 // update header in the file
 ::SetFilePointer(File, 0, NULL, FILE_BEGIN);
 ::WriteFile(File, h, hl, &l, NULL);
 
 // in 24-bit version ...
 if (BPS24)
 {// ... write ListInfo at the end of file
  ::SetFilePointer(File, 0, NULL, FILE_END);
  ::WriteFile(File, &ListBuf, ListBufLen, &l, NULL);
 }
 
 // close the file
 ::CloseHandle(File);

 // invalidate handle
 File = INVALID_HANDLE_VALUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Write data into wav file
BOOL wavWrite(const char *Data, long DataLen)
{time_t Timer;
 long l;

 // get current time
 time(&Timer);
 Time = FNameUTC ? gmtime(&Timer) : localtime(&Timer);

 // have we new minute ?
 if (LastSecond > Time->tm_sec) printf(".");
 LastSecond = Time->tm_sec;
 
 // it's time to new file ?
 if (FilePerHour)
 {// one file per hour ... so have we new hour ?
  if (LastMinute > Time->tm_min) wavClose(); // yes, we will open new file
 }
  else
 {if (FilePerHalf)
  {// one file per half of hour ... so have we new half ?
   if ( ((LastMinute == 59) && (Time->tm_min ==  0)) ||
        ((LastMinute == 29) && (Time->tm_min == 30))
      ) wavClose(); // yes, we will open new file
  }
   else
  {// one file per quarter of hour ... so have we new quarter ?
   if ( ((LastMinute == 59) && (Time->tm_min ==  0)) ||
        ((LastMinute == 14) && (Time->tm_min == 15)) ||
        ((LastMinute == 29) && (Time->tm_min == 30)) ||
        ((LastMinute == 44) && (Time->tm_min == 45))
      ) wavClose(); // yes, we will open new file
  }
 } 
 
 // save the last minute
 LastMinute = Time->tm_min;
 
 // if wav file is not opened, try to do it
 if (File == INVALID_HANDLE_VALUE)
 {if (!wavOpen()) return(FALSE);}
 
 // write data
 if ((!::WriteFile(File, Data, DataLen, (LPDWORD)&l, NULL)) || (l != DataLen)) 
 {wavClose();
  return(FALSE);
 }

 // success
 return(TRUE);
}

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
  sprintf(Name, "%s%d%s", gPreSM, i, gPostSM);

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
 float fData[8192];
 short sData[8192];
 unsigned char cData[4*8192];
 char mName[32];
 int SR, BIS, SF = 16;
 char fileName[_MAX_PATH + 1];
 char *pFN, *pc;

 // create the prefix and suffix for the shared memories names
 strcpy(gPreSM, "CWSL");
 strcpy(gPostSM, "Band");
 ::GetModuleFileName(NULL, fileName, _MAX_PATH);
 #define BASE_FNAME "CWSL_File"
 pFN = strstr(fileName, BASE_FNAME);
 if (pFN != NULL)
 {
  pFN += strlen(BASE_FNAME);
  for (pc = pFN; (*pc != '\0') && (*pc != '.'); pc++);
  *pc = '\0';
  strcat(gPostSM, pFN);
 }
 
 // check number of input parameters
 if (argc < 2)
 {// print usage
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: CWSL_File <BandNr|Frequency_in_band_in_kHz> [Scale_factor] [Flags]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "       Scale_factor can be:\n");
  fprintf(stderr, "          from 0 to 24 for conversion to 16-bit samples\n");
  fprintf(stderr, "          -1 for 24-bit wav files with WAVEFORMATEXTENSIBLE in header\n");
  fprintf(stderr, "          -2 for 24-bit wav files with WAVEFORMATEX in header\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "       Flags: L - File names in local time\n");
  fprintf(stderr, "              H - One file per half of hour\n");
  fprintf(stderr, "              Q - One file per quarter of hour\n");
  fprintf(stderr, "\n");
  return;
 }
 
 // read input parameters
 if ((sscanf(argv[1], "%d", &nMem) != 1) || (nMem < 0))
 {fprintf(stderr, "Bad BandNr|Frequency_in_band_in_kHz (%s)\n", argv[1]); return;}

 // have we user defined scale factor ?
 if (argc > 2)
 {// yes -> try to parse it
  if ((sscanf(argv[2], "%d", &SF) != 1) || (SF < -2) || (SF > 24))
  {fprintf(stderr, "Bad Scale_factor (%s)\n", argv[2]); return;}
 }
 
 // use we 24-bit wav ?
 if (SF < 0)
 {// yes -> set flag
  BPS24 = TRUE;
  fprintf(stdout, "Wav files are in 24-bit format\n");
  
  // use we use an alternative format ?
  if (SF < -1)
  {// yes -> set flag
   BPS24Alt = TRUE;
   fprintf(stdout, "Wav files using WAVEFORMATEX in format tag\n");
  }
 }
  else
 {// no -> print scale factor
  fprintf(stdout, "Scale factor = %d bits\n", SF);
 }

 // have we user defined flags ?
 if (argc > 3)
 {// check flags
  for (pc = argv[3]; *pc != '\0'; pc++)
   switch (*pc)
   {// Local file names
    case 'l':
    case 'L':
     FNameUTC = FALSE;
     fprintf(stdout, "File names in local time\n");
     break;
     
    // One file per half of hour
    case 'h':
    case 'H':
     FilePerHour = FALSE;
     FilePerHalf = TRUE;
     fprintf(stdout, "One file per half of hour\n");
     break;

    // One file per quarter of hour
    case 'q':
    case 'Q':
     FilePerHour = FALSE;
     FilePerHalf = FALSE;
     fprintf(stdout, "One file per quarter of hour\n");
     break;
   }
 }
 
 // find right band
 nMem = FindBand(nMem);
 
 // create name of shared memory
 sprintf(mName, "%s%d%s", gPreSM, nMem, gPostSM);

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
 
 // fill wav file header
 if (BPS24)
 {// 24-bit version
  Hdr24._RIFF[0] = 'R'; Hdr24._RIFF[1] = 'I'; Hdr24._RIFF[2] = 'F'; Hdr24._RIFF[3] = 'F';
  Hdr24.FileLen = sizeof(Hdr24) - 8;
  Hdr24._WAVE[0] = 'W'; Hdr24._WAVE[1] = 'A'; Hdr24._WAVE[2] = 'V'; Hdr24._WAVE[3] = 'E';
  Hdr24._fmt[0] = 'f'; Hdr24._fmt[1] = 'm'; Hdr24._fmt[2] = 't'; Hdr24._fmt[3] = ' ';
  Hdr24.FmtLen = sizeof(WAVEFORMATEXTENSIBLE);
  Hdr24.Format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  Hdr24.Format.Format.nChannels = 2;
  Hdr24.Format.Format.nSamplesPerSec = SR;
  Hdr24.Format.Format.nBlockAlign = 6;
  Hdr24.Format.Format.nAvgBytesPerSec = Hdr24.Format.Format.nSamplesPerSec * Hdr24.Format.Format.nBlockAlign;
  Hdr24.Format.Format.wBitsPerSample = 24;
  Hdr24.Format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  Hdr24.Format.Samples.wValidBitsPerSample = 24;
  Hdr24.Format.dwChannelMask = 3;
  memcpy(&(Hdr24.Format.SubFormat), X_SUBTYPE_PCM, sizeof(GUID));
  Hdr24._auxi[0] = 'a'; Hdr24._auxi[1] = 'u'; Hdr24._auxi[2] = 'x'; Hdr24._auxi[3] = 'i';
  Hdr24.AuxiLen = sizeof(Hdr24.Auxi);
  memset(&(Hdr24.Auxi), 0, sizeof(Hdr24.Auxi));
  Hdr24.Auxi.CenterFreq = SHDR->L0;
  Hdr24._data[0] = 'd'; Hdr24._data[1] = 'a'; Hdr24._data[2] = 't'; Hdr24._data[3] = 'a';
  Hdr24.DataLen = 0;
  
  // alternative format ?
  if (BPS24Alt)
  {// yes -> use WAVEFORMATEX instead of WAVEFORMATEXTENSIBLE
   Hdr24.Format.Format.wFormatTag = WAVE_FORMAT_PCM;
   Hdr24.Format.Format.cbSize = 0;
  }
 }
  else
 {// standard version
  Hdr._RIFF[0] = 'R'; Hdr._RIFF[1] = 'I'; Hdr._RIFF[2] = 'F'; Hdr._RIFF[3] = 'F';
  Hdr.FileLen = sizeof(Hdr) - 8;
  Hdr._WAVE[0] = 'W'; Hdr._WAVE[1] = 'A'; Hdr._WAVE[2] = 'V'; Hdr._WAVE[3] = 'E';
  Hdr._fmt[0] = 'f'; Hdr._fmt[1] = 'm'; Hdr._fmt[2] = 't'; Hdr._fmt[3] = ' ';
  Hdr.FmtLen = sizeof(WAVEFORMATEX);
  Hdr.Format.wFormatTag = WAVE_FORMAT_PCM;
  Hdr.Format.nChannels = 2;
  Hdr.Format.nSamplesPerSec = SR;
  Hdr.Format.nBlockAlign = 4;
  Hdr.Format.nAvgBytesPerSec = Hdr.Format.nSamplesPerSec * Hdr.Format.nBlockAlign;
  Hdr.Format.wBitsPerSample = 16;
  Hdr.Format.cbSize = 0;
  Hdr._auxi[0] = 'a'; Hdr._auxi[1] = 'u'; Hdr._auxi[2] = 'x'; Hdr._auxi[3] = 'i';
  Hdr.AuxiLen = sizeof(Hdr.Auxi);
  memset(&(Hdr.Auxi), 0, sizeof(Hdr.Auxi));
  Hdr.Auxi.CenterFreq = SHDR->L0;
  Hdr._data[0] = 'd'; Hdr._data[1] = 'a'; Hdr._data[2] = 't'; Hdr._data[3] = 'a';
  Hdr.DataLen = 0;
 }

 // register console control handler
 if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ControlHandler, TRUE))
  fprintf(stdout, "Can't register console control handler!\n");

 // main loop
 while((_kbhit() == 0) && (!StopEvent))
 {// wait for new data
  SM.WaitForNewData();
  
  // read block of data
  if (!SM.Read((PBYTE)fData, 2*BIS*sizeof(float))) continue;
 
  if (BPS24)
  {float *pf = fData;
   unsigned char *puc = cData;
   int v, i, im = 2*BIS;
   
   // convert floats into 24-bit integers
   for (i = 0; i < im; i++)
   {// convert float into 32-bit integer
    v = (int)(*(pf++));
   
    // copy highest 24 bits
    v >>= 8; (*(puc++)) = (unsigned char)(v & 0xFF);
    v >>= 8; (*(puc++)) = (unsigned char)(v & 0xFF);
    v >>= 8; (*(puc++)) = (unsigned char)(v & 0xFF);
   }

   // write converted samples into file  
   wavWrite((const char *)cData, 6*BIS);
  }
   else
  {// convert floats  into shorts
   ippsConvert_32f16s_Sfs((Ipp32f *)fData, (Ipp16s *)sData, 2*BIS, ippRndNear, SF);

   // write it into the file
   wavWrite((const char *)sData, 2*BIS*sizeof(short));
  }
 }
  
 // cleanup
 wavClose();
 SM.Close();
}