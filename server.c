/*****************************************************************************
	Author: Ben Gamble
	Creation Date: 05/18/2017
	Last Modified: 01/18/2018
	Last Modified By: Ben Gamble
	Description: Server that waits for a client to connect then receives a
	message from the client.
******************************************************************************/

#include <client_server.h>
#include <sys/time.h>
#include <sys/types.h>

void	*server_register(void *arg);
void	*server_login(void *arg);
void	*server_msg(void *arg);
void	*server_disconnect(void *arg);
void	*server_clist(void *arg);
void	*server_flist(void *arg);
void	*server_fput(void *arg);
void	*server_fget(void *arg);
int		check_user(char filename[], char client_id[], char password[], int fdmax);
void	write_user(char filename[], int socketfd, char client_id[], char password[]);
char	*rem(char *string);

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

static int		num_clients = 0;
static int		num_files = 0;
int				*logged_in;
pthread_mutex_t lock;

typedef struct {
	char command[100];
	char client_id[100];
	char password[100];
	char peer_filename[100]; //file name received from client
	char peer_ip[100];       //ip of client that sent file
	char peer_port[100];     //port of client that sent file
} thread_info;

int main(
	int argc, 
	char **argv)
{
	if(argc < 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if((char *filename = (char *)malloc(256)) == NULL) {
		perror("Failed to allocate memory");
		exit(EXIT_FAILURE);
	}
	strcpy(filename, argv[1]);

	FILE	*flistfp, *clientfp;
	int		sfd, cfd, fdmax, msgerr, terror; 
	int		yes = 1; // address is in use
	int		count = 0;
	char	filename[256], msg[BUFSZ], msg_copy[BUFSZ];
	char	*temp = (char *)malloc(100);
	
	struct addrinfo			hints, *result, *rp;
	struct sockaddr_storage	remoteaddr;
	
	socklen_t	addrlen;
	thread_info	fd;
	pthread_t	thread;
	
	fd_set	master;		// file descriptor list
	fd_set	read_fds;   // temporary file descriptor list
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	strcpy(filename, argv[1]);
	
	// Create seperate files to hold the names of the registered clients and
	// the list of available files for transfer
	if((clientfp = fopen(fd.filename, "wr+")) == 0) {
		perror("Failed to open file");
		exit(EXIT_FAILURE);
	}
	fclose(clientfp);
	if((flistfp = fopen("flist", "wr+")) == 0){
		perror("Failed to open file");
		exit(EXIT_FAILURE);
	}
	fclose(flistfp);
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if((int s = getaddrinfo(NULL, PORT, &hints, &result)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	// Try teh available addressus until one successfully binds
	for(rp = results; rp != NULL; rp = rp->ai_next) {
		if((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0)
			continue;
		
		// Remove "address in use" error
		if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("Failed to set socket nonblocking");
			exit(EXIT_FAILURE);
		}

		if(bind(sfd, rp->ai_addr, rp->ai_addrlen) < 0) {
			close(sfd);
			continue;
		}
		break;
	}

	if(rp == NULL) {
		fprintf(stderr, "Failed to bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	if(listen(sfd, 10) < 0) {
		perror("Failed to listen");
		exit(EXIT_FAILURE);
	}

	// Add the listening socket file descriptor to the master set
	FD_SET(sfd, &master);

	// Current largest file descriptor
	fdmax = sfd;

	while(1) {
		read_fds = master; // Transfer fds in master set to temp set

		// Monitor the fds until one of them has characters to read
		if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
			perror("Select failed");
			exit(EXIT_FAILURE);
		}

		for(int i = 0; i <= fdmax; i++)
		{
			if(FD_ISSET(i, &read_fds)) //Check if the file descriptor is part of the set
			{
				if(i == sfd)
				{
					if((cfd = accept(sfd, (struct sockaddr *)&remoteaddr,
									&addrlen)) < 0) {
						perror("Failed to accept connection");
						exit(EXIT_FAILURE);
					}
					else
					{
						num_clients++;
						// Add file descriptor for new socket connection to the master set
						FD_SET(cfd, &master);
						if(cfd > fdmax)
							fdmax = cfd;
						printf("Client connected to server from %s on socket
								%d.\n", inet_ntop(remoteaddr.ss_family,
								get_in_addr((struct sockaddr*)&remoteaddr), remoteIP,
								INET6_ADDRSTRLEN), cfd);
					}
				}
				else {
					// Receive a message from the client
					if((msgerr = recv(i, msg, BUF_SIZE, 0)) < 0) {
						perror("Failed to receive message");
						close(i);
						FD_CLR(i, &master);
					}
					else if(msgerr == 0) {
						printf("Client disconnected.\n");
						num_clients--;
						fflush(stdout);
						close(i);
						FD_CLR(i, &master);
					}
					else
					{
						strcpy(msg_copy, msg);
						temp = strtok(msg_copy, ",");
						//3 arguments in register and login message: <command, username, password>
						if(strcmp(temp, REGISTER) == 0 || strcmp(temp, LOGIN) == 0)
						{
							while(temp != NULL)
							{
								if(count == 0)
								{
									strcpy(fd.command, temp);
									count++;
								}
								else if(count == 1)
								{
									strcpy(fd.client_id, temp);
									count++;
								}
								else if(count == 2)
								{
									strcpy(fd.password, temp);
								}
								temp = strtok(NULL, " ,");
							}
						}
						else if(strcmp(temp, FPUT) == 0) //fput, filename, ip, port
						{
							while(temp != NULL)
							{
								if(count == 0)
								{
									strcpy(fd.command, temp);
									count++;
								}
								else if(count == 1)
								{
									strcpy(fd.peer_filename, temp);
									count++;
								}
								else if(count == 2)
								{
									strcpy(fd.peer_ip, temp);
									count++;
								}
								else if(count == 3)
								{
									strcpy(fd.peer_port, temp);
								}
								temp = strtok(NULL, " ,");
							}

						}
						//2 arguments in message: <MSG, message> or <DISCONNECT, username>
						else if(strcmp(temp, MSG) == 0)
						{
							while(temp != NULL)
							{
								if(count == 0)
								{
									strcpy(fd.command, temp); 
									count++;
								}
								else if(count == 1)
								{
									strcpy(fd.msg_recv, temp); 
									count++;
								}
								temp = strtok(NULL, ","); //Dont strtok white space or it will split message
							}	
						}
						//Split these from message to strtok " ,"
						else if(strcmp(temp, DISCONNECT) == 0 || strcmp(temp, FGET) == 0)
						{
							while(temp != NULL)
							{
								if(count == 0)
								{
									strcpy(fd.command, temp); 
									count++;
								}
								else if(count == 1)
								{
									strcpy(fd.msg_recv, temp); 
									count++;
								}
								temp = strtok(NULL, " ,");
							}	
						}
						else if(strcmp(temp, CLIST) == 0 || strcmp(temp, FLIST) == 0)
						{
							strcpy(fd.command, temp);
						}
						count = 0;
						
						//Create a thread for the client command
						//Pass each thread the clients information and server information
						if(strcmp(fd.command, REGISTER) == 0)
						{
							terror = pthread_create(&thread, NULL, server_register, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL); 
						}
						else if(strcmp(fd.command, LOGIN) == 0)
						{
							
							terror = pthread_create(&thread, NULL, server_login, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, MSG) == 0)
						{
							terror = pthread_create(&thread, NULL, server_msg, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, CLIST) == 0)
						{
							terror = pthread_create(&thread, NULL, server_clist, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, DISCONNECT) == 0)
						{
							terror = pthread_create(&thread, NULL, server_disconnect, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, FLIST) == 0)
						{
							terror = pthread_create(&thread, NULL, server_flist, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, FPUT) == 0)
						{
							terror = pthread_create(&thread, NULL, server_fput, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
						else if(strcmp(fd.command, FGET) == 0)
						{
							terror = pthread_create(&thread, NULL, server_fget, (void *)&fd);
							if(terror != 0)
							{
								fprintf(stderr, "Failed to create thread: %s\n", strerror(terror));
							}
							pthread_join(thread, NULL);
						}
					}
				}
			}
		}
	}

	free(filename);
	free(msg);
	free(msg_copy);
	free(temp);

	return 0;
}

//Register to the server by writing the users info to specified file
//Ensure there are no duplicate usernames
void *server_register(void *arg)
{
	int exists; 
	char ack[100];
	thread_info fd;
	fd = *((thread_info *)arg);
	
	pthread_mutex_lock(&lock);
	exists = check_user(fd.filename, fd.client_id, fd.password, fd.fdmax);
	pthread_mutex_unlock(&lock);
	if(exists == 1 || exists == 2)
	{
		if(send(fd.curr_fd, DUP_ID, sizeof(DUP_ID), 0) < 0)
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	else if(exists == 0)
	{
		write_user(fd.filename, fd.curr_fd, fd.client_id, fd.password);
	}

	pthread_exit(NULL);
}

//Login to server and send ack once complete
//Check the username and password for each login
void *server_login(void *arg)
{
	int exists;
	int *temp;
	char ack[100];
	thread_info fd;
	fd = *((thread_info *)arg);
	
	pthread_mutex_lock(&lock);
	exists = check_user(fd.filename, fd.client_id, fd.password, fd.fdmax);
	pthread_mutex_unlock(&lock);
	if(exists == 0 || exists == 1)
	{
		if(send(fd.curr_fd, DENIED, sizeof(DENIED), 0) < 0)
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	else if(exists == 2)
	{
		//Allocate memory for new login fd so user can be added to available message list
		temp = realloc(logged_in, sizeof(int) * num_clients);
		logged_in = temp;
		logged_in[num_clients-1] = fd.curr_fd;
		if(send(fd.curr_fd, SUCCESS, sizeof(SUCCESS), 0) < 0)
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	
	pthread_exit(NULL);
}

//Send the client message to every connected user
void *server_msg(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);

	//strcpy(fd.msg_recv, rem(fd.msg_recv));
	//Send the client message to every other client
	for(int j = 0; j < (fd.fdmax + 1); j++)
	{
		for(int k = 0; k < num_clients; k++)
		{
			if (FD_ISSET(j, &fd.master))
			{
				//Send message only to logged in clients, omit self and server
				if (j != fd.server_socket && j != fd.curr_fd && j == logged_in[k])
				{
					if (send(j, fd.msg_recv, strlen(fd.msg_recv), 0) < 0) 
					{
						perror("Failed to send message");
						exit(-1);
					}
				}
			}
		}
	}

	pthread_exit(NULL);
}

//When client disconnects clean up user file and alter logged_in so other users cant 
//try to send that user a message
void *server_disconnect(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);
	int *tempptr;
	int count = 0;
	char temp_info[400], temp_write[400], file_copy[100];
	char *temp = (char *)malloc(100);
	FILE *clientfp;
	FILE *filefp;
	FILE *fp_copy;

	//Make a new file to hold clients that are still connected
	if((fp_copy = fopen("file_copy", "wr+")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}
	if((clientfp = fopen(fd.filename, "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	//Write only connected clients to new file
	fseek(clientfp, 0, SEEK_SET); //Start at beginning of file
	for(int j = 0; j < num_clients; j++)
	{
		memset(temp_info, 0, sizeof(temp_info));
		memset(temp_write, 0, sizeof(temp_write));
		fgets(temp_info, sizeof(temp_info), clientfp);
		strcpy(temp_write, temp_info);
		temp = strtok(temp_info, " ,");
		if(strcmp(temp, fd.msg_recv) != 0)
		{
			fputs(temp_write, fp_copy);
			fflush(fp_copy);
		}
	}
	fclose(clientfp);
	fclose(fp_copy);

	//Change the copied file to the main file
	remove(fd.filename);
	rename("file_copy", fd.filename);


	if((fp_copy = fopen("file_copy", "wr+")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}
	if((filefp = fopen("flist", "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	//Write only connected clients to new file
	fseek(filefp, 0, SEEK_SET); //Start at beginning of file
	for(int j = 0; j < num_files; j++)
	{
		memset(temp_info, 0, sizeof(temp_info));
		memset(temp_write, 0, sizeof(temp_write));
		memset(file_copy, 0, sizeof(file_copy));
		fgets(temp_info, sizeof(temp_info), filefp);
		strcpy(temp_write, temp_info);
		temp = strtok(temp_info, ", ");
		while(temp != NULL)
		{
			if(count == 3)
			{
				strcpy(file_copy, temp);
			}
			count++;
			temp = strtok(NULL, ", ");
		}
		count = 0;
		strcpy(file_copy, rem(file_copy));
		if(atoi(file_copy) !=  fd.curr_fd)
		{
			fputs(temp_write, fp_copy);
			fflush(fp_copy);
		}
	}

	fclose(filefp);
	fclose(fp_copy);

	//Change the copied file to the main file
	remove("flist");
	rename("file_copy", "flist");

	//Erase disconnected user from logged in client list for messaging
	int *temp_logged = (int *)malloc(sizeof(int) * (num_clients-1));
	for(int i = 0; i < num_clients; i++)
	{
		if(fd.curr_fd != i)
			temp_logged[i] = logged_in[i];
	}
	tempptr = realloc(logged_in, sizeof(int) * num_clients);
	logged_in = tempptr;
	for(int i = 0; i < num_clients-1; i++)
	{
		logged_in[i] = temp_logged[i];
	}

	if(send(fd.curr_fd, SUCCESS, sizeof(SUCCESS), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}	

	free(temp_logged);

	pthread_exit(NULL);
}

//Send the user a list of the current users
void *server_clist(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);
	char temp_info[300];
	char *temp = (char *)malloc(100);
	FILE *clientfp;
	
	if((clientfp = fopen(fd.filename, "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	fseek(clientfp, 0, SEEK_SET); //Start at beginning of file
	if (send(fd.curr_fd, &num_clients, sizeof(num_clients), 0) < 0) 
	{
		perror("Failed to send message");
		exit(-1);
	}
	for(int j = 0; j < num_clients; j++)
	{
		memset(temp_info, 0, sizeof(temp_info));
		fgets(temp_info, sizeof(temp_info), clientfp);
		temp = strtok(temp_info, " ,"); 
		if (send(fd.curr_fd, temp, sizeof(temp), 0) < 0) 
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	if (send(fd.curr_fd, SUCCESS, sizeof(SUCCESS), 0) < 0) 
	{
		perror("Failed to send message");
		exit(-1);
	}

	pthread_exit(NULL);
}

//List all of the files available for transfer
void *server_flist(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);
	FILE *flistfp;
	char temp_info[400];
	char *temp = (char *)malloc(100);
	
	if((flistfp = fopen("flist", "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	fseek(flistfp, 0, SEEK_SET); //Start at beginning of file
	//Notify the user how many file names are being sent
	if (send(fd.curr_fd, &num_files, sizeof(num_files), 0) < 0) 
	{
		perror("Failed to send message");
		exit(-1);
	}
	for(int j = 0; j < num_files; j++)
	{
		memset(temp_info, 0, sizeof(temp_info));
		fgets(temp_info, sizeof(temp_info), flistfp);
		temp = strtok(temp_info, " ,"); 
		if (send(fd.curr_fd, temp, sizeof(temp), 0) < 0) 
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	
	fclose(flistfp);
	
	if(send(fd.curr_fd, SUCCESS, sizeof(SUCCESS), 0) < 0)
	{
		perror("Failed to send message");
		exit(-1);
	}
	
	pthread_exit(NULL);
}

//Put the file information in the flist file so it is available for clients to retrieve
void *server_fput(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);
	FILE *flistfp;
	char file_info[400];
	char id[100];
	sprintf(id, "%d", fd.curr_fd);

	if((flistfp = fopen("flist", "a")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	if(strcmp(fd.peer_ip, "127.0.0.1") != 0 || strcmp(fd.peer_port, "2500") != 0)
	{
		if(send(fd.curr_fd, INVALID_IP, sizeof(INVALID_IP), 0) < 0)
		{
			perror("Failed to send message");
			exit(-1);
		}
	}
	else
	{
		memset(file_info, 0, sizeof(file_info));
		strcat(file_info, fd.peer_filename);
		strcat(file_info, ", ");
		strcat(file_info, fd.peer_ip);
		strcat(file_info, ", ");
		strcat(file_info, fd.peer_port);
		strcat(file_info, ", ");
		strcat(file_info, id);
		strcat(file_info, "\n");
		fputs(file_info, flistfp);
		fflush(flistfp);
		fclose(flistfp);
	
		num_files++;
		if(send(fd.curr_fd, SUCCESS, sizeof(SUCCESS), 0) < 0)
		{
			perror("Failed to send message");
			exit(-1);
		}
	}

	pthread_exit(NULL);
}

//Facilitate peer to peer transfer by sending ip and port of client
//that has file to requesting client
void *server_fget(void *arg)
{
	thread_info fd;
	fd = *((thread_info *)arg);
	FILE *flistfp;
	char file_info[400];
	char temp_info[400];
	char sender_info[200];
	char *temp = (char *)malloc(100);
	bool found = 0;
	char filename[100];
	char fileid[100];
	int count = 0;

	if((flistfp = fopen("flist", "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	fseek(flistfp, 0, SEEK_SET); //Start at beginning of file
	for(int j = 0; j < num_files; j++)
	{
		memset(file_info, 0, sizeof(file_info));
		memset(temp_info, 0, sizeof(temp_info));
		fgets(file_info, sizeof(file_info), flistfp);
		strcpy(file_info, rem(file_info));
		strcpy(temp_info, file_info);
		temp = strtok(file_info, " ,");
		if(strcmp(temp, fd.peer_filename) == 0)
		{
			while(temp != NULL)
			{
				if(count == 0)
				{
					strcpy(filename, temp);
				}
				if(count == 3)
				{
					strcpy(fileid, temp);
				}
				count++;
				temp = strtok(NULL, ", ");
			}
			memset(sender_info, 0, sizeof(file_info));
			strcat(sender_info, FGET);
			strcat(sender_info, ", ");
			strcat(sender_info, filename);
			
			found = 1;
			//Send info of client with file  to the requesting client
			if(send(fd.curr_fd, temp_info, sizeof(temp_info), 0) < 0) 
			{
				perror("Failed to send message");
				exit(-1);
			}
			//Let the sending client know to get ready for connection
			if(send(atoi(fileid), sender_info, sizeof(sender_info), 0) < 0) 
			{
				perror("Failed to send message");
				exit(-1);
			}
			break;
		}
	}
	
	if(found == 0)
	{
		if (send(fd.curr_fd, INVALID_ID, sizeof(INVALID_ID), 0) < 0) 
		{
			perror("Failed to send message");
			exit(-1);
		}
	}

	fclose(flistfp);
	
	pthread_exit(NULL);
}

//See if the user is in the specified file
int check_user(char filename[], char client_id[], char password[], int fdmax)
{
	int i = 0, found = 0;
	char temp_info[300];
	char *temp = (char *)malloc(100);
	char id_check[100], pass_check[100];
	FILE *clientfp;

	if((clientfp = fopen(filename, "r")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	fseek(clientfp, 0, SEEK_SET); //Start at beginning of file
	for(int j = 0; j < num_clients; j++)
	{
		memset(temp_info, 0, sizeof(temp_info));
		fgets(temp_info, sizeof(temp_info), clientfp);
		temp = strtok(temp_info, " ,");
		while(temp != NULL)
		{
			if(i == 0)
			{
				strcpy(id_check, temp);
				i++;
			}
			else if(i == 1)
			{
				strcpy(pass_check, temp);
			}
			temp = strtok(NULL, " ,");
		}
		i = 0;
		if(strcmp(client_id, id_check) == 0)
		{
			found = 1; //Username is there and correct
			strcpy(pass_check, rem(pass_check));
			if(strcmp(password, pass_check) == 0)
			{
				found = 2; //Username and password is there and correct
			}
			break;
		}
	}

	free(temp);
	fclose(clientfp);
	
	//Return 0 if username is not in file
	return found;
}

//Write the user info in the specified file and send ack
void write_user(char filename[], int socketfd, char client_id[], char password[])
{
	char client_info[300];
	FILE *clientfp;

	if((clientfp = fopen(filename, "a+")) == 0)
	{
		perror("Failed to open file");
		exit(-1);
	}

	memset(client_info, 0, sizeof(client_info));
	strcat(client_info, client_id);
	strcat(client_info, ", ");
	strcat(client_info, password);
	strcat(client_info, "\n");
	pthread_mutex_lock(&lock);
	fputs(client_info, clientfp);
	pthread_mutex_unlock(&lock);
	fflush(clientfp);
	
	fclose(clientfp);
	
	if(send(socketfd, SUCCESS, sizeof(SUCCESS), 0) < 0)
	{
		perror("Failed to send message");
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
