//
// Client.cpp  -  network client
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
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
 if (_stricmp(t[0], "hardware?") == 0) return cs_inv_cmd;  // return cs_hw;
  else
 if (_stricmp(t[0], "lo?") == 0) return CmdLO(t);
   
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
// Service for command "lo"
char *Client::CmdLO(char **arg)
{static char ret[128];
 int rx;

 // check current state
 if (!m_SM.IsOpen()) return cs_detached;

 // get receiver number
 if (arg[1] == NULL) return cs_inv_cmd;
 rx = atoi(arg[1]);
 
 // check receiver number
 if (rx != m_rx) return cs_other_rx;

 // format response and return it
 sprintf(ret, "%s %d", cs_ok, m_Hdr.L0);
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
 
 // we have no way to set the frequency ... but we don't tell it 
 return cs_ok;
 
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
{DWORD ID;

 // for sure
 iqStop();
 
 // create sender socket
 m_iqSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
 if (m_iqSocket == INVALID_SOCKET) return FALSE;
 
 // start worker thread
 m_hThrd = CreateThread(NULL, 0, Client_iqWorker, this, 0, &ID);
 if (m_hThrd == NULL) {closesocket(m_iqSocket); m_iqSocket = INVALID_SOCKET; return FALSE;}
 
 // close handle to the thread
 CloseHandle(m_hThrd);
 
 // we have success
 return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Stop iq worker thread
void Client::iqStop(void)
{
 // close the iq socket
 if (m_iqSocket != INVALID_SOCKET) closesocket(m_iqSocket);
 m_iqSocket = INVALID_SOCKET;
 
 // if worker running ...
 if (m_hThrd != NULL)
 {// ... wait for it - but not for long
  ::WaitForSingleObject(m_hThrd, 100);
 }
 m_hThrd = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Real worker function
DWORD WINAPI Client_iqWorker(LPVOID lpParam)
{Client *pInst = (Client *)lpParam;
 
 // run worker function
 if (pInst != NULL) pInst->iqWorker();
 
 // clean handle to this thread
 pInst->m_hThrd = NULL; 
 
 // that's all folks
 return 0;
}

///////////////////////////////////////////////////////////////////////////////
// IQ worker function
void Client::iqWorker(void)
{static const float norm = (float)(1.0 / pow(2.0, 31.0));
 float iBuf[BUFFER_SIZE*2], oBuf[BUFFER_SIZE*2];
 float *pi, *po;
 int i;
 iqPacket pkt;
 unsigned short offset;
 
 // clear old data
 m_SM.ClearBytesToRead();
 
 // main loop
 while (m_iqSocket != INVALID_SOCKET)
 {// wait for new data
  m_SM.WaitForNewData(100);
  
  // can we still run ?
  if (m_iqSocket == INVALID_SOCKET) break;
  
  // read block of data
  if (!m_SM.Read((PBYTE)iBuf, sizeof(iBuf))) continue;
 
  // reconfigure data - q samples first
  for (i = 0, pi = iBuf, po = oBuf; i < BUFFER_SIZE; i++)
  {// copy samples to the right places
   po[BUFFER_SIZE] = *(pi++) * norm;
   po[          0] = *(pi++) * norm;
   po++;
  }
 
  // send data 
  offset = 0;
  while(offset < sizeof(oBuf))
  {// fill packet header
   pkt.sequence = m_Seq;
   pkt.offset = offset;
   pkt.length = sizeof(oBuf) - offset;
   if (pkt.length > DATA_SIZE) pkt.length = DATA_SIZE; // trim to max length

   // copy data 
   memcpy (pkt.data, ((char *)oBuf)+offset, pkt.length);
   
   // send it
   if (m_iqSocket == INVALID_SOCKET) break;
   sendto(m_iqSocket, (char *)&pkt, sizeof(pkt), 0, (struct sockaddr*)&m_Addr, sizeof(m_Addr));  
   
   // update offset
   offset += pkt.length;
  }
  
  // increment sequence number
  m_Seq++;
 }
}
