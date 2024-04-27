all: hwsysd hwsys_ctrl

hwsysd: hwsysd.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wall -o hwsysd $^ $(LDLIBS) -lrt

hwsys_ctrl: hwsys_ctrl.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wall -o hwsys_ctrl $^ $(LDLIBS) -lrt -lgpiod