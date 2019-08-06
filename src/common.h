/*
 * Copyright (C) F5 Networks, Inc. 2018 - 2019
 *
 * No part of the software may be reproduced or transmitted in any form or by
 * any means, electronic or mechanical, for any purpose, without express
 * written permission of F5 Networks, Inc.
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#define FAILED      -1
#define SUCCESS     1
#define BUF_SIZE    2048
/*
 * Maximum length of a file path parameter or field used with this library.
 */
#define DESCSOCK_PATHLEN    512
#define DESCSOCK_BUF_SIZE   2048


#define DESCSOCK_DEBUG_PRINT 0
#define DESCSOCK_DEBUGF(fmt, rest...) ({if(DESCSOCK_DEBUG_PRINT) { printf("%s():%d " fmt "\n", __FUNCTION__, __LINE__, ##rest); }})
#define DESCSOCK_LOG(fmt, rest...) printf("descsock: " fmt "\n", ##rest);


#endif /* __COMMON_H__ */
