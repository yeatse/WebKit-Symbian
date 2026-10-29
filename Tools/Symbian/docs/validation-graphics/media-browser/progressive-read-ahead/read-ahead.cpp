extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <string>

static const char* fixture;
static void write_file(const char* path, size_t from, size_t to) {
    FILE *in=fopen(fixture,"rb"),*out=fopen(path,from?"ab":"wb");if(!in||!out||fseek(in,from,SEEK_SET))abort();
    unsigned char data[8192];while(from<to){size_t n=std::min(sizeof(data),to-from);if(fread(data,1,n,in)!=n||fwrite(data,1,n,out)!=n)abort();from+=n;}if(fclose(in)||fclose(out))abort();
}
static AVCodecContext* decoder(AVFormatContext* f,int index){auto* c=avcodec_find_decoder(f->streams[index]->codecpar->codec_id);auto* d=avcodec_alloc_context3(c);if(!d||avcodec_parameters_to_context(d,f->streams[index]->codecpar)<0||avcodec_open2(d,c,nullptr)<0)abort();return d;}
struct Outcome {int first_video=0,first_audio=0,first_samples=0,corrupt=0,errors=0,frames=0,cycles=0;bool eof=false;};
static Outcome simulate(const char* label,const char* path,bool inside,bool grow,size_t initial) {
    write_file(path,0,initial);
    AVFormatContext* format=nullptr;if(avformat_open_input(&format,path,nullptr,nullptr)<0||avformat_find_stream_info(format,nullptr)<0)abort();
    int vi=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0),ai=av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);if(vi<0||ai<0)abort();
    auto* vd=decoder(format,vi);auto* ad=decoder(format,ai);
    if(av_seek_frame(format,-1,0,AVSEEK_FLAG_BACKWARD)<0)abort();avcodec_flush_buffers(vd);avcodec_flush_buffers(ad);
    std::deque<AVPacket*> image,audio;size_t samples=0,low=ad->sample_rate*ad->ch_layout.nb_channels/2;
    AVPacket* packet=av_packet_alloc();AVFrame* af=av_frame_alloc();AVFrame* vf=av_frame_alloc();Outcome out;
    auto fill_audio=[&]{while(!audio.empty()&&131072-samples>=8192){auto* p=audio.front();audio.pop_front();int rc=avcodec_send_packet(ad,p);av_packet_free(&p);if(rc<0){++out.errors;continue;}while(avcodec_receive_frame(ad,af)>=0){samples+=af->nb_samples*ad->ch_layout.nb_channels;av_frame_unref(af);}}};
    for(int cycle=0;cycle<1000;++cycle) {
        out.cycles=cycle+1;
        while(!out.eof&&image.size()<64) {
            if(inside)fill_audio();
            if(audio.size()>=96)break;
            if(samples>=low&&!image.empty())break;
            int rc=av_read_frame(format,packet);if(rc<0){out.eof=true;if(rc!=AVERROR_EOF)++out.errors;break;}
            if(packet->flags&AV_PKT_FLAG_CORRUPT)++out.corrupt;
            if(packet->stream_index==vi)image.push_back(av_packet_clone(packet));
            else if(packet->stream_index==ai&&packet->pts>=0)audio.push_back(av_packet_clone(packet));
            av_packet_unref(packet);
        }
        if(cycle==0){out.first_video=image.size();out.first_audio=audio.size();out.first_samples=samples;if(grow)write_file(path,initial,710764);}
        fill_audio();
        if(!image.empty()) {
            auto* p=image.front();image.pop_front();int rc=avcodec_send_packet(vd,p);av_packet_free(&p);if(rc<0){++out.errors;break;}
            while(avcodec_receive_frame(vd,vf)>=0){++out.frames;av_frame_unref(vf);}
        } else if(out.eof&&audio.empty()&&!samples)break;
        // Deterministic simulated audio consumption per 15 fps display cycle.
        samples-=std::min(samples,(size_t)ad->sample_rate*ad->ch_layout.nb_channels/15);
    }
    for(auto*p:image)av_packet_free(&p);for(auto*p:audio)av_packet_free(&p);
    av_packet_free(&packet);av_frame_free(&af);av_frame_free(&vf);avcodec_free_context(&vd);avcodec_free_context(&ad);avformat_close_input(&format);
    printf("{\"case\":\"%s\",\"initial_bytes\":%zu,\"decode_inside_reader\":%s,\"append_after_first_cycle\":%s,\"first_video_packets\":%d,\"first_audio_packets\":%d,\"first_decoded_samples\":%d,\"corrupt_packets\":%d,\"errors\":%d,\"video_frames\":%d,\"cycles\":%d}\n",label,initial,inside?"true":"false",grow?"true":"false",out.first_video,out.first_audio,out.first_samples,out.corrupt,out.errors,out.frames,out.cycles);
    return out;
}
int main(int argc,char**argv){if(argc!=3)return 2;fixture=argv[1];std::string path=std::string(argv[2])+"/paced.mp4";bool pass=true;
    for(size_t initial:{size_t(65536),size_t(81699)}){
        auto old=simulate("old-growing",path.c_str(),false,true,initial);auto fixed=simulate("scheduled-growing",path.c_str(),true,true,initial);auto truncated=simulate("scheduled-truncated",path.c_str(),true,false,initial);
        pass&=old.corrupt>0&&old.errors>0&&fixed.corrupt==0&&fixed.errors==0&&fixed.frames==180&&fixed.first_samples>=22050&&truncated.corrupt>0&&truncated.errors>0;
    }
    auto full=simulate("scheduled-complete",path.c_str(),true,false,710764);pass&=full.corrupt==0&&full.errors==0&&full.frames==180;
    printf("{\"summary\":\"%s\"}\n",pass?"PASS":"FAIL");return pass?0:1;
}
