CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -pedantic -pthread
BIN_DIR := bin

.PHONY: all clean test stress debug-concurrency

all: $(BIN_DIR)/master $(BIN_DIR)/node $(BIN_DIR)/client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/master: master/master.cpp master/metadata_store.hpp common/network.hpp common/utils.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ master/master.cpp

$(BIN_DIR)/node: node/node.cpp node/storage_handler.hpp common/network.hpp common/utils.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ node/node.cpp

$(BIN_DIR)/client: client/client.cpp client/cli.hpp common/network.hpp common/utils.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ client/client.cpp

test: all
	bash bash/tests/test_client.sh

stress: all
	bash bash/tests/test_concurrent_clients.sh

debug-concurrency: all
	scripts/debug_concurrency.sh helgrind

clean:
	rm -rf $(BIN_DIR) data
