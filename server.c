#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "struct.h"
#include "helpers.h"

#define MAX_CONNECTIONS 600

void run_messages_server(int listenfd, int messagesfd) {

  struct pollfd poll_fds[MAX_CONNECTIONS];
  struct tcp_client connected_clients[MAX_CONNECTIONS];
  struct tcp_client disconnected_clients[MAX_CONNECTIONS];
  int nr_connected = 0;
  int nr_disconnected = 0;
  int rc;
  char buf[COMMANDSIZE + 1];

  memset(buf, 0, COMMANDSIZE + 1);
  struct tcp_message received_packet;

  // Set listenfd socket for listening
  rc = listen(listenfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // add stdio
  poll_fds[0].fd = STDIN_FILENO;
  poll_fds[0].events = POLLIN;

  // we add the two server sockets in poll_fds
  poll_fds[1].fd = listenfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = messagesfd;
  poll_fds[2].events = POLLIN;
  
  int num_clients = 3;

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
          DIE(strcmp(buf, "exit") != 0, "invalid command!\n");
          struct tcp_message exit_request;
          exit_request.type = 2;
          for (int j = 3; j < num_clients; j++) {
            send_all(poll_fds[j].fd, &exit_request, sizeof(exit_request));
          }
          return;

        } else if (poll_fds[i].fd == listenfd) {
          // a connection request came on the listenfd socket
          struct sockaddr_in tcp_cli_addr;
          socklen_t cli_len = sizeof(tcp_cli_addr);
          int newclientfd =
              accept(listenfd, (struct sockaddr *)&tcp_cli_addr, &cli_len);
          DIE(newclientfd < 0, "accept");
          int c = 1;
          rc = setsockopt(newclientfd, IPPROTO_TCP, c, (char *)&(c), sizeof(int));
          DIE(rc != 0, "setsockopt() failed");

          // the new socket returned by accept() is added to poll_fds
          poll_fds[num_clients].fd = newclientfd;
          poll_fds[num_clients].events = POLLIN;
          num_clients++;
          struct tcp_client new_client;

          new_client.fd = newclientfd;
          memcpy(new_client.ip_server, inet_ntoa(tcp_cli_addr.sin_addr),
                 strlen(inet_ntoa(tcp_cli_addr.sin_addr)) + 1);
          new_client.port = ntohs(tcp_cli_addr.sin_port);
		  new_client.subscriptions = 0;
          memcpy(&connected_clients[nr_connected], &new_client, sizeof(new_client));
          nr_connected++;

        // a message from an udp client has been received
        } else if (poll_fds[i].fd == messagesfd) {
          struct udp_message posted_by_udp;
          struct sockaddr_in client_addr;
          socklen_t cli_len = sizeof(client_addr);

					int rc = recvfrom(messagesfd, &posted_by_udp, sizeof(posted_by_udp), MSG_WAITALL,
								(struct sockaddr *)&client_addr, &cli_len);
          DIE(rc < 0, "recv");

          struct tcp_message for_tcp;

          for_tcp.client_addr = client_addr;
          for_tcp.type = 3;
          for (int j = 0; j < nr_connected; j++) {
            for (int k = 0; k < connected_clients[j].subscriptions; k++) {
              // if the user is subscribed to the message's topic
              if (strcmp(posted_by_udp.topic, connected_clients[j].topics[k].topic) == 0) {
                for_tcp.command_type = posted_by_udp.type;

				if(posted_by_udp.type == 3 && strlen(posted_by_udp.content) != 1500) {
					memcpy(for_tcp.command, posted_by_udp.content, strlen(posted_by_udp.content) + 1);
				}else {
                	memcpy(for_tcp.command, posted_by_udp.content, 1500);
				}
				memcpy(for_tcp.topic.topic, posted_by_udp.topic, strlen(posted_by_udp.topic) + 1);
                rc = send_all(connected_clients[j].fd, &for_tcp, sizeof(for_tcp));
                DIE(rc < 0, "udp message not sent to tcp client!\n");
                break;
              }
            }
          }
		memset(posted_by_udp.content, 0, 1500);
        } else {
          // messages have been received from the tcp clients
		  memset(&received_packet, 0, sizeof(received_packet));
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "recv");

          if (rc == 0) {
            // connection has closed
            for (int j = 0; j < nr_connected; j++) {
              if (poll_fds[i].fd == connected_clients[j].fd) {
                  disconnected_clients[nr_disconnected] = connected_clients[j];
                  nr_disconnected++;
                  printf("Client %s disconnected.\n", connected_clients[j].id);
                  for (int k = j; k < nr_connected - 1; k++) {
                      connected_clients[k] = connected_clients[k+1];
                  }
                  // one client has been disconnected
                  nr_connected--;
                  break;
              }
            }

            close(poll_fds[i].fd);

            // the closed socket is taken out from poll_fds
            for (int j = i; j < num_clients - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];
            }

            // the number of clients is decreased
            num_clients--;

          } else {
            if (received_packet.type == 0) {
              for (int j = 0; j < nr_connected; j++) {
                // the server receives an id that is already connected
                if (strcmp(received_packet.id, connected_clients[j].id) == 0) {
                  struct tcp_message exit_request;

                  exit_request.type = 2;
                  rc = send_all(poll_fds[i].fd, &exit_request, sizeof(exit_request));
                  printf("Client %s already connected.\n", received_packet.id);
                  
                  for (int search = 0; search < nr_connected; search++) {
                    if(poll_fds[i].fd == connected_clients[search].fd) {
                      for (int k = search; k < nr_connected - 1; k++) {
                        connected_clients[k] = connected_clients[k + 1];
                      }
                      // disconnect the client because the same id is already connected
                      nr_connected--;
                      break;
                    }
                  }
                }
                if (poll_fds[i].fd == connected_clients[j].fd) {
                  memcpy(connected_clients[j].id, received_packet.id, strlen(received_packet.id) + 1);
                  printf("New client %s connected from %s:%d\n", connected_clients[j].id,
                  					connected_clients[j].ip_server, connected_clients[j].port);

                  // check if the user has been previously connected
                  for (int k = 0; k < nr_disconnected; k++) {
                    if (strcmp(disconnected_clients[k].id, connected_clients[j].id) == 0) {
                      connected_clients[j].subscriptions = disconnected_clients[k].subscriptions;
                      memset(connected_clients[j].topics, 0, sizeof(connected_clients[j].topics));
                      // copy the array of topics he was subscribed to before logging out
                      memcpy(connected_clients[j].topics, disconnected_clients[k].topics,
                             sizeof(disconnected_clients[k].topics));
                      connected_clients[j].subscriptions = disconnected_clients[k].subscriptions;

                      for (int l = k; l < nr_disconnected - 1; l++) {
                        disconnected_clients[l] = disconnected_clients[l + 1];
                      }
                      nr_disconnected--;
                    }
                  }
                  break;
                }
              }
              // for (int clienti = 0; clienti < nr_connected; clienti++) {
              //   for (int topicuri = 0; topicuri < connected_clients[clienti].subscriptions)
              // }
            } else {
              if (strcmp(received_packet.command, "subscribe") == 0) {
                for (int j = 0; j < nr_connected; j++) {
                  if (strcmp(received_packet.id, connected_clients[j].id) == 0) {
                    connected_clients[j].topics[connected_clients[j].subscriptions] = received_packet.topic;
                    connected_clients[j].subscriptions++;
                    break;
                  }
                }
              } else if (strcmp(received_packet.command, "unsubscribe") == 0) {
                for (int l = 0; l < nr_connected; l++) {
                  if (strcmp(received_packet.id, connected_clients[l].id) == 0) {
                      for (int j = 0; j < connected_clients[l].subscriptions; j++) {
                          if (strcmp(connected_clients[l].topics[j].topic, received_packet.topic.topic) == 0) {
                              for (int k = j; k < connected_clients[l].subscriptions - 1; k++) {
                                  connected_clients[l].topics[k] = connected_clients[l].topics[k + 1];
                              }
                              // the topic has been removed from client's list
                              connected_clients[l].subscriptions--;
                              break;
                          }
                      }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {

  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  if (argc != 2) {
    printf("\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  // Turn the port into a number
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Socket opened for receiving connection requests from TCP clients
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd < 0, "TCP socket");

  // Socket opened for receiving messages from UDP clients
  int messagesfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(messagesfd < 0, "UDP socket");

  struct sockaddr_in listen_addr;
  struct sockaddr_in receive_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // We make the sockets addresses reusable
  int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  // Complete the server address, address family and connection port in
  // listen_addr
  memset(&listen_addr, 0, socket_len);
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(port);
  // Receive data in the server port from any ip address
  listen_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind the server address to the TCP socket listenfd
  rc = bind(listenfd, (const struct sockaddr *)&listen_addr, sizeof(listen_addr));
  DIE(rc < 0, "bind");

  enable = 1;
  if (setsockopt(messagesfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  
  // Complete the server address, address family and connection port in
  // receive_addr
  memset(&receive_addr, 0, socket_len);
  receive_addr.sin_family = AF_INET;
  receive_addr.sin_port = htons(port);
  // Receive messages in the server port from any ip address
  receive_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind the server address to the UDP socket messagesfd
  rc = bind(messagesfd, (const struct sockaddr *)&receive_addr, sizeof(receive_addr));
  DIE(rc < 0, "bind");

  run_messages_server(listenfd, messagesfd);

  // Close the file descriptors
  close(listenfd);
  close(messagesfd);

  return 0;
}
