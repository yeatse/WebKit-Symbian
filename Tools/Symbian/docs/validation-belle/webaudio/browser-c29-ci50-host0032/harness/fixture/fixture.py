#!/usr/bin/env python3
"""Prepare by default; --serve starts one finite owned localhost HTTP fixture."""
import argparse,hashlib,json,secrets,struct,time
from http.server import HTTPServer,BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlsplit,parse_qs
from protocol import ORDER,TOKEN,check_event,require
ROOT=Path(__file__).resolve().parent
PCM=[-32768,-16384,-1,0,1,8192,16384,32767]
def wav():
 data=struct.pack('<8h',*PCM)
 return b'RIFF'+struct.pack('<I',36+len(data))+b'WAVEfmt '+struct.pack('<IHHIIHH',16,1,1,32768,65536,2,16)+b'data'+struct.pack('<I',len(data))+data
WAV=wav()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def strict_json(data):
 def pairs(rows):
  result={}
  for key,value in rows:
   require(key not in result);result[key]=value
  return result
 def constant(_):raise ValueError('nonfinite JSON')
 return json.loads(data,object_pairs_hook=pairs,parse_constant=constant)
def sources():
 pins=json.loads((ROOT/'fixture-pins.json').read_text());result={}
 for name,expected in pins.items():
  p=ROOT/name;require(p.is_file()and not p.is_symlink()and sha(p)==expected);result[name]={'sha256':expected,'bytes':p.stat().st_size}
 return result
class Fixture:
 def __init__(self,args):
  self.args=args;self.state={'suite':'classic-webaudio','version':1,'run':args.run,'provenance':'browser-reported','events':[],'received_monotonic':[],'errors':[],'page_requests':0,'wav_requests':0,'serve_requested':args.serve,'deadline_expired':False}
 def save(self):
  temp=self.args.output/'state.json.tmp';temp.write_text(json.dumps(self.state,indent=2,allow_nan=False)+'\n');temp.replace(self.args.output/'state.json')
 def accept(self,value):
  check_event(value,self.args.run);index=len(self.state['events']);require(not any(e['kind']=='error'for e in self.state['events'])and value['sequence']==index)
  require(value['kind']=='error'or(index<len(ORDER)and value['kind']==ORDER[index]))
  self.state['events'].append(value);self.state['received_monotonic'].append(time.monotonic());self.save()
def serve(fixture):
 args=fixture.args;origin='http://localhost:'+str(args.port)
 class Handler(BaseHTTPRequestHandler):
  protocol_version='HTTP/1.0'
  def log_message(self,*args):pass
  def setup(self):super().setup();self.connection.settimeout(3)
  def reply(self,code,value,kind='application/json'):
   data=value if isinstance(value,bytes)else json.dumps(value).encode();self.send_response(code);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Content-Security-Policy',"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self'; base-uri 'none'; form-action 'none'");self.end_headers();self.wfile.write(data)
  def route(self):
   u=urlsplit(self.path);require(self.headers.get('Host')=='localhost:'+str(args.port)and not u.scheme and not u.netloc and not u.fragment and parse_qs(u.query,keep_blank_values=True)=={'run':[args.run]});return u.path
  def reject(self):
   fixture.state['errors'].append('invalid public request');fixture.state['errors']=fixture.state['errors'][:16];fixture.save();self.reply(400,{'error':'invalid fixed fixture request'})
  def do_GET(self):
   try:
    path=self.route()
    if path=='/':
     require(fixture.state['page_requests']==0);fixture.state['page_requests']+=1;fixture.save();html=(ROOT/'page.html').read_text().replace('%%TOKEN%%',args.run).replace('%%RUN%%',json.dumps(args.run));self.reply(200,html.encode(),'text/html; charset=utf-8')
    elif path=='/fixture.wav':require(fixture.state['wav_requests']==0);fixture.state['wav_requests']+=1;fixture.save();self.reply(200,WAV,'audio/wav')
    elif path=='/favicon.ico':self.reply(204,b'','image/x-icon')
    else:raise ValueError('unknown route')
   except(ValueError,KeyError,TypeError):self.reject()
  def do_POST(self):
   try:
    require(self.route()=='/event'and self.headers.get('Origin')==origin and self.headers.get('Content-Type')=='application/json'and 'Transfer-Encoding'not in self.headers)
    length=int(self.headers.get('Content-Length','0'));require(0<length<=16384);body=self.rfile.read(length);require(len(body)==length);fixture.accept(strict_json(body));self.reply(200,{'ok':True})
   except(ValueError,KeyError,TypeError,UnicodeError):self.reject()
  def do_PUT(self):self.reject()
  def do_DELETE(self):self.reject()
  def do_HEAD(self):self.reject()
  def do_OPTIONS(self):self.reject()
  def do_PATCH(self):self.reject()
 server=HTTPServer(('127.0.0.1',args.port),Handler);server.timeout=.2;until=time.monotonic()+args.deadline
 ready={'suite':'classic-webaudio','run':args.run,'url':origin+'/?run='+args.run,'port':args.port,'deadline_seconds':args.deadline,'native_audio_claim':False}
 (args.output/'ready.json').write_text(json.dumps(ready,indent=2)+'\n')
 try:
  while time.monotonic()<until:server.handle_request()
  fixture.state['deadline_expired']=True;fixture.save()
 finally:server.server_close()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--serve',action='store_true');p.add_argument('--run');p.add_argument('--output',required=True,type=Path);p.add_argument('--port',type=int,default=8864);p.add_argument('--deadline',type=int,default=180);args=p.parse_args();args.run=args.run or'wa-'+secrets.token_hex(16)
 require(TOKEN.fullmatch(args.run)and args.output.is_absolute()and not args.output.exists()and 1024<=args.port<=65535 and 15<=args.deadline<=300)
 for path in [args.output,*args.output.parents]:require(not path.is_symlink())
 pins=sources();args.output.mkdir(mode=0o700);fixture=Fixture(args);fixture.save()
 manifest={'status':'SERVING'if args.serve else'PREPARED_NOT_EXECUTED','run':args.run,'sources':pins,'wav':{'bytes':len(WAV),'sha256':hashlib.sha256(WAV).hexdigest(),'sample_rate':32768,'pcm16':PCM},'guest_display_audio_started':False}
 (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({k:manifest[k]for k in ['status','run','guest_display_audio_started']}),flush=True)
 if args.serve:serve(fixture)
if __name__=='__main__':main()
