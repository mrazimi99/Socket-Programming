#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define PORT "8080"
#define MAX_LINE 1024

int beat = 0;

int send_file(int destination_fd, char* file_name);
int receive_file(int source_fd, char* file_name);
void signal_handler(int sig);
void send_heart_beat(int udp_port);

int main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		char* error_message = "Enter broadcast port only.\n";
		write(2, error_message, strlen(error_message));
		return 1;
	}

	// // UDP Server
	int udp_fd, udp_port;
	udp_port = atoi(argv[1]);
	// udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
	// struct sockaddr_in udp_address;
	// udp_address.sin_family = PF_INET;
	// udp_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	// udp_address.sin_port = htons(udp_port);
	// int udp_address_length = sizeof(udp_address);
	// int broadcast = 1;

	// setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

	// // if (bind(udp_fd, (struct sockaddr*)&udp_address, udp_address_length) < 0)
	// // {
	// // 	perror("bind 2\n");
	// // 	return 1;
	// // }

	// TCP Server
	int tcp_port = atoi(PORT);
	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in socket_address;
	socket_address.sin_family = PF_INET;
	socket_address.sin_addr.s_addr = INADDR_ANY;
	socket_address.sin_port = htons(tcp_port);
	int address_length = sizeof(socket_address);

	if (bind(server_fd, (struct sockaddr*)&socket_address, address_length) < 0)
	{
		perror("bind 2\n");
		return 1;
	}

	if (listen(server_fd, 100) < 0)
	{
		perror("listen\n");
		return 1;
	}

	int sockets[100] = {0};
	sockets[0] = server_fd;
	int num = 1;

	char* files[1000] = {NULL};
	int files_num = 0;

	char buffer[MAX_LINE] = {0};
	fd_set read_fd;

	// signal(SIGALRM, signal_handler);
	// alarm(1);

	struct timeval past, now;
	gettimeofday(&now, NULL);
	past = now;
	// Forever
	while (1)
	{
		struct timeval timeout = {0, 0};
		int new_socket = 0;
		int last_node = server_fd;
		FD_ZERO(&read_fd);

		for (int i = 0; i < num; i++)
		{
			if(sockets[i] > 0)
				FD_SET(sockets[i], &read_fd);

			if (sockets[i] > last_node)
				last_node = sockets[i];
		}

		// if (beat)
		// {
		// 	alarm(1);
		// 	send_heart_beat(udp_port);
		// 	beat = 0;
		// }

		int new_event = select(last_node + 1, &read_fd, NULL, NULL, &timeout);

		// if (errno == EINTR)
		// 	continue;

		if (FD_ISSET(server_fd, &read_fd))		// New connection
		{
			if ((new_socket = accept(server_fd, (struct sockaddr*)&socket_address, (socklen_t*)&address_length)) < 0)
				perror("Accept Failed!\n");
			else
			{
				++num;
				for (int i = 1; i < num; i++)
				{
					if (sockets[i] == 0)
					{
						sockets[i] = new_socket;
						char* message = "New socket added\n";
						write(1, message, strlen(message));
						break;
					}
				}
			}
		}
		else
		{
			for (int i = 1; i < num; i++)
			{
				int curent_client = sockets[i];
				if (FD_ISSET(curent_client, &read_fd))
				{
					int read_size = 0;
					if ((read_size = read(curent_client, buffer, MAX_LINE)) > 0)
					{
						buffer[read_size] = '\0';
						if (memmem(buffer, read_size, "download", 8) == buffer)
						{
							int found = 0;
							for (int j = 0; j < files_num; j++)
							{
								if (!strcmp(files[j], buffer + 9))
								{
									send(curent_client, "1", 1, 0);
									send_file(curent_client, files[j]);
									found = 1;
									break;
								}
							}

							if (!found)
								send(curent_client, "0", 1, 0);
						}
						else if (memmem(buffer, MAX_LINE, "upload", 6) == buffer)
						{
							int duplicate = 0;

							for (int j = 0; j < files_num; j++)
							{
								if (!strcmp(files[j], buffer + 7))
								{
									send(curent_client, "0", 1, 0);
									duplicate = 1;
									break;
								}
							}

							if (!duplicate)
							{
								files[files_num] = (char*)malloc((read_size - 7 + 1) * sizeof(char));
								strcpy(files[files_num], buffer + 7);
								send(curent_client, "1", 1, 0);
								receive_file(curent_client, files[files_num++]);
							}
						}
						else
						{
							printf("%s\n", buffer);
							send(curent_client, "0", 1, 0);
						}
					}
					else
					{
						close(sockets[i]);
						sockets[i] = 0;
						char* message = "Socket disconnected\n";
						write(1, message, strlen(message));
					}
				}
			}
		}
		gettimeofday(&now, NULL);
		if (now.tv_sec >= past.tv_sec + 1)
		{
			past = now;
			send_heart_beat(udp_port);
		}
	}
}

int send_file(int destination_fd, char* file_name)
{
	char buffer[MAX_LINE];
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

void signal_handler(int sig)
{
	beat = 1;
}

void send_heart_beat(int udp_port)
{
	int server_socket;
	if ((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
	{ 
		perror("Socket creation failed!"); 
		exit(EXIT_FAILURE);
	}

	int broadcast = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

	struct sockaddr_in broadcast_address;

	broadcast_address.sin_family = PF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST); 
	broadcast_address.sin_port = htons(udp_port);
	char* beat_sound = "mig mig!\n";
	write(1, beat_sound, strlen(beat_sound));
	sendto(server_socket, PORT, strlen(PORT), MSG_DONTWAIT, (const struct sockaddr *)&broadcast_address,
			sizeof broadcast_address);
	close(server_socket);
}