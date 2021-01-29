/**
Code changes made: 
1. Use PF_INET instead of AF_INET as address family
2. Use SOCK_SEQPACKET instead of SOCK_STREAM as socket protocol
Compile : cc -o s blobserver.c -lsctp
Run: ./s


/* To enable socket features used for SCTP socket. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#define MY_PORT_NUM 21233

static void die(const char *s)
{
    perror(s);
    exit(1);
}

static void server(void)
{
    int listen_fd, conn_fd, flags, ret, in;
    struct sctp_sndrcvinfo sndrcvinfo;
    struct sctp_event_subscribe evnts;
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(MY_PORT_NUM);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);
    struct sctp_initmsg initmsg = {
        .sinit_num_ostreams = 5,
        .sinit_max_instreams = 5,
        .sinit_max_attempts = 4,
    };

    listen_fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);

    if (listen_fd < 0)
        die("socket");

    ret = bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0)
        die("bind");

    ret = listen(listen_fd, initmsg.sinit_max_instreams);
    if (ret < 0)
        die("listen");

    for (;;)
    {
        char buffer[1024];

        printf("Waiting for connection\n");

        in = sctp_recvmsg(listen_fd, buffer, sizeof(buffer), (struct sockaddr *)&client_addr, &clilen, 0, 0);
        printf("%d\n", in);
        if (in > 0)
        {
            printf("Received data: %s\n", buffer);
            fflush(stdout);
        }
    }
    close(listen_fd);
}

int main()
{
    server();
    return 0;
}
