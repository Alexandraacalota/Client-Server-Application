#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "struct.h"
#include "helpers.h"

void run_client(int sockfd, struct tcp_client* client) {
  char buf[COMMANDSIZE + 1];
  memset(buf, 0, COMMANDSIZE + 1);

  struct tcp_message recv_packet;
  struct pollfd poll_fds[2];

  // add stdio
  poll_fds[0].fd = STDIN_FILENO;
  poll_fds[0].events = POLLIN;

  // add socket to communicate with server
  poll_fds[1].fd = sockfd;
  poll_fds[1].events = POLLIN;

  int num_clients = 2;

  struct tcp_message client_id;
  client_id.type = 0;
  memcpy(client_id.id, client->id, strlen(client->id) + 1);

  int rc = send_all(sockfd, &client_id, sizeof(struct tcp_message));

  while (1) {
    rc = poll(poll_fds, num_clients, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_clients; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == STDIN_FILENO) {
          // got data from STDIN
          memset(buf, 0, COMMANDSIZE + 1);
          rc = read(STDIN_FILENO, buf, sizeof(buf));
          DIE(rc < 0, "read failed!\n");
          
          buf[strcspn(buf, "\n")] = '\0';
          struct tcp_message request;

          // parse the input string
          if (strcmp(buf, "exit") == 0) {
              return;

          } else if (sscanf(buf, "unsubscribe %s", request.topic.topic) == 1) {
              strcpy(request.command, "unsubscribe");
              memcpy(request.id, client->id, strlen(client->id) + 1);
              printf("Unsubscribed from topic.\n");
              rc = send_all(sockfd, &request, sizeof(request));

          } else if (sscanf(buf, "subscribe %s %hhd", request.topic.topic, &request.topic.sf) == 2) {
              strcpy(request.command, "subscribe");
              memcpy(request.id, client->id, strlen(client->id) + 1);
              printf("Subscribed to topic.\n");
              rc = send_all(sockfd, &request, sizeof(request));

          } else {
              printf("invalid command!\n");
          }
          DIE(rc < 0, "send failed!\n");

        } else if (poll_fds[i].fd == sockfd) {
          // got data from server
          rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
          DIE(rc < 0, "recv_all\n");

          // exit request from the server
          if (recv_packet.type == 2) {
            return;

          // message from the udp clients that pass through the server
          } else if (recv_packet.type == 3) {
          printf("%s:%d - %s - ", inet_ntoa(recv_packet.client_addr.sin_addr),
                 ntohs(recv_packet.client_addr.sin_port), recv_packet.topic.topic);
            if (recv_packet.command_type == 0) {
              struct int_type nr;
              nr.sign = (*(uint8_t *) recv_packet.command);
              nr.number = (*(uint32_t *)(recv_packet.command + 1));
              nr.number = ntohl(nr.number);
              if (nr.sign == 0) {
                printf("INT - %d\n", nr.number);
              } else {
                printf("INT - %d\n", -nr.number);
              }

            } else if (recv_packet.command_type == 1) {
              uint16_t nr = ntohs(*(uint16_t *) recv_packet.command);
              uint16_t integer_part = nr / 100;
              uint16_t decimal_part = nr % 100;
              printf("SHORT_REAL - %d.%02d\n", integer_part, decimal_part);

            } else if (recv_packet.command_type == 2) {
              struct float_type nr;
              nr.sign = (*(uint8_t *) recv_packet.command);
              nr.number = ntohl(*(uint32_t *) (recv_packet.command + 1));
              nr.pow = (*(uint8_t *) (recv_packet.command + 5));
              float number = nr.number * pow(10, -nr.pow);
              if (nr.sign == 0) {
                printf("FLOAT - %.4f\n", number);
              } else {
                printf("FLOAT - %.4f\n", -number);
              }

            } else if (recv_packet.command_type == 3) {
              printf("STRING - %s\n", recv_packet.command);
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  struct tcp_client client;
  int sockfd = -1;

  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  if (argc != 4) {
    printf("\n Usage: %s <ip> <port>\n", argv[0]);
    return 1;
  }

  memcpy(client.id, argv[1], strlen(argv[1]) + 1);
  client.fd = -1;
  client.topics->sf = -1;
  client.subscriptions = 0;


  // Turn port from string to number
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Get a TCP socket to send to the listener
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Complete in serv_addr the server address, the addresses family
  // and the connection port
  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // The client connects to the server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  run_client(sockfd, &client);

  // Close the connection and the socket that connects the client
  // to the server
  close(sockfd);

  return 0;
}
