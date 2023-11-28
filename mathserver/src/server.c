#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 100
#define MAX_CHUNK_SIZE 1024
#define FILE_BUFFER_SIZE 1024

// Error handling
void error(const char* msg) 
{
    perror(msg);
    exit(1);
}

void read_options(int argc, char* argv[], int* portno, char* strategy, int* daemonize) 
{
    int option;
    *portno = -1;
    strategy[0] = '\0';

    while ((option = getopt(argc, argv, "hp:ds:")) != -1) {
        switch (option) {
            case 'h':
                printf("Usage: %s -p <port> -s <strategy>\n", argv[0]);
                exit(0);
            case 'p':
                *portno = atoi(optarg);
                break;
            case 'd':
                *daemonize = 1;
                break;
            case 's':
                strcpy(strategy, optarg); 
                break;
            default:
                fprintf(stderr, "Usage: %s -p <port> -s <strategy>\n", argv[0]);
                exit(3);
        }
    }
}

int parse_client_command(int clientfd, int* clientno, int* matinv_iter, int* kmeans_iter)
{
    int n;
    char buffer[255];
    bzero(buffer, sizeof(buffer));

    n = recv(clientfd, buffer, sizeof(buffer) - 1, 0);
    if (n < 0)
    {
        error("Error in receiving");
    }
    buffer[n] = '\0'; // Null-terminate the data to ensure proper string manipulation

    if (strncmp(buffer, "q", 1) == 0) 
    {
        // printf("Client %d disconnected from server\n", *clientno);
        close(clientfd);
        return 0;
    }
    else if (strncmp(buffer, "kmeans", 6) == 0 || strncmp(buffer, "matinv", 6) == 0)
    {
        printf("Client %d commanded: %s", *clientno, buffer);

        // Initialize variables for tokenizing
        const char s[2] = " ";
        char* token;
        // Initialize variables for receiving parameters
        char mode[10], matinv_I[10] = "fast", kmeans_f[50];
        int matinv_n = 5, kmeans_k = 3;
        float matinv_m = 15.0;
        
        token = strtok(buffer, s);

        // If matinv
        if (strncmp(token, "matinv", 6) == 0)
        {
            // printf("Performing matinv\n");
            int tmp_iter = *matinv_iter;
            tmp_iter++;
            *matinv_iter = tmp_iter;

            while (token != NULL)
            {
                if (strncmp(token, "-n", 2) == 0)
                {
                    token = strtok(NULL, s);
                    matinv_n = atoi(token);
                    // printf("-n: %d\n", matinv_n);
                    token = strtok(NULL, s); // Update token
                }
                else if (strncmp(token, "-I", 2) == 0)
                {
                    token = strtok(NULL, s);
                    strncpy(matinv_I, token, sizeof(matinv_I) - 1);
                    matinv_I[sizeof(matinv_I) - 1] = '\0';
                    // printf("-I: %s\n", matinv_I);
                    token = strtok(NULL, s); // Update token
                }
                else if (strncmp(token, "-m", 2) == 0)
                {
                    token = strtok(NULL, s);
                    matinv_m = atof(token);
                    // printf("-m: %5.2f\n", matinv_m);
                    token = strtok(NULL, s); // Update token
                }
                else
                {
                    token = strtok(NULL, s); // Update token
                }
            }

            char tmp_o[50], tmp_n[5], tmp_I[10], tmp_m[10];
            sprintf(tmp_o, "matinv_client%d_soln%d.txt", *clientno, *matinv_iter);
            sprintf(tmp_n, "%d", matinv_n);
            sprintf(tmp_I, "%s", matinv_I);
            sprintf(tmp_m, "%5.2f", matinv_m);

            pid_t execpid = fork();
            if (execpid == -1)
            {
                error("Error on exec fork");
            }
            else if (execpid == 0)
            {
                // Exec process
                execl("./matinv", "matinv", "-n", tmp_n, "-I", tmp_I, "-m", tmp_m, "-o", tmp_o, NULL);
            }
            else
            {
                // Child process
                if (waitpid(execpid, NULL, 0) == execpid)
                {
                    char tmp_op[255]; // temp_o path :D
                    sprintf(tmp_op, "../computed_results/%s", tmp_o);
                    FILE *file = fopen(tmp_op, "rb");
                    if (file == NULL) {
                        error("Error opening file for reading");
                    }

                    char send_buffer[FILE_BUFFER_SIZE];
                    bzero(send_buffer, FILE_BUFFER_SIZE);
                    ssize_t bytes_read;
                    while ((bytes_read = fread(send_buffer, 1, sizeof(send_buffer), file)) > 0) {
                        // printf("%d\n", (int) bytes_read);
                        n = send(clientfd, send_buffer, bytes_read, 0);
                        bzero(send_buffer, FILE_BUFFER_SIZE);
                        if (n == -1) {
                            error("Error sending data");
                        }
                    }
                    printf("Sending solution: %s\n", tmp_o);
                    fclose(file);
                }
            }
            return 1;
        }
        // If kmeans
        else if (strncmp(token, "kmeans", 6) == 0)
        {
            // printf("Performing kmeans\n");
            char filename[20];
            filename[0] = '\0';

            while (token != NULL)
            {
                if (strncmp(token, "-f", 2) == 0)
                {
                    token = strtok(NULL, s);

                    int tmp_iter = *kmeans_iter;
                    tmp_iter++;
                    *kmeans_iter = tmp_iter;

                    // Receive file from data transferred over socket here
                    sprintf(filename, "temp_client%d_%d.txt", *clientno, *kmeans_iter);
                    FILE *file = fopen(filename, "ab"); // "ab" for append mode
                    if (file == NULL) {
                        close(clientfd);
                        error("Error opening file for appending");
                    }

                    char file_buffer[MAX_CHUNK_SIZE];
                    bzero(file_buffer, MAX_CHUNK_SIZE);
                    ssize_t bytes_received = 0;
                    do {
                        bytes_received = recv(clientfd, file_buffer, sizeof(file_buffer), 0);
                        // printf("%d\n", (int) bytes_received);
                        if (bytes_received > 0)
                        {
                            fwrite(file_buffer, 1, bytes_received, file);
                        }
                        bzero(file_buffer, MAX_CHUNK_SIZE);
                    }
                    while (bytes_received > MAX_CHUNK_SIZE - 1);

                    if (bytes_received == -1) {
                        error("Error receiving data");
                    }
                    fclose(file);
                    // printf("File received successfully\n");

                    token = strtok(NULL, s); // Update token
                }
                else if (strncmp(token, "-k", 2) == 0)
                {
                    token = strtok(NULL, s);
                    kmeans_k = atoi(token);
                    token = strtok(NULL, s); // Update token
                }
                else
                {
                    token = strtok(NULL, s);
                }
            }

            if (filename[0] != '\0')
            {
                char tmp_f[100], tmp_o[50], tmp_k[5];
                sprintf(tmp_f, "./%s", filename);
                sprintf(tmp_o, "kmeans_client%d_soln%d.txt", *clientno, *kmeans_iter);
                sprintf(tmp_k, "%d", kmeans_k);

                pid_t execpid = fork();
                if (execpid == -1)
                {
                    error("Error on exec fork");
                }
                else if (execpid == 0)
                {
                    // Exec process
                    execl("./kmeans", "kmeans", "-k", tmp_k, "-f", tmp_f, "-o", tmp_o, NULL);
                }
                else
                {
                    // Child process
                    if (waitpid(execpid, NULL, 0) == execpid)
                    {
                        char tmp_op[255]; // temp_o path :D
                        sprintf(tmp_op, "../computed_results/%s", tmp_o);
                        FILE *file = fopen(tmp_op, "rb");
                        if (file == NULL) {
                            error("Error opening file for reading");
                        }

                        char send_buffer[FILE_BUFFER_SIZE];
                        bzero(send_buffer, FILE_BUFFER_SIZE);
                        ssize_t bytes_read;
                        while ((bytes_read = fread(send_buffer, 1, sizeof(send_buffer), file)) > 0) {
                            // printf("%d\n", (int) bytes_read);
                            n = send(clientfd, send_buffer, bytes_read, 0);
                            bzero(send_buffer, FILE_BUFFER_SIZE);
                            if (n == -1) {
                                error("Error sending data");
                            }
                        }
                        printf("Sending solution: %s\n", tmp_o);
                        fclose(file);
                    }
                }
            }
            else
            {
                printf("Client %d did not provide a kmeans data file path\n", *clientno);
            }
            return 1;
        }
    }
}

int main(int argc, char* argv[]) 
{
    int portno, daemonize = 0;
    char strategy[255]; // Valid strategies are fork, muxbasic and muxscale
    int method = -1; // 0 is for matrix inversion, 1 is for k-means

    read_options(argc, argv, &portno, strategy, &daemonize);

    if (portno == -1) 
    {
        fprintf(stderr, "Port number not provided. Program terminated\n");
        exit(1);
    }
    if (strategy[0] == '\0') 
    {
        fprintf(stderr, "Strategy not provided. Program terminated\n");
        exit(1);
    }

    if (daemonize) 
    {
        // Daemonize the process
        int dpid = fork();
        if (dpid < 0) 
        {
            error("Fork failed");
        } 
        else if (dpid > 0) 
        {
            // Parent process exits
            exit(0);
        }

        // Create a new session and detach from the terminal
        if (setsid() < 0) 
        {
            error("setsid failed");
        }

        // No change to the working dir because it messes up other stuff
        // chdir("/");

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Redirect standard file descriptors to /dev/null
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
    }

    // Initialize variables
    int sockfd, clientfd;
    int pid;

    // Initialize sockaddr_in structures
    struct sockaddr_in server_addr, client_addr;
    socklen_t clientlen; // Size of socket-related data structures

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) // sockfd returns -1 if failure
    {
        error("Error opening socket");
    }

    // Initialize server_addr by ensuring the structure is empty
    bzero((char*) &server_addr, sizeof(server_addr));

    // Setting up server configurations
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Any incoming connections
    server_addr.sin_port = htons(portno); // Convert to network bytes

    if (bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        error("Binding failed");
    }

    // Listen for clients and limit client connection to 5
    if (listen(sockfd, 5) == 0)
    {
        printf("Listening for clients...\n");
    }

    if (strncmp(strategy, "fork", 4) == 0)
    {
        int connection_count = 0;
        int pids[100];
        while (1)
        {
            clientlen = sizeof(client_addr);
            clientfd = accept(sockfd, (struct sockaddr*) &client_addr, &clientlen);
            if (clientfd < 0)
            {
                error("Error on accept");
            }
            else
            {
                printf("Connected with client %d\n", ++connection_count);
            }

            pid = fork();
            if (pid == 0)
            {
                // Child process
                close(sockfd);
                int matinv_iter = 0, kmeans_iter = 0;
                int client_status = 1;

                if (send(clientfd, &connection_count, sizeof(int), 0) < 0)
                {
                    error("Error sending clientno");
                }

                do
                {
                    client_status = parse_client_command(clientfd, &connection_count, &matinv_iter, &kmeans_iter);
                }
                while (client_status);

                // Exit the child process
                exit(0);
            }
            else if (pid > 0)
            {
                // Parent process
                close(clientfd);
                pids[connection_count - 1] = pid;
            }
            else
            {
                error("Fork failed");
            }

            // Zombie process cleared only after new connections because accept blocks
            for (int p = 0; p < connection_count; p++)
            {
                int status;
                if (waitpid(pids[p], &status, WNOHANG) > 0)
                {
                    int i = p;
                    for (; i < connection_count - 1; i++) {
                        pids[i] = pids[i + 1];
                    }
                    pids[i - 1] = -1; // Mark last as unused
                }
            }
        }
    }
    else if (strncmp(strategy, "muxbasic", 8) == 0)
    {
        // printf("Performing MUX basic\n");
        fd_set active, read_fds;
        int fdmax, connection_count = 0;
        int max_conn = 100;
        int kmeans_iters[max_conn], matinv_iters[max_conn], is[max_conn];
        for (int t = 0; t < max_conn; t++)
        {
            kmeans_iters[t] = 0;
            matinv_iters[t] = 0;
            is[t] = -1;
        }

        // Empty the sets
        FD_ZERO(&active);
        FD_ZERO(&read_fds);

        FD_SET(sockfd, &active);
        fdmax = sockfd;
        
        while (1) 
        {
            read_fds = active;
            // printf("%d\n", fdmax);

            if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) 
            {
                error("Error on select");
            }

            for (int i = 0; i <= fdmax; i++) 
            {
                if (FD_ISSET(i, &read_fds)) 
                {
                    if (i == sockfd) 
                    {
                        // New connection
                        clientlen = sizeof(client_addr);
                        clientfd = accept(sockfd, (struct sockaddr*) &client_addr, &clientlen);
                        if (clientfd < 0) 
                        {
                            error("Error on accept");
                        } 
                        else 
                        {
                            printf("Connected with client %d\n", ++connection_count);
                        }
                        // clientfds[connection_count - 1] = clientfd;
                        is[clientfd] = connection_count;

                        if (send(clientfd, &connection_count, sizeof(int), 0) < 0) 
                        {
                            error("Error sending clientno");
                        }
                        
                        FD_SET(clientfd, &active);
                        if (clientfd > fdmax) 
                        {
                            fdmax = clientfd;
                        }

                    } 
                    else 
                    {
                        // Existing client data
                        int cc = is[i];
                        int kmeans_iter = kmeans_iters[cc - 1];
                        int matinv_iter = matinv_iters[cc - 1];
                        int client_status = 1;

                        client_status = parse_client_command(i, &cc, &matinv_iter, &kmeans_iter);
                        kmeans_iters[cc - 1] = kmeans_iter;
                        matinv_iters[cc - 1] = matinv_iter;
                        is[i] = cc;
                        if (client_status == 0)
                        {
                            is[i] = -1;
                            close(i);
                            FD_CLR(i, &active);
                        }             
                    }
                }
            }
        }
    }
    else if (strncmp(strategy, "muxscale", 8) == 0)
    {
        int max_conn = 100;
        int connection_count = 0;
        int client_fds[max_conn];
        int kmeans_iters[max_conn];
        int matinv_iters[max_conn];
        int is[max_conn];

        for (int t = 0; t < max_conn; t++)
        {
            client_fds[t] = -1;
            kmeans_iters[t] = 0;
            matinv_iters[t] = 0;
            is[t] = -1;
        }

        // Create an epoll instance
        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1)
        {
            error("Error creating epoll instance");
        }

        // Add the server socket to the epoll event structure
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
        {
            error("Error adding the server socket to epoll");
        }

        while (1)
        {
            // Wait for events using epoll
            struct epoll_event events[max_conn];
            int num_events = epoll_wait(epoll_fd, events, max_conn, -1); // Blocking wait

            if (num_events == -1)
            {
                error("Error waiting for events with epoll");
            }

            for (int i = 0; i < num_events; i++)
            {
                int fd = events[i].data.fd;

                if (fd == sockfd)
                {
                    // New connection
                    clientlen = sizeof(client_addr);
                    clientfd = accept(sockfd, (struct sockaddr*) &client_addr, &clientlen);
                    if (clientfd < 0)
                    {
                        error("Error on accept");
                    }
                    else
                    {
                        printf("Connected with client %d\n", ++connection_count);
                    }

                    // Add the new client socket to epoll
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientfd, &ev) == -1)
                    {
                        error("Error adding the client socket to epoll");
                    }

                    // Store information about the new client
                    client_fds[connection_count - 1] = clientfd;
                    is[clientfd] = connection_count;

                    if (send(clientfd, &connection_count, sizeof(int), 0) < 0)
                    {
                        error("Error sending clientno");
                    }
                }
                else
                {
                    // Existing client data
                    int cc = is[fd];
                    int kmeans_iter = kmeans_iters[cc - 1];
                    int matinv_iter = matinv_iters[cc - 1];
                    int client_status = 1;

                    client_status = parse_client_command(fd, &cc, &matinv_iter, &kmeans_iter);
                    kmeans_iters[cc - 1] = kmeans_iter;
                    matinv_iters[cc - 1] = matinv_iter;
                    is[fd] = cc;

                    if (client_status == 0)
                    {
                        is[fd] = -1;
                        close(fd);

                        // Remove the client socket from epoll
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    }
                }
            }
        }

        // Close the epoll instance
        close(epoll_fd);
    }
    else
    {
        printf("Please enter a valid strategy. Strategy you entered is %s\n", strategy);
    }
}
