CXXFLAGS += -O2 -Wall -I./include -lpthread -lportaudio

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

midithing: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

.PHONY: clean
clean:
	rm -f $(OBJ) midithing
