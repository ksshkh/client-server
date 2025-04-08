CC = g++

CFLAGS = -c
LDFLAGS = -luuid -ljansson

CLIENT = client
SOURCES_CLIENT = client.cpp mainClient.cpp errors.cpp
INCLUDES_CLIENT = client.hpp errors.hpp
OBJECTS_CLIENT = $(SOURCES_CLIENT:.cpp=.o)

SERVER = server
SOURCES_SERVER = server.cpp mainServer.cpp errors.cpp
INCLUDES_SERVER = server.hpp errors.hpp
OBJECTS_SERVER = $(SOURCES_SERVER:.cpp=.o)

all: $(CLIENT) $(SERVER)

$(CLIENT): $(OBJECTS_CLIENT)
	$(CC) $(OBJECTS_CLIENT) -o $@ $(LDFLAGS)

$(SERVER): $(OBJECTS_SERVER)
	$(CC) $(OBJECTS_SERVER) -o $@ $(LDFLAGS)

%.o: %.cpp $(INCLUDES_CLIENT) $(INCLUDES_SERVER)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJECTS_CLIENT) $(OBJECTS_SERVER) $(CLIENT) $(SERVER)