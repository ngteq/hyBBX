#if defined(__linux__)
#define _DEFAULT_SOURCE
#endif

#include "baycom_kernel.h"

#include "hybbx/ax25.h"
#include "hybbx/baycom.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/hdlcdrv.h>

static char *baycom_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

struct baycom_kernel_modem {
    hybbx_baycom_config_t cfg;
    baycom_modem_frame_cb frame_cb;
    void *frame_userdata;
    int ctl_fd;
    int pkt_fd;
    int ifindex;
};

static void baycom_kernel_mode_string(const hybbx_baycom_config_t *cfg,
                                      char *out, size_t out_cap)
{
    unsigned rate;
    char suffix = '\0';

    if (out == NULL || out_cap == 0 || cfg == NULL) {
        return;
    }

    switch (cfg->mode) {
    case HYBBX_BAYCOM_MODE_SER12_SOFTDCD:
    case HYBBX_BAYCOM_MODE_PAR96_SOFTDCD:
        suffix = '*';
        break;
    case HYBBX_BAYCOM_MODE_SER12_INV:
        suffix = '+';
        break;
    default:
        break;
    }

    if (cfg->mode == HYBBX_BAYCOM_MODE_PAR96 ||
        cfg->mode == HYBBX_BAYCOM_MODE_PAR96_SOFTDCD) {
        snprintf(out, out_cap, "par96%c", suffix);
        return;
    }

    if (cfg->mode == HYBBX_BAYCOM_MODE_EPP) {
        snprintf(out, out_cap, "epp");
        return;
    }

    rate = cfg->radio_baud / 100u;
    if (rate == 0) {
        rate = 12;
    }
    snprintf(out, out_cap, "ser%u%c", rate, suffix);
}

static int baycom_kernel_hdlc_ioctl(baycom_kernel_modem_t *modem,
                                    struct hdlcdrv_ioctl *hi)
{
    struct ifreq ifr;

    if (modem == NULL || hi == NULL || modem->ctl_fd < 0 ||
        modem->cfg.interface == NULL) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, modem->cfg.interface, IFNAMSIZ - 1);
    ifr.ifr_data = (void *)hi;

    if (ioctl(modem->ctl_fd, SIOCDEVPRIVATE, &ifr) < 0) {
        return -1;
    }

    return 0;
}

static int baycom_kernel_interface_flags(baycom_kernel_modem_t *modem,
                                         unsigned short *flags_out)
{
    struct ifreq ifr;

    if (modem == NULL || flags_out == NULL || modem->ctl_fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, modem->cfg.interface, IFNAMSIZ - 1);
    if (ioctl(modem->ctl_fd, SIOCGIFFLAGS, &ifr) < 0) {
        return -1;
    }

    *flags_out = ifr.ifr_flags;
    return 0;
}

static int baycom_kernel_interface_up(baycom_kernel_modem_t *modem, int up)
{
    struct ifreq ifr;
    unsigned short flags = 0;

    if (baycom_kernel_interface_flags(modem, &flags) < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, modem->cfg.interface, IFNAMSIZ - 1);
    ifr.ifr_flags = flags;
    if (up) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= (unsigned short)~IFF_UP;
    }

    return ioctl(modem->ctl_fd, SIOCSIFFLAGS, &ifr);
}

static int baycom_kernel_set_modem_params(baycom_kernel_modem_t *modem)
{
    struct hdlcdrv_ioctl hi;

    memset(&hi, 0, sizeof(hi));
    hi.cmd = HDLCDRVCTL_SETMODEMPAR;
    hi.data.mp.iobase = (int)modem->cfg.iobase;
    hi.data.mp.irq = (int)modem->cfg.irq;

    return baycom_kernel_hdlc_ioctl(modem, &hi);
}

static int baycom_kernel_set_mode(baycom_kernel_modem_t *modem)
{
    struct hdlcdrv_ioctl hi;
    char modestr[32];

    baycom_kernel_mode_string(&modem->cfg, modestr, sizeof(modestr));

    memset(&hi, 0, sizeof(hi));
    hi.cmd = HDLCDRVCTL_SETMODE;
    strncpy(hi.data.modename, modestr, sizeof(hi.data.modename) - 1);

    return baycom_kernel_hdlc_ioctl(modem, &hi);
}

static int baycom_kernel_set_channel_params(baycom_kernel_modem_t *modem)
{
    struct hdlcdrv_ioctl hi;

    memset(&hi, 0, sizeof(hi));
    hi.cmd = HDLCDRVCTL_SETCHANNELPAR;
    hi.data.cp.tx_delay = (int)modem->cfg.txdelay;
    hi.data.cp.tx_tail = (int)modem->cfg.txtail;
    hi.data.cp.slottime = (int)modem->cfg.slot;
    hi.data.cp.ppersist = (int)modem->cfg.persist;
    hi.data.cp.fulldup = modem->cfg.full_duplex ? 1 : 0;

    return baycom_kernel_hdlc_ioctl(modem, &hi);
}

static int baycom_kernel_modprobe(const hybbx_baycom_config_t *cfg)
{
    char cmd[384];
    char modestr[32];
    int rc;

    if (cfg == NULL || cfg->kernel_module == NULL) {
        return -1;
    }

    baycom_kernel_mode_string(cfg, modestr, sizeof(modestr));
    snprintf(cmd, sizeof(cmd),
             "modprobe %s mode=%s iobase=0x%x irq=%u baud=%u 2>/dev/null",
             cfg->kernel_module, modestr, cfg->iobase, cfg->irq,
             cfg->radio_baud);

    rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "[baycom] modprobe failed (%s)\n", cfg->kernel_module);
        return -1;
    }

    return 0;
}

static int baycom_kernel_ifindex(baycom_kernel_modem_t *modem)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, modem->cfg.interface, IFNAMSIZ - 1);
    if (ioctl(modem->ctl_fd, SIOCGIFINDEX, &ifr) < 0) {
        return -1;
    }

    modem->ifindex = ifr.ifr_ifindex;
    return 0;
}

static int baycom_kernel_open_packet(baycom_kernel_modem_t *modem)
{
    struct sockaddr_ll bind_addr;

    modem->pkt_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_AX25));
    if (modem->pkt_fd < 0) {
        return -1;
    }

    if (baycom_kernel_ifindex(modem) < 0) {
        return -1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sll_family = AF_PACKET;
    bind_addr.sll_protocol = htons(ETH_P_AX25);
    bind_addr.sll_ifindex = modem->ifindex;

    if (bind(modem->pkt_fd, (struct sockaddr *)&bind_addr,
             sizeof(bind_addr)) < 0) {
        return -1;
    }

    return 0;
}

hybbx_result_t baycom_kernel_open(baycom_kernel_modem_t **out,
                                  const hybbx_baycom_config_t *cfg,
                                  baycom_modem_frame_cb cb, void *userdata)
{
    baycom_kernel_modem_t *km;
    unsigned short flags = 0;

    if (out == NULL || cfg == NULL || cfg->interface == NULL) {
        return HYBBX_ERR_INVALID;
    }

    *out = NULL;
    km = calloc(1, sizeof(*km));
    if (km == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    km->cfg = *cfg;
    km->cfg.interface = baycom_strdup(cfg->interface);
    km->cfg.kernel_module = baycom_strdup(cfg->kernel_module);
    km->cfg.device = baycom_strdup(cfg->device);
    km->cfg.mycall = baycom_strdup(cfg->mycall);
    km->cfg.circuit_host = baycom_strdup(cfg->circuit_host);
    km->cfg.link_id = baycom_strdup(cfg->link_id);
    km->cfg.link_password = baycom_strdup(cfg->link_password);
    km->cfg.link_role = baycom_strdup(cfg->link_role);
    km->cfg.frequency_mhz = baycom_strdup(cfg->frequency_mhz);
    km->frame_cb = cb;
    km->frame_userdata = userdata;
    km->ctl_fd = -1;
    km->pkt_fd = -1;

    if (km->cfg.interface == NULL) {
        free(km);
        return HYBBX_ERR_NOMEM;
    }

    if (cfg->kernel_autoload) {
        if (baycom_kernel_modprobe(cfg) < 0) {
            hybbx_baycom_config_free(&km->cfg);
            free(km);
            return HYBBX_ERR_IO;
        }
    }

    km->ctl_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (km->ctl_fd < 0) {
        hybbx_baycom_config_free(&km->cfg);
        free(km);
        return HYBBX_ERR_IO;
    }

    if (baycom_kernel_interface_flags(km, &flags) < 0) {
        fprintf(stderr, "[baycom] interface %s not found (load %s first)\n",
                km->cfg.interface,
                km->cfg.kernel_module != NULL ? km->cfg.kernel_module
                                              : "baycom module");
        baycom_kernel_close(km);
        return HYBBX_ERR_IO;
    }

    if (!(flags & IFF_UP)) {
        if (baycom_kernel_set_modem_params(km) < 0) {
            fprintf(stderr,
                    "[baycom] SETMODEMPAR failed (need CAP_SYS_RAWIO, iface down)\n");
        }
        if (baycom_kernel_set_mode(km) < 0) {
            fprintf(stderr,
                    "[baycom] SETMODE failed (need CAP_NET_ADMIN, iface down)\n");
        }
        if (baycom_kernel_interface_up(km, 1) < 0) {
            fprintf(stderr, "[baycom] failed to bring %s up\n",
                    km->cfg.interface);
            baycom_kernel_close(km);
            return HYBBX_ERR_IO;
        }
    }

    if (baycom_kernel_set_channel_params(km) < 0) {
        fprintf(stderr, "[baycom] channel params ioctl failed\n");
    }

    if (baycom_kernel_open_packet(km) < 0) {
        baycom_kernel_close(km);
        return HYBBX_ERR_IO;
    }

    *out = km;
    return HYBBX_OK;
}

void baycom_kernel_close(baycom_kernel_modem_t *km)
{
    if (km == NULL) {
        return;
    }

    if (km->pkt_fd >= 0) {
        close(km->pkt_fd);
    }
    if (km->ctl_fd >= 0) {
        close(km->ctl_fd);
    }

    hybbx_baycom_config_free(&km->cfg);
    free(km);
}

hybbx_result_t baycom_kernel_poll(baycom_kernel_modem_t *km)
{
    uint8_t buf[HYBBX_AX25_FRAME_MAX + 64];
    ssize_t n;

    if (km == NULL || km->pkt_fd < 0) {
        return HYBBX_ERR_IO;
    }

    for (;;) {
        n = recv(km->pkt_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return HYBBX_OK;
            }
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_OK;
        }

        if (km->frame_cb != NULL && (size_t)n > 0) {
            km->frame_cb(buf, (size_t)n, km->frame_userdata);
        }
    }
}

hybbx_result_t baycom_kernel_send_frame(baycom_kernel_modem_t *km,
                                        const uint8_t *frame, size_t len)
{
    struct sockaddr_ll dest;
    ssize_t n;

    if (km == NULL || frame == NULL || len == 0 || km->pkt_fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    memset(&dest, 0, sizeof(dest));
    dest.sll_family = AF_PACKET;
    dest.sll_protocol = htons(ETH_P_AX25);
    dest.sll_ifindex = km->ifindex;
    dest.sll_halen = 0;

    n = sendto(km->pkt_fd, frame, len, 0, (struct sockaddr *)&dest,
               sizeof(dest));
    if (n < 0 || (size_t)n != len) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

#else /* !__linux__ */

hybbx_result_t baycom_kernel_open(baycom_kernel_modem_t **out,
                                  const hybbx_baycom_config_t *cfg,
                                  baycom_modem_frame_cb cb, void *userdata)
{
    (void)out;
    (void)cfg;
    (void)cb;
    (void)userdata;
    return HYBBX_ERR_UNSUPPORTED;
}

void baycom_kernel_close(baycom_kernel_modem_t *modem)
{
    (void)modem;
}

hybbx_result_t baycom_kernel_poll(baycom_kernel_modem_t *modem)
{
    (void)modem;
    return HYBBX_ERR_UNSUPPORTED;
}

hybbx_result_t baycom_kernel_send_frame(baycom_kernel_modem_t *modem,
                                        const uint8_t *frame, size_t len)
{
    (void)modem;
    (void)frame;
    (void)len;
    return HYBBX_ERR_UNSUPPORTED;
}

#endif /* __linux__ */
