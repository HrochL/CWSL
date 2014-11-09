//
// Client.cpp  -  network client
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Client.h"

///////////////////////////////////////////////////////////////////////////////
// Parse client command
char *Client::ParseCommand(char *cmd)
{char *t[16];
 int i;
 
 // to be safe ...
 if (cmd == NULL) return cs_inv_cmd;
 
 // tokenize
 t[0] = strtok(cmd, cs_sep);
 for (i = 0; (i < 15) && (t[i] != NULL); i++) t[i+1] = strtok(NULL, cs_sep);
 t[15] = NULL;
  
 // parse commands
 if (t[0] == NULL) return cs_inv_cmd;
  else
 if (_stricmp(t[0], "attach") == 0) return CmdAttach(t);
  else
 if (_stricmp(t[0], "detach") == 0) return CmdDetach(t);
  else
 if (_stricmp(t[0], "frequency") == 0) return CmdFrequency(t);
  else
 if (_stricmp(t[0], "start") == 0) return CmdStart(t);
  else
 if (_stricmp(t[0], "stop") == 0) return CmdStop(t);
  else
 if (_stricmp(t[0], "quit") == 0) return cs_quit;
  else
 if (_stricmp(t[0], "hardware?") == 0) return cs_hw;
  else
 if (_stricmp(t[0], "getserial?") == 0) return cs_serial;
   
 // unsupported command
 return cs_notimp_cmd;
}

///////////////////////////////////////////////////////////////////////////////
// Service for command "attach"
char *Client::CmdAttach(char **arg)
{static char ret[128];

 // check current state
 if (m_SM.IsOpen()) return cs_attached;

 // get receiver number
 if (arg[1] == NULL) return cs_inv_cmd;
 m_rx = atoi(arg[1]);
 
 // create name of the shared memory
 sprintf(ret, "CWSL%dBand", m_rx);
 
 // try to open it
 if (!m_SM.Open(ret)) return cs_cant_attach;

 // get info about it's data and save it
 memcpy(&m_Hdr, m_SM.GetHeader(), sizeof(m_Hdr));
 
 // format response and return it
 sprintf(ret, "%s %d", cs_ok, m_Hdr.SampleRate);
 return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Service for command "detach"
char *Client::CmdDetach(char **arg)
{int rx;

 // check current state
 if (!m_SM.IsOpen()) return cs_detached;

 // get receiver number
 if (arg[1] == NULL) return cs_inv_cmd;
 rx = atoi(arg[1]);
 
 // check receiver number
 if (rx != m_rx) return cs_other_rx;

 // close shared memory
 m_SM.Close();

 // return success
 return cs_ok;
}

///////////////////////////////////////////////////////////////////////////////
// Service for command "frequency"
char *Client::CmdFrequency(char **arg)
{static char ret[128];
 long f;

 // check current state
 if (!m_SM.IsOpen()) return cs_detached;

 // get frequency
 if (arg[1] == NULL) return cs_inv_cmd;
 f = atol(arg[1]);
 
 // check it
 if (f == m_Hdr.L0) return cs_ok;
 
 // client try to set other frequency ...
 sprintf(ret, cs_freq, m_Hdr.L0);
 return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Service for command "start"
char *Client::CmdStart(char **arg)
{BOOL iq;
 int port; 

 // which data ?
 if (arg[1] == NULL) return cs_inv_cmd;
  else
 if (_stricmp(arg[1], "iq") == 0) iq = TRUE;  
  else
 if (_stricmp(arg[1], "bandscope") == 0) iq = FALSE;  
  else
 return cs_inv_cmd;
 
 // get port
 if (arg[2] == NULL) return cs_inv_cmd;
 port = atoi(arg[2]);
 if (port < 1) return cs_inv_cmd;

 // for bandscope it is all ...
 if (!iq) return cs_ok;

 // check current state
 if (!m_SM.IsOpen()) return cs_detached;
 if (isIqStarted()) return cs_started;

 // fill client address
 m_Addr.sin_family = AF_INET;
 m_Addr.sin_port = htons(port);

 // try to start iq thread
 if (!iqStart()) return cs_cant_start;

 // success
 return cs_ok;
}

///////////////////////////////////////////////////////////////////////////////
// Service for command "stop"
char *Client::CmdStop(char **arg)
{BOOL iq;

 // which data ?
 if (arg[1] == NULL) return cs_inv_cmd;
  else
 if (_stricmp(arg[1], "iq") == 0) iq = TRUE;  
  else
 if (_stricmp(arg[1], "bandscope") == 0) iq = FALSE;  
  else
 return cs_inv_cmd;
 
 // for bandscope it is all ...
 if (!iq) return cs_ok;

 // check current state
 if (!m_SM.IsOpen()) return cs_detached;
 if (!isIqStarted()) return cs_not_started;

 // stop sending
 iqStop();

 // success
 return cs_ok;
}

///////////////////////////////////////////////////////////////////////////////
// Start iq worker thread
BOOL Client::iqStart(void)
{
 return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Is iq worker thread started?
BOOL Client::isIqStarted(void)
{
 return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Stop iq worker thread
void Client::iqStop(void)
{
}
