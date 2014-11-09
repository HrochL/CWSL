//
// Client.h  -  network client
//
#include <windows.h>
#include "../Utils/SharedMemory.h"

///////////////////////////////////////////////////////////////////////////////
// String constants
static char cs_sep[] = " \r\n"; // command argument separators
static char cs_ok[] = "OK";
static char cs_quit[] = "Client requested to quit ... bye !";
static char cs_hw[] = "OK CWSL";
static char cs_serial[] = "OK N/A";
static char cs_inv_cmd[] = "Error: Invalid Command";
static char cs_notimp_cmd[] = "Error: Not Implemented Command";
static char cs_attached[] = "Error: Client is already attached to receiver";
static char cs_cant_attach[] = "Error: Can't attach to receiver";
static char cs_detached[] = "Error: Client is not attached to receiver";
static char cs_other_rx[] = "Error: Client is attached to other receiver";
static char cs_freq[] = "Error: can't set frequency other than %d Hz";
static char cs_started[] = "Error: iq data are already in sending";
static char cs_cant_start[] = "Error: Can't start thread for iq data sending";
static char cs_not_started[] = "Error: iq data are not in sending";

///////////////////////////////////////////////////////////////////////////////
class Client {

 protected:
 
     // Remote client address
     struct sockaddr_in  m_Addr;
     
     // Index of the current receiver
     int  m_rx;

     // Shared memory
     CSharedMemory  m_SM;
     
     // Local copy of shared memeory header
     SM_HDR  m_Hdr;
 

 public:

     // Construction
     Client(struct sockaddr_in *Addr) {memcpy(&m_Addr, Addr, sizeof(m_Addr));}

     // Destruction
     ~Client() {iqStop(); m_SM.Close();}

     // Parse client command
     char *ParseCommand(char *cmd);
     
     // Command services
     char *CmdAttach(char **arg);
     char *CmdDetach(char **arg);
     char *CmdFrequency(char **arg);
     char *CmdStart(char **arg);
     char *CmdStop(char **arg);
     
     // IQ worker thread functions
     BOOL iqStart();
     BOOL isIqStarted();
     void iqStop();

};
