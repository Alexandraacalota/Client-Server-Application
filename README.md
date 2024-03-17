Copyright 2023 Alexandra-Maria Calota
# Client-Server Application

## Description:

- The server is implemented in `server.c` file and each subscriber in `client.c`.

- Header file `struct.h` contains all the structures used in the homework implementation:
    - `struct topic`: defines the topic along with its store-and-forward flag (0 or 1).
    - `struct tcp_client`: defines a TCP client stored in the `connected_clients` and `disconnected_clients` arrays, along with all the information the server needs for each client: his id, the topics he is subscribed to, the number of topics he is subscribed to, a file descriptor of the socket that connects the client to the server, the IP, and port from which the client connects to the server.
    - `struct udp_message`: defines the message sent from the UDP client, according to its protocol header topic | type | content.
    - `struct tcp_message`: defines the packages sent from the server to the TCP clients and the other way round, according to the protocol over TCP implemented.
    - `struct int_type`: defines the type for the data contained in the message of type INT received from the UDP client, containing a sign and an `uint32_t`.
    - `struct float_type`: defines the type for the data contained in the message of type FLOAT received from the UDP client, containing a sign, an `uint32_t`, and an `uint8_t`.

## Server Implementation:

- The server implementation started from the one in the 7th laboratory, adding a new socket besides `listenfd` on which the server receives connection requests from the TCP client, called `messagesfd`, on which the server receives messages from the UDP clients. The `STDIN_FILENO` is also added to the server sockets list.
- Then, after every `accept()`, the server adds the socket that has just been opened for a connection to a TCP client to `poll_fds` array.
- On `messagesfd` socket can be sent packets of type `struct udp_message`, whereas on the sockets that create a connection between the server and the TCP clients can only be sent packets according to the protocol over TCP, whose header contains the fields:
    - `type`: can be:
             - 0 for messages sent from TCP clients to the server containing only his id
             - 1 for subscribe, unsubscribe, and exit commands from TCP clients
             - 2 for exit requests sent from the server to the TCP client (when the server receives an exit command)
             - 3 for messages sent by the UDP clients that are forwarded by the server to the TCP clients under the `tcp_message` format type.
    - `id`: containing the client’s id
    - `command_type`: that is filled in case of a message sent from the UDP clients
    - `command`: contains either the subscribe or unsubscribe commands, either the message itself stored as a string
    - `struct topic`: that contains the topic string and the sf (store and forward) flag
    - `client_addr`: that contains the sender UDP client’s IP address and port.
- The server contains the two arrays that contain the TCP clients that are logged in and the ones that logged out.

## Client Implementation:

- The client implementation also starts from the one in the 7th laboratory, being able to receive data from `stdin` or the socket resulted from the connection to the server.
- On the socket `sockfd` of the connection to the server it can get packets of type `struct tcp_message` that have field type either 2 (an exit request) or 3 (a message from the UDP client that passes through the server and turns it from type `struct udp_message` to type `struct tcp_message`).
    - `tcp_message` type 2 is an exit command received from the server. The client closes its end of the connection.
    - `tcp_message` type 3 is the message itself that has to be printed according to its own content type: 0, 1, 2, or 3.
