# Compiler and Compile options.
CC = g++
CXXFLAGS = -Wall -Wno-sign-compare -DLOCAL_TEST

# Verbose mode
CXXFLAGS += -DVERBOSE

CXXFLAGS += -O2
#CXXFLAGS += -g

# Macros specifying path for compile.
# SRCS := $(wildcard src/*.cpp)
SRCS := $(filter-out src/validator.cpp, $(wildcard src/*.cpp))
OBJS := $(SRCS:.cpp=.o)
INC = ./include/
LIB = ./lib/

# Pre-Processor.
CXXFLAGS += -I$(INC) -L$(LIB) -lpthread --std=c++11

TARGET = run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CXXFLAGS)

validator:
	$(CC) -o validator src/validator.cpp $(CXXFLAGS)

# Delete binary & object files
clean:
	$(RM) $(TARGET) $(OBJS)
