CXX=g++
LD=g++
CXXFLAGS=-g -Wall
SRC_FILE = $(wildcard *.cpp)
OBJ_FILE = $(patsubst %.cpp, %.o, $(SRC_FILE))

all: server

server : $(OBJ_FILE) 
	$(LD) $^ -o $@ 


#生成依赖文件
%.d : %.cpp
	$(CXX) $(CXXFLAGS) -M $^  > $@

#include $(SRC_FILE:.cpp=.d) 

print:
	@echo $(SRC_FILE)
	@echo $(OBJ_FILE)

clean:
	rm -f *.o
	rm -f *.d
	rm -f server

.PHONY: print clean
