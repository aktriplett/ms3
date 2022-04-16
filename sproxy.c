#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
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
fd_set readfds;
//int option = 1;

int DaemonConnect()
{
  // Create socket.
  int DaemonSocket = socket(AF_INET, SOCK_STREAM, 0);
  //setsockopt(DaemonSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (DaemonSocket < 0)
  {
    error("ERROR opening Daemon socket");
  }
  //clearing _addr with bzero method
  bzero((char *) &daemon_addr, sizeof(daemon_addr));

  //saying to server address, all will be in internet address concept
  daemon_addr.sin_family = AF_INET;
  // converting char ip addr
  inet_aton(daemonip, &daemon_addr.sin_addr.s_addr);
  // convert integer format to network format with htons
  daemon_addr.sin_port = htons(23);
  fprintf(stderr,"Received and converted Daemon IP\n");

  return DaemonSocket;
}

int CproxyConnect(int portno)
{
  // Create socket.
  int CproxySocket = socket(AF_INET, SOCK_STREAM, 0);
  //setsockopt(CproxySocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (CproxySocket < 0)
  {
    error("ERROR opening Cproxy socket");
  }

  //clearing _addr with bzero method
  bzero((char *) &cproxy_addr, sizeof(cproxy_addr));
  //saying to server address, all will be in internet address concept
  cproxy_addr.sin_family = AF_INET;
  // get your address on your own when you start the program
  cproxy_addr.sin_addr.s_addr = INADDR_ANY;
  // convert integer format to network format with htons
  cproxy_addr.sin_port = htons(portno);

  // Bind socket
  if (bind(CproxySocket, (struct sockaddr *) &cproxy_addr, sizeof(cproxy_addr)) < 0)
  {
    error("ERROR on binding Cproxy Socket");
  }

  return CproxySocket;
}

char* setPacket(int type, char* payload, int len, int seq) {
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

int getPacketType(char* packet) {
    return *((int*) packet);
}

char* getPacketMsg(char* packet) {
    return packet + 12;
}


int main(int argc, char *argv[])
{
  // have all necessary command line arguments been given
  if (argc < 2)//or maybe 1?
  {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  //set vars for creating sockets
  int DaemonSocket, CproxySocket;
  int cproxyport;
  //set vars for select
  fd_set readfds;
  struct timeval tv;
  socklen_t len1;
  //vars for receiving and sending messages
  int rv;
  int n, cproxyrecv, daemonrecv = 0;
  int seqNum = 0;

  //port no passed in command line arg, to convert character to int we use atoi
  cproxyport = atoi(argv[1]);

  //calling socket set up functions
  DaemonSocket = DaemonConnect();
  CproxySocket = CproxyConnect(cproxyport);

  //going into listen mode on sproxy, can handle 5 clients
  //fprintf(stderr,"I'm listening\n");
  listen(CproxySocket, 5);
  fprintf(stderr,"I'm listening on cproxy\n");

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
  //set vars for select
  // clear the set
  FD_ZERO(&readfds);
  // add descriptors (fd) to set
  FD_SET(newcproxysocket, &readfds);
  FD_SET(DaemonSocket, &readfds);

  // find the largest descriptor, and plus one.
  //if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;
  //else n = DaemonSocket + 1;
  n = DaemonSocket + 1;
  //timeout is 1 sec to increment hbcount
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  int hbcount = 0;

  fprintf(stderr,"Entering the message loop on server\n");

  while((rv = select(n, &readfds, NULL, NULL, &tv)) >= 0)
  //while(1)
  {
    if (rv < 0)
    {
      error("ERROR on select function\n");
      break;
    }
    if (rv == 0)//Timeout occured, no message received so sending heartbeat
    {
        hbcount++;
        tv.tv_sec = 1;
        setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message
        send(newcproxysocket, packetbuf, 14, 0);//send the heartbeat
        fprintf(stderr,"Server sent a heartbeat to client\n");
        if (hbcount == 3)
        {
            hbcount = 0;
            int newcproxysocket = accept(CproxySocket, (struct sockaddr *) &cproxy_addr, &len1);

            if (newcproxysocket < 0)
            {
              error("ERROR on cproxy accept");
            }

            FD_SET(newcproxysocket, &readfds);
            fprintf(stderr,"sproxy reconnected to cproxy\n");
         }
      }

      //else //no timeout, rv = 1 and we have a message to send
      //{
        fprintf(stderr,"no timeout, we have a message\n");
        //zero out both message buffers
        bzero(daemonbuf, sizeof(daemonbuf));
        bzero(cproxybuf, sizeof(cproxybuf));

        if (FD_ISSET(newcproxysocket, &readfds))
        {
            cproxyrecv = recv(newcproxysocket, cproxybuf, sizeof(cproxybuf), 0);

            if (cproxyrecv <= 0)
            {
              error("ERROR on cproxy receive");
              break;
            }

            else if (getPacketType(cproxybuf) == 2)
            {
                fprintf(stderr,"normal message received\n");
                int result = send(DaemonSocket, getPacketMsg(cproxybuf), cproxyrecv - 12, 0);//forward the message from cproxy to the telnet daemon
                if (result == -1)
                    break;
                cproxyrecv = 0;
            }
            else if (getPacketType(cproxybuf) == 1)
            {
                fprintf(stderr,"heartbeat message received, resetting hbcount\n");
                hbcount = 0;
                //of_index = 0;
                //memset(overflowbuf, 0, 2048 * 100);
            }
        }
        if (FD_ISSET(DaemonSocket, &readfds))
        {
            daemonrecv = recv(DaemonSocket, daemonbuf, sizeof(daemonbuf), 0);

            if (daemonrecv <= 0)
            {
               error("ERROR on daemon receive");
               break;
            }

            fprintf(stderr,"sproxy received a message from daemon\n");
            setPacket(2, daemonbuf, daemonrecv, seqNum); //not a heartbeat, other message
            //memcpy(overflowbuf[of_index], packetbuf, tdRecv + 12);
            //of_index++;
            seqNum++;
            send(newcproxysocket, daemonbuf, daemonrecv, 0);//forward message from daemon to cproxy
            daemonrecv = 0;
        }

      //}
      fprintf(stderr,"I'm waiting for a new message on sproxy\n");

      FD_ZERO(&readfds);
      FD_SET(newcproxysocket, &readfds);
      FD_SET(DaemonSocket, &readfds);
      if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;
      else n = DaemonSocket + 1;
    }
  }
  fprintf(stderr,"Closing server side sockets\n");
  close(DaemonSocket,2);
  close(CproxySocket,2);
  return 0;
}
