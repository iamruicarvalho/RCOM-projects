# Makefile to build the project

# Parameters
CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/

FTP = ftp://
URL = ftp.up.pt/pub/kodi/timestamp.txt

# Targets
.PHONY: download_client
download_client: $(SRC)/download_client.c
	$(CC) $(CFLAGS) $^ -o download

.PHONY: run_example
run_example: download_client
	./download $(FTP)$(URL)

.PHONY: clean
clean:
	rm -f download
