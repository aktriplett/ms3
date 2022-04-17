#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#define BUFFERSIZE 1024

//method called error, exit(1) means you are exiting from the program
void error(char *msg)
{
  perror(msg);
  exit(1);
}

//initialize vars for all functions
struct sockaddr_in daemon_addr, cproxy_addr;
char *daemonip = "127.0.0.1";
char daemonbuf[BUFFERSIZE], cproxybuf[BUFFERSIZE], packetbuf[BUFFERSIZE];
int option = 1;

int DaemonConnect()
{
  // Create socket.
  int DaemonSocket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(DaemonSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (DaemonSocket < 0)
  {
    error("ERROR opening Daemon socket");
  }

  bzero((char *) &daemon_addr, sizeof(daemon_addr));//clearing _addr with bzero method
  daemon_addr.sin_family = AF_INET;//saying to server address, all will be in internet address concept
  inet_aton(daemonip, &daemon_addr.sin_addr.s_addr);// converting char ip addr
  daemon_addr.sin_port = htons(23);// convert integer format to network format with htons
  fprintf(stderr,"Received and converted Daemon IP\n");

  return DaemonSocket;
}

int CproxyConnect(int portno)
{
  int CproxySocket = socket(AF_INET, SOCK_STREAM, 0);// Create socket.
  setsockopt(CproxySocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (CproxySocket < 0)
  {
    error("ERROR opening Cproxy socket");
  }

  bzero((char *) &cproxy_addr, sizeof(cproxy_addr));
  cproxy_addr.sin_family = AF_INET;
  cproxy_addr.sin_addr.s_addr = INADDR_ANY;
  cproxy_addr.sin_port = htons(portno);

  if (bind(CproxySocket, (struct sockaddr *) &cproxy_addr, sizeof(cproxy_addr)) < 0)
  {
    error("ERROR on binding Cproxy Socket");
  }

  return CproxySocket;
}

char* setPacket(int type, char* payload, int len, int seq)
{
   bzero(packetbuf, sizeof(packetbuf));
   char *p = packetbuf;
   *((int*) p) = type;
   p = p + 4;
   *((int*) p) = seq;
   p = p + 4;
   *((int*) p) = len;
   p = p + 4;
   memcpy(p, payload, len);
   return packetbuf;
}

int getPacketType(char* packet)
{
    return *((int*) packet);
}

char* getPacketMsg(char* packet)
{
    return packet + 12;
}

int main(int argc, char *argv[])
{
  int DaemonSocket, CproxySocket;
  int cproxyport;
  socklen_t len1;
  fd_set readfds;
  struct timeval tv;
  int rv;
  int n, len = 0;
  int cproxyrecv, daemonrecv = 0;
  int seqNum = 0;
  int hbcount = 0;
  char buf1[BUFFERSIZE],buf2[BUFFERSIZE];

  if (argc < 1)// have all necessary command line arguments been given
  {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  cproxyport = atoi(argv[1]);//port no passed in command line arg, to convert character to int we use atoi
  DaemonSocket = DaemonConnect();//calling socket set up functions
  CproxySocket = CproxyConnect(cproxyport);

  listen(CproxySocket, 5);//going into listen mode on sproxy, can handle 5 clients

  while(1)
  {
    fprintf(stderr,"I'm listening on cproxy socket\n");
    int newcproxysocket = accept(CproxySocket, (struct sockaddr *) &cproxy_addr, &len1);
    if (newcproxysocket < 0)
    {
      error("ERROR on cproxy accept");
    }
    fprintf(stderr,"Connected to a client on cproxy\n");

    //connect to telnet daemon
    if (connect(DaemonSocket, &daemon_addr, sizeof(daemon_addr)) < 0)
    {
      error("ERROR connecting to daemon");
    }
    fprintf(stderr,"Connected to telnet daemon\n");

    FD_ZERO(&readfds);// clear the set ahead of time
    FD_SET(newcproxysocket, &readfds);// add our descriptors to the set
    FD_SET(DaemonSocket, &readfds);
    if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;// find the largest descriptor, and plus one.
    else n = DaemonSocket + 1;
    tv.tv_sec = 10;// timeout is 10.5 sec to receive data on either socket
    tv.tv_usec = 0; //this is .5 sec

    //Entering the message loop
    while((rv = select(n, &readfds, NULL, NULL, &tv)) >= 0)
    {
      setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message
      fprintf(stderr, "set the hb packet\n");
      //send(newcproxysocket, packetbuf, sizeof(packetbuf), 0);//send the heartbeat

      if (rv == 0)
      {
        hbcount++;
        if (hbcount == 3)
        {
            fprintf(stderr, "reset hb\n");
            hbcount = 0;
            //fprintf(stderr,"hb hit three, reset\n");
            // close(newcproxysocket);
            // int newcproxysocket = accept(CproxySocket, (struct sockaddr *) &cproxy_addr, &len1);
            //
            // if (newcproxysocket < 0)
            // {
            //   error("ERROR on NEW cproxy accept");
            // }
            //
            // FD_SET(newcproxysocket, &readfds);
            // fprintf(stderr,"sproxy reconnected to cproxy\n");
         }
      }
      else
      {
         bzero(cproxybuf, sizeof(cproxybuf));
         bzero(daemonbuf, sizeof(daemonbuf));
         // one or both of the descriptors have data
         if (FD_ISSET(newcproxysocket, &readfds))
         {
             cproxyrecv = recv(newcproxysocket, cproxybuf, sizeof(cproxybuf), 0);
             if (cproxyrecv < 0)
             {
               error("ERROR on cproxy receive\n");
               break;
             }
             send(DaemonSocket, cproxybuf, cproxyrecv, 0);
         }
         if (FD_ISSET(DaemonSocket, &readfds))
         {
             daemonrecv = recv(DaemonSocket, daemonbuf, sizeof(daemonbuf), 0);
             if (daemonrecv < 0)
             {
               error("ERROR on daemon receive\n");
               break;
             }
             send(newcproxysocket, daemonbuf, daemonrecv, 0);
         }
       }
       FD_ZERO(&readfds);// clear the set ahead of time
       FD_SET(newcproxysocket, &readfds);// add our descriptors to the set
       FD_SET(DaemonSocket, &readfds);
       if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;// find the largest descriptor, and plus one.
       else n = DaemonSocket + 1;
       tv.tv_sec = 10;// timeout is 10.5 sec to receive data on either socket
       tv.tv_usec = 0; //this is .5 sec
     }
     close(DaemonSocket,2);
     close(newcproxysocket,2);
  }
  return 0;
}
