#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <netdb.h>
#include <stdbool.h>


#define REQUESTLEN 4096
#define REPLYLEN   100000
#define BIGBUFFLEN 500000


/* Request and reply buffers: */
char request[REQUESTLEN+1];
char reply[REPLYLEN+1];
char bigbuff[BIGBUFFLEN+1];
/* Buffer for file/dir name: */
char stringData[512];
/* How many bytes rcvd: */
int bytesRcvd;
/* Length of rcvd line: */
int lineLength;
/* TCP sockets and passive mode port: */
int sockfd, sockps, portps;
/* Variable for resolving response code: */
int intCode;
/* Additional var (char position in string): */
long int pos;
/* Passive mode IP buffer: */
char ipps[32];

/* Gui loop flag: */
bool makeLoop = true;

/* Pointers and variables for data from file. */
char *servName;
char *userName;
char *userPassword;
unsigned short echoServPort;

/* Additional pointer for explode */
char **rcvdCode;

int get_tcp_socket(char *ip, int port) {
	int socketNum;

	struct hostent *host;
	struct sockaddr_in dest_addr;


	socketNum = socket(AF_INET, SOCK_STREAM, 0);

	dest_addr.sin_family=AF_INET;
	dest_addr.sin_port=htons(port);
	dest_addr.sin_addr.s_addr = inet_addr(ip);


	memset(&(dest_addr.sin_zero), '\0', 8);


	if (connect(socketNum, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr)) == -1 ) {
		//fprintf(stderr, "Error: Cannot connect to host!\n");
		
		return -1;
	}

	return socketNum;
}

size_t strlstchar(char *str, const char ch)
{
    char *chptr = strrchr(str, ch);
    return chptr - str;
}

size_t strfrschar(char *str, const char ch)
{
    char *chptr = strchr(str, ch);
    return chptr - str;
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **)malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

char * concatenateString(char *output, char *str1, char *str2) {
	output = (char *) malloc(1 + strlen(str1) + strlen(str2));
	strcpy(output, str1);
	strcat(output, str2);
	
	return output;
}

int hostname_to_ip(char * hostname , char* ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
         
    if ( (he = gethostbyname( hostname ) ) == NULL) 
    {
        return 1;
    }
 
    addr_list = (struct in_addr **) he->h_addr_list;
     
    for(i = 0; addr_list[i] != NULL; i++) 
    {
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
     
    return 1;
}

void rcvdResponse(int sock, char * rep, bool print) {
	bytesRcvd = recv(sock, rep, REPLYLEN - 1, MSG_CMSG_CLOEXEC);
	rep[bytesRcvd] = '\0';
	
	if (print)
		printf("%s", rep);
}

void getPassiveMode() {
	do {
		intCode = 0;
		sprintf(request,"PASV\r\n");
		write(sockfd, request, strlen(request));

		/* Sleep in microseconds before recursion: */
		usleep(100000);
		rcvdResponse(sockfd, &reply[0], false);
	
		/* Get response status code: */
		memcpy (request, reply, 3);
		request[3] = '\0';
		intCode = atoi(request);


		if (intCode == 227) {
			pos = strlstchar(reply, '(');
			memcpy(request, (reply+pos+1), strlen(reply)-pos);
			pos = strlstchar(request, ')');
			request[pos] = 0;
			rcvdCode = str_split(request, ',');

			/* Getting IP for passive mode: */
			sprintf(ipps,"%s.%s.%s.%s", rcvdCode[0], rcvdCode[1], rcvdCode[2], rcvdCode[3]);
			ipps[strlen(rcvdCode[0])+strlen(rcvdCode[1])+strlen(rcvdCode[2])+strlen(rcvdCode[3])+3] = 0;

			/* Getting port for passive mode: */
			portps = atoi(rcvdCode[4])*256+atoi(rcvdCode[5]);

			/* Deallocation of split array: */
			for (int i = 0; *(rcvdCode + i); i++) { free(*(rcvdCode + i)); }
			free(rcvdCode);
		
			sockps = get_tcp_socket(ipps, portps);
		}
	} while (intCode != 227 || sockps == -1);
}


void printContent(int sock, char *rep, int tab, bool recursive) {
	char **lines = NULL;
	char *line = NULL;
	char *parent = (char*)malloc(1024 * sizeof(char));
	int endBigBuff = 0;
	
	
	bytesRcvd = recv(sock, rep, REPLYLEN - 1, MSG_CMSG_CLOEXEC);
	rep[bytesRcvd] = '\0';

	endBigBuff = bytesRcvd;
	memcpy(&bigbuff[0], &rep[0], bytesRcvd);

	if (bytesRcvd > 0) {
		while (reply[bytesRcvd-1] != 10) {
			bytesRcvd = recv(sock, rep, REPLYLEN - 1, MSG_CMSG_CLOEXEC);
			rep[bytesRcvd] = '\0';
			memcpy(&bigbuff[endBigBuff], &rep[0], bytesRcvd);
			endBigBuff += bytesRcvd;
		}
		bigbuff[endBigBuff] = '\0';


		lines = str_split(bigbuff, '\n');
		
		for (int i = 0; *(lines + i); i++)
		{
			line = *(lines + i);
			lineLength = strlen(line);
			
			

			if ((line[5] == 'c' || line[5] == 'p' || line[5] == 'd' || line[5] == 'f')) {
				
				pos = strlstchar(line, ' ');
				memcpy(stringData, (line+pos+1), lineLength-pos);

				if (line[5] != 'c' && line[5] != 'p') {
					for (int j = 0; j<tab; j++)
						printf("    ");
			
			
					if (line[5] == 'd') {
						printf("/");
					}

					printf("%s\n", stringData);

					if (line[5] == 'd' && recursive) {
						getPassiveMode();
						

						sprintf(request,"PWD\r\n");
						write(sockfd, request, strlen(request));
						rcvdResponse(sockfd, &reply[0], false);


						sprintf(request,"CWD %s\r\n", stringData);
						write(sockfd, request, strlen(request));
						rcvdResponse(sockfd, &reply[0], false);

						sprintf(request,"MLSD\r\n");
						write(sockfd, request, strlen(request));
						printContent(sockps, &reply[0], tab+1, true);

						close(sockps);

					}
				
					line[0] = 0;
					
				} else if (line[5] == 'p') {
					/* Setting up parent directory: */
					strcpy(parent, stringData);
				}
			}
		}
		
		/* After recursion change to parent directory */
		if (recursive && tab > 1) {
			sprintf(request,"CWD %s\r\n", parent);
			write(sockfd, request, strlen(request));
			rcvdResponse(sockfd, &reply[0], false);
		}

		for (int i = 0; *(lines + i); i++) { free(*(lines + i)); }
		free(lines);
		
	} else {
		printf("RESPONSE SIZE: %d!\n", bytesRcvd);
	}
	
	
	free(parent);
}





int main(int argc, char **argv) {

	/* Passing arguments is forbidden. */
		if (argc != 1) {
			fprintf(stderr, "Error: Bad arguments!\n");
			exit(1);
		}
		
	
	/* Allocating memory for data from file. */
		servName = (char*)malloc(256 * sizeof(char));
		userName = (char*)malloc(256 * sizeof(char));
		userPassword = (char*)malloc(256 * sizeof(char));


	/* Getting connection info form file. */
		FILE* file = fopen("App.config", "r");
	
		char fileLine[256];
		char startPath[256];
		char** configPackage;
	
		bool hostIs, portIs, userIs, passIs, allIs;
		hostIs = portIs = userIs = passIs = allIs = false;
		
		startPath[0] = 0;
		
		while (fgets(fileLine, sizeof(fileLine), file)) {
			configPackage = str_split(fileLine, ':');
		
			if (strcmp(configPackage[0], "HOST") == 0 && !hostIs) {
				int pos = strfrschar(configPackage[1], '/');
				
				
				if (pos > -1) {
					if (configPackage[1][pos+1] != '\n') {			
						memcpy(startPath, &configPackage[1][pos+1], strlen(configPackage[1])-pos);
						startPath[strlen(configPackage[1])-pos-2] = 0;
					}
					
					configPackage[1][pos] = '\n';
					configPackage[1][pos+1] = '\0';
				}
				
				memcpy(servName, configPackage[1], strlen(configPackage[1])-1);
				
				if (strlen(configPackage[1]) > 1) {
					hostIs = true;
				}

			} else if (strcmp(configPackage[0], "PORT") == 0 && !portIs) {
				echoServPort = atoi(configPackage[1]);
				if (echoServPort == 0) {
					echoServPort = 21;
					printf("Default port: %d\n", echoServPort);
				}
				
				portIs = true;
			} else if (strcmp(configPackage[0], "USER") == 0 && !userIs) {
				memcpy(userName, configPackage[1], strlen(configPackage[1])-1);
				if (strlen(configPackage[1]) > 1) {
					userIs = true;
				}
			} else if (strcmp(configPackage[0], "PASS") == 0 && !passIs) {
				memcpy(userPassword, configPackage[1], strlen(configPackage[1])-1);
				if (strlen(configPackage[1]) > 1) {
					passIs = true;
				}
			}
			
			for (int i = 0; *(configPackage + i); i++) { free(*(configPackage + i)); }
			free(configPackage);
			
			
			if (hostIs && portIs && userIs && passIs) {
				allIs = true;
				break;
			}
		}
		fclose(file);
		
		if (!allIs) {
			fprintf(stderr, "Error: Configuration file is incomplete!\n");
			exit(1);
		}
	

	/* Resolving domain name to IP address. */
		char ip[32];
		
		int hostResult = hostname_to_ip(servName , ip);
		
		if (hostResult == 1) {
			fprintf(stderr, "Error: Bad hostname or Internet connection fail!\n");
			exit(1);
		}
		printf("%s resolved to %s\n" , servName , ip);

		
	char ** commandParts;

    /* Create communication. */
	sockfd = get_tcp_socket(ip, echoServPort);
    rcvdResponse(sockfd, &reply[0], true);
    
    
    sprintf(request,"USER  %s\r\n", userName);
    write(sockfd, request, strlen(request));
	rcvdResponse(sockfd, &reply[0], true);

    
    sprintf(request,"PASS %s\r\n", userPassword);
    write(sockfd, request, strlen(request));
	rcvdResponse(sockfd, &reply[0], true);


    sprintf(request,"TYPE A\r\n");
    write(sockfd, request, strlen(request));
	rcvdResponse(sockfd, &reply[0], true);

	if (strlen(startPath)) {
		sprintf(request,"CWD  %s\r\n", startPath);
		write(sockfd, request, strlen(request));
		rcvdResponse(sockfd, &reply[0], false);
	}

	
	/* Print start catalog: */
	
	printf("\n========================== MENU ==========================\n");
	printf("Commands: 'ls' and 'cd';   Print all: 'r';\t Quit: 'q'\n");


	long int actionLength;
	char action[5];
	
	action[0] = 0;

	while (makeLoop) {

		if (strlen(action) != 0) {
			printf("ftp> ");
			fgets(action, 100, stdin);
		}
		else {
			printf("\n");
			sprintf(action,"ls\n");
		}
		
		actionLength = strlen(action);

		if (actionLength == 2 && action[0] == 'q') {
			makeLoop = false;
		} else {
		
			getPassiveMode();
			
			if (actionLength == 2 && action[0] == 'r') {
				sprintf(request,"CWD /\r\n");
				write(sockfd, request, strlen(request));
				rcvdResponse(sockfd, &reply[0], false);
				
				sprintf(request,"MLSD\r\n");
				write(sockfd, request, strlen(request));
				
				printf("/\n");
				printContent(sockps, &reply[0], 1, true);
				rcvdResponse(sockfd, &reply[0], false);
				
			} else if (actionLength == 3 && strcmp(action, "ls\n") == 0) {
				printf("\n");
				sprintf(request,"PWD\r\n");
				write(sockfd, request, strlen(request));
				rcvdResponse(sockfd, &reply[0], true);

				sprintf(request,"MLSD\r\n");
				write(sockfd, request, strlen(request));
				printContent(sockps, &reply[0], 0, false);
				rcvdResponse(sockfd, &reply[0], false);
				printf("\n");
			} else if (actionLength > 4 && action[2] == ' ' && action[3] != ' ') {
				
				commandParts = str_split(action, ' ');
				
				if (strcmp(commandParts[0], "cd") == 0) {
					if (strcmp(commandParts[1], "..\n") == 0) {
						sprintf(request,"CDUP\r\n");
						write(sockfd, request, strlen(request));
						rcvdResponse(sockfd, &reply[0], false);
					} else {
						sprintf(request,"CWD %s", commandParts[1]);
						write(sockfd, request, strlen(request));
						rcvdResponse(sockfd, &reply[0], false);
					}
					
					sprintf(request,"PWD\r\n");
					write(sockfd, request, strlen(request));
					rcvdResponse(sockfd, &reply[0], true);
				}
				
				for (int i = 0; *(commandParts + i); i++) { free(*(commandParts + i)); }
				free(commandParts);
			}

		}
		
	}
	
    sprintf(request,"QUIT\r\n");
    write(sockfd, request, strlen(request));
	rcvdResponse(sockfd, &reply[0], true);
    
	close(sockfd);
	close(sockps);

	/* Doing memory deallocation: */
	free(servName);
	free(userName);
	free(userPassword);

	return 0;
}
