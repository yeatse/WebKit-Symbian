#!/usr/bin/env python3
"""Fixed positive IPv4 browser fixture; default prepares without opening keys/listeners."""
import argparse,asyncio,base64,hashlib,json,re,signal,socket,ssl,struct,time
from pathlib import Path
from urllib.parse import urlsplit,parse_qs
import h2.config,h2.connection,h2.events
SUITE='network-guest-c22-ipv4';LENGTHS=[524288]+[16384]*11;FALLBACK=b'HTTP1-ALPN-FALLBACK-C22'
def payload(i):return bytes((i*17+j)%251 for j in range(LENGTHS[i]))
def page(token,port,mode='ipv4',v6port=8826):return (Path(__file__).parent/('page-literal6.html' if mode=='literal6' else 'page.html')).read_text().replace('__RUN_JSON__',json.dumps(token)).replace('__H1_PORT__',str(port)).replace('__V6_PORT__',str(v6port)).encode()
async def serve(a):
 if not re.fullmatch(r'[A-Za-z0-9_-]{8,80}',a.run_token):raise ValueError('fresh token required')
 if not 10<=a.deadline<=300 or not (1024<=a.h2_port<=65535 and 1024<=a.h1_port<=65535 and a.h2_port!=a.h1_port) or (a.mode=='literal6' and (not 1024<=a.v6_port<=65535 or a.v6_port in (a.h2_port,a.h1_port))):raise ValueError('invalid bounded settings')
 origin=f'https://127.0.0.1:{a.h2_port}';url=origin+'/?run='+a.run_token
 if not a.execute:print(json.dumps({'status':'PREPARED_NOT_EXECUTED','run':a.run_token,'url':url,'key_read':False,'listener_started':False}));return
 a.output.mkdir(mode=0o700,parents=True,exist_ok=False);started=time.monotonic();sequence=0
 suite='network-guest-c22-'+a.mode
 state={'suite':suite,'version':1,'run':a.run_token,'mode':a.mode,'h2_connections':[],'h1_requests':[],'literal6':{'https':[],'wss':[]},'errors':[],'stopped':False}
 def save():
  state['elapsed_seconds']=round(time.monotonic()-started,3);p=a.output/'server-state.tmp';p.write_text(json.dumps(state,indent=2)+'\n');p.replace(a.output/'server-state.json')
 def path(value):
  u=urlsplit(value)
  if u.path=='/favicon.ico' and not u.query:return u.path
  if parse_qs(u.query)!={'run':[a.run_token]}:raise ValueError('wrong run query')
  return u.path
 def context(protocols):
  c=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);c.minimum_version=ssl.TLSVersion.TLSv1_2;c.load_cert_chain(str(a.certificate),str(a.key));c.set_alpn_protocols(protocols);return c
 async def h1(reader,writer):
  try:
   alpn=writer.get_extra_info('ssl_object').selected_alpn_protocol();raw=await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'),15);first=raw.split(b'\r\n',1)[0].decode('ascii').split(' ')
   if len(raw)>16384 or len(first)!=3 or first[0]!='GET' or first[2]!='HTTP/1.1' or path(first[1])!='/fallback' or alpn!='http/1.1':raise ValueError('unexpected H1 request/ALPN')
   state['h1_requests'].append({'run':a.run_token,'alpn':alpn,'path':'/fallback','bytes':len(FALLBACK),'sha256':hashlib.sha256(FALLBACK).hexdigest()})
   writer.write(('HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: '+str(len(FALLBACK))+'\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: '+origin+'\r\nConnection: close\r\n\r\n').encode()+FALLBACK);await writer.drain();save()
  except Exception as e:state['errors'].append({'stage':'h1','type':type(e).__name__,'message':str(e)});save()
  finally:writer.close()
 async def h2_handler(reader,writer):
  nonlocal sequence
  sequence+=1;cid=sequence;c=h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=False,header_encoding='utf-8'))
  info={'id':cid,'alpn':writer.get_extra_info('ssl_object').selected_alpn_protocol(),'page_requests':0,'items':[],'peak_active':0,'window_updates':0,'client_settings':{},'bytes_sent':0,'complete_items':[]};state['h2_connections'].append(info);active=set();pending={};tasks=[];wake=asyncio.Event()
  async def respond(sid,p):
   index=None
   try:
    if p=='/':data=page(a.run_token,a.h1_port,a.mode,a.v6_port);content='text/html; charset=utf-8';info['page_requests']+=1
    elif re.fullmatch('/item/(?:[0-9]|1[01])',p):
     index=int(p.rsplit('/',1)[1]);data=payload(index);content='application/octet-stream';info['items'].append({'index':index,'stream':sid,'connection':cid,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()});active.add(sid);info['peak_active']=max(info['peak_active'],len(active));save();await asyncio.sleep(.35)
    elif p=='/favicon.ico':data=b'';content='image/x-icon'
    else:raise ValueError('unexpected h2 path')
    c.send_headers(sid,[(':status','200'),('content-type',content),('content-length',str(len(data))),('cache-control','no-store')],end_stream=not data);writer.write(c.data_to_send());await writer.drain();offset=0
    while offset<len(data):
     n=min(len(data)-offset,c.local_flow_control_window(sid),c.max_outbound_frame_size)
     if n<=0:wake.clear();await asyncio.wait_for(wake.wait(),30);continue
     c.send_data(sid,data[offset:offset+n],end_stream=offset+n==len(data));offset+=n
     if index is not None:info['bytes_sent']+=n
     writer.write(c.data_to_send());await writer.drain()
    if index is not None:info['complete_items'].append(index);active.remove(sid)
    save()
   except asyncio.CancelledError:raise
   except Exception as e:state['errors'].append({'stage':'h2-response','type':type(e).__name__,'message':str(e)});save()
  try:
   if info['alpn']!='h2':raise ValueError('h2 ALPN not negotiated')
   c.initiate_connection();writer.write(c.data_to_send());await writer.drain();save()
   while True:
    data=await asyncio.wait_for(reader.read(65536),60)
    if not data:break
    for event in c.receive_data(data):
     if isinstance(event,h2.events.RemoteSettingsChanged):
      for setting,change in event.changed_settings.items():info['client_settings'][str(int(setting))]=change.new_value
     elif isinstance(event,h2.events.RequestReceived):
      headers=dict(event.headers)
      if headers.get(':method')!='GET':raise ValueError('normal GET required')
      pending[event.stream_id]=path(headers[':path'])
     elif isinstance(event,h2.events.StreamEnded):tasks.append(asyncio.create_task(respond(event.stream_id,pending.pop(event.stream_id))))
     elif isinstance(event,h2.events.WindowUpdated):info['window_updates']+=1;wake.set()
     elif isinstance(event,h2.events.StreamReset):raise ValueError('unexpected stream reset')
    writer.write(c.data_to_send());await writer.drain();save()
   await asyncio.gather(*tasks)
  except (ConnectionResetError,BrokenPipeError):
   if sorted(info['complete_items'])!=(list(range(12)) if a.mode=='ipv4' else []):state['errors'].append({'stage':'h2-connection','type':'PrematureClose','message':'connection closed before fixed items complete'})
   save()
  except Exception as e:state['errors'].append({'stage':'h2-connection','type':type(e).__name__,'message':str(e)});save()
  finally:
   for task in tasks:
    if not task.done():task.cancel()
   if tasks:await asyncio.gather(*tasks,return_exceptions=True)
   writer.close();save()
 async def v6_handler(reader,writer):
  try:
   alpn=writer.get_extra_info('ssl_object').selected_alpn_protocol();peer=writer.get_extra_info('peername')[0]
   if alpn!='http/1.1' or peer!='::1':raise ValueError('IPv6 loopback/H1 ALPN required')
   raw=await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'),15)
   if len(raw)>16384:raise ValueError('normal request exceeds fixed bound')
   lines=raw.decode('ascii').split('\r\n');first=lines[0].split(' ')
   if len(first)!=3 or first[0]!='GET' or first[2]!='HTTP/1.1':raise ValueError('normal GET required')
   p=path(first[1]);headers={}
   for line in lines[1:]:
    if line:
     k,val=line.split(':',1);k=k.lower()
     if k in headers:raise ValueError('duplicate request header')
     headers[k]=val.strip()
   if p=='/literal':
    data=bytes((j*7+3)%251 for j in range(4096));record={'run':a.run_token,'peer':peer,'alpn':alpn,'path':p,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'response_complete':False};state['literal6']['https'].append(record)
    writer.write(('HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: '+str(len(data))+'\r\nAccess-Control-Allow-Origin: '+origin+'\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n').encode()+data);await writer.drain();record['response_complete']=True;save()
   elif p=='/socket':
    key=headers.get('sec-websocket-key','')
    if headers.get('upgrade','').lower()!='websocket' or 'upgrade' not in [x.strip().lower() for x in headers.get('connection','').split(',')] or headers.get('sec-websocket-version')!='13' or len(base64.b64decode(key,validate=True))!=16 or headers.get('origin')!=origin:raise ValueError('normal browser WSS handshake required')
    accept=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
    record={'run':a.run_token,'peer':peer,'alpn':alpn,'path':p,'upgrade':True,'dynamic_accept':True,'echoes':0,'bytes':0,'sha256':None,'client_masked':False,'normal_close':False,'close_code':None};state['literal6']['wss'].append(record)
    writer.write(('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+'\r\n\r\n').encode());await writer.drain();save()
    async def frame():
     head=await asyncio.wait_for(reader.readexactly(2),15);length=head[1]&127
     if head[0]&0x70 or not head[0]&0x80 or not head[1]&0x80 or length>125:raise ValueError('bounded normal masked browser frame required')
     mask=await asyncio.wait_for(reader.readexactly(4),15);body=await asyncio.wait_for(reader.readexactly(length),15)
     return head[0]&15,bytes(b^mask[i%4] for i,b in enumerate(body))
    opcode,data=await frame()
    if opcode!=1 or data!=b'ping':raise ValueError('fixed normal ping text required')
    record.update(echoes=1,bytes=4,sha256=hashlib.sha256(data).hexdigest(),client_masked=True);writer.write(b'\x81\x04'+data);await writer.drain();save()
    opcode,data=await frame()
    if opcode!=8 or len(data)<2 or int.from_bytes(data[:2],'big')!=1000:raise ValueError('normal client close1000 required')
    writer.write(bytes([0x88,len(data)])+data);await writer.drain();record.update(normal_close=True,close_code=1000);save()
   else:raise ValueError('unknown fixed IPv6 literal path')
  except Exception as e:state['errors'].append({'stage':'literal6','type':type(e).__name__,'message':str(e)});save()
  finally:writer.close()
 stop=asyncio.Event();loop=asyncio.get_running_loop()
 for sig in (signal.SIGINT,signal.SIGTERM):loop.add_signal_handler(sig,stop.set)
 servers=[]
 try:
  servers.append(await asyncio.start_server(h2_handler,'127.0.0.1',a.h2_port,ssl=context(['h2','http/1.1']),limit=16384));servers.append(await asyncio.start_server(h1,'127.0.0.1',a.h1_port,ssl=context(['http/1.1']),limit=16384))
  if a.mode=='literal6':
   sock=socket.socket(socket.AF_INET6,socket.SOCK_STREAM);sock.setsockopt(socket.IPPROTO_IPV6,socket.IPV6_V6ONLY,1);sock.bind(('::1',a.v6_port));sock.listen();sock.setblocking(False);servers.append(await asyncio.start_server(v6_handler,sock=sock,ssl=context(['http/1.1']),limit=16384))
  ready={'suite':suite,'run':a.run_token,'mode':a.mode,'url':url,'h2_port':a.h2_port,'h1_port':a.h1_port,'deadline_seconds':a.deadline,'expected_bytes':sum(LENGTHS) if a.mode=='ipv4' else 4096};(a.output/'ready.json').write_text(json.dumps(ready,indent=2)+'\n');save();print(json.dumps({'status':'READY','run':a.run_token,'url':url}),flush=True)
  try:await asyncio.wait_for(stop.wait(),a.deadline)
  except asyncio.TimeoutError:state['deadline_expired']=True
 finally:
  for s in servers:s.close()
  for s in servers:await s.wait_closed()
  state['stopped']=True;save()
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--run-token',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--certificate',type=Path,required=True);p.add_argument('--key',type=Path,required=True);p.add_argument('--h2-port',type=int,default=8824);p.add_argument('--h1-port',type=int,default=8825);p.add_argument('--mode',choices=['ipv4','literal6'],default='ipv4');p.add_argument('--v6-port',type=int,default=8826);p.add_argument('--deadline',type=float,default=180);asyncio.run(serve(p.parse_args()))
