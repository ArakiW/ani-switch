/*
 * ani-switch Switch wrapper (libnx entry point)
 *
 * Adapted from xfangfang/wiliwili (GPL-3.0), which itself credits natinusala,
 * WerWolv, and p-sam for the original implementation.
 *
 * SPDX-License-Identifier: AGPL-3.0
 * Copyright (c) 2026 ani-switch contributors
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <switch.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static int nxlink_sock = -1;
static FILE* startup_log = NULL;
static bool applet_locked = false;
static bool socket_ready = false;
static bool romfs_ready = false;
static bool pl_ready = false;
static bool setsys_ready = false;
static bool set_ready = false;
static bool psm_ready = false;
static bool nifm_ready = false;
static bool lbl_ready = false;
static bool startup_ready = false;

#ifdef ANISWITCH_SWITCH_DEBUG
static void startupLogOpen(void) {
    /* The application directory is the normal deployment location. */
    startup_log = fopen("sdmc:/switch/aniswitch/startup.log", "w");
    if (startup_log == NULL)
        startup_log = fopen("startup.log", "w");
}
#endif

static void startupLog(const char* message) {
#ifdef ANISWITCH_SWITCH_DEBUG
    printf("[ani-switch] %s\n", message);
    fflush(stdout);
    if (startup_log != NULL) {
        fprintf(startup_log, "%s\n", message);
        fflush(startup_log);
    }
#else
    (void)message;
#endif
}

static bool startupResult(const char* stage, Result rc) {
    char message[128];
    snprintf(message, sizeof(message), "%s: result=0x%08lx", stage,
             (unsigned long)rc);
    startupLog(message);
    return R_SUCCEEDED(rc);
}

/* Called from the C++ entry point only in a diagnostic Switch build. */
void aniswitchStartupLog(const char* message) {
    startupLog(message);
}

/* v16.10.8 raw UDP DNS resolver.
 *
 * cpr/curl's newlib gethostbyname hangs on Switch hbmenu applet
 * mode, so the C++ HTTP layer pre-resolves every hostname with
 * this function, rewrites the URL to use the returned IP, and
 * adds a `Host:` header.  Send a single DNS A-query to one of
 * three nameservers, falling through on failure (each with a 5 s
 * timeout):
 *
 *   1. 8.8.8.8      Google Public DNS
 *   2. 223.5.5.5    AliDNS (China-friendly, anycast)
 *   3. 119.29.29.29 DNSPod (China-friendly, anycast)
 *
 * The list is in preference order; the first server to return
 * a valid A record wins.  This is the v19.0.3 improvement —
 * v19.0.2 only used 8.8.8.8 which is routinely blocked /
 * blackholed by domestic ISPs.
 *
 * Returns 0 on success, -1 on any failure (socket/send/recv/parse).
 * Hardcoded fallback IP tables live in C++ code; this function
 * is best-effort. */

#define ANISWITCH_DNS_TIMEOUT_S 5

/* Try one nameserver.  Returns 0 on success, -1 otherwise.
 * `q`, `qlen`, `resp` are caller-allocated 512-byte scratch. */
static int aniswitchTryResolver(
    const char* host,
    const unsigned char* q, int qlen,
    unsigned char* resp,
    char* out_ip, size_t out_len,
    uint32_t ns_addr /* network byte order */) {
    int dfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dfd < 0) return -1;
    struct timeval dtv = { ANISWITCH_DNS_TIMEOUT_S, 0 };
    setsockopt(dfd, SOL_SOCKET, SO_RCVTIMEO, &dtv, sizeof(dtv));
    setsockopt(dfd, SOL_SOCKET, SO_SNDTIMEO, &dtv, sizeof(dtv));
    struct sockaddr_in dsa;
    memset(&dsa, 0, sizeof(dsa));
    dsa.sin_family = AF_INET;
    dsa.sin_port = htons(53);
    dsa.sin_addr.s_addr = ns_addr;
    int sr = sendto(dfd, (const char*)q, qlen, 0, (struct sockaddr*)&dsa, sizeof(dsa));
    if (sr < 0) { close(dfd); return -1; }
    int n = recvfrom(dfd, (char*)resp, 512, 0, NULL, NULL);
    close(dfd);
    if (n < 12) return -1;
    int ancount = (resp[6] << 8) | resp[7];
    if (ancount <= 0) return -1;

    /* Skip the question section. */
    int off = 12;
    while (off < n && resp[off] != 0) {
        if ((resp[off] & 0xC0) == 0xC0) { off += 2; break; }
        off += resp[off] + 1;
    }
    if (off < n) ++off;            /* terminating 0 */
    off += 4;                      /* QTYPE + QCLASS */

    /* Walk answer RRs, take the first A record. */
    for (int a = 0; a < ancount && off < n; ++a) {
        if (off >= n) break;
        if ((resp[off] & 0xC0) == 0xC0) {
            off += 2;
        } else {
            while (off < n && resp[off]) off += resp[off] + 1;
            if (off < n) ++off;
        }
        if (off + 10 > n) break;
        int rtype = (resp[off]   << 8) | resp[off+1];
        int rdlen = (resp[off+8] << 8) | resp[off+9];
        off += 10;
        if (rtype == 1 && rdlen == 4 && off + 4 <= n) {
            snprintf(out_ip, out_len, "%u.%u.%u.%u",
                     resp[off], resp[off+1], resp[off+2], resp[off+3]);
            return 0;
        }
        off += rdlen;
    }
    return -1;
}

int aniswitchResolveHost(const char* host, char* out_ip, size_t out_len) {
    if (!host || !out_ip || out_len < 16) return -1;
    if (out_len > 0) out_ip[0] = 0;

    /* Already a dotted-quad?  Pass through. */
    int dots = 0;
    const char* p = host;
    while (*p) {
        if (*p == '.') ++dots;
        else if (*p < '0' || *p > '9') { dots = -1; break; }
        ++p;
    }
    if (dots == 3) {
        strncpy(out_ip, host, out_len - 1);
        out_ip[out_len - 1] = 0;
        return 0;
    }

    /* Build DNS query: 12-byte header + QNAME labels + QTYPE + QCLASS. */
    unsigned char q[512];
    int qlen = 0;
    /* Header: ID=0x1234, RD=1, QDCOUNT=1, rest 0. */
    q[0]=0x12; q[1]=0x34; q[2]=0x01; q[3]=0x00;
    q[4]=0x00; q[5]=0x01; q[6]=0x00; q[7]=0x00;
    q[8]=0x00; q[9]=0x00; q[10]=0x00; q[11]=0x00;
    qlen = 12;
    const char* seg = host;
    while (*seg && qlen < 480) {
        const char* dot = strchr(seg, '.');
        int len = dot ? (int)(dot - seg) : (int)strlen(seg);
        if (len <= 0 || len > 63) return -1;
        q[qlen++] = (unsigned char)len;
        memcpy(q + qlen, seg, len);
        qlen += len;
        if (!dot) break;
        seg = dot + 1;
    }
    q[qlen++] = 0;          /* root label */
    q[qlen++] = 0; q[qlen++] = 1;  /* QTYPE=A */
    q[qlen++] = 0; q[qlen++] = 1;  /* QCLASS=IN */

    unsigned char resp[512];

    /* Three nameservers, in preference order.  First success wins.
     * Values are pre-computed network byte order (big-endian) for
     * little-endian aarch64 — htonl() is not a constant expression
     * in C so we cannot use it in a static initializer.  The Switch
     * is always little-endian, so a fixed table is fine.
     *
     *   8.8.8.8     -> bytes 08 08 08 08 -> LE uint32 0x08080808
     *   223.5.5.5   -> bytes DF 05 05 05 -> LE uint32 0x050505DF
     *   119.29.29.29-> bytes 77 1D 1D 1D -> LE uint32 0x1D1D1D77
     *
     * 8.8.8.8 is the international default; 223.5.5.5 (AliDNS)
     * and 119.29.29.29 (DNSPod) are China-friendly anycast
     * fallbacks for when 8.8.8.8 is blackholed by the local ISP. */
    static const uint32_t kNameservers[] = {
        0x08080808u,  /* 8.8.8.8      Google */
        0x050505DFu,  /* 223.5.5.5    AliDNS */
        0x1D1D1D77u,  /* 119.29.29.29 DNSPod */
    };
    for (size_t i = 0; i < sizeof(kNameservers)/sizeof(kNameservers[0]); ++i) {
        if (aniswitchTryResolver(host, q, qlen, resp, out_ip, out_len,
                                 kNameservers[i]) == 0) {
            return 0;
        }
    }
    return -1;
}

int aniswitchStartupReady(void) {
    return startup_ready ? 1 : 0;
}

void userAppInit(void) {
#ifdef ANISWITCH_SWITCH_DEBUG
    startupLogOpen();
#endif
    startupLog("userAppInit: begin");

    appletLockExit();
    applet_locked = true;
    startupLog("userAppInit: exit locked");

    /* Initialize BSD sockets.  v16.10.4 switched to
       `socketInitializeDefault()` (3 sessions, sb_efficiency 1)
       because in hbmenu / homebrew-launcher mode the old
       `socketInitialize(&cfg)` with num_bsd_sessions=2 and
       sb_efficiency=1 didn't actually attach a usable network
       profile — every outgoing TCP connect (e.g. to api.bgm.tv)
       returned "Connection timed out" after 15s.  dsh-switch's
       working config (see E:\AI\dsh-switch\src\main.c:33) does
       exactly this.  No nifmInitialize either: it just makes the
       socket path more complex without adding any reachable
       address in hbmenu mode, and a 30s background no-op stays a
       30s no-op. */
    Result rc = socketInitializeDefault();
    if (!startupResult("userAppInit: socketInitialize", rc))
        return;
    socket_ready = true;

#ifdef DEBUG
    nxlink_sock = nxlinkStdio();
    startupLog(nxlink_sock >= 0 ? "userAppInit: nxlink connected"
                                : "userAppInit: nxlink unavailable");
#endif

    /* Filesystem + system services used by borealis / wiliwili-shaped code. */
    rc = romfsInit();
    if (!startupResult("userAppInit: romfsInit", rc))
        return;
    romfs_ready = true;

    rc = plInitialize(PlServiceType_User);
    if (!startupResult("userAppInit: plInitialize", rc))
        return;
    pl_ready = true;

    rc = setsysInitialize();
    if (!startupResult("userAppInit: setsysInitialize", rc))
        return;
    setsys_ready = true;

    rc = setInitialize();
    if (!startupResult("userAppInit: setInitialize", rc))
        return;
    set_ready = true;

    /* These services are used by optional status widgets; keep bootstrap
       alive when a restricted applet cannot acquire one, but record it. */
    rc = psmInitialize();
    if (startupResult("userAppInit: psmInitialize", rc))
        psm_ready = true;

    /* v16.10.4: nifmInitialize removed.  In hbmenu / homebrew applet
       mode calling nifmInitialize(NifmServiceType_User) succeeds but
       never actually grants a network profile to the process, so
       socket connect to e.g. api.bgm.tv hangs for the full
       HTTP::TIMEOUT (15s) and then errors out with
       "Connection timed out".  The system already routes all
       NRO-bound socket traffic through the same BSD stack whether
       nifm was initialised or not, so dropping the call just
       removes a layer that does nothing useful in this mode. */
    nifm_ready = false;

    /* v16.10.5: nifm network diagnostic.  v16.10.3 / v16.10.4
       still report `Connection timed out` to api.bgm.tv, but
       dsh-switch works because it only talks to 127.0.0.1
       (BSD loopback, no internet profile needed).  We need to
       know if the Switch has actually attached an internet
       profile to the current NRO before we can decide whether
       to give up on the live fetch and fall back to the PC-
       prefetched webcache.  Bring nifm back up *only* for the
       diag call (NifmServiceType_System) so the rest of the
       process can stay socket-only and keep working. */
    {
        Result nrc = nifmInitialize(NifmServiceType_System);
        if (!startupResult("userAppInit: nifmInitialize(diag)", nrc)) {
            startupLog("NETDIAG: nifmInitialize failed");
        } else {
            nifm_ready = true;
            NifmNetworkProfileData profile = {0};
            Result pr = nifmGetCurrentNetworkProfile(&profile);
            if (R_SUCCEEDED(pr)) {
                const NifmIpAddressSetting *ipset = &profile.ip_setting_data.ip_address_setting;
                const u8 *ip   = ipset->current_addr.addr;
                const u8 *sub  = ipset->subnet_mask.addr;
                const u8 *gw   = ipset->gateway.addr;
                char buf[160];
                snprintf(buf, sizeof(buf),
                         "NETDIAG: profile ok, ip=%u.%u.%u.%u sn=%u.%u.%u.%u gw=%u.%u.%u.%u",
                         ip[0], ip[1], ip[2], ip[3],
                         sub[0], sub[1], sub[2], sub[3],
                         gw[0], gw[1], gw[2], gw[3]);
                startupLog(buf);
            } else {
                char buf[96];
                snprintf(buf, sizeof(buf),
                         "NETDIAG: no current network profile (rc=0x%x)",
                         (unsigned)pr);
                startupLog(buf);
            }
            nifmExit();
            nifm_ready = false;
        }
    }

    rc = lblInitialize();
    if (!startupResult("userAppInit: lblInitialize", rc))
        return;
    lbl_ready = true;

    startup_ready = true;
    startupLog("userAppInit: ready");

    /* v16.10.8 DNS diagnostic.  cpr/curl's newlib resolver path
       (`gethostbyname` inside libcurl) hangs forever on Switch
       hbmenu applet mode, so the C++ HTTP layer wants to pre-
       resolve every hostname itself and rewrite the URL to use
       the IP directly + a `Host:` header.  To do that we need a
       real resolver.  Try a raw UDP DNS query to 8.8.8.8:53
       (Cloudflare / Google both work); if even that fails we
       can fall back to a hardcoded IP table.  This is purely
       diagnostic — log only, do not change any state. */
    {
        int dfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (dfd < 0) {
            startupLog("NETDIAG3: DNS socket() failed");
        } else {
            struct sockaddr_in dsa;
            memset(&dsa, 0, sizeof(dsa));
            dsa.sin_family = AF_INET;
            dsa.sin_port = htons(53);
            dsa.sin_addr.s_addr = htonl(0x08080808);
            struct timeval dtv = { 3, 0 };
            setsockopt(dfd, SOL_SOCKET, SO_RCVTIMEO, &dtv, sizeof(dtv));
            setsockopt(dfd, SOL_SOCKET, SO_SNDTIMEO, &dtv, sizeof(dtv));
            /* Build a tiny DNS A-query for "api.bgm.tv" using a
               well-known 16-bit ID (0x1234) and 1 question. */
            static const unsigned char dns_q[] = {
                /* header */
                0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                /* question: 3 a, p, i, 4 b, g, m, 3 t, v, 0 */
                0x03, 'a','p','i', 0x04, 'b','g','m', 0x02, 't','v', 0x00,
                /* QTYPE=A(1) QCLASS=IN(1) */
                0x00, 0x01, 0x00, 0x01
            };
            int sr = sendto(dfd, dns_q, sizeof(dns_q), 0,
                            (struct sockaddr*)&dsa, sizeof(dsa));
            if (sr < 0) {
                startupLog("NETDIAG3: DNS sendto failed");
            } else {
                unsigned char resp[512];
                int n = recvfrom(dfd, resp, sizeof(resp), 0, NULL, NULL);
                if (n < 0) {
                    startupLog("NETDIAG3: DNS recvfrom timeout/fail");
                } else {
                    /* Find the first A record answer.  Skip the
                       question section (12-byte header + QNAME
                       + 4-byte QTYPE+QCLASS).  Parse answer RRs
                       by walking the compressed/label-encoded
                       name fields.  For the diagnostic only. */
                    int off = 12;
                    while (off < n && resp[off] != 0) {
                        if ((resp[off] & 0xC0) == 0xC0) { off += 2; break; }
                        off += resp[off] + 1;
                    }
                    off += 5; /* skip terminating 0, QTYPE, QCLASS */
                    int ancount = (resp[6] << 8) | resp[7];
                    char out[64] = "no-A-record";
                    for (int a = 0; a < ancount && off < n; ++a) {
                        if ((resp[off] & 0xC0) == 0xC0) off += 2;
                        else while (off < n && resp[off]) off += resp[off] + 1;
                        if (off + 10 > n) break;
                        int rtype  = (resp[off]   << 8) | resp[off+1];
                        int rdlen  = (resp[off+8] << 8) | resp[off+9];
                        off += 10;
                        if (rtype == 1 && rdlen == 4 && off + 4 <= n) {
                            snprintf(out, sizeof(out), "%u.%u.%u.%u",
                                     resp[off], resp[off+1], resp[off+2], resp[off+3]);
                            break;
                        }
                        off += rdlen;
                    }
                    char buf[96];
                    snprintf(buf, sizeof(buf), "NETDIAG3: DNS api.bgm.tv -> %s", out);
                    startupLog(buf);
                }
            }
            close(dfd);
        }
    }

    /* v16.10.7 raw-socket diagnostic.  cpr/curl/mbedTLS path
       still hangs after 5s even with SSL verify off; the
       "Connection timed out" fires *before* the SSL handshake
       (CONNECTION_TIMEOUT is 5s) which means TCP connect to
       api.bgm.tv:443 is what's failing, not SSL.  This rules
       the SSL-CA theory out.  Test the BSD socket layer
       directly with the loopback address returned by NETDIAG
       so we know whether Switch's BSD stack can even open
       a TCP connection out to the LAN.  Pure libnx, no
       libcurl/mbedTLS. */
    {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            startupLog("NETDIAG2: socket() failed");
        } else {
            struct sockaddr_in sa;
            memset(&sa, 0, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_port = htons(80);
            /* Use a public IP we know cpr/curl can resolve
               (Cloudflare 1.1.1.1) so we sidestep any DNS
               resolver problem on the Switch side. */
            sa.sin_addr.s_addr = htonl(0x01010101);
            struct timeval tv = { 5, 0 };
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            int r = connect(s, (struct sockaddr*)&sa, sizeof(sa));
            if (r == 0) {
                startupLog("NETDIAG2: TCP connect 1.1.1.1:80 OK");
                close(s);
            } else {
                int err = errno;
                startupLog("NETDIAG2: TCP connect 1.1.1.1:80 failed, errno=-1");
                close(s);
                (void)err;
            }
        }
    }
}

void userAppExit(void) {
    startupLog("userAppExit: begin");

    if (lbl_ready) {
        lblExit();
        lbl_ready = false;
    }
    if (nifm_ready) {
        nifmExit();
        nifm_ready = false;
    }
    if (psm_ready) {
        psmExit();
        psm_ready = false;
    }
    if (set_ready) {
        setExit();
        set_ready = false;
    }
    if (setsys_ready) {
        setsysExit();
        setsys_ready = false;
    }
    if (pl_ready) {
        plExit();
        pl_ready = false;
    }
    if (romfs_ready) {
        romfsExit();
        romfs_ready = false;
    }

    if (nxlink_sock != -1) {
        close(nxlink_sock);
        nxlink_sock = -1;
    }
    if (socket_ready) {
        socketExit();
        socket_ready = false;
    }
    if (applet_locked) {
        appletUnlockExit();
        applet_locked = false;
    }

    startup_ready = false;
    startupLog("userAppExit: complete");
    if (startup_log != NULL) {
        fclose(startup_log);
        startup_log = NULL;
    }
}
