#include "curl_setup.h"
#include <curl/curl.h>
#include "curl_addrinfo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static size_t receive_body(char *data, size_t size, size_t count, void *user)
{
    size_t bytes = size * count;
    size_t *received = user;
    for(size_t i = 0; i < bytes; ++i)
        assert(data[i] == 'x');
    *received += bytes;
    return bytes;
}

static CURL *configure(const char *url, const char *ca, size_t *received)
{
    CURL *easy = curl_easy_init();
    assert(easy);
    assert(!curl_easy_setopt(easy, CURLOPT_URL, url));
    assert(!curl_easy_setopt(easy, CURLOPT_CAINFO, ca));
    assert(!curl_easy_setopt(easy, CURLOPT_PROXY, ""));
    assert(!curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L));
    assert(!curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 3L));
    assert(!curl_easy_setopt(easy, CURLOPT_TIMEOUT, 15L));
    assert(!curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS));
    assert(!curl_easy_setopt(easy, CURLOPT_PIPEWAIT, 1L));
    assert(!curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, receive_body));
    assert(!curl_easy_setopt(easy, CURLOPT_WRITEDATA, received));
    return easy;
}

static void resolved(const char *host, int wanted4, int wanted6)
{
    struct addrinfo hints = { 0 };
    struct Curl_addrinfo *list = NULL;
    hints.ai_socktype = SOCK_STREAM;
    assert(!Curl_getaddrinfo_ex(host, "443", &hints, &list));
    int n4 = 0, n6 = 0;
    for(struct Curl_addrinfo *p = list; p; p = p->ai_next) {
        if(p->ai_family == AF_INET) {
            assert(p->ai_addrlen == sizeof(struct sockaddr_in));
            ++n4;
        } else if(p->ai_family == AF_INET6) {
            assert(p->ai_addrlen == sizeof(struct sockaddr_in6));
            ++n6;
        } else
            assert(0);
    }
    assert((n4 > 0) == wanted4 && (n6 > 0) == wanted6);
    Curl_freeaddrinfo(list);
    printf("PASS family resolver %s IPv4=%d IPv6=%d\n", host, n4, n6);
}

static void single(const char *ca, const char *host, int port,
                   const char *mapping, const char *expected_ip)
{
    char url[256];
    snprintf(url, sizeof(url), "https://%s:%d/plain", host, port);
    size_t received = 0;
    CURL *easy = configure(url, ca, &received);
    struct curl_slist *resolve = mapping ? curl_slist_append(NULL, mapping) : NULL;
    if(resolve)
        assert(!curl_easy_setopt(easy, CURLOPT_RESOLVE, resolve));
    CURLcode result = curl_easy_perform(easy);
    if(result)
        fprintf(stderr, "transfer %s: %s\n", url, curl_easy_strerror(result));
    assert(!result);
    long version = 0, status = 0;
    char *ip = NULL;
    assert(!curl_easy_getinfo(easy, CURLINFO_HTTP_VERSION, &version));
    assert(!curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status));
    assert(!curl_easy_getinfo(easy, CURLINFO_PRIMARY_IP, &ip));
    assert(status == 200 && version == CURL_HTTP_VERSION_1_1 && received == 16);
    assert(!strcmp(ip, expected_ip));
    printf("PASS HTTPS %s address=%s ALPN fallback=http/1.1\n", host, ip);
    curl_easy_cleanup(easy);
    curl_slist_free_all(resolve);
}

static void pause_io(void)
{
    const struct timespec delay = { 0, 1000000 };
    nanosleep(&delay, NULL);
}

static void send_all(CURL *easy, const void *bytes, size_t length)
{
    const unsigned char *data = bytes;
    for(int attempts = 0; length && attempts < 10000; ++attempts) {
        size_t sent = 0;
        CURLcode result = curl_easy_send(easy, data, length, &sent);
        assert(result == CURLE_OK || result == CURLE_AGAIN);
        data += sent;
        length -= sent;
        if(result == CURLE_AGAIN)
            pause_io();
    }
    assert(!length);
}

static void wss(const char *ca, int port)
{
    char url[256], request[512], reply[1024] = { 0 };
    snprintf(url, sizeof(url), "https://[::1]:%d/socket", port);
    size_t ignored = 0;
    CURL *easy = configure(url, ca, &ignored);
    assert(!curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1));
    assert(!curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 1L));
    assert(!curl_easy_perform(easy));
    snprintf(request, sizeof(request),
        "GET /socket HTTP/1.1\r\nHost: [::1]:%d\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n", port);
    send_all(easy, request, strlen(request));
    size_t used = 0;
    for(int attempts = 0; !strstr(reply, "\r\n\r\n") && attempts < 10000; ++attempts) {
        size_t received = 0;
        CURLcode result = curl_easy_recv(easy, reply + used, sizeof(reply) - used - 1, &received);
        assert(result == CURLE_OK || result == CURLE_AGAIN);
        used += received;
        if(result == CURLE_AGAIN)
            pause_io();
    }
    assert(strstr(reply, "HTTP/1.1 101 Switching Protocols\r\n"));
    assert(strstr(reply, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    const unsigned char frame[] = { 0x81, 0x84, 1, 2, 3, 4,
                                    'p' ^ 1, 'i' ^ 2, 'n' ^ 3, 'g' ^ 4 };
    send_all(easy, frame, sizeof(frame));
    unsigned char echo[6] = { 0 };
    used = 0;
    for(int attempts = 0; used < sizeof(echo) && attempts < 10000; ++attempts) {
        size_t received = 0;
        CURLcode result = curl_easy_recv(easy, echo + used, sizeof(echo) - used, &received);
        assert(result == CURLE_OK || result == CURLE_AGAIN);
        used += received;
        if(result == CURLE_AGAIN)
            pause_io();
    }
    assert(used == sizeof(echo) && !memcmp(echo, "\x81\x04ping", 6));
    curl_easy_cleanup(easy);
    puts("PASS IPv6 WSS TLS verified, HTTP/1.1 Upgrade and masked text echo");
}

static void multiplex(const char *ca, int port)
{
    enum { requests = 12 };
    CURL *easy[requests];
    size_t received[requests] = { 0 };
    CURLM *multi = curl_multi_init();
    assert(multi);
    assert(!curl_multi_setopt(multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX));
    assert(!curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 1L));
    assert(!curl_multi_setopt(multi, CURLMOPT_MAX_HOST_CONNECTIONS, 1L));
    assert(!curl_multi_setopt(multi, CURLMOPT_MAX_CONCURRENT_STREAMS, 8L));
    for(int i = 0; i < requests; ++i) {
        char url[256];
        snprintf(url, sizeof(url), "https://[::1]:%d/item/%d", port, i);
        easy[i] = configure(url, ca, &received[i]);
        assert(!curl_multi_add_handle(multi, easy[i]));
    }
    int running = 0;
    do {
        assert(!curl_multi_perform(multi, &running));
        if(running)
            assert(!curl_multi_poll(multi, NULL, 0, 100, NULL));
    } while(running);
    int pending = 0, complete = 0;
    CURLMsg *message;
    while((message = curl_multi_info_read(multi, &pending))) {
        assert(message->msg == CURLMSG_DONE);
        if(message->data.result)
            fprintf(stderr, "HTTP/2: %s\n", curl_easy_strerror(message->data.result));
        assert(!message->data.result);
        ++complete;
    }
    assert(complete == requests);
    long connections = 0;
    for(int i = 0; i < requests; ++i) {
        long version = 0, connects = 0;
        assert(!curl_easy_getinfo(easy[i], CURLINFO_HTTP_VERSION, &version));
        assert(!curl_easy_getinfo(easy[i], CURLINFO_NUM_CONNECTS, &connects));
        assert(version == CURL_HTTP_VERSION_2_0);
        assert(received[i] == (i ? 16384 : 512 * 1024));
        connections += connects;
        assert(!curl_multi_remove_handle(multi, easy[i]));
        curl_easy_cleanup(easy[i]);
    }
    assert(connections == 1);
    curl_multi_cleanup(multi);
    printf("PASS HTTP/2 TLS ALPN: %d responses, one IPv6 TLS connection; 512 KiB response crosses flow-control window\n", requests);
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    assert(!curl_global_init(CURL_GLOBAL_DEFAULT));
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    assert(info->features & CURL_VERSION_IPV6);
    assert(info->features & CURL_VERSION_HTTP2);
    printf("curl=%s TLS=%s nghttp2=%s\n", info->version, info->ssl_version, info->nghttp2_version);
    resolved("::1", 0, 1);
    resolved("127.0.0.1", 1, 0);
    resolved("localhost", 1, 1);
    int ipv6 = atoi(argv[2]), ipv4 = atoi(argv[3]), http2 = atoi(argv[4]);
    single(argv[1], "[::1]", ipv6, NULL, "::1");
    single(argv[1], "localhost", ipv6, NULL, "::1");
    char mapping[256];
    snprintf(mapping, sizeof(mapping), "fallback.test:%d:[::1],127.0.0.1", ipv4);
    single(argv[1], "fallback.test", ipv4, mapping, "127.0.0.1");
    wss(argv[1], ipv6);
    multiplex(argv[1], http2);
    curl_global_cleanup();
}
