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

//initialize vars
struct sockaddr_in telnet_addr, sproxy_addr;
char telnetbuf[BUFFERSIZE], sproxybuf[BUFFERSIZE], packetbuf[BUFFERSIZE];

//FUNCTIONS
//telnet connect function
int TelnetConnect(int portno)
{
  int TelnetSocket = socket(AF_INET, SOCK_STREAM, 0);// Create socket
  if (TelnetSocket < 0)
  {
    error("ERROR opening Telnet socket");
  }

  bzero((char *) &telnet_addr, sizeof(telnet_addr));//clearing telnet_addr with bzero method
  telnet_addr.sin_family = AF_INET;//saying to server address, all will be in internet address concept
  telnet_addr.sin_addr.s_addr = INADDR_ANY;// get your address on your own when you start the program
  telnet_addr.sin_port = htons(portno);// convert integer format to network format with htons

  // Bind socket
  if (bind(TelnetSocket, (struct sockaddr *) &telnet_addr, sizeof(telnet_addr)) < 0)
  {
    error("ERROR on binding Telnet Socket");
  }
  return TelnetSocket;
}

//server proxy connect function
int SproxyConnect(char *host, int portno)
{
  int SproxySocket = socket(AF_INET, SOCK_STREAM, 0);// Create socket
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

//int getSessionID()
//int rannum = 0;
//  rannum = rand();
//  return rannum;

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
//main function
int main(int argc, char *argv[])
{
    //have all necessary command line arguments been given
    if (argc < 4) //or should this be 3
    {
      fprintf(stderr, "usage %s hostname port\n", argv[0]);
      exit(0);
    }
    socklen_t len1;
    int rv;
    int n, len = 0;
    int telnetrecv = 0;
    int sproxyrecv = 0;
    int telnetport = atoi(argv[1]);//port no passed in command line arg, to convert character to int we use atoi
    int sproxyport = atoi(argv[3]);
    int TelnetSocket = TelnetConnect(telnetport);
    //int SproxySocket = SproxyConnect(argv[2],sproxyport);

    listen(TelnetSocket, 5);
    fprintf(stderr,"I'm listening on telnet\n");

    //telnet local host triggers this loop
    while(1)
    {
      int newtelnetsocket = accept(TelnetSocket, (struct sockaddr *) &telnet_addr, &len1);
      if (newtelnetsocket <0)
      {
        error("ERROR on telnet accept\n");
      }
      fprintf(stderr,"Connected to telnet local host\n");

      //connect to sproxy
      int SproxySocket = SproxyConnect(argv[2],sproxyport);
      if (connect(SproxySocket, &sproxy_addr, sizeof(sproxy_addr)) < 0)
      {
        error("ERROR connecting to sproxy\n");
      }
      fprintf(stderr,"Connected to sproxy\n");

      //set vars for select
      fd_set readfds;
      FD_ZERO(&readfds);// clear the set
      FD_SET(newtelnetsocket, &readfds);// add descriptors (fd) to set
      FD_SET(SproxySocket, &readfds);
      if (newtelnetsocket > SproxySocket) n = newtelnetsocket + 1;// find the largest descriptor, and plus one.
      else n = SproxySocket + 1;
      struct timeval tv;
      tv.tv_sec = 20;//timeout is 1 sec to increment hbcount
      tv.tv_usec = 0;
      int hbcount = 0;
      //set up the random hb int for the session and send to the server
      //sessionID = getSessionID();

      //Begin message sending loop
      while((rv = select(n, &readfds, NULL, NULL, &tv)) >= 0)
      {
        tv.tv_sec = 1;//timeout is 1 sec to increment hbcount
        //setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message (ID 1)
        //send(SproxySocket, packetbuf, 14, 0);//send the heartbeat contained in packet buf to sproxy
        //fprintf(stderr,"Client sent a heartbeat message to server:%s\n",packetbuf);

        if (rv == 0)//Timeout occured, no message received so sending heartbeat
        {
          setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message (ID 1)
          send(SproxySocket, packetbuf, 14, 0);//send the heartbeat contained in packet buf to sproxy
          fprintf(stderr,"timed out, incrementing hb count\n");
          hbcount++;
          //heartbeat hits 3 so we assume the connection timed out and we close
          if (hbcount == 3)
          {
            fprintf(stderr,"closing socket connection to sproxy\n");
            hbcount = 0;//reset hb count
            close(SproxySocket);//close disconnected socket

            int SproxySocket = SproxyConnect(argv[2],sproxyport);

            if (connect(SproxySocket, &sproxy_addr, sizeof(sproxy_addr)) < 0)
            {
              error("ERROR connecting NEW sproxy\n");
              break;
            }
            fprintf(stderr,"cproxy made a NEW connection to sproxy\n");
          }
        }
        else//no timeout, rv = 1 and we have received
        {
          //fprintf(stderr,"zeroing buffers to receive message\n");
          //zero out both message buffers
          bzero(telnetbuf, sizeof(telnetbuf));
          bzero(sproxybuf, sizeof(sproxybuf));

          //check the telnet buffer
          if (FD_ISSET(newtelnetsocket, &readfds))
          {
            telnetrecv = recv(newtelnetsocket, telnetbuf, sizeof(telnetbuf), 0);
            if (telnetrecv < 0)
            {
              error("ERROR on telnet receive\n");
              break;
            }
            else
            {
              fprintf(stderr, "Received message from telnet\n");
              //setPacket(2, telnetbuf, telnetrecv, 1);
              send(SproxySocket, telnetbuf, telnetrecv, 0);//telnetrecv + 12
              fprintf(stderr,"Forwarding telnet message to sproxy\n");
              //telnetrecv = 0;
            }

          }

          //check the sproxy buffer
          if (FD_ISSET(SproxySocket, &readfds))
          {
            sproxyrecv = recv(SproxySocket, sproxybuf, sizeof(sproxybuf), 0);
            if (sproxyrecv < 0)
            {
              error("ERROR on sproxy receive\n");
              break;
            }
            else
            {
              //fprintf(stderr, "Received message from sproxy\n");
              //if (getPacketType(sproxybuf) == 2)//we are forwarding the sproxy message to telnet
              //{
              //    fprintf(stderr, "Got a ping\n");
              //    send(newtelnetsocket, sproxybuf, sproxyrecv, 0);//sproxyrecv - 12
              //}
              if (getPacketType(sproxybuf) == 1)//we received a heartbeat from sproxy and  will reset the hbcount
              {
                  //fprintf(stderr, "Got a heartbeat, resetting hb count: %s\n", sproxybuf);
                  hbcount = 0;
              }
              else
              {
                send(newtelnetsocket, sproxybuf, sproxyrecv, 0);//sproxyrecv - 12
                //fprintf(stderr, "Got an non-header message from sproxy\n");
              }
            }
          }
        }
        //fprintf(stderr, "I'm getting ready to send/recv more messages\n");
        //clear everything and get ready for more messages
        //setPacket(1, "hb", 2, hbcount);//we know we have to send a heartbeat format message (ID 1)
        //send(SproxySocket, packetbuf, 14, 0);//send the heartbeat contained in packet buf to sproxy
        FD_ZERO(&readfds);
        FD_SET(newtelnetsocket, &readfds);
        FD_SET(SproxySocket, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (newtelnetsocket > SproxySocket) n = newtelnetsocket + 1;
        else n = SproxySocket + 1;
      }
    fprintf(stderr,"Timed out - Closing all connections and setting new IP:\n");
    close(newtelnetsocket);
    close(SproxySocket);
    break;
  }
  return 0;
}
