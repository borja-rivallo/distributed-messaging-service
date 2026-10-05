CC = gcc
CFLAGS = -Wall -Wextra -I. -I/usr/include/tirpc -g
LDFLAGS = -ltirpc -lpthread

all: rpc server rpc_server

rpc:
	rpcgen -NM servidor_registro.x

servidor_registro_clnt.o: servidor_registro_clnt.c servidor_registro.h
	$(CC) $(CFLAGS) -c servidor_registro_clnt.c

servidor_registro_svc.o: servidor_registro_svc.c servidor_registro.h
	$(CC) $(CFLAGS) -c servidor_registro_svc.c

servidor_registro_xdr.o: servidor_registro_xdr.c servidor_registro.h
	$(CC) $(CFLAGS) -c servidor_registro_xdr.c

server: rpc server.o servicio_mensajeria.o servidor_registro_clnt.o servidor_registro_xdr.o
	$(CC) $(CFLAGS) -o server server.o servicio_mensajeria.o servidor_registro_clnt.o servidor_registro_xdr.o $(LDFLAGS)

rpc_server: rpc servidor_registro_server.o servidor_registro_svc.o servidor_registro_xdr.o
	$(CC) $(CFLAGS) -o rpc_server servidor_registro_server.o servidor_registro_svc.o servidor_registro_xdr.o $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f server rpc_server *.o servidor_registro_clnt.c servidor_registro_svc.c servidor_registro_xdr.c servidor_registro.h servidor_registro_client.c

setup:
	apt-get update
	apt-get install -y python3-pip
	pip3 install spyne zeep lxml