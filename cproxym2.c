#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#define BUFFERSIZE 1024

//method called error, exit(0) means you are exiting from the program
void error(char *msg)
{
  perror(msg);
  exit(0);
}
//initialize vars for all functions
struct sockaddr_in telnet_addr, sproxy_addr;
char telnetbuf[BUFFERSIZE], sproxybuf[BUFFERSIZE], packetbuf[BUFFERSIZE];
int option = 1;

//FUNCTIONS
int TelnetConnect(int portno)
{
  // Create socket
  int TelnetSocket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(TelnetSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

  if (TelnetSocket < 0)
  {
    error("ERROR opening Telnet socket");
  }

  bzero((char *) &telnet_addr, sizeof(telnet_addr));//clearing telnet_addr with bzero method
  telnet_addr.sin_family = AF_INET;//saying to server address, all will be in internet address concept
  telnet_addr.sin_addr.s_addr = INADDR_ANY;// get your address on your own when you start the program
  telnet_addr.sin_port = htons(portno);// convert integer format to network format with htons

  if (bind(TelnetSocket, (struct sockaddr *) &telnet_addr, sizeof(telnet_addr)) < 0)
  {
    error("ERROR on binding Telnet Socket");
  }
  return TelnetSocket;
}

int SproxyConnect(char *host, int portno)
{
  // Create socket
  int SproxySocket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(SproxySocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

  if (SproxySocket < 0)
  {
    error("ERROR opening Sproxy socket");
  }

  bzero((char *) &sproxy_addr, sizeof(sproxy_addr));
  sproxy_addr.sin_family = AF_INET;
  inet_aton(host, &sproxy_addr.sin_addr.s_addr);
  sproxy_addr.sin_port = htons(portno);

  fprintf(stderr,"Received and converted server IP\n");

  return SproxySocket;
}

char* setPacket(int type, char* payload, int len, int seq)
{
   bzero(packetbuf, sizeof(packetbuf));
   char *p = packetbuf; //points to start of packetbuf
   *((int*) p) = type;//loads raw integer data to start of p pointer
   p = p + 4; //move pointer by size of int
   *((int*) p) = seq;
   p = p + 4;
   *((int*) p) = len;
   p = p + 4;
   memcpy(p, payload, len);
   return packetbuf;
}

int getPacketType(char* packet)
{
    return *((int*) packet);//starting from initial packet pointer and moving an integer length down is int data
}

char* getPacketMsg(char* packet)
{
    return packet + 12;
}

int main(int argc, char *argv[])
{
    // set vars for creating sockets
    int TelnetSocket, SproxySocket;
    int telnetport, sproxyport;
    socklen_t len1;
    //set vars for select
    fd_set readfds;
    struct timeval tv;
    //set vars for receiving and sending messages
    int rv;
    int n, len = 0;
    char buf1[BUFFERSIZE],buf2[BUFFERSIZE];

    //have all necessary command line arguments been given
    if (argc < 3)
    {
      fprintf(stderr, "usage %s hostname port\n", argv[0]);
      exit(0);
    }

    //port no passed in command line arg, to convert character to int we use atoi
    telnetport = atoi(argv[1]);
    sproxyport = atoi(argv[3]);

    //calling socket set up functions
    TelnetSocket = TelnetConnect(telnetport);
    SproxySocket = SproxyConnect(argv[2],sproxyport);
    //fprintf(stderr,"I'm listening on telnet\n");
    listen(TelnetSocket, 5);
    while(1)
        {
          fprintf(stderr,"I'm listening\n");
          //calling socket set up functions
          //TelnetSocket = TelnetConnect(telnetport);
          //SproxySocket = SproxyConnect(argv[2],sproxyport);

          //going into listen mode on telnet socket, can handle 5 clients
          //fprintf(stderr,"I'm listening on telnet\n");
          //listen(TelnetSocket, 5);

          //creating new socket for particular client that has my address and client address
          //telnet_addr provides all the info i need about the client
          int newtelnetsocket = accept(TelnetSocket, (struct sockaddr *) &telnet_addr, &len1);

          if (newtelnetsocket <0)
          {
            error("ERROR on telnet accept");
          }
          fprintf(stderr,"Connected to a client on telnet\n");

          //connect to sproxy
          if (connect(SproxySocket, &sproxy_addr, sizeof(sproxy_addr)) < 0)
          {
            error("ERROR connecting to sproxy");
          }
          fprintf(stderr,"Connected to sproxy\n");

          ////////////////////////////////////////////////////////////////
          ////////////////////////////////////////////////////////////////
          ////////////////////////////////////////////////////////////////

          //Begin message sending loop
                while(1)
                {
                  // clear the set
                  FD_ZERO(&readfds);

                  // add descriptors (fd) to set
                  FD_SET(newtelnetsocket, &readfds);
                  FD_SET(SproxySocket, &readfds);

                  // find the largest descriptor, and plus one.
                  if (newtelnetsocket > SproxySocket) n = newtelnetsocket + 1;
                  else n = SproxySocket + 1;

                  //timeout is 10.5 sec to receive data on either socket
                  tv.tv_sec = 10;
                  tv.tv_usec = 500000; //this is .5 sec

                  //Engage select function, will return when at least one socket
                  //has traffic or timeout.
                  rv = select(n, &readfds, NULL, NULL, &tv);

                  if (rv == -1)
                  {
                      error("ERROR engaging select function on client");
                  }

                  else if (rv == 0)
                  {
                      printf("No data given before timeout window\n");
                  }
                  else
                  {
                      // one or both of the descriptors have data
                      if (FD_ISSET(newtelnetsocket, &readfds))
                      {
                          len = recv(newtelnetsocket, buf1, sizeof(buf1), 0);
                          if (len <= 0)
                          {
                              break;
                          }
                          send(SproxySocket, buf1, len, 0);
                      }

                      if (FD_ISSET(SproxySocket, &readfds))
                      {
                          len = recv(SproxySocket, buf2, sizeof(buf2), 0);
                          if (len <= 0)
                          {
                              break;
                          }
                          send(newtelnetsocket, buf2, len, 0);
                      }
                  }
                }
              close(SproxySocket,2);
              close(newtelnetsocket,2);
            }
    return 0;
}
