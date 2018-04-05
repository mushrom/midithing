CXXFLAGS += -O2 -Wall -I./include

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

midithing: $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o $@

.PHONY: clean
clean:
	rm -f $(OBJ) midithing
