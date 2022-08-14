CXX ?= g++
DEBUG ?= 1

ifeq ($(DEBUG),1)
	CXX_FLAGS = -g
else
	CXX_FLAGS = -O2
endif

server: main.cpp ./CGImysql/sql_connection_pool.cpp ./http/http.cpp ./log/log.cpp  ./time/lst_timer.cpp config.cpp webserver.cpp
	$(CXX) -o main $^ $(CXX_FLAGS) -lpthread -lmysqlclient

clean:
	rm -r main