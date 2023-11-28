# Server and Client Interaction
A server and client program written in C designed to handle K-means and Matinv calculation requests from the client

Server is designed to handle 3 strategies:
- fork: The connection is made by forking the process to handle communication from a specified number of clients (default: 5) without blocking connection from other clients.
- muxbasic: The communication is made using select calls, which detect requests from each client and handle them effectively
- muxscale: The communication utilizes epoll instance to efficiently handle client communications

Steps to run the server and client:
1. Run `make` to build the server and client executables
2. Run the server, specifying the port number for the socket and the strategy
3. Execute the client program, specifying the IP address of the server and the port number of the server socket
4. Send commands from the client and observe the results within `computed_results` directory (server-side) and `results` directory (client-side)

**Server command:** 
_Execution:_ <server_executable_path> -p <port_number> -s <strategy>

**Client commands:** 
_Execution:_ <client_executable_path> (-ip <ip_address> | localhost) -p <port_number>
Execute the commands below after connecting to the server
_K-means:_ kmeans -f <kmeans_data_file> -k <number_of_clusters>
_Matrix Inverse:_ matinv -n <size_of_matrix> -I <fast | rand>
