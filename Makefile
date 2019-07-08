#
# Top level build directories.
#
SRC_DIR                     = src
OBJ_DIR                     = obj
TEST_DIR                    = test

all: descsock_lib

descsock_lib: $(SRC_DIR)/if_descsock.c
	gcc -Wall $(SRC_DIR)/if_descsock.c
