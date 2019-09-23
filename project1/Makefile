# Compiler and Compile options.
CC = g++
CFLAGS = -Wall -Wno-sign-compare

CFLAGS += -O2
#CFLAGS += -g

# Macros specifying path for compile.
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)
INC = ./include/
LIB = ./lib/

# Pre-Processor.
CFLAGS += -I$(INC) -L$(LIB) -lpthread -fopenmp --std=c++11


TARGET = run


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)



# Delete binary & object files
clean:
	$(RM) $(TARGET) $(OBJS)
