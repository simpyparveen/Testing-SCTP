/*
https://github.com/goldenspider/sctp/blob/master/client.c
Code changes made: 
1. Use PF_INET instead of AF_INET as address family
2. Use SOCK_SEQPACKET instead of SOCK_STREAM as socket protocol

Compile : cc -o c blobserver.c -lsctp
Run: ./c


/* To enable socket features used for SCTP socket. */
#define __EXTENSIONS__

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/sctp.h>
#include <netdb.h>

#define BUFLEN 1024

static void
usage(char *a0)
{
    fprintf(stderr, "Usage: %s <addr>\n", a0);
}

/*
 * Read from the network.
 */
static void
readit(void *vfdp)
{
    int   fd;
    ssize_t   n;
    char   buf[BUFLEN];
    struct msghdr  msg[1];
    struct iovec  iov[1];
    struct cmsghdr  *cmsg;
    struct sctp_sndrcvinfo *sri;
    char   cbuf[sizeof (*cmsg) + sizeof (*sri)];
    union sctp_notification *snp;

    //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    fd = *(int *)vfdp;

    /* Initialize the message header for receiving */
    memset(msg, 0, sizeof (*msg));
    msg->msg_control = cbuf;
    msg->msg_controllen = sizeof (*cmsg) + sizeof (*sri);
    msg->msg_flags = 0;
    cmsg = (struct cmsghdr *)cbuf;
    sri = (struct sctp_sndrcvinfo *)(cmsg + 1);
    iov->iov_base = buf;
    iov->iov_len = BUFLEN;
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;

    while ((n = recvmsg(fd, msg, 0)) > 0) {
        /* Intercept notifications here */
        if (msg->msg_flags & MSG_NOTIFICATION) {
            snp = (union sctp_notification *)buf;
            printf("[ Receive notification type %u ]\n",
                snp->sn_header.sn_type);
            continue;
        }
        msg->msg_control = cbuf;
        msg->msg_controllen = sizeof (*cmsg) + sizeof (*sri);
        printf("[ Receive echo (%u bytes): stream = %hu, ssn = %hu, "
            "flags = %hx, ppid = %u ]\n", n,
            sri->sinfo_stream, sri->sinfo_ssn, sri->sinfo_flags,
            sri->sinfo_ppid);
    }

    if (n < 0) {
        perror("recv");
        exit(1);
    }

    close(fd);
    exit(0);
}

#define MAX_STREAM 64

static void
echo(struct sockaddr_in *addr)
{
    int     fd;
    unsigned char    buf[BUFLEN];
    ssize_t    n;
    int    perr;
    pthread_t   tid;
    struct cmsghdr   *cmsg;
    struct sctp_sndrcvinfo  *sri;
    char    cbuf[sizeof (*cmsg) + sizeof (*sri)];
    struct msghdr   msg[1];
    struct iovec   iov[1];
    int    ret;
    struct sctp_initmsg   initmsg;
    struct sctp_event_subscribe events;

    /* Create a one-one SCTP socket */
//    if ((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) {
    if ((sockfd = socket(PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) == -1) {
        perror("socket");
        exit(1);
    }

    /*
     * We are interested in association change events and we want
     * to get sctp_sndrcvinfo in each receive.
     */
    events.sctp_association_event = 1; 
    events.sctp_data_io_event = 1; 
    ret = setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &events,
        sizeof (events));
    if (ret < 0) {
        perror("setsockopt SCTP_EVENTS");
        exit(1);
    }

    /*
     * Set the SCTP stream parameters to tell the other side when
     * setting up the association.
     */
    memset(&initmsg, 0, sizeof(struct sctp_initmsg));
    initmsg.sinit_num_ostreams = 2;
    initmsg.sinit_max_instreams = 2;
    initmsg.sinit_max_attempts = 2;
    ret = setsockopt(fd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
        sizeof(struct sctp_initmsg));
    if (ret < 0) {
        perror("setsockopt SCTP_INITMSG");
        exit(1);
    }

    if (connect(fd, (struct sockaddr *)addr, sizeof (*addr)) == -1) {
        perror("connect");
        exit(1);
    }

    /* Initialize the message header structure for sending. */
    memset(msg, 0, sizeof (*msg));
    iov->iov_base = buf;
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;
    msg->msg_control = cbuf;
    msg->msg_controllen = sizeof (*cmsg) + sizeof (*sri);
    msg->msg_flags |= 1;

    memset(cbuf, 0, sizeof (*cmsg) + sizeof (*sri));
    cmsg = (struct cmsghdr *)cbuf;
    sri = (struct sctp_sndrcvinfo *)(cmsg + 1);

    cmsg->cmsg_len = sizeof (*cmsg) + sizeof (*sri);
    cmsg->cmsg_level = IPPROTO_SCTP;
    cmsg->cmsg_type  = SCTP_SNDRCV;

    sri->sinfo_ppid   = 1;
    /* Start sending to stream 0. */
    sri->sinfo_stream = 0;

    /* Create a thread to receive network traffic. 
    //perr = pthread_create(&tid, NULL, (void *(*)(void *))readit, &fd);

    if (perr != 0) {
        fprintf(stderr, "pthread_create: %d\n", perr);
        exit(1);
    }*/

    /* Read from stdin and then send to the echo server. */
    while ((n = read(fileno(stdin), buf, BUFLEN)) > 0) {
        iov->iov_len = n;
        if (sendmsg(fd, msg, 0) < 0) {
            perror("sendmsg");
            exit(1);
        }
        /* Send the next message to a different stream. */
        sri->sinfo_stream = (sri->sinfo_stream + 1) % MAX_STREAM;
    }

    //pthread_cancel(tid);
    close(fd);
}

int
main(int argc, char **argv)
{
  struct sockaddr_in servaddr;

  bzero( (void *)&servaddr, sizeof(servaddr) );
  servaddr.sin_family = PF_INET;
  servaddr.sin_port = htons(36412);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//( "10.62.48.97" );

    echo(&servaddr);

    return (0);
}
