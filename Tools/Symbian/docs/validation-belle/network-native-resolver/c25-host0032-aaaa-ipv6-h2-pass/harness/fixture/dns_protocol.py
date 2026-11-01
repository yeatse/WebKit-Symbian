"""Fixed ordinary IN A/AAAA loopback DNS; no forwarding or external lookup."""
import ipaddress,struct
def names_for(token):return {r:r+'-'+token.replace('_','-').lower()[-16:]+'.webkit-belle.test' for r in ['aaaa','dual','fallback']}
def reply(data,names,transport):
 if not 12<=len(data)<=4096:raise ValueError('bounded ordinary DNS query required')
 ident,flags,qd,an,ns,ar=struct.unpack('!6H',data[:12])
 if flags&0xF800 or qd!=1 or an or ns:raise ValueError('one normal DNS question required')
 labels=[];offset=12;wire=0
 for unused in range(128):
  if offset>=len(data):raise ValueError('question incomplete')
  n=data[offset];offset+=1
  if not n:break
  if n>63 or offset+n>len(data):raise ValueError('ordinary DNS label required')
  labels.append(data[offset:offset+n].decode('ascii').lower());offset+=n;wire+=n+1
  if wire>253:raise ValueError('DNS name bound')
 else:raise ValueError('DNS label count bound')
 if offset+4>len(data):raise ValueError('question type incomplete')
 typ,cls=struct.unpack('!HH',data[offset:offset+4]);offset+=4;name='.'.join(labels);roles=[r for r,n in names.items()if n==name];role=roles[0]if len(roles)==1 else None;answers=[];rcode=0 if role else 3
 if cls==1 and role:
  if typ==28:answers=['::1']if role!='fallback'else['::2']
  elif typ==1 and role!='aaaa':answers=['127.0.0.1']
 body=b''
 for address in answers:
  raw=ipaddress.ip_address(address).packed;body+=b'\xc0\x0c'+struct.pack('!HHIH',typ,1,0,len(raw))+raw
 return struct.pack('!6H',ident,0x8400|(flags&0x0100)|rcode,1,len(answers),0,0)+data[12:offset]+body,{'role':role,'name':name if role else None,'qtype':{1:'A',28:'AAAA'}.get(typ,'OTHER'),'answers':answers,'rcode':rcode,'transport':transport}
