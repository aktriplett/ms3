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
    int TelnetSocket, SproxySocket;
    int telnetport, sproxyport;
    socklen_t len1;
    fd_set readfds;
    struct timeval tv;
    int rv;
    int n, len = 0;
    int hbcount = 0;
    int telnetrecv, sproxyrecv = 0;
    char buf1[BUFFERSIZE],buf2[BUFFERSIZE];

    if (argc < 3)//have all necessary command line arguments been given
    {
      fprintf(stderr, "usage %s hostname port\n", argv[0]);
      exit(0);
    }

    telnetport = atoi(argv[1]);//port no passed in command line arg, to convert character to int we use atoi
    sproxyport = atoi(argv[3]);
    TelnetSocket = TelnetConnect(telnetport);//calling socket set up functions
    SproxySocket = SproxyConnect(argv[2],sproxyport);

    listen(TelnetSocket, 5);

    //while(1)
    //{
    fprintf(stderr,"I'm listening on telnet\n");

    int newtelnetsocket = accept(TelnetSocket, (struct sockaddr *) &telnet_addr, &len1);
    if (newtelnetsocket < 0)
    {
      error("ERROR on telnet accept");
    }
    fprintf(stderr,"Connected to telnet local host\n");

    //connect to sproxy
    if (connect(SproxySocket, &sproxy_addr, sizeof(sproxy_addr)) < 0)
    {
      error("ERROR connecting to sproxy");
    }
    fprintf(stderr,"Connected to sproxy\n");

    FD_ZERO(&readfds);// clear the set
    FD_SET(newtelnetsocket, &readfds);// add descriptors (fd) to set
    FD_SET(SproxySocket, &readfds);
    if (newtelnetsocket > SproxySocket) n = newtelnetsocket + 1;// find the largest descriptor, and plus one.
    else n = SproxySocket + 1;
    tv.tv_sec = 10;//timeout is 10.5 sec to receive data on either socket
    tv.tv_usec = 0; //this is .5 sec

    //Begin message sending loop
    while((rv = select(n, &readfds, NULL, NULL, &tv)) >= 0)
    {
      setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message (ID 1)
      fprintf(stderr, "set the hb packet\n");
      send(SproxySocket, packetbuf, 14, 0);//send the heartbeat contained in packet buf to sproxy
      fprintf(stderr,"Client sent a heartbeat message to server:\n");
      if (rv == 0)
      {
        hbcount++;
        //heartbeat hits 3 so we assume the connection timed out and we close
        if (hbcount == 3)
        {
          fprintf(stderr,"hb hit three, reset\n");
          hbcount = 0;//reset hb count
          fprintf(stderr, "reset hb\n");
          close(SproxySocket);//close disconnected socket

          int SproxySocket = SproxyConnect(argv[2],sproxyport);

          if (connect(SproxySocket, &sproxy_addr, sizeof(sproxy_addr)) < 0)
          {
            error("ERROR connecting NEW sproxy\n");
          }
          fprintf(stderr,"cproxy made a NEW connection to sproxy\n");
        }
      }

      else
      {
        bzero(buf1, sizeof(buf1));
        bzero(buf2, sizeof(buf2));
        // one or both of the descriptors have data
        if (FD_ISSET(newtelnetsocket, &readfds))
        {
            len = recv(newtelnetsocket, buf1, sizeof(buf1), 0);
            if (len < 0)
            {
              error("ERROR on telnet receive\n");
              break;
            }
            else
            {
              //setPacket(2, buf1, len, 1);
              //send(SproxySocket, buf1, len, 0);
              //len = 0;
            }
        }

        if (FD_ISSET(SproxySocket, &readfds))
        {
            len = recv(SproxySocket, buf2, sizeof(buf2), 0);
            if (len < 0)
            {
              error("ERROR on sproxy receive\n");
              break;
            }
            else
            {
              //send(newtelnetsocket, buf2, len, 0);
              //if (getPacketType(buf2) == 2)//we are forwarding the sproxy message to telnet
              //{
              //     fprintf(stderr, "Got a ping\n");
              //     send(newtelnetsocket, getPacketMsg(buf2), len - 12, 0);
              //     len = 0;
              // }
              // else if (getPacketType(buf2) == 1)//we received a heartbeat from sproxy and  will reset the hbcount
              // {
              //     fprintf(stderr, "Got a heartbeat\n");
              //     hbcount = 0;
              //     len = 0;
              // }
              // else
              // {
              //   fprintf(stderr, "Inside sproxy buffer\n");
              //   len = 0;
              // }
            }
        }
      }
      FD_ZERO(&readfds);// clear the set
      FD_SET(newtelnetsocket, &readfds);// add descriptors (fd) to set
      FD_SET(SproxySocket, &readfds);
      if (newtelnetsocket > SproxySocket) n = newtelnetsocket + 1;// find the largest descriptor, and plus one.
      else n = SproxySocket + 1;
      tv.tv_sec = 10;//timeout is 10.5 sec to receive data on either socket
      tv.tv_usec = 0; //this is .5 sec
    }
    close(SproxySocket,2);
    close(newtelnetsocket,2);
      //break;
  //}
  return 0;
}
