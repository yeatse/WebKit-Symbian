// Fixed normal file-API contract. No UI, enumeration or diagnostics.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <e32base.h>
#include <f32file.h>
#include <mbedtls/sha256.h>
static_assert(sizeof(wchar_t)==2 && alignof(wchar_t)==2 && sizeof(TText)==2);
static_assert(sizeof(struct stat)==96 && sizeof(off_t)==8 && sizeof(((struct stat*)nullptr)->st_size)==8);
constexpr wchar_t emojiName[]=L"字\U0001F600.dat";
static_assert(emojiName[0]==0x5b57 && emojiName[1]==0xd83d && emojiName[2]==0xde00);
namespace {
constexpr unsigned maximumBytes=2048;
const char* const narrowNames[]={"alpha.bin","zero.txt","字\xF0\x9F\x98\x80.dat"};
const wchar_t* const wideNames[]={L"alpha.bin",L"zero.txt",emojiName};
constexpr unsigned sizes[]={1024,0,24};
const char* const hashes[]={"785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","8a4a90e74c3d9c754d0f3ab6b391646f7de56516d70a5ad20c05e49030a481da"};
const unsigned char unicodeBytes[]={0x42,0x65,0x6c,0x6c,0x65,0x20,0xe6,0x96,0x87,0xe4,0xbb,0xb6,0xe4,0xb8,0x8a,0xe4,0xbc,0xa0,0x20,0xf0,0x9f,0x98,0x80,0x0a};
unsigned passed=0,failed=0;
const char* Bool(bool b){return b?"true":"false";}
struct Outcome{int metadata=-999,metadataErrno=0,open=-999,openErrno=0,read=-999,readErrno=0,close=-999;long long size=-1;unsigned bytes=0;bool regular=false,exact=false,hashed=false;char sha256[65]={};};
void CheckBytes(Outcome&r,unsigned index,const unsigned char*bytes)
{
 r.exact=r.bytes==sizes[index];for(unsigned i=0;r.exact&&i<r.bytes;++i)r.exact=bytes[i]==(index==0?static_cast<unsigned char>(i):unicodeBytes[i]);
 unsigned char hash[32];int e=mbedtls_sha256(bytes,r.bytes,hash,0);r.hashed=!e;
 if(!e){const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;++i){r.sha256[2*i]=hex[hash[i]>>4];r.sha256[2*i+1]=hex[hash[i]&15];}r.sha256[64]=0;r.hashed=!strcmp(r.sha256,hashes[index]);}
}
void Report(const char*api,unsigned index,Outcome&r)
{
 bool pass=!r.metadata&&!r.open&&!r.read&&!r.close&&r.regular&&r.size==sizes[index]&&r.exact&&r.hashed;
 printf("CASE {\"api\":\"%s\",\"file\":%u,\"pass\":%s,\"metadata_rc\":%d,\"metadata_errno\":%d,\"open_rc\":%d,\"open_errno\":%d,\"read_rc\":%d,\"read_errno\":%d,\"close_rc\":%d,\"size\":%lld,\"bytes\":%u,\"regular\":%s,\"exact\":%s,\"hash_ok\":%s,\"sha256\":\"%s\"}\n",api,index,Bool(pass),r.metadata,r.metadataErrno,r.open,r.openErrno,r.read,r.readErrno,r.close,r.size,r.bytes,Bool(r.regular),Bool(r.exact),Bool(r.hashed),r.sha256);fflush(stdout);pass?++passed:++failed;
}
bool Token(char*token)
{
 TBuf<128> line;User::CommandLine(line);line.Trim();if(line.Length()<6||line.Left(6)!=_L("--run "))return false;TPtrC rest=line.Mid(6);if(rest.Length()<8||rest.Length()>64)return false;
 for(TInt i=0;i<rest.Length();++i){TUint c=rest[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;token[i]=static_cast<char>(c);}token[rest.Length()]=0;return true;
}
void Native(RFs&fs,int connected,const wchar_t*path,unsigned index)
{
 Outcome r;unsigned char bytes[maximumBytes];if(connected){r.metadata=connected;Report("native",index,r);return;}
 TPtrC name(reinterpret_cast<const TText*>(path),wcslen(path));TEntry entry;r.metadata=fs.Entry(name,entry);if(!r.metadata){r.size=entry.iSize;r.regular=!entry.IsDir();}
 RFile file;r.open=file.Open(fs,name,EFileRead|EFileShareReadersOnly);
 if(!r.open){r.read=0;for(;;){TBuf8<256>block;int e=file.Read(block);if(e){r.read=e;break;}if(!block.Length())break;if(r.bytes+block.Length()>maximumBytes){r.read=KErrTooBig;break;}memcpy(bytes+r.bytes,block.Ptr(),block.Length());r.bytes+=block.Length();}file.Close();r.close=0;if(!r.read)CheckBytes(r,index,bytes);}
 Report("native",index,r);
}
void Posix(const char*narrow,const wchar_t*wide,unsigned index,bool useWide)
{
 Outcome r;struct stat status;memset(&status,0,sizeof(status));errno=0;r.metadata=useWide?wstat(wide,&status):stat(narrow,&status);r.metadataErrno=errno;
 if(!r.metadata){r.size=status.st_size;r.regular=S_ISREG(status.st_mode);}
 errno=0;int descriptor=useWide?wopen(wide,O_RDONLY,0):open(narrow,O_RDONLY,0);r.open=descriptor<0?-1:0;r.openErrno=errno;
 if(descriptor>=0){unsigned char bytes[maximumBytes];r.read=0;for(;;){unsigned char block[256];errno=0;ssize_t count=read(descriptor,block,sizeof(block));int e=errno;if(count<0){r.read=-1;r.readErrno=e;break;}if(!count)break;if(r.bytes+static_cast<unsigned>(count)>maximumBytes){r.read=-1;break;}memcpy(bytes+r.bytes,block,count);r.bytes+=count;}errno=0;r.close=close(descriptor);if(!r.read)CheckBytes(r,index,bytes);}
 Report(useWide?"wide":"narrow",index,r);
}
}
int main()
{
 char token[65]={};if(!Token(token))return 2;if(!freopen("C:/Data/WebKit/file-io-contract.log","w",stdout))return 2;
 printf("BEGIN {\"suite\":\"file-io-contract\",\"version\":1,\"run\":\"%s\"}\n",token);
 printf("ABI {\"wchar\":%u,\"wchar_align\":%u,\"TText\":%u,\"stat\":%u,\"off_t\":%u,\"st_size\":%u,\"unicode_units\":[23383,55357,56832]}\n",unsigned(sizeof(wchar_t)),unsigned(alignof(wchar_t)),unsigned(sizeof(TText)),unsigned(sizeof(struct stat)),unsigned(sizeof(off_t)),unsigned(sizeof(((struct stat*)nullptr)->st_size)));
 RFs fs;int connected=fs.Connect();
 for(unsigned index=0;index<3;++index){char narrow[512];snprintf(narrow,sizeof(narrow),"C:\\Data\\WebKit\\file-io-%s\\%s",token,narrowNames[index]);wchar_t wide[256];unsigned at=0;const char prefix[]="C:\\Data\\WebKit\\file-io-";for(char c:prefix){if(!c)break;wide[at++]=c;}for(const char*p=token;*p;++p)wide[at++]=*p;wide[at++]=L'\\';for(const wchar_t*p=wideNames[index];*p;++p)wide[at++]=*p;wide[at]=0;Native(fs,connected,wide,index);Posix(narrow,wide,index,false);Posix(narrow,wide,index,true);}
 if(!connected)fs.Close();
 printf("RESULT {\"suite\":\"file-io-contract\",\"version\":1,\"run\":\"%s\",\"passed\":%u,\"failed\":%u,\"total\":%u}\n",token,passed,failed,passed+failed);printf("file-io-contract exiting %d\n",failed?1:0);fflush(stdout);fclose(stdout);return failed?1:0;
}
