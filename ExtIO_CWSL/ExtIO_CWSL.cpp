//
// Winrad interface dll for CWSL 
//

///////////////////////////////////////////////////////////////////////////////
// Includes & Defines
///////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <ipp.h>
#include "../Utils/SharedMemory.h"
#include "resource.h"

// Callback functions
typedef void (*PCallBack)(int cnt, int status, float IQoffs, short *IQdata);

///////////////////////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////////////////////

// instance of this DLL
HINSTANCE hInst = NULL;

// Pointer to CallBack function to WinRad
PCallBack PCB = NULL;

// Handle & ID of worker thread
DWORD   idWrk = 0;
HANDLE  hWrk = NULL;

// Stop flag
volatile bool  StopFlag = false;

// Handle to GUI dialog box
HWND hDlg = NULL;

// Headers from CWSL bands
#define MAX_CWSL   32
SM_HDR hCWSL[MAX_CWSL];

// Index of highest active band plus one 
int nCWSL = 0;

// Number of selected band
int cCWSL = -1;

// Shared memory for selected band
CSharedMemory mCWSL;

// Scale factor for scaling float to short
int SF = 16;


///////////////////////////////////////////////////////////////////////////////
// Utility function
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Get informations about CWSL buffers
void GetCWSLInfo(void)
{CSharedMemory SM;
 char Name[32];
 int i;

 // for all possible bands ...
 nCWSL = 0;
 for (i = 0; i < MAX_CWSL; i++)
 {// create name of shared memory
  sprintf(Name, "CWSL%dBand", i);

  // try to open shared memory
  if (SM.Open(Name))
  {// save data from header of this band
   memcpy(hCWSL+i, SM.GetHeader(), sizeof(SM_HDR));
   
   // update nCWSL
   nCWSL = i + 1;
  
   // close shared memory
   SM.Close();
  }
   else
  {// this band not exist
   memset(hCWSL+i, 0, sizeof(SM_HDR));
  }
 }
}

///////////////////////////////////////////////////////////////////////////////
// Open shared memory for selected band
BOOL CWSLOpen(void)
{BOOL Res;
 char Name[32];

 // for safety ...
 mCWSL.Close();
 
 // have we selected band ?
 if ((cCWSL < 0) || (cCWSL >= nCWSL) || (nCWSL > MAX_CWSL)) return(FALSE);

 // create name of shared memory
 sprintf(Name, "CWSL%dBand", cCWSL);

 // try to open shared memory
 Res = mCWSL.Open(Name);
 
 // inform winrad about chages
 if (PCB != NULL) 
 {// inform about posible sample rate change
  //(*PCB)(-1, 100, 0, NULL);
  
  // inform about unlocking of local oscilator
  (*PCB)(-1, 103, 0, NULL);

  // inform about posible local oscilator change
  (*PCB)(-1, 101, 0, NULL);
  
  // inform about locking of local oscilator
  (*PCB)(-1, 102, 0, NULL);
 }
 
 // return the result
 return(Res);
}

///////////////////////////////////////////////////////////////////////////////
// Close shared memory 
void CWSLClose(void)
{
 mCWSL.Close();
}

///////////////////////////////////////////////////////////////////////////////
// Dialog box procedures
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Initialization of dialog
void InitDialog(HWND hWnd)
{HWND hCtrl;
 char str[128];
 int i, idx, items;

 // Get info about CWSL bands
 GetCWSLInfo();

 // Fill combo box
 hCtrl = GetDlgItem(hWnd, IDC_BAND);
 SendMessage(hCtrl, CB_RESETCONTENT, 0, 0);
 SendMessage(hCtrl, CB_INITSTORAGE, nCWSL, 64*nCWSL);
 for (i = 0; i < nCWSL; i++)
 {// is it valid band ?
  if (hCWSL[i].SampleRate > 0)
  {// yes -> format his descritpion
   sprintf(str, "%2d - %9u Hz", i, hCWSL[i].L0);
  
   // add it into list
   idx = SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM)str);
  
   // set current index
   if (idx > -1) SendMessage(hCtrl, CB_SETITEMDATA, idx, i);
  }
 }
 
 // Get nr of active bands
 items = SendMessage(hCtrl, CB_GETCOUNT, 0, 0);
 if (items < 0) items = 0;

 // if we haven't selected band, try to do it
 if ((cCWSL < 0) && (items > 0)) cCWSL = SendMessage(hCtrl, CB_GETITEMDATA, 0, 0);
 
 // select the right band
 for (i = 0; i < items; i++)
 {// get band index
  idx = SendMessage(hCtrl, CB_GETITEMDATA, i, 0);
  
  // is it the right band ?
  if ((idx > -1) && (idx < nCWSL) && (idx == cCWSL))
  {// yes -> select it and break the loop
   SendMessage(hCtrl, CB_SETCURSEL, i, 0);
   break;
  }
 }

 // Set right scale factor range and current value
 hCtrl = GetDlgItem(hWnd, IDC_SFS);
 SendMessage(hCtrl, UDM_SETRANGE, 0, MAKELPARAM(24, 0));
 SendMessage(hCtrl, UDM_SETPOS,   0, MAKELPARAM(SF, 0));

 // Fill sample rate window
 if (cCWSL > -1) sprintf(str, "%d Hz", hCWSL[cCWSL].SampleRate);
            else *str = '\0';
 SetWindowText(GetDlgItem(hWnd, IDC_SR), str);

 // Fill block len window
 if (cCWSL > -1) sprintf(str, "%d Smpls", hCWSL[cCWSL].BlockInSamples);
            else *str = '\0';
 SetWindowText(GetDlgItem(hWnd, IDC_BL), str);
 
 // Try to open right shared memory
 CWSLOpen();
}

///////////////////////////////////////////////////////////////////////////////
// Main dialog function
int DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{HWND hCtrl;
 int sel, band;

 // what about ?
 switch (uMsg)
 {
  // initialization of dialog
  case WM_INITDIALOG:
   InitDialog(hwndDlg);
   return(TRUE);
   break;
  
  // command message
  case WM_COMMAND:
   // which control ?
   switch (LOWORD(wParam)) 
   {
    // combo for band selection
    case IDC_BAND:
     // was changed selection ?
     if (HIWORD(wParam) == CBN_SELCHANGE)
     {// yes -> get current selection
      hCtrl = GetDlgItem(hwndDlg, IDC_BAND);
      sel = SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
      if (sel > -1)
      {// get band index
       band = SendMessage(hCtrl, CB_GETITEMDATA, sel, 0);
       if ((band > -1) && (band < nCWSL) && (nCWSL <= MAX_CWSL))
       {// set new band index
        cCWSL = band;
       
        // redraw dialog (and reopen shared memory)
        InitDialog(hwndDlg);
       }
      }
     }
    break;   
   }
   return(TRUE);
   break;  

  // notify message
  case WM_NOTIFY:
   // check value of scaling factor
   SF = SendMessage(GetDlgItem(hwndDlg, IDC_SFS), UDM_GETPOS, 0, 0);
   return(TRUE);
   break;
 }
  
 // default processing
 return(FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// Worker thread function
///////////////////////////////////////////////////////////////////////////////
DWORD WINAPI Worker(LPVOID lpParameter)
{float fData[4096];
 short sData[4096];
 int len;
 
 // for safety ...
 CWSLOpen();
 
 // main loop
 while (!StopFlag)
 {// wait for new data
  mCWSL.WaitForNewData();
  
  // get block length
  len = hCWSL[cCWSL].BlockInSamples;
 
  // read block of data
  if (!mCWSL.Read((PBYTE)fData, 2*len*sizeof(float))) continue;
 
  // convert it into shorts
  ippsConvert_32f16s_Sfs((Ipp32f *)fData, (Ipp16s *)sData, 2*len, ippRndNear, SF);

  // write it to WinRad
  if (PCB != NULL) (*PCB)(len, 0, 0, sData);
 } 
 
 // that's all
 return(0);
}

///////////////////////////////////////////////////////////////////////////////
// Exported functions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
extern "C"
bool __declspec(dllexport) __stdcall InitHW(char *name, char *model, int& type)
{
 // basic info about us
 strcpy(name, "CWSL");
 strcpy(model, " ");
 type = 3; // 3 ==> short data sended via callback 

 // Get info about CWSL bands
 GetCWSLInfo();

 // if we have no bands ...
 if (nCWSL < 1)
 {// ... we can't initialize
  strcpy(name, "No CWSL buffers");
  return(false);
 }

 // success
 return(true);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
void __declspec(dllexport) __stdcall ShowGUI(void)
{HWND hOwner;
 RECT rcOwner, rcDlg;
 int w,h;

 // if dialog exist, do nothing
 if (hDlg != NULL) return;
 
 // get info about desktop
 hOwner = GetDesktopWindow();
 GetWindowRect(hOwner, &rcOwner);

 // create modeless dialog
 hDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_EXTIO), hOwner, (DLGPROC)DlgProc);
 if (hDlg == NULL) return;
 GetWindowRect(hDlg, &rcDlg);

 // move dialog to the right top corner like topmost
 w = rcDlg.right - rcDlg.left;
 h = rcDlg.bottom - rcDlg.top;
 SetWindowPos(hDlg, HWND_TOPMOST, rcOwner.right - w, rcOwner.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
void __declspec(dllexport) __stdcall HideGUI(void)
{
 // if dialog not exist, do nothing
 if (hDlg == NULL) return;
 
 // destroy dialog
 DestroyWindow(hDlg);
 hDlg = NULL; 
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
bool __declspec(dllexport) __stdcall OpenHW(void)
{
 // Show GUI
 ShowGUI();
 
 // that's all
 return true;
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
int __declspec(dllexport) __stdcall StartHW(long LOfreq)
{ 
 // start worker thread
 StopFlag = false;
 hWrk = CreateThread(NULL, 0, Worker, NULL, 0, &idWrk);

 // return the number of complex elements returned by callback routine
 return((cCWSL > -1) ? hCWSL[cCWSL].BlockInSamples : 0);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
int __declspec(dllexport) __stdcall GetStatus(void)
{
 return 0; // unused
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
void __declspec(dllexport) __stdcall StopHW(void)
{
 // was started ?
 if (hWrk != NULL)
 {// set stop flag
  StopFlag = true;
  
  // wait for thread
  WaitForSingleObject(hWrk, 100);

  // close thread handle 
  CloseHandle(hWrk);
  hWrk = NULL;
 }
 
 // close shared memory
 CWSLClose();
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
void __declspec(dllexport) __stdcall CloseHW(void)
{
 // Hide GUI
 HideGUI();
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
int __declspec(dllexport) __stdcall SetHWLO(long freq)
{
 // inform winrad about our value
 if (PCB != NULL) (*PCB)(-1, 101, 0, NULL);
 
 // succes
 return(0);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
long __declspec(dllexport) __stdcall GetHWLO(void)
{
 // return it
 return((cCWSL > -1) ? hCWSL[cCWSL].L0 : 0);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
long __declspec(dllexport) __stdcall GetHWSR(void)
{
 // return it
 return((cCWSL > -1) ? hCWSL[cCWSL].SampleRate : 0);
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
void __declspec(dllexport) __stdcall SetCallback(PCallBack _PCB)
{
 // save the pointer
 PCB = _PCB;
}

///////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, unsigned long Command, void* lpReserved)
{
 // which command ?
 switch (Command)
 {
  case DLL_PROCESS_ATTACH:
   // save handle to this dll
   hInst = hinst;
   break;
  
  case DLL_THREAD_ATTACH:    
   break;
   
  case DLL_THREAD_DETACH:
   break;
  
  case DLL_PROCESS_DETACH:
   // free all
   StopHW();
   break; 
 }
 
 // that's all
 return(TRUE);
}
