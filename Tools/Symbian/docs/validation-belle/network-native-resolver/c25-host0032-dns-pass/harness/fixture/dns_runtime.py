"""Private namespace ordinary DNS plus controlled TLS endpoints; import is inert."""
import asyncio,base64,hashlib,json,socket,struct
from urllib.parse import urlsplit,parse_qs
from dns_protocol import reply
async def start(args,state,save,names,context):
 loop=asyncio.get_running_loop();servers=[]
 def resolve(data,transport):
  out,row=reply(data,names,transport)
  if row['role']is None:state['unknown_dns_queries']+=1
  else:state['dns'].append(row)
  save();return out
 class UDP(asyncio.DatagramProtocol):
  def connection_made(self,transport):self.transport=transport
  def datagram_received(self,data,addr):
   try:self.transport.sendto(resolve(data,'udp'),addr)
   except Exception as e:state['errors'].append({'stage':'dns-udp','type':type(e).__name__,'message':str(e)});save()
 async def tcp(reader,writer):
  try:
   n=struct.unpack('!H',await asyncio.wait_for(reader.readexactly(2),5))[0]
   if not 12<=n<=4096:raise ValueError('ordinary DNS TCP bound')
   data=await asyncio.wait_for(reader.readexactly(n),5);out=resolve(data,'tcp');writer.write(struct.pack('!H',len(out))+out);await writer.drain()
  except Exception as e:state['errors'].append({'stage':'dns-tcp','type':type(e).__name__,'message':str(e)});save()
  finally:writer.close()
 async def frame(reader):
  h=await asyncio.wait_for(reader.readexactly(2),10);opcode=h[0]&15;n=h[1]&127
  if h[0]&0x70 or not h[0]&128 or not h[1]&128 or n>125:raise ValueError('normal short masked frame required')
  mask=await reader.readexactly(4);raw=await reader.readexactly(n);return opcode,bytes(v^mask[i%4]for i,v in enumerate(raw))
 async def application(reader,writer,fallback=False):
  try:
   raw=await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'),10)
   if len(raw)>16384:raise ValueError('normal request header bound')
   lines=raw.decode('ascii').split('\r\n');method,target,version=lines[0].split(' ');headers={k.lower():v.strip()for k,v in (l.split(':',1)for l in lines[1:]if l)};u=urlsplit(target)
   if parse_qs(u.query)!={'run':[args.run_token]}:raise ValueError('fresh HTTP token differs')
   host=headers.get('host','').rsplit(':',1)[0].lower();roles=[r for r,n in names.items()if n==host];role=roles[0]if len(roles)==1 else None;peer=writer.get_extra_info('peername')[0];alpn=writer.get_extra_info('ssl_object').selected_alpn_protocol()
   if method!='GET'or version!='HTTP/1.1'or alpn!='http/1.1'or role is None or fallback!=(role=='fallback'):raise ValueError('fixed ordinary role/host/H1 endpoint required')
   if u.path=='/socket'and role=='aaaa':
    if headers.get('upgrade','').lower()!='websocket'or headers.get('sec-websocket-version')!='13':raise ValueError('normal websocket Upgrade required')
    key=headers.get('sec-websocket-key','')
    if len(base64.b64decode(key,validate=True))!=16:raise ValueError('normal browser nonce required')
    accept=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode();writer.write(('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+'\r\n\r\n').encode());await writer.drain();r={'role':role,'host':host,'peer':peer,'alpn':alpn,'upgrade':True,'dynamic_accept':True,'client_masked':True,'echoes':0,'bytes':0,'sha256':None,'normal_close':False,'close_code':None};state['wss'].append(r);opcode,data=await frame(reader)
    if opcode!=1 or data!=b'dns-ping':raise ValueError('fixed browser text differs')
    writer.write(b'\x81'+bytes([len(data)])+data);await writer.drain();r.update(echoes=1,bytes=len(data),sha256=hashlib.sha256(data).hexdigest());opcode,data=await frame(reader)
    if opcode!=8 or len(data)<2 or struct.unpack('!H',data[:2])[0]!=1000:raise ValueError('normal browser close1000 required')
    writer.write(b'\x88'+bytes([len(data)])+data);await writer.drain();r.update(normal_close=True,close_code=1000)
   elif u.path=='/'+role:
    body=bytes((j*13+len(role))%251 for j in range(4096));r={'role':role,'host':host,'peer':peer,'alpn':alpn,'bytes':len(body),'sha256':hashlib.sha256(body).hexdigest(),'response_complete':False};state['https'].append(r);writer.write(('HTTP/1.1 200 OK\r\nContent-Length: '+str(len(body))+'\r\nContent-Type: application/octet-stream\r\nAccess-Control-Allow-Origin: https://127.0.0.1:8824\r\nVary: Origin\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n').encode()+body);await writer.drain();r['response_complete']=True
   else:raise ValueError('fixed HTTP path differs')
  except Exception as e:state['errors'].append({'stage':'application','type':type(e).__name__,'message':str(e)})
  finally:writer.close();save()
 udp,_=await loop.create_datagram_endpoint(UDP,local_addr=('127.0.0.1',53));servers.append(await asyncio.start_server(tcp,'127.0.0.1',53))
 for family,address in [(socket.AF_INET6,'::1'),(socket.AF_INET,'127.0.0.1')]:
  sock=socket.socket(family,socket.SOCK_STREAM)
  if family==socket.AF_INET6:sock.setsockopt(socket.IPPROTO_IPV6,socket.IPV6_V6ONLY,1)
  sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);sock.bind((address,8830));sock.listen();sock.setblocking(False);servers.append(await asyncio.start_server(application,sock=sock,ssl=context(['http/1.1']),limit=16384))
 servers.append(await asyncio.start_server(lambda r,w:application(r,w,True),'127.0.0.1',8831,ssl=context(['http/1.1']),limit=16384));return servers,udp
