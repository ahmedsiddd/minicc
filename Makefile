minicc: minicc.cpp
	g++ -std=c++17 -O2 -Wall -o minicc minicc.cpp
clean:
	rm -f minicc
