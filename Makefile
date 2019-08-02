#
# Copyright (C) F5 Networks, Inc. 2016-2018
#
# No part of the software may be reproduced or transmitted in any
# form or by any means, electronic or mechanical, for any purpose,
# without express written permission of F5 Networks, Inc.
#


## Simple Makefile for now ###
# XXX: add pipeline gitlab support
all: runner debug

runner: tester2.c src/descsock_client.c src/descsock.c src/sys.c src/packet.c src/xfrag_mem.c
	gcc -Wall tester2.c src/descsock_client.c src/descsock.c src/sys.c src/packet.c src/xfrag_mem.c -lpthread -o runner

debug: tester2.c src/descsock_client.c src/descsock.c src/sys.c src/packet.c src/xfrag_mem.c
	gcc -Wall -g tester2.c src/descsock_client.c src/descsock.c src/sys.c src/packet.c src/xfrag_mem.c -lpthread -o debug

clean:
	rm runner
	rm debug


