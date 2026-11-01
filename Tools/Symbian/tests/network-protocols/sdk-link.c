/* Compile/link acceptance against the actual Open C and mbedTLS SDK ABI.
 * This executable is not run by the host acceptance suite. */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <curl/curl.h>
#include <nghttp2/nghttp2.h>
#include <stddef.h>

_Static_assert(AF_INET6 == 0x0806, "Symbian IPv6 family value");
_Static_assert(sizeof(((struct sockaddr_in6 *)0)->sin6_family) == 2,
               "Open C socket family must retain 16 bits");
_Static_assert(sizeof(((struct sockaddr_in6 *)0)->sin6_addr) == 16,
               "IPv6 address width");

int main(void)
{
    struct sockaddr_in6 address = { 0 };
    address.sin6_family = AF_INET6;
    address.sin6_scope_id = 0;
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    if(!(info->features & CURL_VERSION_IPV6) || !(info->features & CURL_VERSION_HTTP2))
        return 1;
    if(!nghttp2_version(0))
        return 2;
    CURL *easy = curl_easy_init();
    curl_easy_setopt(easy, CURLOPT_URL, "https://[::1]/");
    curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, 1L);
    CURLcode result = curl_easy_perform(easy);
    curl_easy_cleanup(easy);
    return result || !address.sin6_family;
}
