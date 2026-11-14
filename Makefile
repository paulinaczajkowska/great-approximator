CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -O2
LDFLAGS =

EXEC = approx-server approx-client

# Wspólne źródła
SRC_COMMON_C = common.c
SRC_COMMON_CPP = err.cpp data.cpp messages.cpp

OBJ_COMMON = $(SRC_COMMON_C:.c=.o) $(SRC_COMMON_CPP:.cpp=.o)

# Serwer
SRC_SERVER = approx-server.cpp
OBJ_SERVER = $(SRC_SERVER:.cpp=.o)

# Klient
SRC_CLIENT = approx-client.cpp
OBJ_CLIENT = $(SRC_CLIENT:.cpp=.o)

.PHONY: all clean

all: $(EXEC)

approx-server: $(OBJ_SERVER) $(OBJ_COMMON)
	$(CC) $(OBJ_SERVER) $(OBJ_COMMON) -o $@ $(LDFLAGS)

approx-client: $(OBJ_CLIENT) $(OBJ_COMMON)
	$(CC) $(OBJ_CLIENT) $(OBJ_COMMON) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) -Wall -Wextra -O2 -c $< -o $@

clean:
	rm -f $(OBJ_SERVER) $(OBJ_CLIENT) $(OBJ_COMMON) $(EXEC)
