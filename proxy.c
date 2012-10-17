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
void handler(int sig);
char* request_to_uri(char* request);
int handle_request(int socket);
int get_client_socket(int port, socklen_t *chilen, struct sockaddr_in *serv_addr, struct sockaddr_in *cli_adr);
void get_server_response(int server_socket, int client_socket);
int get_server_socket(char* host);
void trim(char * s);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc < 2) {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }
    
    int port = atoi(argv[1]);
    open_server(port);
    exit(0);
}

int open_server(int port)
{
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = get_client_socket(port, &clilen, &serv_addr, &cli_addr);
    listen(sockfd, 5);

    printf("Now listening on %d.\n", port);
    Signal(SIGCHLD, handler); // reap dead child processes
    
    while(1)
    {
        // accept request from client
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");

//        if (Fork() == 0)
        {
            //close(sockfd);
            printf("connection opened\n");
            handle_request(newsockfd);
            close(newsockfd);
        }
    }
    close(sockfd);
    return 0;
}

int handle_request(int socket)
{
    char buffer[MAXLINE];
    int n;
    rio_t rio;
    char line_1[MAXLINE];
    char line_2[MAXLINE];
    int server_socket;
    
    bzero(buffer, MAXLINE);

    Rio_readinitb(&rio, socket);
    n = Rio_readlineb(&rio, buffer, MAXLINE);
    memcpy(line_1, buffer, n);
    n = Rio_readlineb(&rio, buffer, MAXLINE);
    memcpy(line_2, buffer, n);

    char* host[n-6];
    strncpy(host, line_2+6, n-6); // +6 to remove leading 'Host: ', ends in 2 spaces
    trim(host);

    printf("%d -- %s", strlen(host), host);
    server_socket = Open_clientfd(host, 80);
    
    Rio_writen(server_socket, line_1, sizeof(line_1));
    Rio_writen(server_socket, line_2, sizeof(line_2));

    printf("Request:\n%s%s", line_1, line_2);
    while((n = Rio_readlineb(&rio, buffer, MAXLINE)) != 0)
    {
        printf("%s", buffer);
        if(n == 0)
            break;
        Rio_writen(server_socket, buffer, n);
    }
    
    get_server_response(server_socket, socket);

    close(server_socket);

    return 0;
}

void trim(char * s) 
{
    char * p = s;
    int l = strlen(p);

    while(isspace(p[l - 1])) p[--l] = 0;
    while(* p && isspace(* p)) ++p, --l;

    memmove(s, p, l + 1);
}

int get_server_socket(char* host)
{
    int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, "http", &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
        {
            perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("connect");
            continue;
        }
        break;
    }
    if (p == NULL) 
    {
        fprintf(stderr, "failed to connect\n");
        exit(2);
    }
    freeaddrinfo(servinfo); 
    return sockfd;
}

int get_client_socket(int port, socklen_t *clilen, struct sockaddr_in *serv_addr, struct sockaddr_in *cli_addr)
{
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    bzero((char *) serv_addr, sizeof(*serv_addr));

    (*serv_addr).sin_family = PF_INET;
    (*serv_addr).sin_addr.s_addr = INADDR_ANY;
    (*serv_addr).sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) serv_addr, sizeof(*serv_addr)) < 0) 
        error("ERROR on binding");

    (*clilen) = sizeof((*cli_addr));

    return sockfd;
}

void get_server_response(int server_socket, int client_socket)
{
    int n;
    rio_t rio;
    Rio_readinitb(&rio, server_socket);
    char buffer[MAXLINE];

    printf("Response:\n");
    while((n = Rio_readlineb(&rio, buffer, MAXLINE)) != 0)
    {
        printf("%s", buffer);
        Rio_writen(client_socket, buffer, n);
        //if(strlen(buffer) <= 2)
        //    break;
    }
}

char* request_to_uri(char* request)
{
    char request_copy[MAXLINE];
    memcpy(request_copy, request, MAXLINE);
    
    char *token = NULL;
    char *uri;
    token = strtok(request_copy, " ");
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


