#ifndef _STRUCT_H_
#define _STRUCT_H_

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

#include "helpers.h"

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define COMMANDSIZE 1024
#define MAX_TOPICS 25
#define ID_MAXSIZE 20
#define POSTSIZE 1500

struct topic {
  char topic[50];
  uint8_t sf;
};

struct tcp_client {
  char id[ID_MAXSIZE + 1];
  struct topic topics[MAX_TOPICS];
  int subscriptions;
  int fd;
  char ip_server[14];
  int port;
};

struct udp_message {
  char topic[50];
  uint8_t type;
  char content[POSTSIZE];
};

struct tcp_message {
  uint16_t type;
  char id[ID_MAXSIZE + 1];
  uint8_t command_type;
  char command[POSTSIZE];
  struct topic topic;
  struct sockaddr_in client_addr;
};

struct int_type {
  uint8_t sign;
  uint32_t number;
};

struct float_type {
  uint8_t sign;
  uint32_t number;
  uint8_t pow;
};

#endif
