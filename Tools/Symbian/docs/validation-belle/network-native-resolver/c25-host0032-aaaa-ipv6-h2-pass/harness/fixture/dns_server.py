"""Only the fixed real loopback DNS service, in root's dedicated network NS."""
import asyncio,struct
from dns_protocol import reply
async def start(state,save,names):
 def resolve(data,transport):
  answer,row=reply(data,names,transport)
  if row['role']is None:state['unknown_dns_queries']+=1
  else:state['dns'].append(row)
  save();return answer
 class UDP(asyncio.DatagramProtocol):
  def connection_made(self,t):self.transport=t
  def datagram_received(self,data,addr):
   try:self.transport.sendto(resolve(data,'udp'),addr)
   except Exception as e:state['errors'].append({'stage':'dns-udp','type':type(e).__name__,'message':str(e)});save()
 async def tcp(reader,writer):
  try:
   n=struct.unpack('!H',await asyncio.wait_for(reader.readexactly(2),5))[0]
   if not 12<=n<=4096:raise ValueError('normal DNS TCP length bound')
   data=await asyncio.wait_for(reader.readexactly(n),5);answer=resolve(data,'tcp');writer.write(struct.pack('!H',len(answer))+answer);await writer.drain()
  except Exception as e:state['errors'].append({'stage':'dns-tcp','type':type(e).__name__,'message':str(e)});save()
  finally:writer.close()
 loop=asyncio.get_running_loop();udp,_=await loop.create_datagram_endpoint(UDP,local_addr=('127.0.0.1',53));server=await asyncio.start_server(tcp,'127.0.0.1',53);return udp,server
