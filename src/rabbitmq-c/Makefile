CC=gcc
LD=gcc
CFLAGS=-std=gnu90 -O3 -DNDEBUG -fPIC \
       -Wall -Wextra -Wstrict-prototypes -Wno-unused-function -fno-common -fvisibility=hidden
CPPFLAGS=-I. -DAMQP_BUILD -DHAVE_CONFIG_H -DWITH_SSL=1 -Drabbitmq_EXPORTS
LIBS=-lssl -lcrypto -lrt -lpthread
LDFLAGS=-shared -Wl,-soname,librabbitmq.so.4

HEADERS=amqp_framing.h amqp_hostcheck.h amqp_openssl_bio.h \
	amqp_openssl_hostname_validation.h amqp_private.h  \
        amqp_socket.h amqp_ssl_socket.h amqp_table.h       \
        amqp_tcp_socket.h amqp_time.h threads.h

SOURCES=amqp_api.c amqp_connection.c amqp_consumer.c amqp_framing.c \
        amqp_hostcheck.c amqp_mem.c amqp_openssl.c amqp_openssl_bio.c \
        amqp_openssl_hostname_validation.c amqp_socket.c amqp_table.c \
        amqp_tcp_socket.c amqp_time.c amqp_url.c

OBJECTS=$(SOURCES:%.c=%.o)

all: librabbitmq.so.4.3.1

$(OBJECTS): config.h $(HEADERS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

librabbitmq.so.4.3.1: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(LIBS) $(OBJECTS)

install: librabbitmq.so.4.3.1
	cp $< /usr/local/lib/.
	(cd /usr/local/lib/ && \
         ln -s librabbitmq.so.4.3.1 librabbitmq.so.4 && \
         ln -s librabbitmq.so.4.3.1 librabbitmq.so)
	ldconfig -v


clean:
	rm -f $(OBJECTS) librabbitmq.so.4.3.1
	-unlink /usr/local/lib/librabbitmq.so.4
	-unlink /usr/local/lib/librabbitmq.so
