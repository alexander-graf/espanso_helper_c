CC=gcc
CFLAGS=-Wall -Wextra `pkg-config --cflags gtk+-3.0`
LDFLAGS=`pkg-config --libs gtk+-3.0`
TARGET=espanso_helper

$(TARGET): espanso_helper_c.c
	$(CC) $(CFLAGS) -o $(TARGET) espanso_helper_c.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
