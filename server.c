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

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
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
    char* end_of_file = strchr(start_of_file, ' ');
    int length = end_of_file - start_of_file;
    //printf("request: \n%s\n", request);
    //printf("length: %d\n", length);
    
    if (length > 1) {
        strncpy(file_name, start_of_file, length);
        file_name[length] = '\0';
    }

    printf("file name: %s\n", file_name);
    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    // if (need_proxy(...)) {
    //    proxy_remote_file(app, client_socket, file_name);
    // } else {
    serve_local_file(client_socket, file_name);
    //}
}

void serve_local_file(int client_socket, const char *path) {
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response
    printf("file to be opened: %s\n", path);
    FILE* fptr;
    fptr = fopen(path + 1, "r");
    int file_size;

    char* extension = strchr(path, '.');
    printf("extension: %s\n", extension);

    char response[BUFFER_SIZE] = "HTTP/1.0 200 OK\r\n";
    if (extension == NULL) {
        printf("null\n");
        strcat(response, "Content-Type: application/octet-stream\r\n");
    } else if (strcmp(extension, ".html") == 0 || strcmp(extension, ".txt") == 0) {
        printf("text\n");
        strcat(response, "Content-Type: text/plain; charset=UTF-8\r\n");
    } else if (strcmp(extension, ".jpg") == 0) {
        printf("image\n");
        strcat(response, "Content-Type: image/jpeg\r\n");
    }

    if (fptr == NULL) {
        perror("file does not exist \n");
        send(client_socket, response, strlen(response), 0);
        return;
    } else {
        fseek(fptr, 0L, SEEK_END);
        file_size = ftell(fptr);
        rewind(fptr);
        char file_size_str[BUFFER_SIZE];
        sprintf(file_size_str, "%d", file_size);

        strcat(response, "Content-Length: ");
        strcat(response, file_size_str);
    }


    char response_pt2[] = "\r\n"
                          "\r\n";
    strcat(response, response_pt2);

    int i = 0;
    int buffer;
    char file_contents[BUFFER_SIZE];

    while ((buffer = fgetc(fptr)) != EOF ) {
        file_contents[i++] = buffer;
    }
    file_contents[i++] = '\0';
    
    strcat(response, file_contents);

    printf("response:\n%s\n", response);
    send(client_socket, response, strlen(response), 0);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response

    char response[] = "HTTP/1.0 501 Not Implemented\r\n\r\n";
    send(client_socket, response, strlen(response), 0);
}