#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char *fixture;
static long size(const char *p) { struct stat st; if (stat(p,&st)) abort(); return st.st_size; }
static void copy_bytes(const char *target, long begin, long end) {
    FILE *in=fopen(fixture,"rb"), *out=fopen(target,begin?"ab":"wb");
    if (!in || !out || fseek(in,begin,SEEK_SET)) abort();
    unsigned char buf[8192];
    while(begin<end) { size_t n=(end-begin)>sizeof(buf)?sizeof(buf):(size_t)(end-begin); if(fread(buf,1,n,in)!=n || fwrite(buf,1,n,out)!=n)abort(); begin+=n; }
    if(fclose(out) || fclose(in))abort();
}
static int run(const char *label,const char *path,int append_after_clone) {
    AVFormatContext *format=NULL;
    if(avformat_open_input(&format,path,NULL,NULL)<0 || avformat_find_stream_info(format,NULL)<0)abort();
    int video=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0); if(video<0)abort();
    const AVCodec *codec=avcodec_find_decoder(format->streams[video]->codecpar->codec_id);
    AVCodecContext *decode=avcodec_alloc_context3(codec);
    if(!decode || avcodec_parameters_to_context(decode,format->streams[video]->codecpar)<0 || avcodec_open2(decode,codec,NULL)<0)abort();
    if(av_seek_frame(format,-1,0,AVSEEK_FLAG_BACKWARD)<0)abort();
    avcodec_flush_buffers(decode);
    AVPacket *packet=av_packet_alloc(),*queued=NULL;
    while(av_read_frame(format,packet)>=0) {
        if(packet->stream_index==video && packet->dts==22528) { queued=av_packet_clone(packet);break; }
        av_packet_unref(packet);
    }
    if(!queued)abort();
    printf("{\"case\":\"%s\",\"phase\":\"queued\",\"file_size\":%ld,\"position\":%lld,\"dts\":%lld,\"size\":%d,\"flags\":%d}\n",label,size(path),(long long)queued->pos,(long long)queued->dts,queued->size,queued->flags);
    if(append_after_clone)copy_bytes(path,size(path),size(fixture));
    int result=avcodec_send_packet(decode,queued);
    printf("{\"case\":\"%s\",\"phase\":\"consume\",\"file_size\":%ld,\"size\":%d,\"flags\":%d,\"send_result\":%d}\n",label,size(path),queued->size,queued->flags,result);
    int ok=append_after_clone ? (queued->size==1565 && (queued->flags&AV_PKT_FLAG_CORRUPT) && result<0) : (queued->size==2166 && !(queued->flags&AV_PKT_FLAG_CORRUPT) && result==0);
    av_packet_free(&queued);av_packet_free(&packet);avcodec_free_context(&decode);avformat_close_input(&format);return ok;
}
int main(int argc,char **argv) {
    if(argc!=3)return 2;fixture=argv[1];
    char path[4096];snprintf(path,sizeof(path),"%s/partial.mp4",argv[2]);
    int full=run("full-before-open",fixture,0);
    copy_bytes(path,0,81699);
    int cached=run("append-after-clone",path,1);
    int reopened=run("reopen-completed-file",path,0);
    printf("{\"summary\":\"%s\",\"full\":%d,\"cached_short\":%d,\"reopened\":%d}\n",full&&cached&&reopened?"PASS":"FAIL",full,cached,reopened);
    return full&&cached&&reopened?0:1;
}
