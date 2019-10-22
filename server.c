#include "commons.h"

#define PORT "8080"

int beat = 0;

void signal_handler(int sig);
void send_heart_beat(int udp_port);

int main(int argc, char const *argv[])
{
	if (argc != 2)
		die_with_error("Enter only broadcast port!\n");

	// UDP Server
	int udp_fd, udp_port;
	udp_port = atoi(argv[1]);

	// TCP Server
	int tcp_port = atoi(PORT);
	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in socket_address;
	socket_address.sin_family = PF_INET;
	socket_address.sin_addr.s_addr = INADDR_ANY;
	socket_address.sin_port = htons(tcp_port);
	int address_length = sizeof(socket_address);

	if (bind(server_fd, (struct sockaddr*)&socket_address, address_length) < 0)
		die_with_error("Could not bind to TCP port!\n");

	if (listen(server_fd, 100) < 0)
		die_with_error("Could not perform listening!\n");

	int sockets[100] = {0};
	sockets[0] = server_fd;
	int num = 1;

	char buffer[MAX_LINE] = {0};
	fd_set read_fd;

	signal(SIGALRM, signal_handler);
	alarm(1);

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

		if (beat)
		{
			send_broadcast(udp_port, "mig mig!");
			beat = 0;
			alarm(1);
		}

		int new_event = select(last_node + 1, &read_fd, NULL, NULL, &timeout);

		if (new_event < 0 && errno == EINTR)
			continue;

		if (FD_ISSET(server_fd, &read_fd))		// New connection
		{
			if ((new_socket = accept(server_fd, (struct sockaddr*)&socket_address, (socklen_t*)&address_length)) < 0)
				logger("Could not accept client's connection!\n", 2);
			else
			{
				++num;
				for (int i = 1; i < num; i++)
				{
					if (sockets[i] == 0)
					{
						sockets[i] = new_socket;
						logger("New socket added\n", 1);
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
							char* file_name = buffer + 9;
							int file_fd;

							if ((file_fd = open(file_name, O_RDONLY)) > 0)
							{
								close(file_fd);
								send(curent_client, "1", 1, 0);
								send_file(curent_client, file_name);
							}
							else
								send(curent_client, "0", 1, 0);
						}
						else if (memmem(buffer, MAX_LINE, "upload", 6) == buffer)
						{
							send(curent_client, "1", 1, 0);
							receive_file(curent_client, buffer + 7);
						}
						else
							send(curent_client, "0", 1, 0);
					}
					else
					{
						close(sockets[i]);
						sockets[i] = 0;
						logger("Socket disconnected\n", 1);
					}
				}
			}
		}
	}
}

// Functions

void signal_handler(int sig)
{
	beat = 1;
}