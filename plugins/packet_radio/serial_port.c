#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "serial_port.h"
#include "hybbx/util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void hybbx_serial_params_default(hybbx_serial_params_t *params,
                               unsigned int baud)
{
    if (params == NULL) {
        return;
    }

    memset(params, 0, sizeof(*params));
    params->baud = baud;
    params->data_bits = 8;
    params->parity = HYBBX_SERIAL_PARITY_NONE;
    params->stop_bits = 1;
    params->assert_modem_lines = 0;
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct hybbx_serial_port {
    HANDLE handle;
    unsigned int baud;
};

static int win32_normalize_device(const char *device, char *out, size_t out_len)
{
    const char *name;
    unsigned com_num = 0;

    if (device == NULL || out_len == 0) {
        return 0;
    }

    if (strncmp(device, "\\\\.\\", 4) == 0) {
        return hybbx_strlcpy(out, device, out_len) < out_len;
    }

    name = device;
    if (name[0] == '/' && strncmp(name, "/dev/ttyS", 9) == 0 &&
        name[9] >= '0' && name[9] <= '9' && name[10] == '\0') {
        com_num = (unsigned)(name[9] - '0') + 1u;
        return (size_t)snprintf(out, out_len, "\\\\.\\COM%u", com_num) < out_len;
    }

    if ((name[0] == 'C' || name[0] == 'c') &&
        (name[1] == 'O' || name[1] == 'o') &&
        (name[2] == 'M' || name[2] == 'm')) {
        name += 3;
    }

    if (*name == '\0') {
        return 0;
    }

    while (*name >= '0' && *name <= '9') {
        com_num = com_num * 10u + (unsigned)(*name - '0');
        name++;
    }
    if (*name != '\0' || com_num == 0) {
        return 0;
    }

    return (size_t)snprintf(out, out_len, "\\\\.\\COM%u", com_num) < out_len;
}

static hybbx_result_t win32_configure(HANDLE handle,
                                      const hybbx_serial_params_t *params)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        return HYBBX_ERR_IO;
    }

    dcb.BaudRate = (DWORD)params->baud;
    dcb.ByteSize = (BYTE)(params->data_bits == 7 ? 7 : 8);
    switch (params->parity) {
    case HYBBX_SERIAL_PARITY_EVEN:
        dcb.Parity = EVENPARITY;
        dcb.fParity = TRUE;
        break;
    case HYBBX_SERIAL_PARITY_ODD:
        dcb.Parity = ODDPARITY;
        dcb.fParity = TRUE;
        break;
    case HYBBX_SERIAL_PARITY_NONE:
    default:
        dcb.Parity = NOPARITY;
        dcb.fParity = FALSE;
        break;
    }
    dcb.StopBits = params->stop_bits == 2 ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = params->assert_modem_lines ?
        DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    dcb.fRtsControl = params->assert_modem_lines ?
        RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fNull = FALSE;

    if (!SetCommState(handle, &dcb)) {
        return HYBBX_ERR_IO;
    }

    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(handle, &timeouts)) {
        return HYBBX_ERR_IO;
    }

    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 const hybbx_serial_params_t *params)
{
    hybbx_serial_port_t *port;
    char path[64];
    hybbx_result_t rc;

    if (out == NULL || device == NULL || params == NULL || params->baud == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (!win32_normalize_device(device, path, sizeof(path))) {
        return HYBBX_ERR_INVALID;
    }

    port = calloc(1, sizeof(*port));
    if (port == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    port->handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (port->handle == INVALID_HANDLE_VALUE) {
        free(port);
        return HYBBX_ERR_IO;
    }

    rc = win32_configure(port->handle, params);
    if (rc != HYBBX_OK) {
        CloseHandle(port->handle);
        free(port);
        return rc;
    }

    port->baud = params->baud;
    *out = port;
    return HYBBX_OK;
}

void hybbx_serial_close(hybbx_serial_port_t *port)
{
    if (port == NULL) {
        return;
    }

    if (port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
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
        DWORD chunk = 0;

        if (!WriteFile(port->handle, data + written,
                       (DWORD)(len - written), &chunk, NULL)) {
            return HYBBX_ERR_IO;
        }
        if (chunk == 0) {
            return HYBBX_ERR_IO;
        }
        written += (size_t)chunk;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_read(hybbx_serial_port_t *port,
                                 uint8_t *buf, size_t buf_len,
                                 size_t *read_len)
{
    DWORD n = 0;

    if (port == NULL || buf == NULL || read_len == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!ReadFile(port->handle, buf, (DWORD)buf_len, &n, NULL)) {
        return HYBBX_ERR_IO;
    }

    *read_len = (size_t)n;
    return HYBBX_OK;
}

int hybbx_serial_fd(const hybbx_serial_port_t *port)
{
    (void)port;
    return -1;
}

#elif defined(__AMIGA__)

#include <proto/exec.h>
#include <devices/serial.h>

struct hybbx_serial_port {
    struct IOExtSer *io;
    struct MsgPort *port;
    char device_name[32];
    unsigned long unit;
    unsigned int baud;
};

static int amiga_parse_device(const char *device, char *name, size_t name_len,
                              unsigned long *unit)
{
    const char *colon;
    char *end;

    if (unit == NULL || name == NULL || name_len == 0) {
        return 0;
    }

    *unit = 0;
    if (device == NULL || device[0] == '\0') {
        hybbx_strlcpy(name, SERIALNAME, name_len);
        return 1;
    }

    colon = strchr(device, ':');
    if (colon != NULL) {
        size_t prefix = (size_t)(colon - device);

        if (prefix == 0 || prefix >= name_len) {
            return 0;
        }
        memcpy(name, device, prefix);
        name[prefix] = '\0';
        *unit = strtoul(colon + 1, &end, 10);
        return end != colon + 1 && *end == '\0';
    }

    if (device[0] >= '0' && device[0] <= '9') {
        *unit = strtoul(device, &end, 10);
        if (end != device && *end == '\0') {
            hybbx_strlcpy(name, SERIALNAME, name_len);
            return 1;
        }
    }

    hybbx_strlcpy(name, device, name_len);
    return 1;
}

static hybbx_result_t amiga_configure(struct IOExtSer *io,
                                      const hybbx_serial_params_t *params)
{
    io->IOSer.io_Command = SDCMD_SETPARAMS;
    io->io_Baud = (ULONG)params->baud;
    io->io_ReadLen = (UBYTE)(params->data_bits == 7 ? 7 : 8);
    io->io_WriteLen = (UBYTE)(params->data_bits == 7 ? 7 : 8);
    io->io_StopBits = (UBYTE)(params->stop_bits == 2 ? 2 : 1);
    io->io_SerFlags = (UBYTE)(io->io_SerFlags & (UBYTE)~SERF_PARTY_ON);
    if (params->parity == HYBBX_SERIAL_PARITY_EVEN ||
        params->parity == HYBBX_SERIAL_PARITY_ODD) {
        io->io_SerFlags |= SERF_PARTY_ON;
    }
    io->io_SerFlags |= SERF_XDISABLED;

    if (DoIO((struct IORequest *)io) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 const hybbx_serial_params_t *params)
{
    hybbx_serial_port_t *port;
    hybbx_result_t rc;

    if (out == NULL || device == NULL || params == NULL || params->baud == 0) {
        return HYBBX_ERR_INVALID;
    }

    port = calloc(1, sizeof(*port));
    if (port == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    if (!amiga_parse_device(device, port->device_name, sizeof(port->device_name),
                            &port->unit)) {
        free(port);
        return HYBBX_ERR_INVALID;
    }

    port->port = CreatePort(NULL, 0);
    if (port->port == NULL) {
        free(port);
        return HYBBX_ERR_NOMEM;
    }

    port->io = (struct IOExtSer *)CreateExtIO(port->port,
                                              (LONG)sizeof(*port->io));
    if (port->io == NULL) {
        DeletePort(port->port);
        free(port);
        return HYBBX_ERR_NOMEM;
    }

    port->io->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
    if (OpenDevice(port->device_name, port->unit,
                   (struct IORequest *)port->io, 0) != 0) {
        DeleteExtIO((struct IORequest *)port->io);
        DeletePort(port->port);
        free(port);
        return HYBBX_ERR_IO;
    }

    rc = amiga_configure(port->io, params);
    if (rc != HYBBX_OK) {
        CloseDevice((struct IORequest *)port->io);
        DeleteExtIO((struct IORequest *)port->io);
        DeletePort(port->port);
        free(port);
        return rc;
    }

    port->baud = params->baud;
    *out = port;
    return HYBBX_OK;
}

void hybbx_serial_close(hybbx_serial_port_t *port)
{
    if (port == NULL) {
        return;
    }

    if (port->io != NULL) {
        CloseDevice((struct IORequest *)port->io);
        DeleteExtIO((struct IORequest *)port->io);
    }
    if (port->port != NULL) {
        DeletePort(port->port);
    }
    free(port);
}

hybbx_result_t hybbx_serial_write(hybbx_serial_port_t *port,
                                  const uint8_t *data, size_t len)
{
    if (port == NULL || data == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    port->io->IOSer.io_Command = CMD_WRITE;
    port->io->IOSer.io_Data = (APTR)data;
    port->io->IOSer.io_Length = (ULONG)len;
    port->io->IOSer.io_Flags = 0;

    if (DoIO((struct IORequest *)port->io) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_serial_read(hybbx_serial_port_t *port,
                                 uint8_t *buf, size_t buf_len,
                                 size_t *read_len)
{
    if (port == NULL || buf == NULL || read_len == NULL || buf_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    port->io->IOSer.io_Command = CMD_READ;
    port->io->IOSer.io_Data = (APTR)buf;
    port->io->IOSer.io_Length = (ULONG)buf_len;
    port->io->IOSer.io_Flags = IOF_QUICK;

    if (DoIO((struct IORequest *)port->io) != 0) {
        *read_len = 0;
        return HYBBX_OK;
    }

    *read_len = (size_t)port->io->IOSer.io_Actual;
    return HYBBX_OK;
}

int hybbx_serial_fd(const hybbx_serial_port_t *port)
{
    (void)port;
    return -1;
}

#else /* POSIX */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct hybbx_serial_port {
    int fd;
    unsigned int baud;
    int assert_modem_lines;
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

static hybbx_result_t posix_assert_modem_lines(int fd, int on)
{
#ifdef TIOCMGET
    int flags;

    if (ioctl(fd, TIOCMGET, &flags) != 0) {
        return HYBBX_ERR_IO;
    }
    if (on) {
        flags |= TIOCM_RTS | TIOCM_DTR;
    } else {
        flags &= ~(TIOCM_RTS | TIOCM_DTR);
    }
    if (ioctl(fd, TIOCMSET, &flags) != 0) {
        return HYBBX_ERR_IO;
    }
#else
    (void)fd;
    (void)on;
#endif
    return HYBBX_OK;
}

static hybbx_result_t configure_port(int fd, const hybbx_serial_params_t *params)
{
    struct termios tty;
    tcflag_t databits;

    if (tcgetattr(fd, &tty) != 0) {
        return HYBBX_ERR_IO;
    }

    cfmakeraw(&tty);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    if (params->data_bits == 7) {
        databits = CS7;
    } else {
        databits = CS8;
    }
    tty.c_cflag |= databits;
    if (params->parity == HYBBX_SERIAL_PARITY_EVEN) {
        tty.c_cflag |= PARENB;
    } else if (params->parity == HYBBX_SERIAL_PARITY_ODD) {
        tty.c_cflag |= PARENB | PARODD;
    }
    if (params->stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    }
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (set_baud_rate(&tty, params->baud) != HYBBX_OK) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return HYBBX_ERR_IO;
    }

    tcflush(fd, TCIOFLUSH);
    return posix_assert_modem_lines(fd, params->assert_modem_lines);
}

hybbx_result_t hybbx_serial_open(hybbx_serial_port_t **out,
                                 const char *device,
                                 const hybbx_serial_params_t *params)
{
    hybbx_serial_port_t *port;
    hybbx_result_t rc;

    if (out == NULL || device == NULL || params == NULL || params->baud == 0) {
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

    rc = configure_port(port->fd, params);
    if (rc != HYBBX_OK) {
        close(port->fd);
        free(port);
        return rc;
    }

    port->baud = params->baud;
    port->assert_modem_lines = params->assert_modem_lines;
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

    if (port->assert_modem_lines) {
        hybbx_result_t rc = posix_assert_modem_lines(port->fd, 1);

        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    while (written < len) {
        ssize_t n = write(port->fd, data + written, len - written);

        if (n < 0) {
#if EAGAIN == EWOULDBLOCK
            if (errno == EAGAIN) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_ERR_IO;
        }
        written += (size_t)n;
    }

    if (tcdrain(port->fd) != 0) {
        return HYBBX_ERR_IO;
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
#if EAGAIN == EWOULDBLOCK
        if (errno == EAGAIN) {
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
            *read_len = 0;
            return HYBBX_OK;
        }
        return HYBBX_ERR_IO;
    }

    *read_len = (size_t)n;
    return HYBBX_OK;
}

int hybbx_serial_fd(const hybbx_serial_port_t *port)
{
    if (port == NULL) {
        return -1;
    }

    return port->fd;
}

#endif
