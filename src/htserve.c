/* htserve - minimal C99 static-file HTTP server, port 808 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <process.h>
  #include <direct.h>
  #define chdir _chdir
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define THREAD_RET unsigned __stdcall
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <pthread.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define INVALID_SOCKET (-1)
  #define THREAD_RET void*
#endif

#define PORT 8880
#define REQ_MAX 4096
#define CHUNK 65536

static const struct { const char *ext, *mime; } MIME[] = {
    {".html","text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".js",  "application/javascript; charset=utf-8"},
    {".mjs", "application/javascript; charset=utf-8"},
    {".json","application/json; charset=utf-8"},
    {".wasm","application/wasm"},
    {".svg", "image/svg+xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg","image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain; charset=utf-8"},
    {".data","application/octet-stream"},
};

static const char *mime_for(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    for (size_t i = 0; i < sizeof(MIME)/sizeof(MIME[0]); i++) {
        size_t n = strlen(MIME[i].ext);
        if (strlen(dot) == n) {
            int eq = 1;
            for (size_t j = 0; j < n; j++)
                if (tolower((unsigned char)dot[j]) != MIME[i].ext[j]) { eq = 0; break; }
            if (eq) return MIME[i].mime;
        }
    }
    return "application/octet-stream";
}

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s) {
    char *o = s;
    for (char *p = s; *p; ) {
        if (*p == '%' && p[1] && p[2]) {
            int a = hexv((unsigned char)p[1]), b = hexv((unsigned char)p[2]);
            if (a >= 0 && b >= 0) { *o++ = (char)((a << 4) | b); p += 3; continue; }
        }
        *o++ = *p++;
    }
    *o = 0;
}

static void send_all(sock_t s, const char *buf, size_t n) {
    while (n) {
        int sent = send(s, buf, (int)n, 0);
        if (sent <= 0) return;
        buf += sent; n -= (size_t)sent;
    }
}

static void send_status(sock_t s, const char *line, const char *body) {
    char hdr[256];
    int len = (int)strlen(body);
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %s\r\nContent-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n", line, len);
    send_all(s, hdr, (size_t)hl);
    send_all(s, body, (size_t)len);
}

/* dynamic buffer for building the listing HTML */
typedef struct { char *p; size_t n, cap; } buf_t;
static int buf_grow(buf_t *b, size_t need) {
    if (b->n + need <= b->cap) return 0;
    size_t c = b->cap ? b->cap : 8192;
    while (c < b->n + need) c *= 2;
    char *np = (char*)realloc(b->p, c);
    if (!np) return -1;
    b->p = np; b->cap = c;
    return 0;
}
static void buf_append(buf_t *b, const char *s, size_t n) {
    if (buf_grow(b, n) != 0) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
}
static void buf_str(buf_t *b, const char *s) { buf_append(b, s, strlen(s)); }

static void buf_html_escape(buf_t *b, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&': buf_str(b, "&amp;"); break;
            case '<': buf_str(b, "&lt;"); break;
            case '>': buf_str(b, "&gt;"); break;
            case '"': buf_str(b, "&quot;"); break;
            default: buf_append(b, s, 1);
        }
    }
}
static void buf_url_encode(buf_t *b, const char *s) {
    static const char *hex = "0123456789ABCDEF";
    for (const unsigned char *u = (const unsigned char*)s; *u; u++) {
        unsigned char c = *u;
        int safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                   c == '.' || c == '~' || c == '/';
        if (safe) buf_append(b, (const char*)&c, 1);
        else {
            char esc[3] = { '%', hex[c >> 4], hex[c & 15] };
            buf_append(b, esc, 3);
        }
    }
}

typedef struct { char *name; int is_dir; } entry_t;
static int entry_cmp(const void *a, const void *b) {
    const entry_t *x = (const entry_t*)a, *y = (const entry_t*)b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
#ifdef _WIN32
    return _stricmp(x->name, y->name);
#else
    return strcasecmp(x->name, y->name);
#endif
}
static int has_html_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
#ifdef _WIN32
    return _stricmp(dot, ".html") == 0 || _stricmp(dot, ".htm") == 0;
#else
    return strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0;
#endif
}

static void send_listing(sock_t s, const char *fs_dir, const char *url_dir) {
    DIR *d = opendir(fs_dir);
    if (!d) { send_status(s, "404 Not Found", "not found\n"); return; }

    entry_t *ents = NULL;
    size_t en = 0, ecap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", fs_dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (en == ecap) {
            ecap = ecap ? ecap * 2 : 32;
            ents = (entry_t*)realloc(ents, ecap * sizeof(entry_t));
            if (!ents) { closedir(d); send_status(s, "500 Internal Server Error", "oom\n"); return; }
        }
        ents[en].name = strdup(de->d_name);
        ents[en].is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        en++;
    }
    closedir(d);
    qsort(ents, en, sizeof(entry_t), entry_cmp);

    buf_t body = {0};
    buf_str(&body, "<!doctype html><meta charset=\"utf-8\"><title>");
    buf_html_escape(&body, url_dir);
    buf_str(&body, "</title><h1>");
    buf_html_escape(&body, url_dir);
    buf_str(&body, "</h1><ul>");
    if (strcmp(url_dir, "/") != 0) {
        buf_str(&body, "<li><a href=\"../\">../</a></li>");
    }
    for (size_t i = 0; i < en; i++) {
        buf_str(&body, "<li>");
        int bold = !ents[i].is_dir && has_html_ext(ents[i].name);
        if (bold) buf_str(&body, "<b>");
        buf_str(&body, "<a href=\"");
        buf_url_encode(&body, ents[i].name);
        if (ents[i].is_dir) buf_str(&body, "/");
        buf_str(&body, "\">");
        buf_html_escape(&body, ents[i].name);
        if (ents[i].is_dir) buf_str(&body, "/");
        buf_str(&body, "</a>");
        if (bold) buf_str(&body, "</b>");
        buf_str(&body, "</li>");
        free(ents[i].name);
    }
    free(ents);
    buf_str(&body, "</ul>");

    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n", body.n);
    send_all(s, hdr, (size_t)hl);
    send_all(s, body.p, body.n);
    free(body.p);
}

static THREAD_RET handle_conn(void *arg) {
    sock_t s = *(sock_t*)arg;
    free(arg);

#ifdef _WIN32
    DWORD tmo = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));
#else
    struct timeval tmo = {5, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tmo, sizeof(tmo));
#endif

    char req[REQ_MAX + 1];
    int total = 0;
    while (total < REQ_MAX) {
        int n = recv(s, req + total, REQ_MAX - total, 0);
        if (n <= 0) goto done;
        total += n;
        req[total] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    req[total] = 0;

    if (strncmp(req, "GET ", 4) != 0) { send_status(s, "405 Method Not Allowed", "method\n"); goto done; }
    char *p = req + 4;
    char *sp = strchr(p, ' ');
    if (!sp) { send_status(s, "400 Bad Request", "bad\n"); goto done; }
    *sp = 0;

    char *q = strchr(p, '?');
    if (q) *q = 0;

    url_decode(p);

    if (strstr(p, "..") || strchr(p, '\\') || strchr(p, ':')) {
        send_status(s, "400 Bad Request", "bad path\n"); goto done;
    }

    while (*p == '/') p++;

    char path[1024];
    int is_dir_req = (*p == 0) || (p[strlen(p)-1] == '/');
    if (*p == 0) {
        snprintf(path, sizeof(path), "index.html");
    } else if (is_dir_req) {
        snprintf(path, sizeof(path), "%sindex.html", p);
    } else {
        snprintf(path, sizeof(path), "%s", p);
    }

    printf("GET /%s\n", path); fflush(stdout);
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (is_dir_req) {
            char fs_dir[1024], url_dir[1024];
            if (*p == 0) { snprintf(fs_dir, sizeof(fs_dir), "."); snprintf(url_dir, sizeof(url_dir), "/"); }
            else { snprintf(fs_dir, sizeof(fs_dir), "%s", p); snprintf(url_dir, sizeof(url_dir), "/%s", p); }
            printf("  -> listing %s\n", fs_dir); fflush(stdout);
            send_listing(s, fs_dir, url_dir);
            goto done;
        }
        printf("  -> 404\n"); fflush(stdout); send_status(s, "404 Not Found", "not found\n"); goto done;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); send_status(s, "500 Internal Server Error", "seek\n"); goto done; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); send_status(s, "500 Internal Server Error", "tell\n"); goto done; }
    rewind(f);

    char hdr[512];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n"
        "Connection: close\r\n\r\n", mime_for(path), sz);
    send_all(s, hdr, (size_t)hl);

    char *buf = (char*)malloc(CHUNK);
    if (buf) {
        size_t r;
        while ((r = fread(buf, 1, CHUNK, f)) > 0) send_all(s, buf, r);
        free(buf);
    }
    fclose(f);

done:
    CLOSESOCK(s);
    return 0;
}

static void spawn(sock_t client) {
    sock_t *arg = (sock_t*)malloc(sizeof(sock_t));
    if (!arg) { CLOSESOCK(client); return; }
    *arg = client;
#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0, handle_conn, arg, 0, NULL);
    if (!h) { free(arg); CLOSESOCK(client); return; }
    CloseHandle((HANDLE)h);
#else
    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_conn, arg) != 0) { free(arg); CLOSESOCK(client); return; }
    pthread_detach(tid);
#endif
}

int main(int argc, char **argv) {
    const char *public_path = ".";
    if (argc == 1) {
        /* default */
    } else if (argc == 3 && strcmp(argv[1], "--public") == 0) {
        public_path = argv[2];
    } else {
        fprintf(stderr, "usage: htserve [--public <path>]\n");
        return 1;
    }
    if (chdir(public_path) != 0) {
        fprintf(stderr, "cannot chdir to %s\n", public_path);
        return 1;
    }

#ifdef _WIN32
    WSADATA w;
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) { fprintf(stderr, "WSAStartup failed\n"); return 1; }
#endif

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { fprintf(stderr, "socket failed\n"); return 1; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr*)&a, sizeof(a)) != 0) { fprintf(stderr, "bind :%d failed\n", PORT); return 1; }
    if (listen(srv, 64) != 0) { fprintf(stderr, "listen failed\n"); return 1; }

    printf("serving %s on http://localhost:%d/\n", public_path, PORT);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in ca;
#ifdef _WIN32
        int cl = sizeof(ca);
#else
        socklen_t cl = sizeof(ca);
#endif
        sock_t c = accept(srv, (struct sockaddr*)&ca, &cl);
        if (c == INVALID_SOCKET) continue;
        spawn(c);
    }
}
