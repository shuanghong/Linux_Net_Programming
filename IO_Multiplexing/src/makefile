###################### all define ############################### 
DEFINE += -DLINUX

###################### all Compiler ############################### 
### C Pre-Processor ###
CPP = gcc

### C Compiler 
CC = gcc

### C++ Compiler ### 
CXX = g++


###################### all Compiler flags ############################### 
FLAGS += -g
FLAGS += -O0
FLAGS += -Wall
 
FLAGS += -pthread    #POSIX thread lib

CFLAGS += $(FLAGS) 
CXXFLAGS += $(FLAGS) 
CXXFLAGS += -std=c++11 


###################### Linker ############################### 
LD = gcc 


###################### all link libs and path ############################### 
LDLIBS += -pthread  
# ANSI C library
LDLIBS += -lc 
# Math functions in separate lib
LDLIBS += -lm

# common lib path
LDLIBPATH += -L/usr/lib/i386-linux-gnu/

# gcc related lib path
LDLIBPATH += -L/usr/lib/gcc/i686-linux-gnu/4.8/		#32bit
LDLIBPATH += -L/usr/lib/gcc/x86_64-linux-gnu/4.8/    #64bit

###################### all object files ############################### 

C_OBJS := 
CPP_OBJ := TcpServerSelectDemo.o
OBJS := $(C_OBJS) $(CPP_OBJ)

APP = TcpServerSelectDemo

all: $(APP) 
$(APP): $(OBJS) 
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(C_OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $< $(INCLUDE)

$(CPP_OBJS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $< $(INCLUDE)

clean:
	-rm -f $(APP) $(OBJS) *.elf *.gdb *.o
	-rm -f tag* cscope*

