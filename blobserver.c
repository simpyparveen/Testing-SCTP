/**
Code changes made: 
1. Use PF_INET instead of AF_INET as address family
2. Use SOCK_SEQPACKET instead of SOCK_STREAM as socket protocol
Compile : cc -o s blobserver.c -lsctp
Run: ./s

Ref : https://github.com/goldenspider/sctp/blob/master/server.c
/* To enable socket features used for SCTP socket. */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/sctp.h>

#define BUFLEN 1024

/*
 * Given an event notification, print out what it is.
 */
static void
handle_event(void *buf)
{
    struct sctp_assoc_change *sac;
    struct sctp_send_failed  *ssf;
    struct sctp_paddr_change *spc;
    struct sctp_remote_error *sre;
    union sctp_notification  *snp;
    char    addrbuf[INET6_ADDRSTRLEN];
    const char   *ap;
    struct sockaddr_in  *sin;
    struct sockaddr_in6  *sin6;

    snp = buf;

    switch (snp->sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
        sac = &snp->sn_assoc_change;
        printf("^^^ assoc_change: state=%hu, error=%hu, instr=%hu "
            "outstr=%hu\n", sac->sac_state, sac->sac_error,
            sac->sac_inbound_streams, sac->sac_outbound_streams);
        break;
    case SCTP_SEND_FAILED:
        ssf = &snp->sn_send_failed;
        printf("^^^ sendfailed: len=%hu err=%d\n", ssf->ssf_length,
            ssf->ssf_error);
        break;
    case SCTP_PEER_ADDR_CHANGE:
        spc = &snp->sn_paddr_change;
        if (spc->spc_aaddr.ss_family == PF_INET) {
            sin = (struct sockaddr_in *)&spc->spc_aaddr;
            ap = inet_ntop(PF_INET, &sin->sin_addr, addrbuf,
                INET6_ADDRSTRLEN);
        } else {
            sin6 = (struct sockaddr_in6 *)&spc->spc_aaddr;
            ap = inet_ntop(PF_INET6, &sin6->sin6_addr, addrbuf,
                INET6_ADDRSTRLEN);
        }
        printf("^^^ intf_change: %s state=%d, error=%d\n", ap,
            spc->spc_state, spc->spc_error);
        break;
    case SCTP_REMOTE_ERROR:
        sre = &snp->sn_remote_error;
        printf("^^^ remote_error: err=%hu len=%hu\n",
            ntohs(sre->sre_error), ntohs(sre->sre_length));
        break;
    case SCTP_SHUTDOWN_EVENT:
        printf("^^^ shutdown event\n");
        break;
    default:
        printf("unknown type: %hu\n", snp->sn_header.sn_type);
        break;
    }
}

/*
 * Receive a message from the network.
 */
static void *
getmsg(int fd, struct msghdr *msg, void *buf, size_t *buflen,
    ssize_t *nrp, size_t cmsglen)
{
    ssize_t  nr = 0;
    struct iovec iov[1];

    *nrp = 0;
    iov->iov_base = buf;
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;

    /* Loop until a whole message is received. */
    for (;;) {
        msg->msg_flags = 1;
        msg->msg_iov->iov_len = *buflen;
        msg->msg_controllen = cmsglen;

        nr += recvmsg(fd, msg, 0);
        if (nr <= 0) {
            /* EOF or error */
        	printf("EOF!!!!\n");
            *nrp = nr;
            return (NULL);
        }

        /* Whole message is received, return it. */
        if (msg->msg_flags & MSG_EOR) {
            *nrp = nr;
            printf("end ?\n");
            return (buf);
        }

        /* Maybe we need a bigger buffer, do realloc(). */
        if (*buflen == nr) {
            buf = realloc(buf, *buflen * 2);
            if (buf == 0) {
                fprintf(stderr, "out of memory\n");
                exit(1);
            }
            *buflen *= 2;
        }

        /* Set the next read offset */
        iov->iov_base = (char *)buf + nr;
        iov->iov_len = *buflen - nr;

    }
}

/*
 * The echo server.
 */
static void
echo(int fd)
{
    ssize_t   nr;
    struct sctp_sndrcvinfo *sri;
    struct msghdr  msg[1];
    struct cmsghdr  *cmsg;
    char   cbuf[sizeof (*cmsg) + sizeof (*sri)];
    char   *buf;
    size_t   buflen;
    struct iovec  iov[1];
    size_t   cmsglen = sizeof (*cmsg) + sizeof (*sri);
    printf("sctp_sndrcvinfo sizeof=%d cmsghdr sizeof=%d\n",sizeof (*sri),sizeof (*cmsg));
    /* Allocate the initial data buffer */
    buflen = BUFLEN;
    if ((buf = malloc(BUFLEN)) == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    /* Set up the msghdr structure for receiving */
    memset(msg, 0, sizeof (*msg));
    msg->msg_control = cbuf;
    msg->msg_controllen = cmsglen;
    msg->msg_flags = 0;
    cmsg = (struct cmsghdr *)cbuf;
    sri = (struct sctp_sndrcvinfo *)(cmsg + 1);

    /* Wait for something to echo */
    while ((buf = getmsg(fd, msg, buf, &buflen, &nr, cmsglen)) != NULL) {

        /* Intercept notifications here */
        if (msg->msg_flags & MSG_NOTIFICATION) {
            handle_event(buf);
            continue;
        }

        iov->iov_base = buf;
        msg->msg_iov = iov;
        msg->msg_iovlen = 1;
        iov->iov_len = nr;
        msg->msg_control = cbuf;
        msg->msg_controllen = sizeof (*cmsg) + sizeof (*sri);

        printf("got %u bytes on stream %hu:\n", nr,
               sri->sinfo_stream);
        write(0, buf, nr);

        /* Echo it back */
        //msg->msg_flags = 1;
        if (sendmsg(fd, msg, 0) < 0) {
            perror("sendmsg");
            exit(1);
        }
    }

    if (nr < 0) {
        perror("recvmsg");
    }
    close(fd);
}

int main(void)
{
    int    lfd;
    int    cfd;
    int    onoff = 1;
    struct sockaddr_in  sin[1];
    struct sctp_event_subscribe events;
    struct sctp_initmsg  initmsg;

//    if ((lfd = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP)) == -1) {
    if ((lfd = socket(PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP)) == -1) {
        perror("socket");
        exit(1);
    }

    sin->sin_family = PF_INET;
    sin->sin_port = htons(36412);
    sin->sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (struct sockaddr *)sin, sizeof (*sin)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(lfd, 1) == -1) {
        perror("listen");
        exit(1);
    }

    (void) memset(&initmsg, 0, sizeof(struct sctp_initmsg));
    initmsg.sinit_num_ostreams = 2;
    initmsg.sinit_max_instreams = 2;
    initmsg.sinit_max_attempts = 2;
    if (setsockopt(lfd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
           sizeof(struct sctp_initmsg)) < 0) { 
        perror("SCTP_INITMSG");
        exit (1);
    }

    /* Events to be notified for */
    (void) memset(&events, 0, sizeof (events));
    events.sctp_data_io_event = 1;
    events.sctp_association_event = 1;
    events.sctp_send_failure_event = 1;
    events.sctp_address_event = 1;
    events.sctp_peer_error_event = 1;
    events.sctp_shutdown_event = 1;

    /* Wait for new associations */
    for (;;) {
        if ((cfd = accept(lfd, NULL, 0)) == -1) {
            perror("accept");
            exit(1);
        }

        /* Enable ancillary data */
        if (setsockopt(cfd, IPPROTO_SCTP, SCTP_EVENTS, &events,
               sizeof (events)) < 0) {
            perror("setsockopt SCTP_EVENTS");
            exit(1);
        }
        /* Echo back any and all data */
        echo(cfd);
    }
}
