// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only metadata frontend using the existing EKA2L1 E32 parser.
// Does not load/execute an image or alter its header bytes.
#include <common/buffer.h>
#include <loader/e32img.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>

static std::uint32_t U32(const std::vector<char>& bytes, std::size_t at)
{
    return static_cast<std::uint8_t>(bytes[at])
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[at + 1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[at + 2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[at + 3])) << 24);
}

static bool Check(const eka2l1::loader::e32img& image)
{
    const auto& h = image.header;
    const auto& bytes = image.data;
    const auto within = [&](std::uint64_t at, std::uint64_t length) {
        return at <= bytes.size() && length <= bytes.size() - at;
    };
    if (h.code_offset < 156 || !within(0, h.code_offset)
        || !within(h.code_offset, h.code_size) || ((h.flags >> 28) & 15) != 1)
        return false;
    // Standard Symbian header CRC: polynomial 0xedb88320, init zero,
    // no complement; the CRC field is logically 0xc90fdaa2 during calculation.
    std::uint32_t crc = 0;
    for (std::size_t i = 0; i < h.code_offset; ++i) {
        const auto value = i >= 20 && i < 24
            ? static_cast<std::uint8_t>(0xc90fdaa2U >> ((i - 20) * 8))
            : static_cast<std::uint8_t>(bytes[i]);
        crc ^= value;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
    }
    if (crc != h.header_crc || h.dll_ref_table_count == 0
        || image.import_section.imports.size() != h.dll_ref_table_count
        || !within(h.import_offset, 4))
        return false;
    const std::uint32_t size = U32(bytes, h.import_offset);
    if (size < 4 || size % 4 || !within(h.import_offset, size))
        return false;
    const std::size_t end = static_cast<std::size_t>(h.import_offset) + size;
    std::size_t cursor = static_cast<std::size_t>(h.import_offset) + 4;
    std::set<std::uint32_t> offsets;
    std::set<std::string> names;
    std::vector<std::size_t> name_positions;
    for (const auto& block : image.import_section.imports) {
        if (cursor > end || end - cursor < 8 || block.number_of_imports <= 0)
            return false;
        const auto name_offset = U32(bytes, cursor);
        const auto count = U32(bytes, cursor + 4);
        cursor += 8;
        if (count != block.ordinals.size() || count != static_cast<std::uint32_t>(block.number_of_imports)
            || count > (end - cursor) / 4 || name_offset >= size)
            return false;
        const auto name_at = static_cast<std::size_t>(h.import_offset) + name_offset;
        name_positions.push_back(name_at);
        std::size_t name_end = name_at;
        while (name_end < end && bytes[name_end])
            ++name_end;
        if (name_end == end)
            return false;
        const std::string name(bytes.data() + name_at, name_end - name_at);
        if (name.empty() || name != block.dll_name || !names.insert(name).second)
            return false;
        for (std::size_t j = 0; j < count; ++j) {
            const auto offset = U32(bytes, cursor);
            cursor += 4;
            if (offset != block.ordinals[j] || offset % 4 || offset > h.code_size
                || h.code_size - offset < 4 || !offsets.insert(offset).second
                || !(U32(bytes, static_cast<std::size_t>(h.code_offset) + offset) & 65535))
                return false;
        }
    }
    for (const auto at : name_positions) {
        if (at < cursor)
            return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2 || std::filesystem::file_size(argv[1]) > 128U * 1024U * 1024U)
        return 2;
    eka2l1::common::ro_std_file_stream stream(argv[1], true);
    if (!stream.valid())
        return 3;
    const auto image = eka2l1::loader::parse_e32img(&stream, true);
    if (!image || !Check(*image))
        return 4;
    std::cout << "{\"status\":\"PASS\",\"uid3\":" << image->header.uid3
              << ",\"flags\":" << image->header.flags
              << ",\"capabilities\":" << image->header_extended.info.cap1
              << ",\"header_crc\":" << image->header.header_crc
              << ",\"header_crc_verified\":true,\"import_bounds_and_code_slots_verified\":true"
              << ",\"uid_checksum_verified_by_EKA\":true"
              << ",\"dll_count\":" << image->header.dll_ref_table_count
              << ",\"imports\":[";
    bool first = true;
    for (const auto& block : image->import_section.imports) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"dll\":" << std::quoted(block.dll_name)
                  << ",\"slots\":" << block.number_of_imports << '}';
    }
    std::cout << "]}\n";
    return 0;
}
