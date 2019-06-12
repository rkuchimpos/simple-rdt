# NAME: Jesse Catalan, Ricardo Kuchimpos
# EMAIL: jessecatalan77@gmail.com, rkuchimpos@gmail.com
# ID: 204785152, 704827423

default:
	g++ -o server server.cpp packet.cpp utils.cpp
	g++ -o client client.cpp packet.cpp utils.cpp

.PHONY: dist
dist:
	zip -r project2_204785152_704827423.zip server.cpp client.cpp packet.hpp packet.cpp utils.hpp utils.cpp Makefile README report.pdf

.PHONY: clean
clean:
	rm -f server
	rm -f client
	rm -f project2_204785152_704827423.zip
	rm -f *.file