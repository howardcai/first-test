#
# Copyright (C) F5 Networks, Inc. 2016-2018
#
# No part of the software may be reproduced or transmitted in any
# form or by any means, electronic or mechanical, for any purpose,
# without express written permission of F5 Networks, Inc.
#

all: runner debug

runner: tester2.c descsock_client.c src/descsock.c src/kern/sys.c src/net/packet.c src/net/xfrag_mem.c
	gcc -Wall tester2.c descsock_client.c src/descsock.c src/kern/sys.c src/net/packet.c src/net/xfrag_mem.c -o runner

debug: tester2.c descsock_client.c src/descsock.c src/kern/sys.c src/net/packet.c src/net/xfrag_mem.c
	gcc -Wall -g tester2.c descsock_client.c src/descsock.c src/kern/sys.c src/net/packet.c src/net/xfrag_mem.c -o debug

clean:
	rm runner
	rm debug


