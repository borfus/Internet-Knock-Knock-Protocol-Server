#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

void error(char *msg)
{
    fprintf(stderr, "%s %s\n", msg, strerror(errno));
    exit(1);
}

int catch_signal(int sig, void (*handler)(int))
{
    struct sigaction action;
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(sig, &action, NULL);
}

int open_listener_socket()
{
    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1)
    {
        error("Can't open socket");
    }
    return s;
}

void bind_to_port(int socket, int port)
{
    struct sockaddr_in name;
    name.sin_family = PF_INET;
    name.sin_port = (in_port_t)htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1)
    {
        error("Can't set the reuse option on the socket");
    }
    int c = bind(socket, (struct sockaddr *)&name, sizeof(name));
    if (c == -1)
    {
        error("Can't bind to socket");
    }
}

int say(int socket, char *s)
{
    int result = send(socket, s, strlen(s), 0);
    if (result == -1)
    {
        fprintf(stderr, "%s: %s\n", "Error talking to the client", strerror(errno));
    }
    return result;
}

int read_in(int socket, char *buf, int len)
{
    char *s = buf;
    int slen = len;
    int c = recv(socket, s, slen, 0);
    while ((c > 0) && (s[c-1] != '\n'))
    {
        s += c;
        slen -= c;
        c = recv(socket, s, slen, 0);
    }
    if (c < 0)
    {
        return c;
    }
    else if (c == 0)
    {
        buf[0] = '\0';
    }
    else
    {
        s[c - 1] = '\0';
    }
    return len - slen;
}

int listener_d;

void handle_shutdown(int sig)
{
    if (listener_d)
    {
        close(listener_d);
    }
    fprintf(stderr, "Bye!\n");
    exit(0);
}

char *conversation[] = 
{
    "Knock! Knock!\r\n\0",
    "who's there?\r\n\0",
    "Oscar\r\n\0",
    "Oscar who?\r\n\0",
    "oscar silly question, you get a silly answer\r\n\0"
};

int main()
{
    if (catch_signal(SIGINT, handle_shutdown) == -1)
    {
        error("Can't set the interrupt handler");
    }

    listener_d = open_listener_socket();
    bind_to_port(listener_d, 30000);
    if (listen(listener_d, 10) == -1)
    {
        error ("Can't listen");
    }
    puts("Waiting for connection");
    int connect_d;

    for(;;)
    {
        struct sockaddr_storage client_addr;
        unsigned int address_size = sizeof(client_addr);
        connect_d = accept(listener_d, (struct sockaddr *)&client_addr, &address_size);
        if (connect_d == -1)
        {
            error("Can't open secondary socket");
        }
        say(connect_d, "Internet Knock-Knock Protocol Server\r\nVersion 1.0\r\n");
        for(int i = 0; i < 5; i++)
        {
            if (i % 2 == 0)
            {
                say(connect_d, conversation[i]);
            }
            else
            {
                char buffer[255];
                read_in(connect_d, buffer, sizeof(buffer));

                if (buffer[0] == '\0')
                {
                    say(connect_d, "Error receiving message\n");
                    printf("%s\n", buffer);
                    i--;
                }
                if (strncasecmp("Who's there?", buffer, 12) != 0 && strncasecmp("Oscar who?", buffer, 10) != 0)
                {
                    say(connect_d, "You didn't say the right thing! Try again next time.\n");
                    printf("%s\n", buffer);
                    break;
                }
            }
        }
        close(connect_d);
    }   
    handle_shutdown(connect_d);
}
