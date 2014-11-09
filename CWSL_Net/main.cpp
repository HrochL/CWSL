#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include "Client.h"

#define LISTEN_PORT     11000
#define BUFFER_LEN      4096
#define MAX_CLIENTS     32

///////////////////////////////////////////////////////////////////////////////
// Global types & variables

// Data type to describe one client
typedef struct {

 // client index
 int idx;
 
 // client address
 struct sockaddr_in addr;
 
 // client socket 
 SOCKET sock;

} CLIENT;

// Array of client's descriptors
CLIENT Clns[MAX_CLIENTS];

// Server socket
SOCKET sListen = INVALID_SOCKET;


///////////////////////////////////////////////////////////////////////////////
// Worker function for the client thread
DWORD WINAPI clientThread(LPVOID lpParam)
{CLIENT *pc = Clns + (int)lpParam;
 SOCKET sock = pc->sock;
 Client cln(&(pc->addr));
 char szBuff[BUFFER_LEN], *rpl;
 int ret;

 // print info
 printf("%02d new client from %s:%d\n", pc->idx, inet_ntoa(pc->addr.sin_addr), ntohs(pc->addr.sin_port));

 // main loop
 while(1)
 {// perform a blocking recv() call
  ret = recv(sock, szBuff, BUFFER_LEN, 0);
  if (ret == 0) break;       // Graceful close
   else 
  if (ret == SOCKET_ERROR)
  {printf("%02d recv() failed: %d\n", pc->idx, WSAGetLastError());
   break;
  }
 
  // print command
  szBuff[ret] = '\0';
  printf("%02d RECV: '%s'\n", pc->idx, szBuff);

  // parse command
  rpl = cln.ParseCommand(szBuff);

  // send reply
  send(sock, rpl, strlen(rpl), 0);
  printf("%02d SEND: '%s'\n", pc->idx, rpl);

  // is it request to quit?
  if (strcmp(rpl, cs_quit) == 0) break;
 }

 // print info
 printf("%02d connection with client %s:%d closed\n", pc->idx, inet_ntoa(pc->addr.sin_addr), ntohs(pc->addr.sin_port));

 // close socket
 closesocket(sock);
 
 // free slot
 pc->sock = INVALID_SOCKET;

 // that's all
 return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Initialize client slots
void clientInit(void)
{int i;

 // initialize all slots
 for (i = 0; i < MAX_CLIENTS; i++)
 {// initialize
  Clns[i].idx = i;
  memset(&(Clns[i].addr), 0, sizeof(Clns[i].addr));
  Clns[i].sock = INVALID_SOCKET;
 }
}

///////////////////////////////////////////////////////////////////////////////
// Free all client slots
void clientFree(void)
{int i;

 // close all sockets
 for (i = 0; i < MAX_CLIENTS; i++)
  if (Clns[i].sock != INVALID_SOCKET) closesocket(Clns[i].sock);
}

///////////////////////////////////////////////////////////////////////////////
// Create new client
bool clientNew(struct sockaddr_in *Addr, SOCKET Sock)
{int i;
 HANDLE h;
 DWORD id;

 // find the first free client slot
 for (i = 0; i < MAX_CLIENTS; i++)
  if (Clns[i].sock == INVALID_SOCKET) break;
 if (i >= MAX_CLIENTS) return false; 
    
 // fill it
 memcpy(&(Clns[i].addr), Addr, sizeof(struct sockaddr_in));
 Clns[i].sock = Sock;
 
 // create client thread
 h = CreateThread(NULL, 0, clientThread, (LPVOID)i, 0, &id);
 if (h == NULL) {closesocket(Sock); Clns[i].sock = INVALID_SOCKET; return false;}

 // close handle to thread
 CloseHandle(h);

 // success
 return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Console control handler
BOOL WINAPI ControlHandler(DWORD CtrlType)
{
 switch(CtrlType)
 {case CTRL_BREAK_EVENT: 
   fprintf(stdout, "\nCtrl+Break event!\n");
   closesocket(sListen);
   return(TRUE);
   break;

  case CTRL_C_EVENT:   
   fprintf(stdout, "\nCtrl+C event!\n");
   closesocket(sListen);
   return(TRUE);
   break;

  case CTRL_CLOSE_EVENT:  
   fprintf(stdout, "\nConsole closing event!\n");
   closesocket(sListen);
   return(TRUE);
   break;
 }
 return(FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// Main function
int main(int argc, char **argv)
{WSADATA wsd;
 SOCKET sClient;
 struct sockaddr_in local, client;
 int iAddrSize;

 // print info
 printf("Starting ...\n");

 // initialize slots
 clientInit();

 // initialize sockets
 if (WSAStartup(MAKEWORD(2,2), &wsd) != 0)
 {printf("Failed to load Winsock!\n");
  return 1;
 }

 // create our listening socket
 sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
 if (sListen == SOCKET_ERROR)
 {printf("socket() failed: %d\n", WSAGetLastError()); return 1;}

 // bind it to our well-known port
 local.sin_addr.s_addr = htonl(INADDR_ANY);
 local.sin_family = AF_INET;
 local.sin_port = htons(LISTEN_PORT);
 if (bind(sListen, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
 {printf("bind() failed: %d\n", WSAGetLastError()); return 1;}
    
 // set socket into listen mode
 listen(sListen, 8);
 
 // register console control handler
 if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ControlHandler, TRUE))
  printf("Can't register console control handler!\n");
    
 // print info
 printf("Started\n");

 // main loop
 while (1)
 {// perform a blocking accept() call
  iAddrSize = sizeof(client);
  sClient = accept(sListen, (struct sockaddr *)&client, &iAddrSize);
  if (sClient == INVALID_SOCKET)
  {printf("accept() failed: %d\n", WSAGetLastError()); break;}
    
  // create new client thread
  if (!clientNew(&client, sClient)) printf("clientNew() failed: %d\n", GetLastError());
 }
 
 // print info
 printf("Stopping ...\n");

 // close listen socket
 closesocket(sListen);

 // free all clients
 clientFree();
 
 // cleanup sockets
 WSACleanup();
 
 // print info
 printf("Stopped\n");

 // that't all
 return 0;
}
