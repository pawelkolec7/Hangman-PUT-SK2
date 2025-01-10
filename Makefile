CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextras

SERVER = server
CLIENT = klient.py

all: $(SERVER)

$(SERVER): server.cpp
	$(CXX) $(CXXFLAGS) -o $(SERVER) server.cpp
install_client:
	sudo apt-get update && sudo apt-get install -y python3-tk python3-pil python3-pil.imagetk python3-pip
run_client: install_client
	python3 $(CLIENT)	
clean:
	rm -f $(SERVER)
