#ifndef HYBBX_SOCKET_H
#define HYBBX_SOCKET_H

#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include <sys/socket.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Suppress SIGPIPE on send() (SO_NOSIGPIPE on BSD/macOS; noop on Linux). */
void hybbx_socket_nosigpipe(int fd);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SOCKET_H */
