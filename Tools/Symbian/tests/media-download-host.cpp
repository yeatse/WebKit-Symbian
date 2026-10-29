// Host regression probe for the EKA media transfer implementation.
#include <drivers/media_download.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using Transfer = eka2l1::drivers::media_download;

static const char* name(Transfer::result value)
{
    switch (value) {
    case Transfer::result::pending: return "pending";
    case Transfer::result::complete: return "complete";
    case Transfer::result::cancelled: return "cancelled";
    case Transfer::result::invalid_url: return "invalid_url";
    case Transfer::result::network_error: return "network_error";
    case Transfer::result::timeout: return "timeout";
    case Transfer::result::http_error: return "http_error";
    case Transfer::result::tls_error: return "tls_error";
    case Transfer::result::file_error: return "file_error";
    case Transfer::result::resource_error: return "resource_error";
    }
    return "unknown";
}

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    std::string url = argv[1];
    if (argc > 3 && std::string(argv[3]) == "nul")
        url.append("\0ignored", 8);
    auto transfer = Transfer::start(url);
    const auto start = std::chrono::steady_clock::now();
    while (transfer->status() == Transfer::result::pending) {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(8)) {
            transfer->cancel();
            std::cerr << "FAIL: transfer deadline\n";
            return 1;
        }
        if (argc > 3 && std::string(argv[3]) == "cancel" && elapsed > std::chrono::milliseconds(80))
            transfer->cancel();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto status = transfer->status();
    std::string path = transfer->path();
    std::cout << "status=" << name(status) << " http=" << transfer->http_status() << " error=" << transfer->error() << '\n';
    if (std::string(name(status)) != argv[2]) return 1;
    if (status == Transfer::result::complete) {
        std::ifstream file(path, std::ios::binary);
        std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (body != "native-media-test\n") return 1;
    }
    transfer.reset();
    for (unsigned attempt = 0; !path.empty() && std::filesystem::exists(path) && attempt < 200; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!path.empty() && std::filesystem::exists(path)) {
        std::cerr << "FAIL: leaked temporary file\n";
        return 1;
    }
    std::cout << "PASS: media-download\n";
    return 0;
}
