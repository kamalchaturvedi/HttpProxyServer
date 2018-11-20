/*
 * ProxyServer.c
 *
 *  Created on: Nov 14, 2018
 *      Author: kamalchaturvedi15
 */

#include <asm-generic/socket.h>
#include <bits/types/FILE.h>
#include <bits/types/struct_timeval.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include<time.h>
#include <unistd.h>
#include <openssl/md5.h>
#include<dirent.h>

#define BUFSIZE 2048
#define SERVERSIZE 100
#define PORTSIZE 10
#define HASHVALUESIZE 33
#define NONE (fd_set *) NULL
int checkError(int, char *, int*);
unsigned long fileSize(char *);
void handleClientRequest(int, int, int);

char baseCacheDirectory[] = "./cache/";
char hostIpCacheInfoDirectory[] = "./hostIpMapping.txt";
char blockedHostInfoDirectory[] = "./blockedHostInfo.txt";
char fileNotFoundResponse[] =
		"HTTP/1.0 404 Not Found\r\n"
				"<html><head><title>404 Not Found</head></title>\r\n"
				"<body><p>404 Not Found: The requested resource could not be found!</p></body></html>\r\n";

char internalServerErrorResponse[] =
		"HTTP/1.0 500 Internal Server Error\r\n"
				"<html><head><title>500 Internal Server Error</head></title><body><p>\r\n"
				"These messages in the exact format as shown above should be sent back to the client if any of the above error occurs. "
				"</p></body></html>\r\n";

char badErrorResponse[] =
		"HTTP/1.0 400 Bad Request\r\n"
				"<html><head><title>400 Bad Request</head></title><body><p>\r\n"
				"These messages in the exact format as shown above should be sent back to the client if any of the above error occurs. "
				"</p></body></html>\r\n";

char forbiddenResponse[] =
		"HTTP/1.1 403 Forbidden\r\n"
				"<html><head><title>403 Forbidden</head></title><body><p>\r\n"
				"These messages in the exact format as shown above should be sent back to the client if any of the above error occurs. "
				"</p></body></html>\r\n";

void sendCachedFileToClient(int fd_client, char* requestHash) {
	char dirName[HASHVALUESIZE + sizeof(baseCacheDirectory)];
	int fileSizeRead;
	strcpy(dirName, baseCacheDirectory);
	strcat(dirName, requestHash);
	char buf[2048];
	FILE *openingFileForReading;
	openingFileForReading = fopen(dirName, "r");
	if (openingFileForReading == NULL) {
		printf("Error in opening file");
	} else {
		while ((fileSizeRead = fread(buf, 1, BUFSIZE, openingFileForReading))
				> 0) {
			if (write(fd_client, buf, fileSizeRead) < fileSizeRead)
				break;
		}
	}
	fclose(openingFileForReading);
	printf("Successfully sent cached file");
}

char * cachedHostNameExists(char *hostname) {
	FILE *openingFileForReading;
	char line[256];
	char *linePointer, *tempPointer;
	openingFileForReading = fopen(hostIpCacheInfoDirectory, "r");
	if (openingFileForReading == NULL) {
		printf("Error in opening HostCacheInfo file");
	} else {
		while (fgets(line, sizeof(line), openingFileForReading)) {
			linePointer = line;
			tempPointer = linePointer;
			while (strncmp(tempPointer, " ", 1) != 0)
				tempPointer += 1;
			if (strncmp(linePointer, hostname, tempPointer - linePointer)
					== 0) {
				char *serverIp;
				linePointer = tempPointer + 1;
				tempPointer = linePointer;
				while (strncmp(tempPointer, "\n", 1) != 0)
					tempPointer += 1;
				serverIp = malloc(tempPointer - linePointer);
				strncpy(serverIp, linePointer, tempPointer - linePointer);
				printf("\nFound cached IP host mapping\n");
				return serverIp;
			}
			bzero(line, 256);
		}
		fclose(openingFileForReading);
	}
	return NULL;
}

int isHostBlocked(char *hostname) {
	FILE *openingFileForReading;
	char line[256];
	int isHostBlockedFlag = 0;
	openingFileForReading = fopen(blockedHostInfoDirectory, "r");
	if (openingFileForReading == NULL) {
		printf("Error in opening BlockedHostInfo file");
	} else {
		while (fgets(line, sizeof(line), openingFileForReading)) {
			if (strncmp(line, hostname, strlen(hostname)) == 0) {
				isHostBlockedFlag = 1;
				break;
			}
		}
	}
	fclose(openingFileForReading);
	return isHostBlockedFlag;
}

void addHostNameToCache(char *hostname, char *serverIp) {
	FILE *openingFileForWriting;
	int dataWritten;
	char buf[sizeof(hostname) + 4 + strlen(serverIp)];
	strcpy(buf, hostname);
	strcat(buf, " ");
	strcat(buf, serverIp);
	strcat(buf, "\n");
	openingFileForWriting = fopen(hostIpCacheInfoDirectory, "ab");
	if (openingFileForWriting == NULL) {
		printf("Error in opening file");
	} else {
		dataWritten = fwrite(buf, 1, strlen(buf), openingFileForWriting);
		fflush(openingFileForWriting);
		if (dataWritten < 0) {
			printf("Cannot write to file");
		}
		fclose(openingFileForWriting);
	}
}
void relayDataFromClientToServerAndViceVersa(struct timeval* tv, fd_set* rfds,
		int server_sock, int fd_client, char* requestHash) {
	int serverResp, dataWritten;
	char dirName[HASHVALUESIZE + sizeof(baseCacheDirectory)];
	strcpy(dirName, baseCacheDirectory);
	strcat(dirName, requestHash);
	char buf[2048];
	if (access(dirName, F_OK) != -1) {
		// old cache exists, hence deleting that file
		remove(dirName);
	}
	FILE *openingFileForWriting;
	openingFileForWriting = fopen(dirName, "ab");

	for (;;) {
		tv->tv_sec = 60;
		tv->tv_usec = 0;
		FD_SET(server_sock, rfds);
		FD_SET(fd_client, rfds);
		if (select(FD_SETSIZE, rfds, NONE, NONE, &*tv) < 0) {
			break;
		}
		if (FD_ISSET(server_sock, rfds)) {
			/* data coming in */
			printf("*");
			memset(buf, '\0', strlen(buf));
			if ((serverResp = read(server_sock, buf, BUFSIZE)) < 1)
				break;
			/*printf("%s", buf);*/
			fflush(stdout);
			if (openingFileForWriting == NULL) {
				printf("Error in opening file");
			} else {
				dataWritten = fwrite(buf, 1, serverResp, openingFileForWriting);
				fflush(openingFileForWriting);
				if (dataWritten < 0) {
					printf("Cannot write to file");
				}
			}
			if (write(fd_client, buf, serverResp) < serverResp)
				break;
		} else {
			if ((serverResp = read(fd_client, buf, sizeof(buf))) < 1)
				break;

			if (write(server_sock, buf, strlen(buf)) < serverResp)
				break;
		}
	}
	if (openingFileForWriting)
		fclose(openingFileForWriting);
}

int setupConnectionWithServerAndSendClientRequest(
		struct sockaddr_in* serveraddr, int portNo, int on,
		char clientRequest[BUFSIZE], char *serverIp) {
	int server_sock;
	bzero((char*) serveraddr, sizeof(*serveraddr));
	serveraddr->sin_family = AF_INET;
	serveraddr->sin_addr.s_addr = inet_addr(serverIp);
	/*bcopy((char*) server->h_addr, (char*)&serveraddr->sin_addr.s_addr, server->h_length);*/
	serveraddr->sin_port = htons(portNo);
	// connect to the server and relay all data back to the client
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &on, 4);
	if (connect(server_sock, (struct sockaddr*) &*serveraddr,
			sizeof(struct sockaddr)) != 0) {
		printf("connection with the server failed...\n");
		exit(0);
	} else
		printf("connected to the server..\n");

	if (write(server_sock, clientRequest, BUFSIZE) < 0) {
		exit(5);
	}
	return server_sock;
}

char * getHashValueOfFileRequested(char *fileRequestedWithHostName) {
	printf("%s", fileRequestedWithHostName);
	MD5_CTX context;
	int requestLength = strlen(fileRequestedWithHostName);
	unsigned char digest[16];
	char *output = (char *) malloc(33);
	MD5_Init(&context);
	while (requestLength > 0) {
		if (requestLength > 512) {
			MD5_Update(&context, fileRequestedWithHostName, 512);
		} else {
			MD5_Update(&context, fileRequestedWithHostName, requestLength);
		}
		requestLength -= 512;
		fileRequestedWithHostName += 512;
	}
	MD5_Final(digest, &context);

	for (int n = 0; n < 16; ++n) {
		snprintf(&(output[n * 2]), 16 * 2, "%02x", (unsigned int) digest[n]);
	}
	return output;
}

int checkIfCacheAlreadyExistsForFile(char *hashValue, int timeout) {
	DIR *pDir;
	char dirName[HASHVALUESIZE + sizeof(baseCacheDirectory)];
	strcpy(dirName, baseCacheDirectory);
	strcat(dirName, hashValue);
	struct dirent *entry;
	pDir = opendir(baseCacheDirectory);
	if (pDir == NULL) {
		printf("Cannot open parent directory");
	} else {
		while ((entry = readdir(pDir)) != NULL) {
			if (strcmp(entry->d_name, hashValue) == 0) {
				struct stat attrib;
				stat(dirName, &attrib);
				time_t fileLastModifiedTime = (time_t) attrib.st_mtim.tv_sec;
				time_t currentTime;
				time(&currentTime);
				int secondsDiff = difftime(currentTime, fileLastModifiedTime);
				if (secondsDiff <= timeout)
					return 1;
				else {
					printf("\nCached file found, but timedout\n");
					return 0;
				}
			}
		}
	}
	return 0;
}
void handleClientRequest(int fd_proxy, int fd_client, int timeout) {
	char *ptr, requestVersionType[10], serverRequested[SERVERSIZE], *tempPtr,
			portNumber[PORTSIZE];
	int portNo, on = 1;
	char *requestHash;
	struct timeval tv;
	fd_set rfds;
	char * server;
	struct sockaddr_in serveraddr;
	int server_sock;
	char clientRequest[BUFSIZE];
	close(fd_proxy);
	memset(clientRequest, 0, BUFSIZE);
	read(fd_client, clientRequest, 2047);
	char *buf = malloc(strlen(clientRequest));
	strcpy(buf, clientRequest);
	ptr = strstr(buf, " HTTP/1.1");
	if (ptr != NULL)
		sprintf(requestVersionType, " HTTP/1.1");
	else {
		ptr = strstr(buf, " HTTP/1.0");
		sprintf(requestVersionType, " HTTP/1.0");
	}
	if (ptr == NULL) {
		printf("Not an HTTP request\n");
		bzero(buf, BUFSIZE);
		sprintf(buf, "%s", internalServerErrorResponse);
		send(fd_client, buf, strlen(buf), 0);
	} else {
		*ptr = 0;
		if (strncmp(buf, "GET ", 4) != 0) {
			printf("Unsupported HTTP request\n");
			bzero(buf, BUFSIZE);
			sprintf(buf, "%s", badErrorResponse);
			send(fd_client, buf, strlen(buf), 0);
		} else {
			/*printf("%s", buf);*/
			ptr = buf + 4;
			while ((strncmp(ptr, "//", 2) != 0)) {
				ptr = ptr + 1;
			}
			ptr = ptr + 2;
			tempPtr = ptr;
			while ((strncmp(tempPtr, "/", 1) != 0)
					&& (strncmp(tempPtr, ":", 1) != 0)) {
				tempPtr = tempPtr + 1;
			}
			bzero(serverRequested, SERVERSIZE);
			bzero(portNumber, PORTSIZE);
			if (strncmp(tempPtr, ":", 1) == 0) {
				strncpy(serverRequested, ptr, tempPtr - ptr);
				ptr = 1 + tempPtr;
				while (strncmp(tempPtr, "/", 1) != 0) {
					tempPtr = tempPtr + 1;
				}
				strncpy(portNumber, ptr, tempPtr - ptr);
				sscanf(portNumber, "%d", &portNo);
			} else {
				portNo = 80;
				strncpy(serverRequested, ptr, tempPtr - ptr);
			}

			if (ptr[strlen(ptr) - 1] == '/') {
				strcat(ptr, "index.html");
			}
			printf("\n%s\n", serverRequested);
			if (isHostBlocked(serverRequested)) {
				bzero(buf, BUFSIZE);
				sprintf(buf, "%s", forbiddenResponse);
				send(fd_client, buf, strlen(buf), 0);
				return;
			}
			server = cachedHostNameExists(serverRequested);
			if (server == NULL) {
				server = malloc(INET_ADDRSTRLEN);
				bzero(server, INET_ADDRSTRLEN);
				//server = gethostbyname(serverRequested);
				struct hostent *host_entry;
				host_entry = gethostbyname(serverRequested);
				inet_ntop(AF_INET, (void *) host_entry->h_addr_list[0], server,
				INET_ADDRSTRLEN);
				/*
				 server = inet_ntoa(host_entry->h_addr_list[0]);*/
				addHostNameToCache(serverRequested, server);
			}
			if (server == NULL) {
				printf("Requested server not found");
				bzero(buf, BUFSIZE);
				sprintf(buf, "%s", badErrorResponse);
				send(fd_client, buf, strlen(buf), 0);
			} else {
				requestHash = getHashValueOfFileRequested(ptr);
				if (checkIfCacheAlreadyExistsForFile(requestHash, timeout)
						== 0) {
					server_sock = setupConnectionWithServerAndSendClientRequest(
							&serveraddr, portNo, on, clientRequest, server);
					// now transfer everything
					relayDataFromClientToServerAndViceVersa(&tv, &rfds,
							server_sock, fd_client, requestHash);
					printf("\nClosing connection to server\n");
					close(server_sock);
				} else {
					printf("\nRequested file already exists in cache\n");
					sendCachedFileToClient(fd_client, requestHash);
				}
			}
		}
	}
}

int main(int argc, char **argv) {
	struct sockaddr_in proxy_addr, client_addr;
	socklen_t client_sock_len = sizeof(client_addr);
	int fd_proxy, fd_client;
	int timeOut;
	char buf[2048];
	int opt = 1, portNo, pid;
	if (argc != 3) {
		fprintf(stderr, "usage: %s <port> <timeout>\n", argv[0]);
		exit(1);
	}
	portNo = atoi(argv[1]);
	timeOut = atoi(argv[2]);
	fd_proxy = checkError(socket(AF_INET, SOCK_STREAM, 0),
			"Could not establish socket for the server", &fd_proxy);
	setsockopt(fd_proxy, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	checkError(fcntl(fd_proxy, F_GETFL),
			"Could not get the flags on the TCP socket", &fd_proxy);
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = INADDR_ANY;
	proxy_addr.sin_port = htons(portNo);
	checkError(
			bind(fd_proxy, (struct sockaddr *) &proxy_addr, sizeof(proxy_addr)),
			"Could not bind server\n", &fd_proxy);
	checkError(listen(fd_proxy, 10), "Could not listen to new connections\n",
			&fd_proxy);
	while (1) {
		/* Block till we have an open connection in the queue */
		fd_client = accept(fd_proxy, (struct sockaddr *) &client_addr,
				&client_sock_len);
		if (fd_client == -1) {
			perror("Error accepting connection with client\n");
			exit(1);
		} else {
			printf("Connected to client ... \n");
			pid = fork();
			//pid = 0;
			if (pid < 0) {
				perror("ERROR on fork");
				bzero(buf, BUFSIZE);
				sprintf(buf, "%s", internalServerErrorResponse);
				send(fd_client, buf, strlen(buf), 0);
				exit(1);
			}
			if (pid == 0) {
				handleClientRequest(fd_proxy, fd_client, timeOut);
				close(fd_client);
				exit(0);
			}
		}
		if (pid > 0) {
			close(fd_client);
			waitpid(0, NULL, WNOHANG);
		}
	}
	close(fd_proxy);
	return 0;
}

/*Method to check for errors if any*/
int checkError(int id, char * errorMessage, int * socket) {
	if (id == -1) {
		perror(errorMessage);
		close(*socket);
		exit(1);
	}
	return id;
}

/*Method to get size of a file*/
unsigned long fileSize(char *fileName) {
	FILE * f = fopen(fileName, "r");
	fseek(f, 0, SEEK_END);
	unsigned long length = (unsigned long) ftell(f);
	fclose(f);
	return length;
}
