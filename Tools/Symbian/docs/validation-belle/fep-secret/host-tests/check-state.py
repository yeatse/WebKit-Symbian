from pathlib import Path
import subprocess,json,re
out=Path(__file__).resolve().parent
main=(out/'overlay/src/emu/qt/src/mainwindow.cpp').read_text()
hui=(out/'overlay/src/emu/dispatch/src/hui.cpp').read_text()
def method(text, signature):
 a=text.index(signature);brace=text.index('{',a);level=1;i=brace+1
 while level:
  if text[i]=='{':level+=1
  if text[i]=='}':level-=1
  i+=1
 return text[a:i]
main_methods='\n\n'.join(method(main,s) for s in ['bool main_window::input_dialog_open(', 'void main_window::input_dialog_close()', 'void main_window::on_input_dialog_delay_launch_asked(', 'void main_window::on_finished_text_input(', 'void main_window::on_input_dialog_open_request(', 'void main_window::on_input_dialog_close_request('])
hui_methods='\n\n'.join(method(hui,s) for s in ['void ehui_input_view_controller::set_notify_info(', 'bool ehui_input_view_controller::is_another_input_dialog_active(', 'void ehui_input_view_controller::cancel(', 'void ehui_input_view_controller::on_input_view_complete('])
preamble=r'''
#include <string>
#include <functional>
#include <utility>
#include <vector>
#include <memory>
#include <cstdint>
#include <iostream>
#include <cstdlib>
using quint64=std::uint64_t;
#define emit
#define LOG_ERROR(...)
namespace eka2l1::drivers::ui {using input_dialog_complete_callback=std::function<void(const std::u16string&,bool)>;}
using callback=eka2l1::drivers::ui::input_dialog_complete_callback;
struct QString {std::u16string text;std::u16string toStdU16String() const{return text;}static QString fromStdU16String(const std::u16string&t){return{t};}};
struct QDialog {enum{Rejected,Accepted};};
struct main_window;
struct symbian_input_dialog {std::function<void(const QString&,bool)> finished;std::u16string text;int done_count=0;bool secret=false;explicit symbian_input_dialog(main_window*){}void finished_input(const QString&,bool){}void done(int result){++done_count;if(result==QDialog::Rejected&&finished)finished({text},true);}void open_for_input(const QString&s,int,bool mode){text=s.text;secret=mode;}};
std::vector<std::function<void()>> timers,signals;
std::vector<std::unique_ptr<symbian_input_dialog>> dialogs;
struct QTimer {template<class F>static void singleShot(int,main_window*,F f){timers.push_back(std::move(f));}};
template<class S,class F>void connect(symbian_input_dialog*d,S,main_window*,F f){d->finished=std::move(f);dialogs.emplace_back(d);}
struct main_window {
callback input_complete_callback_;std::u16string input_initial_text_;int input_text_max_len_=0;bool input_text_secret_=false;quint64 input_dialog_request_id_=0;symbian_input_dialog* input_dialog_=nullptr;
bool input_dialog_open(const std::u16string&,int,callback,bool=false);void input_dialog_close();void on_input_dialog_delay_launch_asked(quint64);void on_finished_text_input(const QString&,bool);void on_input_dialog_open_request(quint64);void on_input_dialog_close_request(quint64);
void input_dialog_delay_launch_asked(quint64 id){signals.push_back([this,id]{on_input_dialog_delay_launch_asked(id);});}void input_dialog_close_request(quint64 id){signals.push_back([this,id]{on_input_dialog_close_request(id);});}
};
namespace epoc {constexpr int error_none=0,error_cancel=-1;struct notify_info{int* result=nullptr;int sts=1;int requester=1;bool empty()const{return !sts||!result;}void complete(int code){*result=code;sts=0;}};}
struct kernel_system {bool alive=true;bool is_thread_alive(int){return alive;}};
struct ehui_input_view_controller{std::u16string result_;epoc::notify_info info_;std::uint64_t request_id_=0;void set_notify_info(epoc::notify_info);bool is_another_input_dialog_active(kernel_system*);void cancel(kernel_system*);void on_input_view_complete(kernel_system*,std::uint64_t,const std::u16string&,bool);};
'''
tests=r'''
unsigned checks=0;
void check(bool yes){++checks;if(!yes){std::cerr<<"FAIL assertion "<<checks<<"\n";std::exit(1);}}
void flush(std::vector<std::function<void()>>&q){auto todo=std::move(q);q.clear();for(auto&f:todo)f();}
void clear_tasks(){timers.clear();signals.clear();dialogs.clear();}
int main(){
 // Normal submit and empty submit both succeed, once only.
 for(auto text:{std::u16string(u"fixture"),std::u16string{}}){
 main_window w;unsigned count=0;bool accepted=false;std::u16string value;
 check(w.input_dialog_open(text,100,[&](auto&s,bool a){++count;accepted=a;value=s;}));flush(signals);check(!w.input_dialog_);flush(timers);auto* d=w.input_dialog_;check(d);
 d->finished({text},false);check(count==1&&accepted&&value==text);check(!w.input_dialog_&&!w.input_complete_callback_);check(d->done_count==1);d->finished({u"late"},true);check(count==1);clear_tasks();}
 // Escape carries cancellation, no document text, and ignores duplicate signal.
 {main_window w;unsigned count=0;bool accepted=true;std::u16string value=u"before";
 check(w.input_dialog_open(u"fixture",100,[&](auto&s,bool a){++count;accepted=a;value=s;}));flush(signals);flush(timers);auto*d=w.input_dialog_;d->done(QDialog::Rejected);check(count==1&&!accepted&&value.empty());check(!w.input_dialog_&&!w.input_complete_callback_);d->finished({u"late"},false);check(count==1);clear_tasks();}
 // Guest close before the 200-ms launch cancels rather than displaying a ghost.
 {main_window w;unsigned count=0;bool accepted=true;check(w.input_dialog_open(u"fixture",100,[&](auto&s,bool a){++count;accepted=a;check(s.empty());}));flush(signals);check(timers.size()==1);w.input_dialog_close();flush(signals);check(count==1&&!accepted);flush(timers);check(!w.input_dialog_&&dialogs.empty());clear_tasks();}
 // A previous queued launch/close/completion cannot complete a new request.
 {main_window w;unsigned old_count=0,new_count=0;check(w.input_dialog_open(u"old",100,[&](auto&,bool){++old_count;}));auto old_id=w.input_dialog_request_id_;flush(signals);w.on_input_dialog_close_request(old_id);check(old_count==1);check(w.input_dialog_open(u"new",100,[&](auto&,bool){++new_count;}));flush(signals);flush(timers);auto*d=w.input_dialog_;check(d&&d->text==u"new");w.on_input_dialog_close_request(old_id);check(w.input_dialog_==d&&new_count==0);w.on_input_dialog_open_request(old_id);check(w.input_dialog_==d);d->finished({u"new"},false);check(new_count==1);clear_tasks();}
 // Callback may begin a fresh request; old signal/close must not erase it.
 {main_window w;unsigned count=0;check(w.input_dialog_open(u"one",100,[&](auto&,bool){++count;check(w.input_dialog_open(u"two",100,[&](auto&,bool){++count;}));}));flush(signals);flush(timers);auto*d=w.input_dialog_;d->finished({u"one"},false);check(count==1&&w.input_complete_callback_);flush(signals);flush(timers);auto*next=w.input_dialog_;check(next&&next!=d);d->finished({u"late"},true);check(w.input_dialog_==next&&count==1);next->finished({u"two"},false);check(count==2);clear_tasks();}
 // Production dispatch controller preserves accepted-empty vs cancel and IDs.
 {kernel_system k;ehui_input_view_controller c;int result=99;c.set_notify_info({&result});auto id=c.request_id_;c.on_input_view_complete(&k,id,u"fixture",true);check(result==0&&c.result_==u"fixture");c.on_input_view_complete(&k,id,u"late",true);check(c.result_==u"fixture");
 c.set_notify_info({&result});id=c.request_id_;c.on_input_view_complete(&k,id,u"ignored",false);check(result==-1&&c.result_.empty());c.set_notify_info({&result});id=c.request_id_;c.on_input_view_complete(&k,id,u"",true);check(result==0&&c.result_.empty());
 c.set_notify_info({&result});auto old=c.request_id_;c.cancel(&k);check(result==-1&&c.result_.empty());c.set_notify_info({&result});result=99;id=c.request_id_;c.on_input_view_complete(&k,old,u"late",true);check(result==99&&c.result_.empty());c.on_input_view_complete(&k,id,u"new",true);check(result==0&&c.result_==u"new");}

 // Secret/plain mode belongs to the request; a stale queued secret cannot relabel a new plain editor.
 {main_window w;unsigned count=0;check(w.input_dialog_open(u"fixture",100,[&](auto&,bool){++count;},true));auto old_id=w.input_dialog_request_id_;flush(signals);w.on_input_dialog_close_request(old_id);check(count==1&&!w.input_text_secret_&&w.input_initial_text_.empty());check(w.input_dialog_open(u"ordinary",100,[&](auto&,bool){++count;},false));flush(signals);flush(timers);check(w.input_dialog_&&!w.input_dialog_->secret&&w.input_dialog_->text==u"ordinary");w.input_dialog_->finished({u"ordinary"},false);check(count==2&&!w.input_text_secret_&&w.input_initial_text_.empty());clear_tasks();}
 {main_window w;bool accepted=false;check(w.input_dialog_open(u"fixture",100,[&](auto&,bool a){accepted=a;},true));flush(signals);flush(timers);check(w.input_dialog_&&w.input_dialog_->secret&&w.input_initial_text_.empty());w.input_dialog_->finished({u"fixture"},false);check(accepted&&!w.input_text_secret_);clear_tasks();}

 std::cout<<checks<<" assertions PASS: extracted production Qt dialog and dispatch methods; no UI/guest\n";
}
'''
source=out/'state-tests.cpp';source.write_text(preamble+'\n'+main_methods+'\n'+hui_methods+'\n'+tests)
cmd=['g++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(source),'-o',str(out/'state-tests')]
with (out/'state-build.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
if r.returncode:raise SystemExit(r.returncode)
with (out/'state-tests.log').open('w') as f:r=subprocess.run([str(out/'state-tests')],stdout=f,stderr=subprocess.STDOUT)
print((out/'state-tests.log').read_text());raise SystemExit(r.returncode)
