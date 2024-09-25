# Define variables
CROSSCC = aarch64-linux-gnu-gcc
CROSSCFLAGS=-Wall -pthread
CROSSLDFLAGS=-static \
-I/home/palaska/Desktop/rtes/libwebsockets-build/include \
-I/home/palaska/Desktop/rtes/jansson-build/include \
-I/home/palaska/Desktop/rtes/openssl-build/include \
-I/home/palaska/Desktop/rtes/zlib-build/include \
-L/home/palaska/Desktop/rtes/libwebsockets-build/lib \
-L/home/palaska/Desktop/rtes/jansson-build/lib \
-L/home/palaska/Desktop/rtes/openssl-build/lib \
-L/home/palaska/Desktop/rtes/zlib-build/lib \
-lwebsockets -lssl -lcrypto -lz -ljansson -ldl

TARGET = rtes
SRC = rtes.c

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(SRC)
	$(CROSSCC) $(CROSSCFLAGS) $^ -o $(TARGET) $(CROSSLDFLAGS)

# Clean up
clean:
	rm -f $(TARGET)
