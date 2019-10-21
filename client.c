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

int download_file(int server_fd, char* file_name);
int upload_file(int server_fd, char* file_name);

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

	// HeartBeat
	int heartbeat_fd = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in heartbeat_address;
	memset(&heartbeat_address, 0, sizeof(heartbeat_address)); 
	heartbeat_address.sin_family = PF_INET;
	heartbeat_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	heartbeat_address.sin_port = htons(heart_beat_port);
	int heartbeat_address_length = sizeof(heartbeat_address);
	int broadcast = 1;

	setsockopt(heartbeat_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast));

	if (bind(heartbeat_fd, (struct sockaddr*)&heartbeat_address, heartbeat_address_length) < 0)
	{
		char* error_message = "Could not bind to heartbeat port!\n";
		write(1, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	}

	int received_data = 0;
	int server_is_alive = 0;
	char heart_beat_message[100];
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

		if (FD_ISSET(heartbeat_fd, &heartbeat_fd_set) && (received_data = recvfrom(heartbeat_fd, heart_beat_message, 100, 0, NULL, NULL)) < 0)
		{
			printf("ok\n");
			server_is_alive = 0;
			break;
		}
		else if (received_data > 0)
		{
			heart_beat_message[received_data] = '\0';
			printf("Server is up with port: %s\n", heart_beat_message);
			server_is_alive = 1;
			break;
		}

		gettimeofday(&now, NULL);
		if (now.tv_sec > past.tv_sec + 1)
		{printf("o213k\n");
			server_is_alive = 0;
			break;
		}
	}

	// TCP
	int client_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in socket_address, server_address;
	socket_address.sin_family = PF_INET;
	socket_address.sin_addr.s_addr = INADDR_ANY;
	socket_address.sin_port = htons(client_port);
	int address_length = sizeof socket_address;

	// Server
	// if (server_is_alive)
	{
		int server_port = atoi(heart_beat_message);
		server_address.sin_family = PF_INET;
		server_address.sin_addr.s_addr = INADDR_ANY;
		server_address.sin_port = htons(server_port);
		int server_address_length = sizeof server_address;

		if (bind(client_fd, (struct sockaddr*)&socket_address, address_length) < 0)
		{
			char* error_message = "Could not bind client!\n";
			write(1, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}

		if (connect(client_fd, (struct sockaddr*)&server_address, server_address_length) < 0)
		{
			char* error_message = "Could not connect to server!\n";
			write(1, error_message, strlen(error_message));
			exit(EXIT_FAILURE);
		}
	}

	fd_set read_fd;

	while (1)
	{
		char command[100];
		int read_size;

		struct timeval timeout = {0, 0};
		FD_ZERO(&read_fd);
		FD_SET(0, &read_fd);
		select(0 + 1, &read_fd, NULL, NULL, &timeout);
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
				download_file(client_fd, command);
			else if (memmem(command, read_size, "upload", 6) == command)
				upload_file(client_fd, command);
			else
			{
				char* message = "Invalid command. Try again!\n";
				write(1, message, strlen(message));
			}
		}	
	}
}

int download_file(int server_fd, char* command)
{
	char buffer[1024];
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

	printf("%d\n", message_length);

	int file_fd;
	char* file_name = command + 9;
	if ((file_fd = open(file_name, O_RDWR | O_CREAT, S_IRWXU)) < 0)
	{
		char* error_message = "Create error!\n";
		write(2, error_message, strlen(error_message));
		return 0;
	}

	int read_length;
	while((read_length = read(server_fd, buffer, sizeof(buffer))) > 0)
	{
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

	char buffer[1024];
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