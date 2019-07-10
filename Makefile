#
# Top level build directories.
#
SRC_DIR                     = src
OBJ_DIR                     = obj
TEST_DIR                    = test

all: descsock_lib



descsock_lib: tester.c $(SRC_DIR)/descsock.c $(SRC_DIR)/kern/sys.c $(SRC_DIR)/net/packet.c $(SRC_DIR)/net/xfrag_mem.c
	gcc -Wall tester.c $(SRC_DIR)/descsock.c $(SRC_DIR)/kern/sys.c $(SRC_DIR)/net/packet.c $(SRC_DIR)/net/xfrag_mem.c -o descsock_lib

clean:
	rm descsock_lib


