.PHONY: clean

FLAGS=-O2 -std=c++11 -Wall -Wextra -pedantic -pthread -g $(CXXFLAGS)

test: main.cpp *.h
	$(CXX) $(FLAGS) main.cpp -o $@

clean:
	$(RM) test

