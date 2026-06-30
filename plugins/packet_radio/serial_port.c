#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "serial_port.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#error "hybbx serial port: Windows support not yet implemented"
#elif defined(__AMIGA__)
#error "hybbx serial port: AmigaOS support not yet implemented"
#else

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

struct hybbx_serial_port {
    int fd;
    unsigned int baud;
};

#ifndef cfmakeraw
static void hybbx_cfmakeraw(struct termios *tty)
{
    tty->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                      IGNCR | ICRNL | IXON);
    tty->c_oflag &= ~OPOST;
    tty->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty->c_cflag &= ~(CSIZE | PARENB);
    tty->c_cflag |= CS8;
}
#define cfmakeraw hybbx_cfmakeraw
#endif

static hybbx_result_t set_baud_rate(struct termios *tty, unsigned int baud)
{
#if defined(__linux__) || defined(__GLIBC__)
    if (cfsetspeed(tty, (speed_t)baud) == 0) {
        return HYBBX_OK;
    }
#endif

    {
        speed_t speed = B0;
        switch (baud) {
        case 50: speed = B50; break;
        case 75: speed = B75; break;
        case 110: speed = B110; break;
        case 134: speed = B134; break;
        case 150: speed = B150; break;
        case 200: speed = B200; break;
        case 300: speed = B300; break;
        case 600: speed = B600; break;
        case 1200: speed = B1200; break;
        case 1800: speed = B1800; break;
        case 2400: speed = B2400; break;
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
#ifdef B19200
        case 19200: speed = B19200; break;
#endif
#ifdef B38400
        case 38400: speed = B38400; break;
#endif
#ifdef B57600
        case 57600: speed = B57600; break;
#endif
#ifdef B115200
        case 115200: speed = B115200; break;
#endif
        default:
            return HYBBX_ERR_UNSUPPORTED;
        }

        cfsetispeed(tty, speed);
        cfsetospeed(tty, speed);
    }

    return HYBBX_OK;
}

static hybbx_result_t configure_port(int fd, unsigned int baud)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        return HYBBX_ERR_IO;
    }

    cfmakeraw(&tty);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (set_baud_rate(&tty, baud) != HYBBX_OK) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return HYBBX_ERR_IO;
    }

    tcflush(fd, TCIOFLUSH);
    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 unsigned int baud)
{
    hybbx_serial_port_t *port;
    hybbx_result_t rc;

    if (out == NULL || device == NULL || baud == 0) {
        return HYBBX_ERR_INVALID;
    }

    port = calloc(1, sizeof(*port));
    if (port == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    port->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd < 0) {
        free(port);
        return HYBBX_ERR_IO;
    }

    rc = configure_port(port->fd, baud);
    if (rc != HYBBX_OK) {
        close(port->fd);
        free(port);
        return rc;
    }

    port->baud = baud;
    *out = port;
    return HYBBX_OK;
}

void hybbx_serial_close(hybbx_serial_port_t *port)
{
    if (port == NULL) {
        return;
    }

    if (port->fd >= 0) {
        close(port->fd);
    }
    free(port);
}

hybbx_result_t hybbx_serial_write(hybbx_serial_port_t *port,
                                  const uint8_t *data, size_t len)
{
    size_t written = 0;

    if (port == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    while (written < len) {
        ssize_t n = write(port->fd, data + written, len - written);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_ERR_IO;
        }
        written += (size_t)n;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_read(hybbx_serial_port_t *port,
                                 uint8_t *buf, size_t buf_len,
                                 size_t *read_len)
{
    ssize_t n;

    if (port == NULL || buf == NULL || read_len == NULL) {
        return HYBBX_ERR_INVALID;
    }

    n = read(port->fd, buf, buf_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *read_len = 0;
            return HYBBX_OK;
        }
        return HYBBX_ERR_IO;
    }

    *read_len = (size_t)n;
    return HYBBX_OK;
}

#endif
