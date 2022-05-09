all: server client

server: bin/sdstored

client: bin/sdstore

bin/sdstored: obj/sdstored.o
	gcc -g obj/sdstored.o -o bin/sdstored

obj/sdstored.o: src/sdstored.c
	gcc -Wall -g -c src/sdstored.c -o obj/sdstored.o

bin/sdstore: obj/sdstore.o
	gcc -g obj/sdstore.o -o bin/sdstore

obj/sdstore.o: src/sdstore.c
	gcc -Wall -g -c src/sdstore.c -o obj/sdstore.o

clean:
	rm obj/* tmp/* bin/*

test:
	#xfce4-terminal -e bin/sdstored
	#xfce4-terminal -e bin/sdstore
testServ: 
	./bin/sdstored config.txt bin/sdstore-transformations/

testCli:
	./bin/sdstore proc-file teste.txt teste1 nop



#./bin/sdstored configuracao.txt ./SDStore-transf
