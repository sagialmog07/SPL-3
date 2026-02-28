CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude
LDFLAGS:=-lpthread -lboost_system -lboost_thread

# הוספנו כאן את bin/GameDataManager.o לרשימה
OBJS:=bin/ConnectionHandler.o bin/StompClient.o bin/StompProtocol.o bin/GameDataManager.o bin/event.o

all: bin/StompWCIClient

bin/StompWCIClient: $(OBJS)
	@echo 'Linking all objects into binary: bin/StompWCIClient'
	g++ -o bin/StompWCIClient $(OBJS) $(LDFLAGS)

bin/ConnectionHandler.o: src/ConnectionHandler.cpp
	g++ $(CFLAGS) -o bin/ConnectionHandler.o src/ConnectionHandler.cpp

bin/StompClient.o: src/StompClient.cpp
	g++ $(CFLAGS) -o bin/StompClient.o src/StompClient.cpp

bin/StompProtocol.o: src/StompProtocol.cpp
	g++ $(CFLAGS) -o bin/StompProtocol.o src/StompProtocol.cpp

# כלל קומפילציה חדש עבור GameDataManager
bin/GameDataManager.o: src/GameDataManager.cpp
	g++ $(CFLAGS) -o bin/GameDataManager.o src/GameDataManager.cpp

bin/event.o: src/event.cpp
	g++ $(CFLAGS) -o bin/event.o src/event.cpp

.PHONY: clean
clean:
	rm -f bin/*