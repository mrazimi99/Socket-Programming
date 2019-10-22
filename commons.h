#define _GNU_SOURCE

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

int send_file(int destination_fd, char* file_name);
int receive_file(int source_fd, char* file_name);
void send_broadcast(int udp_port, char* message);
void logger(char* message, int write_fd);
void die_with_error(char* message);

int send_file(int destination_fd, char* file_name)
{
	char buffer[MAX_LINE];
	int file_fd;

	if ((file_fd = open(file_name, O_RDONLY)) < 0)
	{
		logger("Could not open file!\n", 2);
		return 0;
	}

	int read_length;
	while((read_length = read(file_fd, buffer, sizeof(buffer))) > 0)
	{
		if (send(destination_fd, buffer, read_length, 0) != read_length)
		{
			logger("Could not write to file properly!\n", 2);
			return 0;
		}
	}

	if(read_length < 0)
	{
    	logger("File has not received properly!\n", 2);
		return 0;
	}

	logger("File is sent!\n", 1);
	return 1;
}

int receive_file(int source_fd, char* file_name)
{
	char buffer[MAX_LINE];
	int file_fd;
	if ((file_fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0)
	{
		logger("Could not create file!\n", 2);
		return 0;
	}

	int read_length;
	fd_set read_fd;
	struct timeval timeout = {1, 0};
	setsockopt(source_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

	while((read_length = read(source_fd, buffer, sizeof(buffer))) > 0)
	{
		if (write(file_fd, buffer, read_length) != read_length)
		{
			logger("Could not write to file properly!\n", 2);
			return 0;
		}

		if (read_length < MAX_LINE)
			break;
	}

	if(read_length < 0)
	{
    	logger("File has not received properly!\n", 2);
		return 0;
	}

	logger("New file is received!\n", 1);
	return 1;
}

void send_broadcast(int udp_port, char* message)
{
	int broadcast_socket;
	if ((broadcast_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		die_with_error("Broadcast socket creation failed!\n");

	int broadcast = 1;
	setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

	struct sockaddr_in broadcast_address;
	broadcast_address.sin_family = PF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST); 
	broadcast_address.sin_port = htons(udp_port);

	sendto(broadcast_socket, message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *)&broadcast_address,
			sizeof broadcast_address);
	close(broadcast_socket);
}

void logger(char* message, int write_fd)
{
	write(write_fd, message, strlen(message));
}

void die_with_error(char* message)
{
	logger(message, 2);
	exit(EXIT_FAILURE);
}