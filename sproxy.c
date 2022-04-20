#include <stdio.h>
#include <stdlib.h>
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

int DaemonConnect()
{
  int DaemonSocket = socket(AF_INET, SOCK_STREAM, 0);// Create socket.
  if (DaemonSocket < 0)
  {
    error("ERROR opening Daemon socket");
  }
  bzero((char *) &daemon_addr, sizeof(daemon_addr));//clearing _addr with bzero method

  daemon_addr.sin_family = AF_INET;  //saying to server address, all will be in internet address concept
  inet_aton(daemonip, &daemon_addr.sin_addr.s_addr);// converting char ip addr
  daemon_addr.sin_port = htons(23);// convert integer format to network format with htons
  fprintf(stderr,"Received and converted Daemon IP\n");
  return DaemonSocket;
}

int CproxyConnect(int portno)
{
  int CproxySocket = socket(AF_INET, SOCK_STREAM, 0);// Create socket.
  if (CproxySocket < 0)
  {
    error("ERROR opening Cproxy socket");
  }
  bzero((char *) &cproxy_addr, sizeof(cproxy_addr));//clearing _addr with bzero method
  cproxy_addr.sin_family = AF_INET;//saying to server address, all will be in internet address concept
  cproxy_addr.sin_addr.s_addr = INADDR_ANY;// get your address on your own when you start the program
  cproxy_addr.sin_port = htons(portno);// convert integer format to network format with htons

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
  int cproxyport;
  fd_set readfds;
  struct timeval tv;
  socklen_t len1;
  int rv;
  int n;
  int cproxyrecv, daemonrecv = 0;
  int seqNum = 0;

  //port no passed in command line arg, to convert character to int we use atoi
  cproxyport = atoi(argv[1]);
  //calling socket set up functions
  //DaemonSocket = DaemonConnect();
  int CproxySocket = CproxyConnect(cproxyport);
  listen(CproxySocket, 5);//going into listen mode on sproxy, can handle 5 clients
  fprintf(stderr,"I'm listening on cproxy\n");

  //telnet local host triggers this loop
  while(1)
  {
    int newcproxysocket = accept(CproxySocket, (struct sockaddr *) &cproxy_addr, &len1);
    if (newcproxysocket < 0)
    {
      error("ERROR on first cproxy accept");
    }
    fprintf(stderr,"Connected to a client on cproxy\n");

    int DaemonSocket = DaemonConnect();
    if (connect(DaemonSocket, &daemon_addr, sizeof(daemon_addr)) < 0)
    {
      error("ERROR connecting to daemon");
    }
    fprintf(stderr,"Connected to telnet daemon\n");

    //set vars for select
    FD_ZERO(&readfds);// clear the set
    FD_SET(newcproxysocket, &readfds);// add descriptors (fd) to set
    FD_SET(DaemonSocket, &readfds);
    if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;  // find the largest descriptor, and plus one.
    else n = DaemonSocket + 1;
    //n = DaemonSocket + 1;
    tv.tv_sec = 5;//timeout is 1 sec to increment hbcount
    tv.tv_usec = 0;
    int hbcount = 0;
    int sessionID = 0;
    fprintf(stderr,"Entering the message loop on server\n");

    while(1)
    {
      rv = select(n, &readfds, NULL, NULL, &tv);

      if (rv == -1)
      {
          error("ERROR engaging select function on client");
      }
      //tv.tv_sec = 1;//timeout is 1 sec to increment hbcount
      //setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message
      //send(newcproxysocket, packetbuf, sizeof(packetbuf), 0);//send the heartbeat
      //fprintf(stderr,"Server sent a heartbeat message to client: %s\n", packetbuf);

      else if (rv == 0)//Timeout occured, no message received so sending heartbeat
      {
          setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message
          send(newcproxysocket, packetbuf, sizeof(packetbuf), 0);//send the heartbeat
          fprintf(stderr,"timed out, incrementing hb count\n");
          hbcount++;
          if (hbcount == 3)
          {
              hbcount = 0;
              fprintf(stderr,"closing socket connection to cproxy\n");
              close(newcproxysocket);
              int newcproxysocket = accept(CproxySocket, (struct sockaddr *) &cproxy_addr, &len1);

              if (newcproxysocket < 0)
              {
                error("ERROR on NEW cproxy accept");
                break;
              }

              FD_SET(newcproxysocket, &readfds);
              fprintf(stderr,"sproxy reconnected to cproxy\n");
           }
        }
        else if (rv == 1)//no timeout, rv = 1 and we have a message to send
        {
          //fprintf(stderr,"no timeout, we have a message\n");
          bzero(daemonbuf, sizeof(daemonbuf));//zero out both message buffers
          bzero(cproxybuf, sizeof(cproxybuf));

          if (FD_ISSET(newcproxysocket, &readfds))
          {
              cproxyrecv = recv(newcproxysocket, cproxybuf, sizeof(cproxybuf), 0);
              fprintf(stderr,"I'm in the cproxy branch, cproxy recv is %d\n", cproxyrecv);
              if (cproxyrecv <= 0)
              {
                error("ERROR on cproxy receive");
                break;
              }

              else if (getPacketType(cproxybuf) == 1)
              {
                  //fprintf(stderr,"heartbeat message received, resetting hbcount: %s\n", cproxybuf);
                  hbcount = 0;
              }

              else//getPacketType(cproxybuf) == 2)
              {
                  //fprintf(stderr,"normal message received\n");
                  send(DaemonSocket, cproxybuf, cproxyrecv, 0);//forward the message from cproxy to the telnet daemon # cproxyrecv-12
                  //cproxyrecv = 0;
              }
          }
          if (FD_ISSET(DaemonSocket, &readfds))
          {
              daemonrecv = recv(DaemonSocket, daemonbuf, sizeof(daemonbuf), 0);
              fprintf(stderr,"I'm in the daemon branch, daemon recv is %d\n", daemonrecv);
              if (daemonrecv <= 0)
              {
                 error("ERROR on daemon receive");
                 break;
              }
              //fprintf(stderr,"sproxy received a message from daemon\n");
              //setPacket(2, daemonbuf, daemonrecv, seqNum); //not a heartbeat, other message
              //seqNum++;
              send(newcproxysocket, daemonbuf, daemonrecv, 0);//forward message from daemon to cproxy
              //daemonrecv = 0;
          }
        }

        else
        {
          fprintf(stderr,"evaluating else, rv is %d\n", rv);
        }
        //setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message
        //send(newcproxysocket, packetbuf, sizeof(packetbuf), 0);//send the heartbeat
        //fprintf(stderr,"I'm waiting for a new message on sproxy\n");
        FD_ZERO(&readfds);
        FD_SET(newcproxysocket, &readfds);
        FD_SET(DaemonSocket, &readfds);
        tv.tv_sec = 5;//timeout is 1 sec to increment hbcount
        tv.tv_usec = 0;
        if (newcproxysocket > DaemonSocket) n = newcproxysocket + 1;
        else n = DaemonSocket + 1;
    }
    fprintf(stderr,"Closing server side sockets\n");
    close(DaemonSocket,2);
    close(newcproxysocket,2);
  }
  return 0;
}
