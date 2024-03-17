#include <sys/socket.h>
#include <sys/types.h>

#include "struct.h"

// Receives exactly len bytes from buffer
int recv_all(int sockfd, void *buffer, size_t len) {

    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while (bytes_remaining) {
        int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
		DIE(rc == -1, "recv() failed!\n");
		if (rc == 0)
			return bytes_received;

		bytes_received += rc;
		bytes_remaining -= rc;
    }
    return bytes_received;
}

// Send exactly len bytes from buffer
int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while (bytes_remaining) {
		int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
	  	DIE(rc == -1, "send() failed!\n");
	  	if (bytes_sent == 0)
			return bytes_sent;

	  	bytes_sent += rc;
	  	bytes_remaining -= rc;
	}
  
  	return bytes_sent;
}
