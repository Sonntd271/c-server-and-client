#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_CHUNK_SIZE 1024

void error(const char* msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[])
{
    int Port_position=0;
    int Port_IP_Position=0;
    if (argc < 5)
    {
        if(strcmp(argv[2], "-p") != 0){
        fprintf(stderr, "Usage: %s -ip <hostname> -p <port>\n", argv[0]);
        exit(1);
        }
        Port_IP_Position=1;
        Port_position=3;
    }
    else{
    if(strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-p") != 0){
        fprintf(stderr, "Usage: %s -ip <hostname> -p <port>\n", argv[0]);
        exit(1);
    }
    Port_IP_Position=2;
    Port_position=4;
    }

    // Initialize variables
    int sockfd, portno, n;
    struct sockaddr_in server_addr;
    struct hostent* server;
    char buffer[255];
    portno = atoi(argv[Port_position]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Error opening socket");
    }

    server = gethostbyname(argv[Port_IP_Position]);
    if (server == NULL)
    {
        fprintf(stderr, "Error, no such host");
    }

    bzero((char*) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        error("Connection failed");
    }
    else
    {
        printf("Connected to server\n");
    }

    int clientno;
    n = recv(sockfd, &clientno, sizeof(int), 0);
    if (n < 0)
    {
        error("Error in receiving clientno");
    }
    // printf("My client number is %d\n", clientno);

    // Clear buffer and gets input
    int matinv_iter = 0;
    int kmeans_iter = 0;
    // printf("Buffer: %s\n", buffer);
    while (strncmp(buffer, "q", 1) != 0)
    {
        bzero(buffer, 255);
        fprintf(stdout, "Enter a command for the server: ");
        fflush(stdout);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            error("Error reading input");
        }

        n = send(sockfd, buffer, strlen(buffer), 0);
        if (n < 0)
        {
            error("Error in sending");
        }

        // For sending kmeans data file
        const char s[2] = " ";
        char* token;
        token = strtok(buffer, s);
        char kmeans_f[50], filename[50];
        int km = 0;
        kmeans_f[0] = '\0';
        bzero(filename, 50);

        if (strncmp(token, "q", 1) == 0)
        {
            printf("Disconnecting from server\n");
            close(sockfd);
            exit(0);
        }
        else if (strncmp(token, "kmeans", 6) == 0 || strncmp(token, "matinv", 6) == 0)
        {
            if (strncmp(token, "kmeans", 6) == 0)
            {
                km = 1;
                while (token != NULL)
                {
                    token = strtok(NULL, s);
                    if (token != NULL && strncmp(token, "-f", 2) == 0)
                    {
                        token = strtok(NULL, s);
                        strncpy(kmeans_f, token, sizeof(kmeans_f) - 1);
                        kmeans_f[sizeof(kmeans_f) - 1] = '\0';
                        kmeans_f[strcspn(kmeans_f, "\n")] = 0; // Remove trailing newline character

                        FILE *file = fopen(kmeans_f, "rb");
                        if (file == NULL) {
                            close(sockfd);
                            error("Error opening file for reading");
                        }

                        char file_buffer[MAX_BUFFER_SIZE];
                        bzero(file_buffer, MAX_BUFFER_SIZE);
                        ssize_t bytes_read;
                        while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                            n = send(sockfd, file_buffer, bytes_read, 0);
                            bzero(file_buffer, MAX_BUFFER_SIZE);
                            // printf("%d\n", n);
                            if (n == -1) {
                                error("Error sending data");
                            }
                        }

                        fclose(file);
                    }
                }
                sprintf(filename, "kmeans_client%d_soln%d.txt", clientno, ++kmeans_iter);
            }
            else if (strncmp(token, "matinv", 6) == 0)
            {
                km = 0;
                sprintf(filename, "matinv_client%d_soln%d.txt", clientno, ++matinv_iter);
            }

            if (kmeans_f[0] == '\0' && km == 1)
            {
                printf("Please enter the kmeans data file path using -f\n");
                kmeans_iter--;
            }
            else
            {
                char filename_p[255];
                bzero(filename_p, 255);
                sprintf(filename_p, "./results/%s", filename);
                FILE *file = fopen(filename_p, "ab"); // "ab" for append mode
                if (file == NULL) {
                    close(sockfd);
                    error("Error opening file for appending");
                }

                char recv_buffer[MAX_CHUNK_SIZE];
                bzero(recv_buffer, MAX_CHUNK_SIZE);
                ssize_t bytes_received = 0;
                do {
                    bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
                    // printf("%d\n", (int) bytes_received);
                    if (bytes_received > 0)
                    {
                        fwrite(recv_buffer, 1, bytes_received, file);
                    }
                    bzero(recv_buffer, MAX_CHUNK_SIZE);
                }
                while (bytes_received > MAX_CHUNK_SIZE - 1); 
                printf("Received the solution: %s\n", filename);
                fclose(file);
            }
        }
        else
        {
            printf("Invalid command, please use either kmeans or matinv, or q to quit\n");
        }
    }

    close(sockfd);
    exit(0);
}
