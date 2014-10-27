//
// CWSL_Tee.h - DLL Interface for CW Skimmer Server Listener Tee
//

#ifdef CWSL_TEE_EXPORTS
#define CWSL_TEE_API __declspec(dllexport)
#else
#define CWSL_TEE_API __declspec(dllimport)
#endif

#define RATE_48KHZ    0
#define RATE_96KHZ    1
#define RATE_192KHZ   2

// IqProc must be called BLOCKS_PER_SEC times per second
#define BLOCKS_PER_SEC  93.75

#define MAX_RX_COUNT  8

#pragma pack(push, 16) 
typedef struct {float Re, Im;} Cmplx;
typedef Cmplx *CmplxA;
typedef CmplxA *CmplxAA;
#pragma pack(pop) 

// this callback procedure passes 7-channel I/Q data from the receivers for
// the waterfall and decoding
typedef void (__stdcall *tIQProc)(int RxHandle, CmplxAA Data);

// this callback procedure passes I/Q data from the 1-st receiver in small chunks
// for DSP processing, and receives processed audio back
typedef void (__stdcall *tAudioProc)(int RxHandle, CmplxA InIq, CmplxA OutLR, int OutCount);

// this callback procedure passes the total number of bytes to be sent to FPGA
// and the number of already sent bytes. The client application may use it
// to display a progress bar
typedef void (__stdcall *tLoadProgressProc)(int RxHandle, int Current, int Total);

// if an error occurs, call this callback procedure and pass it the error
// message as a parameter, then stop the radio
typedef void (__stdcall *tErrorProc)(int RxHandle, char *ErrText);

//Optional. Call this procedure when the status bits change
typedef void (__stdcall *tStatusBitsProc)(int RxHandle, unsigned char Bits);

typedef struct {

    char *DeviceName;
    int   MaxRecvCount;
    float ExactRates[3];

} SdrInfo, *PSdrInfo;


typedef struct {

 int  THandle;
 int  RecvCount;
 int  RateID;
 BOOL LowLatency; //32-bit boolean
 
 tIQProc           pIQProc;
 tAudioProc        pAudioProc;
 tStatusBitsProc   pStatusBitProc;
 tLoadProgressProc pLoadProgressProc;
 tErrorProc        pErrorProc;

} SdrSettings, *PSdrSettings;


extern "C" CWSL_TEE_API void __stdcall GetSdrInfo(PSdrInfo pInfo);

extern "C" CWSL_TEE_API void __stdcall StartRx(PSdrSettings pSettings);

extern "C" CWSL_TEE_API void __stdcall StopRx(void);

extern "C" CWSL_TEE_API void __stdcall SetRxFrequency(int Frequency, int Receiver);

extern "C" CWSL_TEE_API void __stdcall SetCtrlBits(unsigned char Bits);

extern "C" CWSL_TEE_API int __stdcall  ReadPort(int PortNumber);

