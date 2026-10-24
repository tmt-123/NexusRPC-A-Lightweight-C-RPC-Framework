CXX ?= g++
CXXFLAGS ?= -std=c++11 -g
CPPFLAGS += $(if $(MUDUO_ROOT),-I$(MUDUO_ROOT)/include)
LDFLAGS += $(if $(MUDUO_ROOT),-L$(MUDUO_ROOT)/lib)
LDLIBS += -ljsoncpp -lmuduo_net -lmuduo_base -pthread

all : server publish_client subscribe_client
server : server.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
publish_client : publish_client.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
subscribe_client : subscribe_client.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

.PHONY: all clean
clean:
	rm -f server publish_client subscribe_client
