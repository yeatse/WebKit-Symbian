#include <services/fs/file_share.h>
#include <services/fs/file_attributes.h>
#include <vfs/vfs.h>
#include <common/cvt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace eka2l1;
static unsigned checks;
static void require(bool condition, const char *name) {
    ++checks;
    if (!condition) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
}
static std::string read(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    // Explicit public f32file.h matrix, including both directions. EFileShareAny
    // remains incompatible with ReadersOnly even for a read-only Any handle.
    struct Mode { unsigned mode; bool write; } modes[] = {{0,false},{1,false},{2,false},{3,false},{3,true}};
    const bool expected[5][5] = {
        {false,false,false,false,false},
        {false,true,false,true,false},
        {false,false,true,true,true},
        {false,true,true,true,true},
        {false,false,true,true,true}
    };
    for (unsigned i=0;i<5;++i) {
        for (unsigned j=0;j<5;++j) {
            file_share_state state;
            state.acquire(modes[i].mode,modes[i].write);
            require(state.compatible(modes[j].mode,modes[j].write)==expected[i][j],"official share matrix");
            require(state.use_count==1,"compatibility query does not publish a claim");
            state.release(modes[i].mode,modes[i].write);
            require(state.use_count==0 && state.can_acquire(0,true),"final release allows exclusive");
        }
    }
    file_share_state readers;
    readers.acquire(1,false);readers.acquire(1,false);readers.acquire(3,false);
    require(!readers.compatible(3,true),"multiple readers reject writer");
    readers.release(1,false);
    require(!readers.compatible(2,false),"one remaining reader keeps promotion");
    readers.release(1,false);
    require(readers.compatible(2,true),"last restrictive reader demotes optional reader");
    readers.acquire(2,true);readers.release(2,true);readers.release(3,false);
    require(readers.use_count==0,"all independent open claims released");
    require(!readers.can_acquire(1,true),"ReadersOnly write argument invalid");
    require(requested_read_only_change(1,1)==read_only_change::invalid,"overlapping masks fail");
    require(requested_read_only_change(0x118,0)==read_only_change::unchanged,"immutable attribute bits ignored");
    require(requested_read_only_change(2,0)==read_only_change::unsupported,"unsupported hidden fails explicitly");
    require(requested_read_only_change(1,2)==read_only_change::unsupported,"unsupported combined update is not partial");
    require(requested_read_only_change(1,0)==read_only_change::set,"read-only set mask");
    require(requested_read_only_change(0,1)==read_only_change::clear,"read-only clear mask");

    const auto root=std::filesystem::absolute(argv[1]);
    require(std::filesystem::create_directory(root),"fresh isolated root");
    { std::ofstream f(root/"sentinel.bin",std::ios::binary);f<<"clipboard-sentinel\n"; }
    auto create_io=[&](io_system &io) {
        auto fs=create_physical_filesystem(epocver::epoc95,"test");
        require(io.add_filesystem(fs).has_value(),"actual physical VFS registered");
        require(io.mount_physical_path(drive_c,drive_media::physical,0,common::utf8_to_ucs2(root.string())),"actual physical mount");
    };
    {
        io_system io;create_io(io);
        auto info=io.get_entry_info(u"C:\\sentinel.bin");
        require(info && !(info->attribute & io_attrib_write_protected),"initial writable entry");
        require(io.set_entry_read_only(u"C:\\sentinel.bin",true)==abstract_file_system_err_code::ok,"persist read-only through actual VFS");
        info=io.get_entry_info(u"c:\\SENTINEL.BIN");
        require(info && (info->attribute & io_attrib_write_protected),"case-variant entry reports read-only");
        require(!io.open_file(u"C:\\sentinel.bin",WRITE_MODE|BIN_MODE),"read-only replacement rejected before truncate");
        require(!io.open_file(u"C:\\sentinel.bin",READ_MODE|WRITE_MODE|BIN_MODE),"read-only read-write open rejected");
        require(read(root/"sentinel.bin")=="clipboard-sentinel\n","failed writes preserve exact content");
        auto reader=io.open_file(u"C:\\sentinel.bin",READ_MODE|BIN_MODE);
        require(reader && reader->valid(),"read-only file remains readable");
    }
    {
        io_system io;create_io(io);
        auto info=io.get_entry_info(u"C:\\sentinel.bin");
        require(info && (info->attribute & io_attrib_write_protected),"read-only survives VFS reconstruction");
        require(io.set_entry_read_only(u"C:\\missing.bin",true)==abstract_file_system_err_code::no,"missing file set fails");
        require(io.set_entry_read_only(u"C:\\sentinel.bin",false)==abstract_file_system_err_code::ok,"clear read-only through actual VFS");
        info=io.get_entry_info(u"C:\\sentinel.bin");
        require(info && !(info->attribute & io_attrib_write_protected),"cleared bit read back");
        auto writer=io.open_file(u"C:\\sentinel.bin",READ_MODE|WRITE_MODE|BIN_MODE);
        require(writer && writer->valid(),"cleared file opens writable");
        require(read(root/"sentinel.bin")=="clipboard-sentinel\n","nontruncating reopen preserves content");
        require(writer->seek(5,file_seek_mode::beg)==5,"open-file position before rename");
        require(io.set_entry_read_only(u"C:\\sentinel.bin",true)==abstract_file_system_err_code::ok,"set read-only on existing writer");
        const auto invalid_raw=io.get_raw_path(u"C:\\missing-parent\\renamed.bin");
        require(invalid_raw.has_value(),"uncreated rename destination resolved");
        require(!writer->rename(u"C:\\missing-parent\\renamed.bin",*invalid_raw),"failed rename does not close old FILE");
        require(writer->file_name()==u"C:\\sentinel.bin" && writer->tell()==5 && read(root/"sentinel.bin")=="clipboard-sentinel\n","failed rename preserves name position and bytes");
        const auto renamed_raw=io.get_raw_path(u"C:\\renamed.bin");
        require(renamed_raw.has_value(),"same-drive rename destination resolved");
        require(writer->rename(u"C:\\renamed.bin",*renamed_raw),"read-only open file renamed without reopen");
        require(writer->file_name()==u"C:\\renamed.bin" && writer->tell()==5,"rename retains FILE cursor and updates path");
        info=io.get_entry_info(u"C:\\renamed.bin");
        require(info && (info->attribute & io_attrib_write_protected),"renamed read-only attribute preserved");
        require(!std::filesystem::exists(root/"sentinel.bin") && read(root/"renamed.bin")=="clipboard-sentinel\n","rename preserves content and moves source");
        require(io.set_entry_read_only(u"C:\\renamed.bin",false)==abstract_file_system_err_code::ok,"clear renamed read-only file");
    }
    std::cout << "PASS " << checks << " actual sharing and physical VFS checks\n";
}
