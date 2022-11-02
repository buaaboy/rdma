.PHONY: clean

CFLAGS  := -Wall -Werror -g
LD      := gcc
LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread

APPS    := rdma-client rdma-server temp-client

all: ${APPS}

rdma-client: rdma-common.o rdma-client.o
	${LD} -o $@ $^ ${LDLIBS}

rdma-server: rdma-common.o rdma-server.o
	${LD} -o $@ $^ ${LDLIBS}

temp-client: rdma-common.o data-sender.o client-invoker.o
	${LD} -o $@ $^ ${LDLIBS}
clean:
	rm -f *.o ${APPS}

