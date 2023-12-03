# Makefile to build the project

# Parameters
CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/

# Targets
.PHONY: download_client
download_client: $(SRC)/download_client.c
	$(CC) $(CFLAGS) $^ -o download

.PHONY: clean
clean:
	rm -f download
