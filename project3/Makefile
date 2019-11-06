# Compiler and Compile options.
CC = g++
CXXFLAGS = -Wall

# Verbose mode
# CXXFLAGS += -DVERBOSE

CXXFLAGS += -O2
#CXXFLAGS += -g

# Macros specifying path for compile.
# SRCS := $(wildcard src/*.cpp)
SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:.cpp=.o)
INC = ./include/
LIB = ./lib/

# Pre-Processor.
CXXFLAGS += -I$(INC) -L$(LIB) -lpthread --std=c++11

TARGET = run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CXXFLAGS)

# Delete binary & object files
clean:
	$(RM) $(TARGET) $(OBJS)
