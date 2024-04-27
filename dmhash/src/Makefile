all: main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)  -Wall -lmbedtls -lmbedcrypto -o dmhash $^ $(LDLIBS) -lrt 
