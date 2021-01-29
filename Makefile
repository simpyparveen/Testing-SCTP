all:
	gcc blobserver.c -lsctp -o server
	gcc blobclient.c -lsctp -o client
