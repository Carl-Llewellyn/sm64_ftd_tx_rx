CC = gcc
CFLAGS = -Iinclude
LDFLAGS = -Linclude -lftd2xx
TARGET = sm64_ftd_tx_rx
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
