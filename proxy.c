/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Jouke van der Maas, 10186883, jouke.vandermaas@gmail.com 
 *     Marysia Winkels, 10163727, marysia@live.nl
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void error(const char *msg);
int open_server(int port);
int client_request(char* request, int exsock);
void handler(int sig);
char* request_to_uri(char* request);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    //int clientfd, port; 
    //char *host, buf[MAXLINE];
    //rio_t rio; 
    //
    /* Check arguments */
    if (argc < 2) {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }
    
    int port = atoi(argv[1]);
    //
    //clientfd = Open_clientfd(host, port);
    //Rio_readinitb(&rio, clientfd);
    //
    //while(Fgets(buf, MAXLINE, stdin) != NULL) {
      //Rio_writen(clientfd, buf, strlen(buf));
      //Rio_readlineb(&rio, buf, MAXLINE);
      //Fputs(buf, stdout);
    //}
    //Close(clientfd);
    open_server(port);
    exit(0);
}

int open_server(int port)
{
     int sockfd, newsockfd;
     socklen_t clilen;
     char buffer[MAXLINE];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");

     bzero((char *) &serv_addr, sizeof(serv_addr));

     serv_addr.sin_family = PF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(port);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
        sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

     listen(sockfd,5);
     printf("Now listening on %d.\n", port);
     Signal(SIGCHLD, handler);
     while(1)
     {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, 
                    (struct sockaddr *) &cli_addr, 
                    &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");
        printf("Incoming request:\n");
        if (Fork() == 0)
        {
            close(sockfd);
            bzero(buffer,MAXLINE);

            n = recv(newsockfd, buffer, MAXLINE, 0);
            if (n < 0) error("ERROR reading from socket");
            printf("%s\n\n", buffer);
            //send(newsockfd, "Hello!", 6, 0);

            client_request(buffer, newsockfd);

            //n = send(newsockfd,response,MAXLINE, 0);
            //if (n < 0) error("ERROR writing to socket");
            close(newsockfd);
            exit(0);
        }
    }
    close(sockfd);
    return 0;
}

int client_request(char* request, int exsock)
{
    int sockfd, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    char response[MAXLINE];

    char hostname[MAXLINE];
    char pathname[MAXLINE];
    int port = 80;
    bzero(hostname, MAXLINE);
    bzero(pathname, MAXLINE);

    char request2[MAXLINE];
    memcpy(request2, request, MAXLINE);
    char* uri = request_to_uri(request2);
    if(parse_uri(uri, hostname, pathname, &port)<0)
        return -1;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL)     
        error("ERROR: no such host");

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    bzero(response,MAXLINE);

    n = send(sockfd,request,MAXLINE, 0);
    
    if (n < 0) 
         error("ERROR writing to socket");
    
    bzero(response,MAXLINE);

    n = 0;
    while((n = recv(sockfd,response, MAXLINE, 0)) > 0)
    {
        if (n < 0) 
            error("ERROR reading from socket");
        printf("Response:\n%s", response);
        send(exsock, response, MAXLINE, 0);
    };
    printf("\n\n");

    return 0;
}

char* request_to_uri(char* request)
{
    
    char *token = NULL;
    char *uri;
    token = strtok(request, " ");
    while (token) {
        if(strncasecmp(token, "http://", 7) == 0)
        {
            uri = token;
            break;
        }
        token = strtok(NULL, " ");
    }
    return uri;
}

void handler(int sig)
{
    pid_t pid;
    int stat;
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        ;
    return;
}

void error(const char *msg)
{
    fprintf(stderr, msg);
    exit(1);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}


