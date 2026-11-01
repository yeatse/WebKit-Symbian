#include "../overlay/Source/WTF/wtf/symbian/SymbianFilePath.h"
#include <cassert>
#include <cstdio>
#include <string>
using WTF::FileSystemImpl::SymbianFilePath;
unsigned checks=0;
void check(bool value) { ++checks; assert(value); }
void preserved(std::u16string_view text)
{
    SymbianFilePath value { WTF::StringView(text) };
    check(static_cast<bool>(value));
    for (unsigned i=0;i<text.size();++i)check(static_cast<unsigned>(value.data()[i])==text[i]);
    check(value.data()[text.size()]==0);
}
int main()
{
    preserved(u"C:\\Data\\alpha.bin");preserved(u"C:\\Data\\字.dat");preserved(u"C:\\Data\\字😀.dat");preserved(u"C:/Data/字😀.dat");preserved(u"relative-file.dat");preserved(u"C:\\Data\\e\u0301.dat");
    std::u16string maximum(254,u'x');maximum+=u"😀";preserved(maximum);
    SymbianFilePath latin { WTF::StringView(std::string_view("C:\\Data\\a\xe9.dat")) };
    check(static_cast<bool>(latin));check(latin.data()[9]==0xE9);
    errno=0;SymbianFilePath empty { WTF::StringView(std::u16string_view()) };check(!empty);check(!empty.data());check(errno==ENOENT);
    errno=0;SymbianFilePath tooLong { WTF::StringView(std::u16string(257,u'x')) };check(!tooLong);check(!tooLong.data());check(errno==ENAMETOOLONG);
    errno=0;SymbianFilePath embedded { WTF::StringView(std::u16string_view(u"C:\\a\0b",6)) };check(!embedded);check(!embedded.data());check(errno==EINVAL);
    std::u16string source=u"C:\\Data\\字😀.dat";SymbianFilePath copied { WTF::StringView(source) };source[0]='X';check(copied.data()[0]=='C');
    std::printf("actual UTF16 path helper %u string assertions PASS\n",checks);
}
