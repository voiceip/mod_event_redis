# Customise these as appropriate
MODNAME = mod_event_redis.so
MODOBJ = mod_event_redis.o
MODCFLAGS = -Wall -Werror 
MODLDFLAGS = -lssl 

CXX = g++
CXXFLAGS = -fPIC -g -ggdb -I/usr/include  `pkg-config --cflags freeswitch` $(MODCFLAGS) -std=c++1y
LDFLAGS = `pkg-config --libs freeswitch` -lcpp_redis -ltacopie  -lz -lpthread -lrt $(MODLDFLAGS) 

.PHONY: all
all: $(MODNAME)

$(MODNAME): $(MODOBJ)
	@$(CXX) -shared -o $@ $(MODOBJ) $(LDFLAGS)

.cpp.o: $<
	@$(CXX) $(CXXFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f $(MODNAME) $(MODOBJ)

.PHONY: install
install: $(MODNAME)
	install -d $(DESTDIR)/usr/lib/freeswitch/mod
	install $(MODNAME) $(DESTDIR)/usr/lib/freeswitch/mod
	install -d $(DESTDIR)/etc/freeswitch/autoload_configs
	install event_redis.conf.xml $(DESTDIR)/etc/freeswitch/autoload_configs/