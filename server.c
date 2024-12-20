#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FTP_PORT 21
#define BUFFER_SIZE 1024
#define MAX_ARGUMENTS 10
#define ROOT_DIR "/server_data"

#define NUM_VALID_COMMANDS 29

char current_dir[BUFFER_SIZE] = ROOT_DIR;

const char *valid_commands[] = {"USER", "PASS", "ACCT", "CWD",  "CDUP", "SMNT",
                                "QUIT", "REIN", "PORT", "PASV", "TYPE", "STRU",
                                "MODE", "RETR", "STOR", "DELE", "RNFR", "RNTO",
                                "ABOR", "LIST", "NLST", "SITE", "SYST", "STAT",
                                "HELP", "NOOP", "PWD",  "MKD",  "RMD"};

typedef struct {
     int active;  // 1 for active, 0 for passive
     int data_socket;
     struct sockaddr_in client_addr;
} DataConnection;

DataConnection data_connection = {0, -1, {0}};

typedef struct {
     bool authenticated;
     char username[BUFFER_SIZE];
} ClientSession;

ClientSession client_session;

const char *valid_users[][2] = {{"user1", "password1"}, {"user2", "password2"}};
const int NUM_USERS = 2;

void split_client_input(const char *input, char *tokens[], int *token_count) {
     *token_count = 0;
     char buffer[BUFFER_SIZE];

     strncpy(buffer, input, sizeof(buffer) - 1);
     buffer[sizeof(buffer)] = '\0';

     char *token = strtok(buffer, " ");
     while (token != NULL && *token_count < MAX_ARGUMENTS) {
          tokens[*token_count] = token;
          (*token_count)++;
          token = strtok(NULL, " ");
     }
}

bool validate_credentials(const char *username, const char *password) {
     for (int i = 0; i < NUM_USERS; i++) {
          if (strcmp(username, valid_users[i][0]) == 0 &&
              strcmp(password, valid_users[i][1]) == 0) {
               return true;
          }
     }
     return false;
}

bool path_exists(const char *relative_path) {
     relative_path++;
     printf("%s\n", relative_path);
     char *absolute_path = realpath(relative_path, NULL);
     if (absolute_path == NULL) {
          // realpath failed, the path may not exist
          perror("Error resolving absolute path");
          return false;
     }

     // Check if the absolute path exists
     if (access(absolute_path, F_OK) == 0) {
          printf("Path exists: %s\n", absolute_path);
          free(absolute_path);  // Remember to free the allocated memory for the
                                // absolute path
          return true;
     } else {
          printf("Path does not exist: %s\n", absolute_path);
          free(absolute_path);  // Free the memory
          return false;
     }
}

bool is_within_server_data(const char *path) {
     char abs_path[BUFFER_SIZE];
     realpath(path, abs_path);
     return strncmp(abs_path, ROOT_DIR, strlen(ROOT_DIR)) == 0;
}

bool set_path(const char *new_dir) {
     if (new_dir == NULL || strcmp(new_dir, "") == 0) {
          return false;
     }

     char temp_path[BUFFER_SIZE];
     snprintf(temp_path, sizeof(temp_path), "%s", current_dir);

     char *token = strtok(strdup(new_dir), "/");
     while (token != NULL) {
          if (strcmp(token, "..") == 0) {
               // Handle "..", go up one directory if possible
               // Avoid going above /server_data
               if (strcmp(temp_path, ROOT_DIR) == 0) {
                    // Already at /server_data, can't go further up
                    return false;
               }

               // Try to move one directory up
               char *last_slash = strrchr(temp_path, '/');
               if (last_slash != NULL) {
                    *last_slash = '\0';  // Remove the last directory
               }
          } else {
               // Otherwise, it's a subdirectory, so append it
               if (strlen(temp_path) + strlen(token) + 2 < sizeof(temp_path)) {
                    strcat(temp_path, "/");
                    strcat(temp_path, token);
               } else {
                    return false;  // Path too long
               }
          }
          token = strtok(NULL, "/");
     }

     // modify from here
     char public_path[BUFFER_SIZE];
     char user_path[BUFFER_SIZE];

     snprintf(public_path, sizeof(public_path), "%.900s/public", ROOT_DIR);

     if (client_session.authenticated && client_session.username != NULL) {
          snprintf(user_path, sizeof(user_path), "%.900s/%.100s", ROOT_DIR,
                   client_session.username);
     }

     if ((path_exists(temp_path) && is_within_server_data(temp_path)) &&
         (strncmp(temp_path, public_path, strlen(public_path)) == 0 ||
          (client_session.authenticated && client_session.username != NULL &&
           strncmp(temp_path, user_path, strlen(user_path)) == 0))) {
          strncpy(current_dir, temp_path, sizeof(current_dir) - 1);
          return true;
     }

     return false;  // The path is invalid or outside of /server_data
}

int is_valid_command(const char *command) {
     for (int i = 0; i < NUM_VALID_COMMANDS; i++) {
          if (strcmp(command, valid_commands[i]) == 0) {
               return i;
          }
     }
     return -1;
}

void execute_command(char *tokens[], int tokens_count, char *response,
                     int client_sock) {
     int command_id = is_valid_command(tokens[0]);

     switch (command_id) {
          case 0:  // USER
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else {
                    strncpy(client_session.username, tokens[1],
                            sizeof(client_session.username) - 1);
                    snprintf(response, BUFFER_SIZE,
                             "331 User name okay, need password.\r\n");
               }
               break;
          case 1:  // PASS
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else if (strlen(client_session.username) == 0) {
                    snprintf(response, BUFFER_SIZE,
                             "503 Login with USER first.\r\n");
               } else if (validate_credentials(client_session.username,
                                               tokens[1])) {
                    client_session.authenticated = true;
                    snprintf(response, BUFFER_SIZE,
                             "230 User logged in, proceed.\r\n");
               } else {
                    snprintf(response, BUFFER_SIZE, "530 Not logged in.\r\n");
               }
               break;
          case 3:  // CWD
               bool allowed = set_path(tokens[1]);
               if (allowed)
                    snprintf(response, BUFFER_SIZE, "250 Ok\r\n");
               else
                    snprintf(response, BUFFER_SIZE,
                             "550 Error: Invalid path\r\n");
               break;
          case 6:  // QUIT
               client_session.authenticated = false;
               memset(client_session.username, 0,
                      sizeof(client_session.username));
               snprintf(response, BUFFER_SIZE, "221 Goodbye.\r\n");
               break;
          case 9:  // PASV
          {
               int pasv_socket = socket(AF_INET, SOCK_STREAM, 0);
               struct sockaddr_in pasv_addr = {0};
               pasv_addr.sin_family = AF_INET;
               pasv_addr.sin_addr.s_addr = INADDR_ANY;
               pasv_addr.sin_port = 0;  // chosen by OS

               bind(pasv_socket, (struct sockaddr *)&pasv_addr,
                    sizeof(pasv_addr));
               listen(pasv_socket, 1);

               socklen_t addr_len = sizeof(pasv_addr);
               getsockname(pasv_socket, (struct sockaddr *)&pasv_addr,
                           &addr_len);
               unsigned int port = ntohs(pasv_addr.sin_port);
               snprintf(response, BUFFER_SIZE,
                        "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).\r\n",
                        (pasv_addr.sin_addr.s_addr & 0xFF),
                        (pasv_addr.sin_addr.s_addr >> 8) & 0xFF,
                        (pasv_addr.sin_addr.s_addr >> 16) & 0xFF,
                        (pasv_addr.sin_addr.s_addr >> 24) & 0xFF, port >> 8,
                        port & 0xFF);
               data_connection.data_socket = pasv_socket;
               data_connection.active = 0;
               printf("Server in passive mode on port %u\n", port);
          } break;
          case 10:  // TYPE
               snprintf(response, BUFFER_SIZE, "200 - Command ok \r\n");
               break;
          case 13:  // RETR
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else {
                    int data_sock;
                    char file_path[BUFFER_SIZE];
                    char *absolute_path = realpath(current_dir + 1, NULL);

                    snprintf(file_path, sizeof(file_path), "%.900s/%.100s",
                             absolute_path, tokens[1]);

                    printf("%s\n", file_path);

                    // Check if the file exists and is accessible
                    FILE *file = fopen(file_path, "rb");
                    if (!file) {
                         printf("!file\n");
                         snprintf(response, BUFFER_SIZE,
                                  "550 File not found or access denied.\r\n");
                    } else {
                         printf("Open a socket for data trnasfer\n");
                         // Open a socket for data transfer
                         if (data_connection.active) {
                              data_sock = socket(AF_INET, SOCK_STREAM, 0);
                              if (connect(data_sock,
                                          (struct sockaddr *)&data_connection
                                              .client_addr,
                                          sizeof(data_connection.client_addr)) <
                                  0) {
                                   printf(
                                       "Failed to connect to client in active "
                                       "mode.");
                                   snprintf(
                                       response, BUFFER_SIZE,
                                       "425 Can't open data connection.\r\n");
                                   fclose(file);
                                   break;
                              }
                              printf("First if all ok\n");
                         } else {
                              printf("ELSE\n");
                              struct sockaddr_in client_data_addr = {0};
                              socklen_t addr_len = sizeof(client_data_addr);
                              data_sock =
                                  accept(data_connection.data_socket,
                                         (struct sockaddr *)&client_data_addr,
                                         &addr_len);
                              if (data_sock < 0) {
                                   printf(
                                       "Failed to accept data connection in "
                                       "passive mode.");
                                   snprintf(
                                       response, BUFFER_SIZE,
                                       "425 Can't open data connection.\r\n");
                                   fclose(file);
                                   break;
                              }
                              printf("Else all ok\n");
                         }

                         printf("Sending client 150\n");

                         // Inform client that the transfer is starting
                         snprintf(response, BUFFER_SIZE,
                                  "150 Opening data connection for file "
                                  "transfer.\r\n");
                         send(client_sock, response, strlen(response), 0);

                         printf("Transfering the file to client\n");
                         // Transfer the file
                         char file_buffer[BUFFER_SIZE];
                         ssize_t bytes_read;
                         while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE,
                                                    file)) > 0) {
                              send(data_sock, file_buffer, bytes_read, 0);
                              printf("Sent %ld bytes.\n", bytes_read);
                         }

                         printf("Transfer finnished\n");

                         fclose(file);
                         close(data_sock);

                         // Inform client that the transfer is complete
                         snprintf(response, BUFFER_SIZE,
                                  "226 Transfer complete.\r\n");
                    }
               }
               break;
          case 14:  // STOR
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else {
                    int data_sock;
                    char file_path[BUFFER_SIZE];

                    char *absolute_path = realpath(current_dir + 1, NULL);

                    // Concatenate the base directory and filename
                    snprintf(file_path, sizeof(file_path), "%.900s/%.100s",
                             absolute_path, tokens[1]);

                    printf("%s\n", file_path);
                    // Check if the directory exists, if not, create it
                    struct stat st;
                    if (stat(current_dir, &st) == -1) {
                         // Directory does not exist, create it
                         mkdir("server_data", 0755);
                         mkdir(current_dir, 0755);
                    }

                    // Open a socket for data transfer
                    if (data_connection.active) {
                         data_sock = socket(AF_INET, SOCK_STREAM, 0);
                         connect(
                             data_sock,
                             (struct sockaddr *)&data_connection.client_addr,
                             sizeof(data_connection.client_addr));
                    } else {
                         struct sockaddr_in client_data_addr = {0};
                         socklen_t addr_len = sizeof(client_data_addr);
                         data_sock = accept(
                             data_connection.data_socket,
                             (struct sockaddr *)&client_data_addr, &addr_len);
                    }

                    // Open the file in the specified directory
                    FILE *file = fopen(file_path, "wb");
                    if (!file) {
                         snprintf(response, BUFFER_SIZE,
                                  "550 Failed to open file.\r\n");
                    } else {
                         char file_buffer[BUFFER_SIZE];
                         ssize_t bytes_received;
                         while ((bytes_received = recv(data_sock, file_buffer,
                                                       BUFFER_SIZE, 0)) > 0) {
                              fwrite(file_buffer, 1, bytes_received, file);
                              printf("Received %ld bytes.\n", bytes_received);
                         }
                         fclose(file);
                         snprintf(response, BUFFER_SIZE,
                                  "226 Transfer complete.\r\n");
                    }
                    close(data_sock);
               }
               break;
          case 19:  // LIST
          {
               DIR *dir;
               struct dirent *entry;
               char full_response[BUFFER_SIZE] =
                   "";  // Buffer to hold all file names

               // Open the current directory
               char *absolute_path = realpath(current_dir + 1, NULL);

               // dir = opendir(current_dir);
               dir = opendir(absolute_path);
               if (dir == NULL) {
                    perror("LIST error");
                    snprintf(response, BUFFER_SIZE,
                             "550 Failed to open directory.\r\n");
                    break;
               }

               // Append each file name to the full response buffer
               while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0) {
                         continue;
                    }

                    // Check for buffer overflow
                    if (strlen(full_response) + strlen(entry->d_name) + 2 >=
                        BUFFER_SIZE) {
                         fprintf(stderr,
                                 "Directory listing too large for buffer.\n");
                         snprintf(response, BUFFER_SIZE,
                                  "550 Directory listing too large.\r\n");
                         closedir(dir);
                         break;
                    }

                    strncat(full_response, entry->d_name,
                            BUFFER_SIZE - strlen(full_response) - 1);
                    strncat(full_response, "\r\n",
                            BUFFER_SIZE - strlen(full_response) - 1);
               }

               closedir(dir);

               // Send the full directory listing
               if (strlen(full_response) > 0) {
                    snprintf(response, BUFFER_SIZE, "%s", full_response);
               } else {
                    snprintf(response, BUFFER_SIZE,
                             "550 Directory is empty.\r\n");
               }
          } break;

          case 24:  // HELP
               snprintf(
                   response, BUFFER_SIZE,
                   //"214-The following commands are recognized: \nHELP  CWD  LIST  MKD  STOR\nPASS  PASV  PWD  QUIT  RETR\nRMD\n214 Help OK\r\n");
             "COMMANDS\nHELP\nUsage: HELP\nDescription: Lists available commands.\nUSER\nUsage: USER <username>\nDescription: Sends the username.\nPASS\nUsage: PASS <password>\nDescription: Sends the password.\nPASV\nUsage: PASV\nDescription: Switches to passive mode for data transfer.\nPWD\nUsage: PWD\nDescription: Displays the current directory.\nCWD\nUsage: CWD <directory>\nDescription: Changes the current directory.\nMKD\nUsage: MKD <directory>\nDescription: Creates a new directory at the current location.\nLIST\nUsage: LIST\nDescription: Lists files and directories in the current directory.\nRMD\nUsage: RMD <directory>\nDescription: Removes the specified directory (must be empty).\nTYPE\nUsage: TYPE <type>\nWarning: Not fully implemented, only supports binary mode.\nDescription: Sets the file transfer type (e.g., ASCII or binary).\nRETR\nUsage: RETR <filename>\nDescription: Downloads the file from the server.\nSTOR\nUsage: STOR <filename>\nDescription: Uploads a file.\nQUIT\nUsage: QUIT\nDescription: Disconnects\r\n");
          break;
          case 26:  // PWD
               snprintf(response, BUFFER_SIZE, "257 \"%.1000s\"\r\n",
                        current_dir);
               break;
          case 27:  // MKD
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else {
                    char dir_path[BUFFER_SIZE * 2];
                    char *absolute_path = realpath(current_dir + 1, NULL);

                    snprintf(dir_path, sizeof(dir_path), "%s/%s", absolute_path,
                             tokens[1]);

                    if (mkdir(dir_path, 0755) == 0) {
                         snprintf(response, BUFFER_SIZE,
                                  "257 \"%s\" directory created.\r\n",
                                  tokens[1]);
                    } else {
                         perror("MKD error");
                         snprintf(response, BUFFER_SIZE,
                                  "550 Failed to create directory.\r\n");
                    }
               }
               break;
          case 28:  // RMD
               if (tokens_count < 2) {
                    snprintf(
                        response, BUFFER_SIZE,
                        "501 Syntax error in parameters or arguments.\r\n");
               } else {
                    char dir_path[BUFFER_SIZE * 2];
                    char *absolute_path = realpath(current_dir + 1, NULL);
                    snprintf(dir_path, sizeof(dir_path), "%s/%s", absolute_path,
                             tokens[1]);

                    if (rmdir(dir_path) == 0) {
                         snprintf(response, BUFFER_SIZE,
                                  "250 \"%s\" directory removed.\r\n",
                                  tokens[1]);
                    } else {
                         perror("RMD error");
                         snprintf(response, BUFFER_SIZE,
                                  "550 Failed to remove directory.\r\n");
                    }
               }
               break;

          default:
               snprintf(response, BUFFER_SIZE,
                        "502 Command: %s not implemented \r\n",
                        valid_commands[command_id]);
               break;
     }
}

void handle_client(int client_sock) {
     char buffer[BUFFER_SIZE];
     char response[BUFFER_SIZE];
     char *tokens[MAX_ARGUMENTS];
     int tokens_count = 0;

     //snprintf(response, BUFFER_SIZE, "220 FTP Server Ready\r\n");
     snprintf(response, BUFFER_SIZE, "220 FTP Server Ready\nRun HELP for all available commands\n\nWARNING!\n--------\nFiles:\nServer must have a directory named server_data placed inside the same directory(it might not be created by the server automatically).\nClient must have a directory named data placed inside the same directory.\nUsers:\nA user is automatically logged in as anonymous, once they connect.\nUsers are: user1 (password1) / user2 (password2)\nAll users (even anonymous) are allowed in server_data/public and all its subdirectories\nOnce a user has logged in, they can access server_data/<username> as well as server_data/public.\nUsers are not allowed to go back to root (/server_data) once they have entered a subdirectory(/public || /<username>\r\n");
     send(client_sock, response, strlen(response), 0);

     strncpy(current_dir, ROOT_DIR, BUFFER_SIZE);

     while (1) {
          ssize_t len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
          if (len <= 0) {
               perror("Connection closed or error on receiving!\n");
               break;
          }

          buffer[len - 2] = '\0';  // eliminate /r/n and end string
          split_client_input(buffer, tokens, &tokens_count);

          for (int i = 0; i < tokens_count; i++) {
               printf("Token[%d]: %s\n", i, tokens[i]);
          }
          printf("\n");

          if (tokens_count > 0 && is_valid_command(tokens[0]) > -1)
               execute_command(tokens, tokens_count, response, client_sock);
          else
               snprintf(response, BUFFER_SIZE, "500 Invalid command!\r\n");

          send(client_sock, response, strlen(response), 0);
     }

     close(client_sock);
}

void ftp_server() {
     int server_fd, client_sock;
     struct sockaddr_in server_addr, client_addr;
     socklen_t addr_len = sizeof(client_addr);

     server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd < 0) {
          perror("Socket creation failed\n");
          return;
     }

     server_addr.sin_family = AF_INET;
     server_addr.sin_addr.s_addr = INADDR_ANY;
     server_addr.sin_port = htons(FTP_PORT);
     if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
         0) {
          perror("Bind failed\n");
          close(server_fd);
          return;
     }

     if (listen(server_fd, 5) < 0) {
          perror("Listen failed\n");
          return;
     }

     printf("FTP Server started on port %d...\n", FTP_PORT);

     while (1) {
          client_sock =
              accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
          if (client_sock < 0) {
               perror("Accept failed\n");
               continue;
          }

          char client_ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
          printf("New client connected from %s:%d\n", client_ip,
                 ntohs(client_addr.sin_port));

          handle_client(client_sock);
     }

     close(server_fd);
}

int main() {
     ftp_server();
     return 0;
}
