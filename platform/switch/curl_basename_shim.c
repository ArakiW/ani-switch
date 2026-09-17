// SPDX-License-Identifier: AGPL-3.0
//
// v18.0: vendored curl 8.4.0 (mbedTLS backend) calls the POSIX
// basename(3) helper from lib/mime.c (curl_mime_filedata path
// stripping) and the newlib C library we ship for the Switch
// does not export it — the link fails with:
//
//   undefined reference to `basename'
//
// GNU libc has two variants, basename() and basename() with
// GNU basename(_POSIX_C_SOURCE) versus the XPG/ISO-C
// basename().  The cpr/curl code uses the GNU form (modifies
// the input in place) and our shim matches that — enough for
// cURL to strip a trailing "/" and walk back to the last path
// component.  This is only compiled into the Switch build;
// Linux / macOS / Windows get their own libc.

#include <string.h>

char* basename(char* path) {
    if (!path || !*path) {
        static char dot = '.';
        return &dot;
    }
    // Strip trailing slashes.
    size_t len = strlen(path);
    char* end = path + len - 1;
    while (end > path && *end == '/') {
        *end = '\0';
        --end;
    }
    // Walk back to the last separator.
    char* p = end;
    while (p > path && *(p - 1) != '/') {
        --p;
    }
    return p;
}
