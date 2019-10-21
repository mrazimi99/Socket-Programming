#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_LINE 512

int can_send_broadcast = 0;

int download_file(int server_fd, char* file_name);
int upload_file(int server_fd, char* file_name);
void send_broadcast(int udp_port, char* message);
void receive_broadcast(int broadcast_fd, const char* my_port);
void signal_handler(int sig);

int send_file(int destination_fd, char* file_name);
int receive_file(int source_fd, char* file_name);

int main(int argc, char const *argv[])
{
	if (argc != 4)
	{
		char* error_message = "Enter 3 needed ports.\n";
		write(2, error_message, strlen(error_message));
		return 1;
	}

	int heart_beat_port = atoi(argv[1]);
	int client_broadcast_port = atoi(argv[2]);
	int client_port = atoi(argv[3]);

	// BroadCast
	int broadcast_fd = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in broadcast_address;
	memset(&broadcast_address, 0, sizeof(broadcast_address)); 
	broadcast_address.sin_family = PF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	broadcast_address.sin_port = htons(client_broadcast_port);
	int broadcast_address_length = sizeof(broadcast_address);
	int broadcast = 1;

	setsockopt(broadcast_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast));

	if (bind(broadcast_fd, (struct sockaddr*)&broadcast_address, broadcast_address_length) < 0)
	{
		char* error_message = "Could not bind to broadcast port!\n";
		write(1, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	}

	// HeartBeat
	int heartbeat_fd = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in heartbeat_address;
	memset(&heartbeat_address, 0, sizeof(heartbeat_address)); 
	heartbeat_address.sin_family = PF_INET;
	heartbeat_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	heartbeat_address.sin_port = htons(heart_beat_port);
	int heartbeat_address_length = sizeof(heartbeat_address);
	broadcast = 1;

	setsockopt(heartbeat_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast));

	if (bind(heartbeat_fd, (struct sockaddr*)&heartbeat_address, heartbeat_address_length) < 0)
	{
		char* error_message = "Could not bind to heartbeat port!\n";
		write(1, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	}

	int received_data = 0;
	int server_is_alive = 0;
	char heart_beat_message[128];
	struct timeval past, now;
	gettimeofday(&now, NULL);
	past = now;

	fd_set heartbeat_fd_set;

	while (1)
	{
		FD_ZERO(&heartbeat_fd_set);
		FD_SET(heartbeat_fd, &heartbeat_fd_set);
		struct timeval timeout = {0, 0};
		select(heartbeat_fd + 1, &heartbeat_fd_set, NULL, NULL, &timeout);

		if (FD_ISSET(heartbeat_fd, &heartbeat_fd_set) && (received_data = recvfrom(heartbeat_fd, heart_beat_message, 128, 0, NULL, NULL)) < 0)
		{printf("1\n");
			server_is_alive = 0;
			break;
		}
		else if (received_data > 0)
		{
			char* message = "Server is up with port: ";
			heart_beat_message[received_data] = '\0';
			write(1, message, strlen(message));
			write(1, heart_beat_message, strlen(heart_beat_message));
			write(1, "\n", strlen("\n"));
			server_is_alive = 1;
			break;
		}

		gettimeofday(&now, NULL);
		if (now.tv_sec > past.tv_sec + 1)
		{printf("2\n");
			server_is_alive = 0;
			break;
		}
	}

	// TCP BroadCast
	int client_connection = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in client_address;
	client_address.sin_family = PF_INET;
	client_address.sin_addr.s_addr = INADDR_ANY;
	client_address.sin_port = htons(client_port);
	int client_address_length = sizeof client_address;
	int reuse = 1;
	setsockopt(client_connection, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

	if (bind(client_connection, (struct sockaddr*)&client_address, client_address_length) < 0)
	{
		perror("bind 342\n");
		return 1;
	}

	if (listen(client_connection, 20) < 0)
	{
		perror("listen\n");
		return 1;
	}

	// Server
	int server_connection = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in socket_address, server_address;
	socket_address.sin_family = PF_INET;
	socket_address.sin_addr.s_addr = INADDR_ANY;
	socket_address.sin_port = htons(client_port);
	int address_length = sizeof socket_address;

	if (server_is_alive)
	{
		int server_port = atoi(heart_beat_message);
		server_address.sin_family = PF_INET;
		server_address.sin_addr.s_addr = INADDR_ANY;
		server_address.sin_port = htons(server_port);
		int server_address_length = sizeof server_address;
		int reuse = 1;
		setsockopt(server_connection, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

		if (bind(server_connection, (struct sockaddr*)&socket_address, address_length) < 0)
		{
			char* error_message = "Could not bind client!\n";
			write(1, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}

		if (connect(server_connection, (struct sockaddr*)&server_address, server_address_length) < 0)
		{
			char* error_message = "Could not connect to server!\n";
			write(1, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}
	}

	fd_set read_fd;
	int find_with_broadcast = 0;

	signal(SIGALRM, signal_handler);
	alarm(1);

	// struct timeval past, now;
	// gettimeofday(&now, NULL);
	// past = now;

	// Forever
	while (1)
	{
		char command[100], broadcast_message[100];
		int read_size;

		struct timeval timeout = {0, 1000};
		FD_ZERO(&read_fd);
		FD_SET(0, &read_fd);
		FD_SET(broadcast_fd, &read_fd);
		FD_SET(client_connection, &read_fd);
		int maxfd = broadcast_fd > client_connection ? broadcast_fd : client_connection;
		int new_event = select(maxfd + 1, &read_fd, NULL, NULL, &timeout);

		if (can_send_broadcast && find_with_broadcast)
		{
			can_send_broadcast = 0;
			char* sending_message = "Sending broadcast request\n";
			write(1, sending_message, strlen(sending_message));
			send_broadcast(client_broadcast_port, broadcast_message);
			alarm(1);
		}

		if (new_event < 0 && errno == EINTR)
			continue;

		int data_is_ready = FD_ISSET(0, &read_fd);

		if (data_is_ready && (read_size = read(0, command, 100)) < 0)
		{
			char* error_message = "Could not get command!\n";
			write(1, error_message, strlen(error_message));
		}
		else if (data_is_ready)
		{
			command[read_size - 1] = '\0';

			if (memmem(command, read_size, "download", 8) == command)
			{
				int download_result = 0;
				if (server_is_alive && (download_result = download_file(server_connection, command)) < 0)
					exit(EXIT_FAILURE);
				else if (download_result == 0)
				{
					find_with_broadcast = 1;

					for (int i = 0; i < strlen(argv[3]); i++)
					{
						broadcast_message[i] = argv[3][i];
					}

					for (int i = strlen(argv[3]); i < strlen(command) + 1; i++)
					{
						broadcast_message[i] = command[i - strlen(argv[3]) + 8];
					}
				}
				
			}
			else if (memmem(command, read_size, "upload", 6) == command)
			{
				if (server_is_alive)
					upload_file(server_connection, command);
				else
				{
					char* error_message = "There is no server!\n";
					write(1, error_message, strlen(error_message));
				}
				
			}
			else
			{
				char* message = "Invalid command. Try again!\n";
				write(1, message, strlen(message));
			}
		}

		// gettimeofday(&now, NULL);
		// if (find_with_broadcast && now.tv_sec >= past.tv_sec + 1)
		// {
		// 	past = now;
		// 	char* sending_message = "Sending broadcast request\n";
		// 	// write(1, sending_message, strlen(sending_message));
		// 	send_broadcast(client_broadcast_port, broadcast_message);
		// }

		if (FD_ISSET(broadcast_fd, &read_fd))
		{
			const char* port = argv[3];
			receive_broadcast(broadcast_fd, port);
		}

		if (FD_ISSET(client_connection, &read_fd) && find_with_broadcast)		// New connection
		{
			int new_socket;
			if ((new_socket = accept(client_connection, (struct sockaddr*)&client_address, (socklen_t*)&client_address_length)) < 0)
				perror("Accept Failed!\n");
			else
			{
				struct sockaddr_in sin;
				socklen_t len = sizeof(sin);

				find_with_broadcast = 0;
				send(new_socket, "1", 1, 0);
				receive_file(new_socket, broadcast_message + strlen(argv[3]) + 1);
				close(new_socket);
			}
		}
	}
}

int download_file(int server_fd, char* command)
{
	char buffer[MAX_LINE];
	send(server_fd, command, strlen(command), 0);
	int message_length;
	if((message_length = read(server_fd, buffer, sizeof(buffer))) <= 0)
	{
		char* error_message = "Server is down!\n";
		write(2, error_message, strlen(error_message));
		return -1;
	}
	else if (message_length == 1 && buffer[0] == '0')
	{
		char* error_message = "File does not exist!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	int file_fd;
	char* file_name = command + 9;
	if ((file_fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0)
	{
		char* error_message = "Create error!\n";
		write(2, error_message, strlen(error_message));
		return -1;
	}

	int read_length;
	fd_set read_fd;
	struct timeval timeout = {0, 100};
	FD_ZERO(&read_fd);
	FD_SET(server_fd, &read_fd);
	select(server_fd + 1, &read_fd, NULL, NULL, &timeout);

	while(FD_ISSET(server_fd, &read_fd) && (read_length = read(server_fd, buffer, sizeof(buffer))) > 0)
	{
		struct timeval timeout = {0, 100};
		FD_ZERO(&read_fd);
		FD_SET(server_fd, &read_fd);
		select(server_fd + 1, &read_fd, NULL, NULL, &timeout);

		if (write(file_fd, buffer, read_length) != read_length)
		{
			char* error_message = "Write error!\n";
			write(2, error_message, strlen(error_message));
			return -1;
		}
	}

	if(read_length < 0)
	{
    	char* error_message = "Read error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	char* message = "File is downloaded!\n";
	write(2, message, strlen(message));
	return 1;
}

int upload_file(int server_fd, char* command)
{
	int file_fd;
	char* file_name = command + 7;
	if ((file_fd = open(file_name, O_RDONLY)) < 0)
	{
		char* error_message = "Open error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	char buffer[MAX_LINE];
	send(server_fd, command, strlen(command), 0);
	int message_length;

	if((message_length = read(server_fd, buffer, sizeof(buffer))) <= 0)
	{
		char* error_message = "Server is down!\n";
		write(2, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	}
	else if (message_length == 1 && buffer[0] == '0')
	{
		char* error_message = "File already exists!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	int read_length;
	while((read_length = read(file_fd, buffer, sizeof(buffer))) > 0)
	{
		if (send(server_fd, buffer, read_length, 0) != read_length)
		{
			char* error_message = "Write error!\n";
			write(2, error_message, strlen(error_message));
			return 0;
		}
	}

	if(read_length < 0)
	{
    	char* error_message = "Read error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	char* message = "File is uploaded!\n";
	write(2, message, strlen(message));
	return 1;
}

void send_broadcast(int udp_port, char* message)
{
	int broadcast_socket;
	if ((broadcast_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
	{ 
		perror("Socket creation failed!"); 
		exit(EXIT_FAILURE);
	}

	int broadcast = 1;
	setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

	struct sockaddr_in broadcast_address;

	broadcast_address.sin_family = PF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST); 
	broadcast_address.sin_port = htons(udp_port);
	char* beat_sound = "broad!\n";
	write(1, beat_sound, strlen(beat_sound));
	sendto(broadcast_socket, message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *)&broadcast_address,
			sizeof broadcast_address);
	close(broadcast_socket);
}

void receive_broadcast(int broadcast_fd, const char* my_port)
{
	int received_data;
	char response[128];
	char* file_name = response;

	if ((received_data = recvfrom(broadcast_fd, response, 128, 0, NULL, NULL)) < 0)
	{
		char* error_message = "Broadcast receive failed!\n";
		write(2, error_message, strlen(error_message));
	}
	else if (received_data > 0)
	{
		response[received_data] = '\0';
		if (memmem(response, strlen(response), my_port, strlen(my_port)) == response)
			return;

		char* port = response;
		int length = 0;

		while (file_name[0] != ' ')
		{
			++length;
			++file_name;
		}

		port[length] = '\0';
		++file_name;

		int file_fd;
		if ((file_fd = open(file_name, O_RDONLY)) > 0)
		{
			close(file_fd);

			int new_connection = socket(PF_INET, SOCK_STREAM, 0);
			struct sockaddr_in my_address, other_address;
			my_address.sin_family = PF_INET;
			my_address.sin_addr.s_addr = INADDR_ANY;
			my_address.sin_port = htons(atoi(my_port));
			int address_length = sizeof my_address;

			int other_port = atoi(port);
			other_address.sin_family = PF_INET;
			other_address.sin_addr.s_addr = INADDR_ANY;
			other_address.sin_port = htons(other_port);
			int other_address_length = sizeof other_address;
			int reuse = 1;
			setsockopt(new_connection, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

			if (bind(new_connection, (struct sockaddr*)&my_address, address_length) < 0)
			{
				char* error_message = "Could not bind 1!\n";
				write(1, error_message, strlen(error_message));
				exit(EXIT_FAILURE);
			}

			if (connect(new_connection, (struct sockaddr*)&other_address, other_address_length) < 0)
			{
				char* error_message = "Could not connect to 123!\n";
				write(1, error_message, strlen(error_message));
				return;
			}

			fd_set read_fd;
			FD_ZERO(&read_fd);
			FD_SET(new_connection, &read_fd);
			struct timeval timeout = {0, 1000};
			select(new_connection + 1, &read_fd, NULL, NULL, &timeout);
			char buffer[10];

			if (FD_ISSET(new_connection, &read_fd))
				read(new_connection, buffer, 10);
			else
				return;
			send_file(new_connection, file_name);
			close(new_connection);
		}
	}
}

void signal_handler(int sig)
{
	can_send_broadcast = 1;
}














int send_file(int destination_fd, char* file_name)
{
	char buffer[1024];
	int file_fd;
	if ((file_fd = open(file_name, O_RDONLY)) < 0)
	{
		char* error_message = "Open error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	int read_length;
	while((read_length = read(file_fd, buffer, sizeof(buffer))) > 0)
	{
		if (send(destination_fd, buffer, read_length, 0) != read_length)
		{
			char* error_message = "Write error!\n";
			write(2, error_message, strlen(error_message));
			return 0;
		}
	}

	if(read_length < 0)
	{
    	char* error_message = "Read error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	char* message = "File is sent!\n";
	write(2, message, strlen(message));
	return 1;
}

int receive_file(int source_fd, char* file_name)
{
	char buffer[128];
	int file_fd;
	if ((file_fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0)
	{
		char* error_message = "Create error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	int read_length;
	fd_set read_fd;
	struct timeval timeout = {0, 100};
	FD_ZERO(&read_fd);
	FD_SET(source_fd, &read_fd);
	select(source_fd + 1, &read_fd, NULL, NULL, &timeout);

	while(FD_ISSET(source_fd, &read_fd) && (read_length = read(source_fd, buffer, sizeof(buffer))) > 0)
	{
		
		struct timeval timeout = {0, 100};
		FD_ZERO(&read_fd);
		FD_SET(source_fd, &read_fd);
		select(source_fd + 1, &read_fd, NULL, NULL, &timeout);

		if (write(file_fd, buffer, read_length) != read_length)
		{
			char* error_message = "Write error!\n";
			write(2, error_message, strlen(error_message));
			return 0;
		}
	}

	if(read_length < 0)
	{
    	char* error_message = "Read error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	char* message = "New file is received!\n";
	write(2, message, strlen(message));
	return 1;
}