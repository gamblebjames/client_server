/*	Name:			Ben Gamble
	File:			client.c
	Date Created:	5/18/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h> #include <dirent.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PORT 2500
#define CLIENT_PORT 2501
#define BUF_SIZE 1000 //Maximum size for messages

void client_register(int socketfd);
void client_login(int socketfd);
void client_msg(int socketfd);
void client_disconnect(int socketfd);
void client_clist(int socketfd);
void client_flist(int socketfd);
void client_fput(int socketfd, char ip[], int port);
void client_fget(int socketfd);
void get_file(char file_recv[]);
void send_file(char filename[]);
char *rem(char *string);

char const DISCONNECT[] = "DISCONNECT";
char const REGISTER[] = "REGISTER";
char const LOGIN[] = "LOGIN";
char const MSG[] = "MSG";
char const CLIST[] = "CLIST";
char const FLIST[] = "FLIST";
char const FPUT[] = "FPUT";
char const FGET[] = "FGET";

char const SUCCESS[] = "<0x00>";
char const DENIED[] = "<0x01>";
char const DUP_ID[] = "<0x02>";
char const INVALID_ID[] = "<0x03>";
char const INVALID_IP[] = "<0x04>";
char const INVALID_FORMAT[] = "<0xFF>";

bool logged_in = 0; //Flag for when user is logged in
bool registered = 0; //Flag for when user is registered
char disc_user[100]; //Store user id when the user disconnects to notify server

int main(int argc, char **argv)
{
	int socketfd, fdmax, msg_chk;
	int count = 0;
	char *temp = malloc(100);
	char filename[100];
	char command[100];
	struct sockaddr_in sin;
	char msg_send[BUF_SIZE];
	char msg_recv[BUF_SIZE];
	char msg[BUF_SIZE];
	fd_set master;
	fd_set read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Failed to create socket");
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(sin.sin_zero, '\0', sizeof(sin.sin_zero));

	char ip[INET_ADDRSTRLEN];
	int port = (int)ntohs(sin.sin_port);
	inet_ntop(AF_INET, &sin.sin_addr.s_addr, ip, INET_ADDRSTRLEN);

	if(connect(socketfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("Failed to connect");
		return -1;
	}
	printf("Connected to server.\n");

    FD_SET(0, &master);
    FD_SET(socketfd, &master);
	
	fdmax = socketfd;
	while(1)
	{
		read_fds = master;
		if(registered == 0 && logged_in == 0)
			printf("Enter one of the following commands: REGISTER, LOGIN, DISCONNECT\n");
		else if(registered == 1 && logged_in == 0)
			printf("Enter one of the following commands: LOGIN, DISCONNECT\n");
		else if(registered == 1 && logged_in == 1)
			printf("Enter one of the following commands: MSG, CLIST, FLIST, FPUT, FGET, DISCONNECT\n");
		//Check for input on each file and don't block until there is input
		if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0 -1)
		{
			perror("Select failed");
			return -1;
		}
		
		for(int i = 0; i < (fdmax + 1); i++ )
		{
			if(FD_ISSET(i, &read_fds))
			{
				if (i == 0)
				{
					fgets(msg_send, BUF_SIZE, stdin); //Get user command
					strcpy(msg_send, rem(msg_send));
					//Check what the user command is and call the respective function
					//Check the user status -> registered and logged in
					if(registered == 0 && logged_in == 0)
					{
						if(strcmp(msg_send, DISCONNECT) == 0)
						{
							client_disconnect(socketfd);
							logged_in = 0;
							registered = 0;
							return 0;
						}
						else if(strcmp(msg_send, LOGIN) == 0)
						{
							client_login(socketfd);
						}
						else if(strcmp(msg_send, REGISTER) == 0)
						{
							client_register(socketfd);
						}
						else
							printf("Invalid command: REGISTER, LOGIN, DISCONNECT\n");
					}
					else if(registered == 1 && logged_in == 0)
					{
						if(strcmp(msg_send, DISCONNECT) == 0)
						{
							client_disconnect(socketfd);
							logged_in = 0;
							registered = 0;
							return 0;
						}
						else if(strcmp(msg_send, LOGIN) == 0)
						{
							client_login(socketfd);
						}
						else
							printf("Invalid command: DISCONNECT, LOGIN\n");
					}
					else if(registered == 1 && logged_in == 1)
					{
						if(strcmp(msg_send, DISCONNECT) == 0)
						{
							client_disconnect(socketfd);
							logged_in = 0;
							return 0;
						}
						else if(strcmp(msg_send, MSG) == 0)
						{
							client_msg(socketfd);
						}
						else if(strcmp(msg_send, CLIST) == 0)
						{
							client_clist(socketfd);
						}
						else if(strcmp(msg_send, FLIST) == 0)
						{
							client_flist(socketfd);
						}
						else if(strcmp(msg_send, FPUT) == 0)
						{
							client_fput(socketfd, ip, port);
						}
						else if(strcmp(msg_send, FGET) == 0)
						{
							client_fget(socketfd);
						}
						else
							printf("Invalid command: MSG, CLIST, FLIST, FPUT, FGET, DISCONNECT\n");
					}
				}
				else //Client receives message
				{
					memset(msg_recv, 0, sizeof(msg_recv));
					memset(msg, 0, sizeof(msg));
					msg_chk = recv(socketfd, msg_recv, BUF_SIZE, 0);
					strcpy(msg, msg_recv);
					temp = strtok(msg_recv, " ,");
					while(temp != NULL)
					{
						if(count == 0)
						{
							strcpy(command, temp);
						}
						if(count == 1)
						{
							strcpy(filename, temp);
						}
						count++;
						temp = strtok(NULL, ", ");
					}
					//FGET has the client prepare to send a file to another user
					if(strcmp(command, FGET) == 0)
					{
						send_file(filename);
					}
					else //If it isn't FGET then it is a message from other clients
						printf("Client:%s\n" , msg);
					fflush(stdout);
				}
			}
		}
	}
	
	if(close(socketfd) < 0)
	{
		perror("Error closing");
		return -1;
	}

	return 0;
}

//Register with the server to have client information added to client list
void client_register(int socketfd)
{
	char client_id[100], password[100];
	char client_info[300];
	char ack[100];

	//Construct the information string <REGISTER, id, password>
	printf("Enter a username: ");
	fgets(client_id, sizeof(client_id), stdin);
	strcpy(client_id, rem(client_id));
	printf("Enter a password: ");
	fgets(password, sizeof(password), stdin);
	strcpy(password, rem(password));
	memset(client_info, 0, sizeof(client_info));
	strcpy(client_info, REGISTER);
	strcat(client_info, ", ");
	strcat(client_info, client_id);
	strcat(client_info, ", ");
	strcat(client_info, password);
	if(send(socketfd, client_info, sizeof(client_info), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	client_info[0] = '\0';
	if(recv(socketfd, ack, sizeof(ack), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	printf("Server: %s\n", ack);
	if(strcmp(ack, DUP_ID) == 0)
	{
		printf("User already exists.\n");
	}
	else
	{
		registered = 1;
		strcpy(disc_user, DISCONNECT);
		strcat(disc_user, ", ");
		strcat(disc_user, client_id);
	}
	ack[0] = '\0';
}

//Login to the server
void client_login(int socketfd)
{
	char client_id[100], password[100];
	char client_info[300];
	char ack[100];

	//Construct the information string <LOGIN, id, password>
	printf("Enter your username: ");
	fgets(client_id, sizeof(client_id), stdin);
	strcpy(client_id, rem(client_id));
	printf("Enter your password: ");
	fgets(password, sizeof(password), stdin);
	strcpy(password, rem(password));
	memset(client_info, 0, sizeof(client_info));
	strcpy(client_info, LOGIN);
	strcat(client_info, ", ");
	strcat(client_info, client_id);
	strcat(client_info, ", ");
	strcat(client_info, password);
	if(send(socketfd, client_info, sizeof(client_info), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	client_info[0] = '\0';
	if(recv(socketfd, ack, sizeof(ack), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	printf("Server: %s\n", ack);
	if(strcmp(ack, SUCCESS) == 0)
	{
		logged_in = 1;
		registered = 1;
		printf("You are now logged in.\n");
	}
	else	
		printf("Failed to log in, incorrect username or password.\n");

	ack[0] = '\0';
}

//Get a message from the user and send it to the server where it will be broadcast
void client_msg(int socketfd)
{
	char msg_send[BUF_SIZE];
	char client_info[100+BUF_SIZE];

	//Construct the information string <MSG, msg>
	printf("Enter a message: ");
	fgets(msg_send, sizeof(msg_send), stdin);
	strcpy(msg_send, rem(msg_send));
	memset(client_info, 0, sizeof(client_info));
	strcpy(client_info, MSG);
	strcat(client_info, ", ");
	strcat(client_info, msg_send);
	if(send(socketfd, client_info, strlen(client_info), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
}

//Disconnect the client from the server and have the server remove the client from the list
void client_disconnect(int socketfd)
{
	char ack[100];

	if(send(socketfd, disc_user, strlen(disc_user), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	if(recv(socketfd, ack, sizeof(ack), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	printf("Disconnected from server.\n");
}

//Retrieve the client list from the server
void client_clist(int socketfd)
{
	char *clist_recv = (char *)malloc(100);
	char ack[100];
	int num_clients;

	//Notify server a client list request is being made
	if(send(socketfd, CLIST, strlen(CLIST), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	//Get the number of clients being sent from the server
	if(recv(socketfd, &num_clients, sizeof(num_clients), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	//Receive and print the client list from the server
	for(int j = 0; j < num_clients; j++)
	{
		if(recv(socketfd, clist_recv, sizeof(clist_recv), 0) < 0)
		{
			perror("Failed to receive message");
			exit(-1);
		}
		printf("%s\n", clist_recv);
	}
	if(recv(socketfd, ack, sizeof(ack), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	printf("Server: %s\n", ack);
	ack[0] = '\0';

	free(clist_recv);
}

//List all of the files available for download and the clients that have them
void client_flist(int socketfd)
{
	char *flist_recv = (char *)malloc(100);
	int num_files;
	char ack[100];

	//Notify server a file list request is being made
	if(send(socketfd, FLIST, strlen(FLIST), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	//Get the number of files being sent from the server
	if(recv(socketfd, &num_files, sizeof(num_files), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	//Receive and print the file list from the server
	for(int j = 0; j < num_files; j++)
	{
		if(recv(socketfd, flist_recv, sizeof(flist_recv), 0) < 0)
		{
			perror("Failed to receive message");
			exit(-1);
		}
		printf("Available file: %s\n", flist_recv);
	}
	if(recv(socketfd, ack, sizeof(ack), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	printf("Server: %s\n", ack);
	ack[0] = '\0';

	free(flist_recv);
}

//Let the server know this client has a file available for download
void client_fput(int socketfd, char ip[], int port)
{
	char filename[100];
	char file_send[400];
	char strport[100];
	char ack[100];
	DIR *dirp;
	struct dirent *ent;
	bool flag = 0;
	sprintf(strport, "%d", port); //int to string

	dirp = opendir(".");

	printf("Enter the name of the file you want to make available: ");
	fgets(filename, sizeof(filename), stdin);
	strcpy(filename, rem(filename));
	//Check if the specified file exists in the current working directory
	while((ent = readdir(dirp)) != NULL)
	{
		if(strcmp(ent->d_name, filename) == 0)
		{
			memset(file_send, 0, sizeof(file_send));
			memset(ack, 0, sizeof(ack));
			strcpy(file_send, FPUT);
			strcat(file_send, ", ");
			strcat(file_send, filename);
			strcat(file_send, ", ");
			strcat(file_send, ip);
			strcat(file_send, ", ");
			strcat(file_send, strport);
			if(send(socketfd, file_send, sizeof(file_send), 0) < 0)
			{
				perror("Failed to send message");
				exit(-1);
			}
			if(recv(socketfd, ack, sizeof(ack), 0) < 0)
			{
				perror("Failed to receive message");
				exit(-1);
			}
			printf("Server: %s\n", ack);
			flag = 1;
		}
	}
	if(flag == 0)
	{
		printf("File not found in current working directory.\n");
		printf("Server: %s\n", INVALID_FORMAT);
	}
	flag = 0;
	closedir(dirp);
}


//Get a file from a peer
void client_fget(int socketfd)
{
	char filename[100];
	char file_send[200];
	char file_recv[400];
	
	printf("Enter the name of the file you want to get: ");
	fgets(filename, sizeof(filename), stdin);
	memset(file_send, 0, sizeof(file_send));
	memset(file_recv, 0, sizeof(file_recv));
	strcpy(file_send, FGET);
	strcat(file_send, ", ");
	strcat(file_send, filename);
	if(send(socketfd, file_send, sizeof(file_send), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	file_send[0] = '\0';
	if(recv(socketfd, file_recv, sizeof(file_recv), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}
	//printf("Server: %s\n", file_recv);

	get_file(file_recv);
}

//Connect to the client and receive the file
void get_file(char file_recv[])
{
	char filename[100];
	char ip[100], port[100], fileid[100], sz[100];
	char *temp = malloc(100);
	int count = 0;
	int socketfd, data;
	int yes = 1;
	struct sockaddr_in sin;
	FILE *file;
	ssize_t bytes;

	temp = strtok(file_recv, ", ");
	while(temp != NULL)
	{
		if(count == 0)
		{
			strcpy(filename, temp);
			count++;
		}
		else if(count == 1)
		{
			strcpy(ip, temp);
			count++;
		}
		else if(count == 2)
		{
			strcpy(port, temp);
			count++;
		}
		else if(count == 3)
		{
			strcpy(fileid, temp);
		}
		temp = strtok(NULL, ", ");
	}

	//Connect to the client with the file to be downloaded
	if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Failed to create socket");
		exit(-1);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(CLIENT_PORT);
	sin.sin_addr.s_addr = inet_addr(ip);
	memset(sin.sin_zero, '\0', sizeof(sin.sin_zero));

	//Allow reuse of socket
	if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("Failed to set socket nonblocking");
		exit(-1);
	}

	if(connect(socketfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("Failed to connect");
		exit(-1);
	}
	printf("Connected to client. Receiving file.\n");

	if(recv(socketfd, sz, sizeof(sz), 0) < 0)
	{
		perror("Failed to receive message");
		exit(-1);
	}

	//if((file = fopen(filename, "w")) == 0)
	//Download the file from the peer
	if((file = fopen(filename, "w")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	data = atoi(sz);

	while(((bytes = recv(socketfd, sz, sizeof(sz), 0)) > 0) && (data > 0))
	{
		fwrite(sz, sizeof(char), bytes, file);
		data = data - bytes;
	}

	fclose(file);
	if(close(socketfd) < 0)
	{
		perror("Error closing");
		exit(-1);
	}
}

//Wait for the client requesting a file to connect then send the file
void send_file(char filename[])
{
	int client_socket, connect_socket, sinlen, file;
	int num_data = 0, sent_bytes = 0;
	int yes = 1;
	char size[100];
	off_t offset;
	ssize_t flen;
	struct sockaddr_in sin;
	struct stat file_stat;

	
	if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Socket error");
		exit(-1);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(CLIENT_PORT);
	sin.sin_addr.s_addr = INADDR_ANY;
	memset(sin.sin_zero, '\0', sizeof(sin.sin_zero));
	
	//Set socket to non blocking so the socket can be reused
	if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("Failed to set socket nonblocking");
		exit(-1);
	}
	
	if(bind(client_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("Failed to bind");
		exit(-1);
	}

	//Listen for a client connection to the socket
	if(listen(client_socket, 5) < 0)
	{
		perror("Failed to listen");
		exit(-1);
	}
	
	sinlen = sizeof(sin);
	
	if((connect_socket = accept(client_socket, (struct sockaddr *)&sin, &sinlen)) < 0)
	{
		perror("Failed to accept connection");
		exit(-1);
	}

	//Sendfile requires an integer so use open instead of fopen
	if((file = open(filename, O_RDONLY)) < 0)
	{
		perror("Failed to open file");
		exit(-1);
	}
	//Retrieve the file length that is being sent
	if(fstat(file, &file_stat) < 0)
	{
		perror("Failed to get file stats");
		exit(-1);
	}
	sprintf(size, "%li", file_stat.st_size);

	if((flen = send(connect_socket, size, sizeof(size), 0)) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}

	offset = 0;
	num_data = file_stat.st_size;
	while(((sent_bytes = sendfile(connect_socket, file, &offset, file_stat.st_size)) > 0) && (num_data > 0))
	{
		num_data = num_data - sent_bytes;
	}

	if(close(connect_socket) < 0)
	{
		perror("Error closing");
		exit(-1);
	}
	if(close(client_socket) < 0)
	{
		perror("Error closing");
		exit(-1);
	}
}

//Remove the newline from fgets input
char *rem(char *string)
{
	int size = strlen(string)-1;
	if((size > 0) && (string[size] == '\n'))
		string[size] = '\0';
	return string;
}

