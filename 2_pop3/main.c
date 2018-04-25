#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

/* Size of server response buffer: */
#define RCVBUFSIZE 4096
/* Size of message ids array: */
#define IDSMEMORY 100000
/* Big buffer length for concatenated responsed: */
#define BIGBUFFLEN 100000
/* Connection status: */
int connection = -1;
/* While loop program running flag: */
bool keepRunning = true;
/* For TCP socket: */
int sock;
/* Request buffer: */
char *echoString;
/* Concatenation of responses: */
char *bigBuffer;
/* Response buffer: */
char echoBuffer[RCVBUFSIZE];
/* Emails array: */
char** ids;
/* Pointer for last index of ids put: */
unsigned long int idsPointer = 0;
/* Number of existed emails on run: */
unsigned long int allMsgOnRun;
/* Flag for infomation if memory was alocated: */
bool allocated = false;
/* How many received: */
int receivedCounter = 0;

/* Pointers and variables for data from file. */
char *servName;
char *userName;
char *userPassword;
unsigned int sleepTime;
unsigned short echoServPort;


/* Declaration of functions: */
void endConnection();
void INThandler(int);
int hostname_to_ip(char*, char*);
void queryResponse(int*, char*, char*, bool);
char** str_split(char*, const char);
char * concatenateString(char*, char*, char*);


int main(int argc, char **argv) {
	/* Catch CTRL-C pressed. */
		signal(SIGINT, INThandler);
	
	/* Passing arguments is forbidden. */
		if (argc != 1) {
			fprintf(stderr, "Error: Bad arguments!\n");
			exit(1);
		}
		
    /* Checking if file config exists. */      
		if(!(access("App.config", F_OK ) != -1)) {
		    fprintf(stderr, "Error: Configuration file does not exist!\n");
		    exit(1);
		}
	
	/* Allocating memory for data from file. */
		servName = (char*)malloc(256 * sizeof(char));
		userName = (char*)malloc(256 * sizeof(char));
		userPassword = (char*)malloc(256 * sizeof(char));
		
	
	/* Getting connection info form file. */
		FILE* file = fopen("App.config", "r");
	
		char fileLine[256];
		char** configPackage;
	
		bool hostIs, portIs, userIs, passIs, timeIs, allIs;
		hostIs = portIs = userIs = passIs = timeIs = allIs = false;
		
		while (fgets(fileLine, sizeof(fileLine), file)) {
			configPackage = str_split(fileLine, ':');
		
			if (strcmp(configPackage[0], "HOST") == 0 && !hostIs) {
				memcpy(servName, configPackage[1], strlen(configPackage[1])-1);
				
				if (strlen(configPackage[1]) > 1) {
					hostIs = true;
				}

			} else if (strcmp(configPackage[0], "PORT") == 0 && !portIs) {
				echoServPort = atoi(configPackage[1]);
				if (echoServPort == 0) {
					echoServPort = 110;
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
			} else if (strcmp(configPackage[0], "TIME") == 0 && !timeIs) {
				sleepTime = atoi(configPackage[1]);
				if (sleepTime == 0) {
					sleepTime = 1;
					printf("Default update time: %ds\n", sleepTime);
				}
					
				timeIs = true;
			}
			
			for (int i = 0; *(configPackage + i); i++) { free(*(configPackage + i)); }
			free(configPackage);
			
			
			if (hostIs && portIs && userIs && passIs && timeIs) {
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
	

	
	time_t dateTime;
	
	/* Number of bytes from server response: */
	int num;
	/* Structure for complex info of socket: */
	struct sockaddr_in echoServAddr;
	/* Variable for request length: */
	unsigned int echoStringLen;
	
	/* Array for server response lines: */
	char** lines;
	/* Array for stat emails number: */
	char** packStat;
	/* All emails number when request STAT: */
	int emailsStat;
	/* Current server respone line: */
	char* line;
	/* Length of current server respone line: */
	int lineLength;
    /* Buffer for id string from server line: */
    char id[256];
    /* Buffer for stat emails number string: */
    char statEmails[20];
	/* New messages counter: */
	int newMsgCount;
	/* Response messages counter: */
	int responseMsgCount;
	/* Main loop iteration counter: */
	unsigned long int updated = 0;
	/* Connecting attempts counter: */
	unsigned long int attempts;
	/* Flag for information if email id is in array: */
	bool inArray;
	/* Flag for complete whole buffer: */
	bool endWhileIfBufferEnd;
	/* Current char - chcecking end of response: */
	char endChar;
	/* Additional pointer for concatenation: */
	char *concatenatePointer;
	
	


	/* Filling complex server address with data form file. */
		memset(&echoServAddr, 0, sizeof(echoServAddr));
		echoServAddr.sin_family = AF_INET;
		echoServAddr.sin_addr.s_addr = inet_addr(ip);
		echoServAddr.sin_port = htons(echoServPort);
	
	
	/* Allocating memory for ids of emails. */
		ids = (char **)malloc(sizeof(char*) * IDSMEMORY);
		
		for (int i = 0; i < IDSMEMORY; i++) {
			ids[i] = (char *)malloc(sizeof(char) * 128);
			memset(ids[i], 0, sizeof(char) * 128);
			
			if (*ids[i] != 0)
				printf("NOT NULL: %d!\n", *ids[i]);
		}
		
		bigBuffer = (char *)malloc(sizeof(char) * BIGBUFFLEN);
		
		allocated = true;
		
		
	/* Iteration pointers: */
    int i;
    int k;
    
    
    /* Main loop. */
    while(keepRunning) {
    	attempts = 0;
    	
    	/* Connecting to server with error handling. */
			while ((connection == -1) && keepRunning) {
				attempts+=1;
				
				sock = socket(AF_INET, SOCK_STREAM, 0);
				connection = connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr));
				num = recv(sock, echoBuffer, sizeof(echoBuffer),0);
				echoBuffer[num] = '\0';
			
				if (connection == -1) {
					printf("Connection error! \nNumber of connect attempts: %ld\n\n", attempts);
					close(sock);
					sleep(5);
				
				} else {
					break;
				}
			}
			
		if (attempts > 1 && connection != -1) {
			printf("Connection established!\n");
		}
		
		/* Break main loop if CTRL-C pressed while trying to reconnect. */
		if (connection == -1) {
			break;
		}
    
		/* POP3 authorization. */
			concatenatePointer = concatenateString(concatenatePointer, (char*)"USER ", userName);
			echoString = concatenateString(echoString, concatenatePointer, (char*)"\n");
			queryResponse(&sock, echoString, echoBuffer, false);
			free(echoString);
			free(concatenatePointer);
			if (echoBuffer[0] == '-') {
				fprintf(stderr, "Error: Incorrect username or password!\n");
				endConnection();
			}

			concatenatePointer = concatenateString(concatenatePointer, (char*)"PASS ", userPassword);
			echoString = concatenateString(echoString, concatenatePointer, (char*)"\n");
			queryResponse(&sock, echoString, echoBuffer, false);
			free(echoString);
			free(concatenatePointer);
			if (echoBuffer[0] == '-') {
				fprintf(stderr, "Error: Incorrect username or password!\n");
				endConnection();
			}
			
			echoString = (char *)"STAT\n";
			queryResponse(&sock, echoString, echoBuffer, false);
			memcpy(statEmails, &echoBuffer[4], strlen(echoBuffer)-3);
			statEmails[strlen(echoBuffer)-6] = '\0';
			packStat = str_split(statEmails, ' ');
			
			emailsStat = atoi(packStat[0]);
			
			for (i = 0; *(packStat + i); i++) { free(*(packStat + i)); }
			free(packStat);
			
	    
	    /* Request for list of emails with unique id. */
		echoString = (char *)"UIDL\n";
		
		newMsgCount = 0;
	    responseMsgCount = 0;
	    
	    /* Concatenation of response buffers. */
		bzero(bigBuffer, BIGBUFFLEN);
		endWhileIfBufferEnd = true;
	    while (endWhileIfBufferEnd) {
			queryResponse(&sock, echoString, echoBuffer, false);
			memcpy(&bigBuffer[strlen(bigBuffer)], echoBuffer, strlen(echoBuffer));
			
			lines = str_split(echoBuffer, '\n');
			

			for (i = 0; *(lines + i); i++) {
				line = *(lines + i);
				
				if (line[0] == '.') {
					endWhileIfBufferEnd = false;
				}
				
				free(*(lines + i)); 
			}
			free(lines);
		}
		

		
		/* Explode server response to array. */
		lines = str_split(bigBuffer, '\n');
		
		for (i = 0; *(lines + i); i++)
		{
			line = *(lines + i);
			
			if (line[0] != '.' && line[0] != '+' && line[0] != '-') {
				responseMsgCount += 1;
			
				lineLength = strlen(line);
				// printf("%s\n", line);
				
				/* Get only id */
				configPackage = str_split(line, ' ');
				memcpy(id, configPackage[1], strlen(configPackage[1]));
				id[strlen(configPackage[1])] = '\0';
				free(configPackage[0]);
				free(configPackage[1]);
				free(configPackage);
				// printf("%s\n", id);
				
				inArray = false;
				/* Checking if response email id is in array. */
				for (k=0; k<idsPointer+1; k++) {
					if (strcmp(id,ids[k]) == 0) {
						inArray = true;
						break;
					}
				}
			
				/* If current email id didn't exist in array */
				if (inArray == false) {
					newMsgCount++;
					
					if (updated>0) {
						receivedCounter += 1;
					}
				
					/* Put new id into ids array: */
					memcpy(ids[idsPointer], id, strlen(id));
					idsPointer++;
				}
			
			
	    	}
		    free(*(lines + i));
		}
		free(lines);
	    
	    

	    
	    /* Print information about new messages */
	    if (newMsgCount > 0) {
	    	time(& dateTime);
	    
	    	if (updated == 0) {
	    		allMsgOnRun = emailsStat;
	    		printf("Number of messages in your mailbox: %ld.\n==> Waiting for new messages...\n", allMsgOnRun);
	    		
    		}
	    	else if (newMsgCount == 1) {
	    		printf("\n%s", ctime(&dateTime));
	    		printf("==> You have got new message!\n");
    		}
    		else {
    			printf("\n%s", ctime(&dateTime));
    			printf("==> You have got %d new messages!\n", newMsgCount);
			}
    		
	    }

	    
		echoString = (char *)"QUIT\n";
		queryResponse(&sock, echoString, echoBuffer, false);
		close(sock);
		connection = -1;
		
		updated++;
		
		sleep(sleepTime);
	}

	
	exit(0);
}

/*
    End connection function
 */
void endConnection() {
	keepRunning = false;
	
	if (connection != -1) {
		echoString = (char *)"QUIT\n";
		queryResponse(&sock, echoString, echoBuffer, false);
		close(sock);
	}

	printf("Disconnected! Socket closed.\n\n");
	
	/* Print summary */
	printf("==> Total of received messages: %d.\n", receivedCounter);

	free(servName);
	free(userName);
	free(userPassword);

	/* Free ids memory & bigBuffer if allocated */
	if (allocated) {
		for (int i = 0; i < IDSMEMORY; i++) {
			free(ids[i]);
		}
		free(ids);
		
		free(bigBuffer);
	}
	
	
	exit(0);
}

/*
    CTRL-C pressed handler function
 */
void INThandler(int sig)
{
	char  c;

	signal(sig, SIG_IGN);
	printf("\nDo you really want to quit? [y/n] ");
	c = getchar();
	
	if (c == 'y' || c == 'Y') {
		endConnection();
	}
	else
		signal(SIGINT, INThandler);
		
	getchar();
}

/*
    Get ip from domain name
 */
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

/*
    Send query to server and get response
 */
void queryResponse(int *socket, char *query, char *response, bool printIt) {
	int bytesRcvd;
	
	write(*socket, query, strlen(query));
	
	bzero(response, RCVBUFSIZE);
	bytesRcvd = recv(*socket, response, RCVBUFSIZE - 1, MSG_CMSG_CLOEXEC);
	response[bytesRcvd] = '\0';
	
	
	if (printIt) {
		printf("%s", response);
	}
}

/*
    Like explode function in PHP (from Internet)
 */
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

/*
    Concatenate strings
 */
char * concatenateString(char *output, char *str1, char *str2) {
	output = (char *) malloc(1 + strlen(str1) + strlen(str2));
	strcpy(output, str1);
	strcat(output, str2);
	
	return output;
}
