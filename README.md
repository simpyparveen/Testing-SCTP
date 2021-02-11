# Testing-SCTP
Testing SCTP for one-to-many

The features of SCTP socket APIs include consistency, accessibility, and compatibility.
SCTP Socket APIs were designed to provide the following features:

    Maintain consistency with existing socket APIs
    Provide a base for access to new SCTP Features
    Provide compatibility so that most existing TCP and UDP applications can be migrated to SCTP with few changes

In order to facilitate easy migration of existing TCP and UDP applications, two different styles of SCTP APIs have been formulated:

    UDP-Style API – Semantics are similar to that defined for connectionless protocols like UDP
    TCP-Style API – Semantics are similar to that defined for connection-oriented protocols like TCP

Though SCTP allows for both TCP and UDP style of socket APIs to be defined and used, in AIX® 5.3, only support for UDP-style socket syntax is provided because the UDP-style API provides more flexibility in accessing new features of SCTP. Using the UDP-style API a typical server uses the following sequence of calls during the lifetime of an association.

    socket()
    bind()
    listen()
    recvmsg()
    sendmsg()
    close()

A typical client uses the following sequence of socket API calls:

    socket()
    sendmsg()
    recvmsg()
    close()

Application USAGE: Client sends data using stdin and server recieves each message on different streams.
Compile/Run slient and server. The server is connected until client finishes sending all streams(limits can be modified).



# Theory Ref: https://www.ibm.com/support/knowledgecenter/ssw_aix_71/network/sctp_socketapis.html
