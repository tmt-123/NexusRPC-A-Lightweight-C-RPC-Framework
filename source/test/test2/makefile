CXX ?= g++
CXXFLAGS ?= -std=c++11 -g
CPPFLAGS += $(if $(MUDUO_ROOT),-I$(MUDUO_ROOT)/include)
LDFLAGS += $(if $(MUDUO_ROOT),-L$(MUDUO_ROOT)/lib)
LDLIBS += -ljsoncpp -lmuduo_net -lmuduo_base -pthread

all : reg_server rpc_server rpc_client
reg_server : registry_server.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
rpc_server : rpc_server.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
rpc_client : rpc_client.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

.PHONY: all clean
clean:
	rm -f reg_server rpc_server rpc_client
