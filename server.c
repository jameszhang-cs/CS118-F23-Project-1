#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001
#define MEGABYTE 1024 * 1024

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    char *request = malloc(strlen(buffer) + 1);
    strcpy(request, buffer);

    char file_name[BUFFER_SIZE] = "/index.html";
    //Parsing request header to find file name

    char* start_of_file = strchr(request, '/');
    char* end_of_file = strchr(start_of_file, '\r');
    int length = end_of_file - start_of_file - 9;
    
    if (length > 1) {
        strncpy(file_name, start_of_file, length);
        file_name[length] = '\0';
    }

    printf("file name: %s\n", file_name);
    //converting %20 in request to spaces
    char* space = strstr(file_name, "%20");
    while (space != NULL) {
        space[0] = ' ';
        memmove(space + 1, space + 3, strlen(space + 3) + 1);
        space = strstr(file_name, "%20");
    }

    //converting %25 in request to %
    char* percent = strstr(file_name, "%25");
    while (percent != NULL) {
        percent[0] = '%';
        memmove(percent + 1, percent + 3, strlen(percent + 3) + 1);
        percent = strstr(file_name, "%25");
    }

    char *extension = strrchr(file_name, '.');
    
    if (extension != NULL && strcmp(extension, ".ts") == 0) {
        proxy_remote_file(app, client_socket, request);
    } else {
        serve_local_file(client_socket, file_name);
    }
}

void send_error_response(int client_socket) {
    char response[] = "HTTP/1.0 404 Not Found\r\n\r\n";
    send(client_socket, response, strlen(response), 0);
}

void send_file_content(int client_socket, FILE *fptr) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
}

void serve_local_file(int client_socket, const char *path) {
    FILE *fptr = fopen(path + 1, "rb"); // Open in binary mode
    char response[MEGABYTE] = "";

    if (fptr == NULL) {
        perror("File does not exist\n");
        send_error_response(client_socket);
        return;
    }

    fseek(fptr, 0, SEEK_END);
    int file_size = ftell(fptr);
    rewind(fptr);

    char *extension = strrchr(path, '.');

    strcat(response, "HTTP/1.0 200 OK\r\n");

    if (extension == NULL) {
        printf("null\n");
        strcat(response, "Content-Type: application/octet-stream\r\n");
    } else if (strcmp(extension, ".html") == 0) {
        printf("html\n");
        strcat(response, "Content-Type: text/html; charset=UTF-8\r\n");
    } else if (strcmp(extension, ".txt") == 0) {
        printf("text\n");
        strcat(response, "Content-Type: text/plain; charset=UTF-8\r\n");
    } else if (strcmp(extension, ".jpg") == 0) {
        printf("image\n");
        strcat(response, "Content-Type: image/jpeg\r\n");
    }

    char file_size_str[BUFFER_SIZE];
    sprintf(file_size_str, "%d", file_size);

    strcat(response, "Content-Length: ");
    strcat(response, file_size_str);

    char response_pt2[] = "\r\n\r\n";
    strcat(response, response_pt2);

    send(client_socket, response, strlen(response), 0);
    send_file_content(client_socket, fptr);
    fclose(fptr);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_host/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response
    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(app->remote_port);
    server_addr.sin_addr.s_addr = inet_addr(app->remote_host);
    
    if (connect(remote_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        char response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    char buffer[1024];
    int bytes_received;

    if (send(remote_socket, request, strlen(request), 0) <= 0) {
        char response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    while ((bytes_received = recv(remote_socket, buffer, sizeof(buffer), 0)) > 0) {
        send(client_socket, buffer, bytes_received, 0);
    }

    close(remote_socket);
}