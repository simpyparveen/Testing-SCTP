/*

Code changes made for UDP stlye: 
1. Use PF_INET instead of AF_INET as address family
2. Use SOCK_SEQPACKET instead of SOCK_STREAM as socket protocol

Compile : cc -o c blobclient.c -lsctp
Run: ./c 127.0.0.1


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
static void client(void)
{
    int conn_fd, ret;
    const char *msg = "Hello, Server!";
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(MY_PORT_NUM);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    conn_fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
    if (conn_fd < 0)
        die("socket()");

    struct sctp_initmsg initmsg = {
        .sinit_num_ostreams = 5,
        .sinit_max_instreams = 5,
        .sinit_max_attempts = 4,
    };

    setsockopt(conn_fd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
    int i;
    for (i = 0; i < 4; i++)
    {
        printf("before sending\n");
        ret = sctp_sendmsg(conn_fd, (void *)msg, strlen(msg) + 1, (struct sockaddr *)&servaddr, sizeof(struct sockaddr), 0, 0, 0, 0, 0);
        printf("after sending\n");
        if (ret < 0)
            printf("failed to send %d\n", i);
        else
            printf("success %d\n", i);
    }
    close(conn_fd);
}

int main()
{
    client();
}
