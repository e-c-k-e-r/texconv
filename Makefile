# Compiler and flags
CXX	 	:= g++
CXXFLAGS:= -std=c++11 -O2 -Wall -Wextra

# Project files
SOURCES := textool.cpp common.cpp image.cpp imagecontainer.cpp conv16bpp.cpp twiddler.cpp convpal.cpp palette.cpp preview.cpp
HEADERS := common.h image.h imagecontainer.h vqtools.h twiddler.h palette.h
OBJECTS := $(SOURCES:.cpp=.o)

# Output binary
TARGET  := texconv

# Default build
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile rules
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean
clean:
	rm -f $(OBJECTS) $(TARGET)
