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
  //set vars for creating sockets
  int DaemonSocket, CproxySocket;
  int cproxyport;
  socklen_t len1;
  //set vars for select
  fd_set readfds;
  struct timeval tv;
  //vars for receiving and sending messages
  int rv;
  int n, len = 0;
  char buf1[BUFFERSIZE],buf2[BUFFERSIZE];

  // have all necessary command line arguments been given
  if (argc < 1)
  {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  //port no passed in command line arg, to convert character to int we use atoi
  cproxyport = atoi(argv[1]);

  //calling socket set up functions
  DaemonSocket = DaemonConnect();
  CproxySocket = CproxyConnect(cproxyport);

  //going into listen mode on sproxy, can handle 5 clients
  //fprintf(stderr,"I'm listening\n");
  listen(CproxySocket, 5);
  while(1)
  {
    fprintf(stderr,"I'm listening\n");
    //calling socket set up functions
    //DaemonSocket = DaemonConnect();
    //CproxySocket = CproxyConnect(cproxyport);

    //going into listen mode on sproxy, can handle 5 clients
    //fprintf(stderr,"I'm listening\n");
    //listen(CproxySocket, 5);

    //creating new socket for particular client that has my address and client address
    //cproxy_addr provides all the info i need about the client
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

    ////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////
    //Entering the message loop
    while(1)
    {
        // clear the set ahead of time
        FD_ZERO(&readfds);

        // add our descriptors to the set
        FD_SET(newcproxysocket, &readfds);
        FD_SET(DaemonSocket, &readfds);


        // find the largest descriptor, and plus one.
        if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;
        else n = DaemonSocket + 1;

        // timeout is 10.5 sec to receive data on either socket
        tv.tv_sec = 10;
        tv.tv_usec = 500000; //this is .5 sec

        //Engage select function, will return when at least one socket
        //has traffic or timeout.
        rv = select(n, &readfds, NULL, NULL, &tv);

        if (rv == -1)
        {
           error("ERROR engaging select function on server");
        }

        else if (rv == 0)
        {
             printf("No data given before timeout window.\n");
        }
        else
        {
             // one or both of the descriptors have data
             if (FD_ISSET(newcproxysocket, &readfds))
             {
                 len = recv(newcproxysocket, buf1, sizeof(buf1), 0);
                 if (len <= 0)
                 {
                    break;
                 }
                 send(DaemonSocket, buf1, len, 0);
             }
             if (FD_ISSET(DaemonSocket, &readfds))
             {
                 len = recv(DaemonSocket, buf2, sizeof(buf2), 0);
                 if (len <= 0)
                 {
                    break;
                 }
                 send(newcproxysocket, buf2, len, 0);
             }
         }
     }
     close(DaemonSocket,2);
     close(newcproxysocket,2);
  }
  return 0;
}
