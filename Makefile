# Makefile for RPAL interpreter (rpal20)

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

# OS-specific link flags and clean command.
# On Windows, static link avoids MSYS libstdc++ DLL path issues, and we
# use 'del' since 'rm' isn't available in cmd.exe.
ifeq ($(OS),Windows_NT)
    LDFLAGS = -static
    RM      = cmd /C del /F /Q
else
    LDFLAGS =
    RM      = rm -f
endif

SRCS = rpal20.cpp Lexer.cpp Node.cpp Parser.cpp Standardizer.cpp CSEMachine.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = rpal20

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	-$(RM) $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all clean
