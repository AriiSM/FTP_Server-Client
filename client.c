#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define FTP_PORT 21
#define BUFFER_SIZE 1024

ssize_t receive_full_response(int sock, char *buffer, size_t buffer_size) {
     ssize_t total_len = 0;
     ssize_t len;

     while ((size_t)total_len < buffer_size - 1) {
          len = recv(sock, buffer + total_len,
                     buffer_size - 1 - (size_t)total_len, 0);
          if (len <= 0) {
               if (len == 0) {
                    printf("Connection closed by the server.\n");
               } else {
                    perror("Error receiving data");
               }
               break;
          }
          total_len += len;

          if (strstr(buffer, "\r\n") || strstr(buffer, "\n")) {
               break;
          }
     }

     buffer[total_len] = '\0';
     return total_len;
}

int start_data_connection(const char *ip, int port) {
     int data_sock = socket(AF_INET, SOCK_STREAM, 0);
     if (data_sock < 0) {
          perror("Data socket creation failed");
          return -1;
     }

     struct sockaddr_in data_addr = {0};
     data_addr.sin_family = AF_INET;
     data_addr.sin_port = htons((uint16_t)port);
     inet_pton(AF_INET, ip, &data_addr.sin_addr);

     if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) <
         0) {
          perror("Data connection failed");
          close(data_sock);
          return -1;
     }

     return data_sock;
}
void handle_retr_command(int server_sock, const char *filename,
                         const char *data_ip, int data_port) {
     // Send the RETR command to the server
     char command[BUFFER_SIZE];
     snprintf(command, sizeof(command), "RETR %s\r\n", filename);
     send(server_sock, command, strlen(command), 0);

     printf("Waintg for initial response\n");

     // Establish the data connection (passive mode only)
     int data_sock = start_data_connection(data_ip, data_port);
     if (data_sock < 0) {
          printf("Failed to establish data connection.\n");
          return;
     }
     // Receive the initial server response
     char response[BUFFER_SIZE];
     ssize_t response_len =
         recv(server_sock, response, sizeof(response) - 1, 0);
     if (response_len <= 0) {
          perror("Failed to receive response from server");
          return;
     }
     response[response_len] = '\0';
     printf("Server: %s", response);

     // Check if the server approved the RETR command
     if (strncmp(response, "150", 3) != 0) {
          printf("Server did not approve RETR command.\n");
          return;
     }
     printf("Openning file\n");
     char filepath[BUFFER_SIZE];
     snprintf(filepath, sizeof(filepath), "./data/%s", filename);
     FILE *file = fopen(filepath, "wb");
     if (!file) {
          perror("Failed to open file for writing");
          close(data_sock);
          return;
     }
     printf("Reading file\n");
     // Receive the file data from the server
     char file_buffer[BUFFER_SIZE];
     ssize_t bytes_received;
     while ((bytes_received =
                 recv(data_sock, file_buffer, sizeof(file_buffer), 0)) > 0) {
          if (bytes_received > 0) {
               fwrite(file_buffer, 1, (size_t)bytes_received, file);
               printf("Received %ld bytes.\n", bytes_received);
          }
     }

     fclose(file);
     close(data_sock);

     printf("Closed data stream, waiting for final reply\n");

     // Receive the final server response
     response_len = recv(server_sock, response, sizeof(response) - 1, 0);
     if (response_len > 0) {
          response[response_len] = '\0';
          printf("Server: %s", response);
     }
}

void handle_pasv_command(int control_sock, char *data_ip, int *data_port) {
     char buffer[BUFFER_SIZE];

     send(control_sock, "PASV\r\n", strlen("PASV\r\n"), 0);
     receive_full_response(control_sock, buffer, BUFFER_SIZE);
     printf("Server response: %s\n", buffer);

     // Parse PASV response to extract IP and port
     int h1, h2, h3, h4, p1, p2;
     sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &h1, &h2,
            &h3, &h4, &p1, &p2);
     snprintf(data_ip, INET_ADDRSTRLEN, "%d.%d.%d.%d", h1, h2, h3, h4);
     *data_port = (p1 << 8) | p2;

     printf("Passive mode IP: %s, Port: %d\n", data_ip, *data_port);
}

void handle_stor_command(int control_sock, const char *filename,
                         const char *data_ip, int data_port) {
     char buffer[BUFFER_SIZE];

     char filepath[BUFFER_SIZE];
     snprintf(filepath, sizeof(filepath), "data/%s", filename);

     FILE *file = fopen(filepath, "rb");
     if (!file) {
          perror("Failed to open file");
          return;
     }
     printf("Opened file!\n");

     int data_sock = start_data_connection(data_ip, data_port);
     if (data_sock < 0) {
          fclose(file);
          printf("Failed to open socket!\n");
          return;
     }

     // Send STOR command
     snprintf(buffer, BUFFER_SIZE, "STOR %s\r\n", filename);
     send(control_sock, buffer, strlen(buffer), 0);
     printf("Sent STOR command\n");

     // Send file data
     size_t bytes_read;
     while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
          send(data_sock, buffer, bytes_read, 0);
          printf("Sending %ld bytes.\n", bytes_read);
     }

     printf("Closing data connection after file transfer.\n");

     fclose(file);
     close(data_sock);

     // Receive final server response
     receive_full_response(control_sock, buffer, BUFFER_SIZE);
     printf("Server response: %s\n", buffer);
}

void send_user_command(int sock, const char *username) {
     char buffer[BUFFER_SIZE];
     snprintf(buffer, sizeof(buffer), "USER %s\r\n", username);
     send(sock, buffer, strlen(buffer), 0);

     recv(sock, buffer, sizeof(buffer) - 1, 0);
     printf("Server: %s", buffer);
}

void send_pass_command(int sock, const char *password) {
     char buffer[BUFFER_SIZE];
     snprintf(buffer, sizeof(buffer), "PASS %s\r\n", password);
     send(sock, buffer, strlen(buffer), 0);

     recv(sock, buffer, sizeof(buffer) - 1, 0);
     printf("Server: %s", buffer);
}

void ftp_client(const char *server_ip) {
     int sock;
     struct sockaddr_in server_addr;
     char buffer[BUFFER_SIZE];
     char command[BUFFER_SIZE];

     sock = socket(AF_INET, SOCK_STREAM, 0);
     if (sock < 0) {
          perror("Socket creation failed\n");
          return;
     }

     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(FTP_PORT);
     inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

     if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
         0) {
          perror("Connection failed!\n");
          close(sock);
          return;
     }

     ssize_t len = receive_full_response(sock, buffer, BUFFER_SIZE);
     if (len <= 0) return;
     printf("Server greeting: %s\n", buffer);

     char data_ip[INET_ADDRSTRLEN];
     int data_port;

     while (1) {
          printf(">> ");
          if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
               perror("Error reading input!\n");
               break;
          }

          command[strcspn(command, "\n")] = '\0';

          if (strncmp(command, "PASV", 4) == 0) {
               handle_pasv_command(sock, data_ip, &data_port);
          } else if (strncmp(command, "STOR", 4) == 0) {
               char *filename = command + 5;
               handle_stor_command(sock, filename, data_ip, data_port);
          } else if (strncmp(command, "RETR", 4) == 0) {
               char filename[BUFFER_SIZE];
               sscanf(command, "RETR %s", filename);
               handle_retr_command(sock, filename, data_ip, data_port);
          } else if (strncmp(command, "USER", 4) == 0) {
               char username[BUFFER_SIZE];
               sscanf(command, "USER %s", username);
               send_user_command(sock, username);
          } else if (strncmp(command, "PASS", 4) == 0) {
               char username[BUFFER_SIZE];
               sscanf(command, "PASS %s", username);
               send_pass_command(sock, username);
          } else {
               snprintf(buffer, BUFFER_SIZE, "%.1021s\r\n", command);
               send(sock, buffer, strlen(buffer), 0);

               len = receive_full_response(sock, buffer, BUFFER_SIZE);
               if (len <= 0) break;
               printf("%s\n", buffer);

               if (strcmp(command, "QUIT") == 0) {
                    printf("Exiting...\n");
                    break;
               }
          }
     }

     close(sock);
}

int main(int argc, char *argv[]) {
//     ftp_client("127.0.0.1");
        if (argc < 2) {
        fprintf(stderr, "Usage: %s <IP_ADDRESS>\n", argv[0]);
        return EXIT_FAILURE;
        }

        ftp_client(argv[1]);
        return 0;
}