#!/usr/bin/env python3
"""Read-only-root namespace runner; raw audit data exists in anonymous pipes only."""
import errno,ipaddress,json,os,re,signal,stat,subprocess,tempfile,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FATAL=re.compile(rb'panicked|KERN-EXEC|USER-EXEC|segmentation fault|access violation|SIGSEGV|SIGABRT|Unable to launch process',re.I)
PEER_ROLES=('AF_NETLINK_KERNEL_ROUTE','AF_INET_LOOPBACK','AF_UNIX_TASK_X11_ABSTRACT','AF_UNIX_TASK_X11_PATH','AF_UNIX_TASK_PULSE','OTHER_AF_NETLINK_UNKNOWN_PROTOCOL','OTHER_AF_UNIX_PATH','OTHER_AF_UNIX_ABSTRACT','OTHER_AF_INET_NONLOOPBACK','OTHER_AF_UNSPEC','OTHER_UNFINISHED_CONNECT','OTHER_FAMILY')
UNIX_PATH_PREFIXES=('TASK_TMPDIR','TMP_X11_DIR','TASK_PULSE_RUNTIME_DIR','RUN','TMP_OTHER','WORKSPACE_OTHER','HOME_OTHER','OTHER','LITERAL_X95','LITERAL_PULSE_NATIVE')
PEER_ROLES+=tuple('OTHER_AF_UNIX_PATH_'+category for category in UNIX_PATH_PREFIXES)
def unexpected_unix_path_role(peer,spec):
 """Classify a pathname lexically; return a fixed role, never its value."""
 def safe_absolute(value):return isinstance(value,str)and value.startswith('/')and '\0'not in value and '..'not in value.split('/')
 def under(prefix):
  return safe_absolute(prefix)and prefix.rstrip('/')and prefix.rstrip('/')!='/'and(peer==prefix.rstrip('/')or peer.startswith(prefix.rstrip('/')+'/'))
 category='OTHER'
 if safe_absolute(peer):
  env=spec.get('env',{});pulse=env.get('PULSE_SERVER','')
  pulse_dir=pulse[5:].rsplit('/',1)[0]if isinstance(pulse,str)and pulse.startswith('unix:')else''
  if peer=='/tmp/.X11-unix/X95':category='LITERAL_X95'
  elif peer=='/workspace/symbian-media-work/runtime/native':category='LITERAL_PULSE_NATIVE'
  elif under(env.get('TMPDIR','')):category='TASK_TMPDIR'
  elif under('/tmp/.X11-unix'):category='TMP_X11_DIR'
  elif under(pulse_dir):category='TASK_PULSE_RUNTIME_DIR'
  elif under('/run'):category='RUN'
  elif under('/tmp'):category='TMP_OTHER'
  elif under(env.get('HOME',''))or under('/home')or under('/root'):category='HOME_OTHER'
  elif under('/workspace'):category='WORKSPACE_OTHER'
 return 'OTHER_AF_UNIX_PATH_'+category
def trace_pid(line):
 match=re.match(rb'\s*(?:\[pid\s+)?([0-9]+)\]?\s+',line)
 return int(match.group(1))if match else None
def track_route_sockets(line,route_fds):
 """Keep only confirmed NETLINK_ROUTE descriptors in memory; no trace output."""
 if re.search(rb'\bexecve(?:at)?\(',line):route_fds.clear();return
 pid=trace_pid(line)
 if pid is None:
  if re.search(rb'\b(?:socket|close|dup|dup2|dup3|fcntl)\(',line):route_fds.clear()
  return
 match=re.search(rb'\b(socket|close|dup|dup2|dup3|fcntl)\((.*)\)\s+=\s+(-?[0-9]+)\b',line)
 if not match:
  if re.search(rb'\b(?:socket|close|dup|dup2|dup3|fcntl)\(|<\.\.\. (?:socket|close|dup|dup2|dup3|fcntl) resumed>',line):
   route_fds.clear()
  return
 call,args,returned=match.groups();returned=int(returned)
 if call==b'socket':
  if returned>=0:
   route_fds.difference_update(key for key in tuple(route_fds)if key[1]==returned)
   if re.fullmatch(rb'AF_NETLINK\s*,.*?,\s*NETLINK_ROUTE',args):route_fds.add((pid,returned))
  return
 first=re.match(rb'([0-9]+)\b',args)
 if not first:return
 source=int(first.group(1));known=(pid,source)in route_fds
 if call==b'close':route_fds.difference_update(key for key in tuple(route_fds)if key[1]==source)
 elif returned>=0 and(call in [b'dup',b'dup2',b'dup3']or(call==b'fcntl'and re.search(rb'\bF_DUPFD(?:_CLOEXEC)?\b',args))):
  route_fds.difference_update(key for key in tuple(route_fds)if key[1]==returned)
  if known:route_fds.add((pid,returned))
def unexpected_peer_role(line,spec,route_fds):
 """Return a fixed public role only, never a peer address, path, or payload."""
 if b'<unfinished ...>'in line or re.search(rb'<\.\.\. connect resumed>',line):return 'OTHER_UNFINISHED_CONNECT'
 frame=re.match(rb'^\s*(?:(?:\[pid\s+)?[0-9]+\]?\s+)?connect\(\s*([0-9]+)\s*,(.*),\s*[0-9]+\s*\)\s*=\s*-?[0-9]+\b',line)
 if not frame:return 'OTHER_UNFINISHED_CONNECT'
 descriptor,body=frame.groups()
 match=re.match(rb'\s*\{\s*sa_family\s*=\s*([A-Za-z0-9_x]+)(?=\s*[,}])',body)
 family=match.group(1)if match else None
 family={b'0':b'AF_UNSPEC',b'1':b'AF_UNIX',b'2':b'AF_INET',b'10':b'AF_INET6',b'16':b'AF_NETLINK'}.get(family,family)
 if family==b'AF_UNSPEC':return 'OTHER_AF_UNSPEC'
 if family==b'AF_NETLINK':
  if re.search(rb'\bnl_pid=0\b',body)and(trace_pid(line),int(descriptor))in route_fds:return 'AF_NETLINK_KERNEL_ROUTE'
  return 'OTHER_AF_NETLINK_UNKNOWN_PROTOCOL'
 if family in [b'AF_INET',b'AF_INET6']:
  pattern=rb'inet_addr\("([^"]+)"\)'if family==b'AF_INET'else rb'inet_pton\(AF_INET6,\s*"([^"]+)"'
  match=re.search(pattern,body)
  if match:
   try:
    address=ipaddress.ip_address(match.group(1).decode('ascii'))
    if address.version!=(4 if family==b'AF_INET'else 6):return 'OTHER_FAMILY'
    return 'AF_INET_LOOPBACK'if address.is_loopback else'OTHER_AF_INET_NONLOOPBACK'
   except(ValueError,UnicodeError):pass
  return 'OTHER_FAMILY'
 if family in [b'AF_UNIX',b'AF_LOCAL']:
  match=re.search(rb'sun_path=(@?)("(?:[^"\\]|\\.)*")',body)
  try:peer=json.loads(match.group(2))if match else None
  except(ValueError,UnicodeError):peer=None
  abstract=bool(match and match.group(1))or isinstance(peer,str)and peer.startswith('\0')
  if abstract and isinstance(peer,str):peer=peer.lstrip('\0')
  display=spec.get('env',{}).get('DISPLAY','')
  if re.fullmatch(r':[0-9]+(?:\.[0-9]+)?',display)and peer=='/tmp/.X11-unix/X'+display[1:].split('.')[0]:
   return 'AF_UNIX_TASK_X11_ABSTRACT'if abstract else'AF_UNIX_TASK_X11_PATH'
  pulse=spec.get('env',{}).get('PULSE_SERVER','')
  if pulse.startswith('unix:')and peer==pulse[5:]:return 'AF_UNIX_TASK_PULSE'
  return 'OTHER_AF_UNIX_ABSTRACT'if abstract else'OTHER_AF_UNIX_PATH'
 return 'OTHER_FAMILY'
def parsed_connect(body):
 """Parse one complete strace connect, with quoted sockaddr braces respected."""
 start=re.match(rb'^connect\(\s*([0-9]+)\s*,\s*',body)
 if not start:raise ValueError('CONNECT_FRAME')
 offset=start.end()
 if body[offset:offset+1]!=b'{':raise ValueError('CONNECT_SOCKADDR')
 quoted=escaped=False;depth=0;end=None
 for index in range(offset,len(body)):
  char=body[index]
  if quoted:
   if escaped:escaped=False
   elif char==92:escaped=True
   elif char==34:quoted=False
  elif char==34:quoted=True
  elif char==123:depth+=1
  elif char==125:
   depth-=1
   if depth==0:end=index+1;break
 if end is None or quoted:raise ValueError('CONNECT_SOCKADDR')
 address=body[offset:end]
 tail=re.fullmatch(rb'\s*,\s*([0-9]+)\s*\)\s*=\s*(-?[0-9]+)(?:\s+([A-Z][A-Z0-9_]*)(?:\s+\([^\r\n]*\))?)?\s*',body[end:])
 if not tail:raise ValueError('CONNECT_RESULT')
 length,returned,error=tail.groups();length=int(length);returned=int(returned)
 if not 2<=length<=65535 or returned not in [0,-1]or(returned==0 and error is not None)or(returned==-1 and error is None):raise ValueError('CONNECT_RESULT')
 if error is not None and error not in [value.encode()for value in errno.errorcode.values()]:raise ValueError('CONNECT_RESULT')
 family_match=re.match(rb'^\{\s*sa_family\s*=\s*([A-Za-z0-9_x]+)(?=\s*[,}])',address)
 family=family_match.group(1)if family_match else None
 family={b'0':b'AF_UNSPEC',b'1':b'AF_UNIX',b'2':b'AF_INET',b'10':b'AF_INET6',b'16':b'AF_NETLINK'}.get(family,family)
 if family not in [b'AF_UNSPEC',b'AF_UNIX',b'AF_LOCAL',b'AF_INET',b'AF_INET6',b'AF_NETLINK']:raise ValueError('CONNECT_UNKNOWN_FAMILY')
 peer=None;abstract=False
 if family in [b'AF_UNIX',b'AF_LOCAL']:
  unix=re.fullmatch(rb'\{\s*sa_family\s*=\s*[A-Za-z0-9_x]+\s*,\s*sun_path\s*=\s*(@?)("(?:[^"\\]|\\.)*")\s*\}',address)
  if not unix:raise ValueError('CONNECT_UNIX_ADDRESS')
  try:peer=json.loads(unix.group(2))
  except(ValueError,UnicodeError):raise ValueError('CONNECT_UNIX_ADDRESS')from None
  if not isinstance(peer,str)or not peer:raise ValueError('CONNECT_UNIX_ADDRESS')
  abstract=bool(unix.group(1))or peer.startswith('\0')
  try:minimum=2+len(peer.encode('utf-8'))+int(bool(unix.group(1)))
  except UnicodeError:raise ValueError('CONNECT_UNIX_ADDRESS')from None
  if not minimum<=length<=110:raise ValueError('CONNECT_UNIX_LENGTH')
 elif family==b'AF_NETLINK':
  if length<12:raise ValueError('CONNECT_NETLINK_ADDRESS')
  if not re.fullmatch(rb'\{\s*sa_family\s*=\s*[A-Za-z0-9_x]+\s*,\s*nl_pid=[0-9]+\s*,\s*nl_groups=(?:0x[0-9a-fA-F]+|[0-9]+)\s*\}',address):raise ValueError('CONNECT_NETLINK_ADDRESS')
 elif family in [b'AF_INET',b'AF_INET6']:
  if length<(16 if family==b'AF_INET'else 28):raise ValueError('CONNECT_INET_ADDRESS')
  pattern=rb'\{\s*sa_family\s*=\s*[A-Za-z0-9_x]+\s*,\s*sin_port=htons\(([0-9]+)\)\s*,\s*sin_addr=inet_addr\("([^"]+)"\)\s*\}'if family==b'AF_INET'else rb'\{\s*sa_family\s*=\s*[A-Za-z0-9_x]+\s*,\s*sin6_port=htons\(([0-9]+)\)\s*,\s*(?:sin6_flowinfo=htonl\([0-9]+\)\s*,\s*)?inet_pton\(AF_INET6,\s*"([^"]+)",\s*&sin6_addr\)(?:\s*,\s*sin6_scope_id=[0-9]+)?\s*\}'
  inet=re.fullmatch(pattern,address)
  if not inet or not 0<=int(inet.group(1))<=65535:raise ValueError('CONNECT_INET_ADDRESS')
  try:parsed=ipaddress.ip_address(inet.group(2).decode('ascii'))
  except(ValueError,UnicodeError):raise ValueError('CONNECT_INET_ADDRESS')from None
  if parsed.version!=(4 if family==b'AF_INET'else 6):raise ValueError('CONNECT_INET_ADDRESS')
 return family,peer,abstract,returned==0 or error==b'EINPROGRESS'

class ConnectAudit:
 """Pair split records by TID in bounded memory. Export fixed counts only."""
 def __init__(self,spec,result,audit_errors):
  self.spec=spec;self.result=result;self.audit_errors=audit_errors;self.pending={}
  result.setdefault('connect_parse_errors',0);result.setdefault('unpaired_connects',0)
 def unexpected(self,role):
  self.result['unexpected_peers']+=1
  self.result['unexpected_peer_role_counts'][role]+=1
 def fail(self,role='OTHER_FAMILY',unpaired=False):
  self.result['connect_parse_errors']+=1;self.audit_errors.append(True);self.unexpected(role)
  if unpaired:self.result['unpaired_connects']+=1
 def feed(self,line,route_fds):
  prefix=re.match(rb'^\s*(?:\[pid\s+)?([0-9]+)\]?\s+',line)
  body=line[prefix.end():].strip()if prefix else line.strip()
  resumed=body.startswith(b'<... connect resumed>')
  if body.startswith(b'<... connect')and not resumed:self.fail('OTHER_UNFINISHED_CONNECT',True);return
  if not(body.startswith(b'connect')or resumed):return
  if prefix is None:self.fail();return
  pid=int(prefix.group(1))
  if pid<=0:self.fail();return
  if resumed:
   first=self.pending.pop(pid,None)
   if first is None:self.fail('OTHER_UNFINISHED_CONNECT',True);return
   body=first+body[len(b'<... connect resumed>'):]
  elif pid in self.pending:
   self.pending.pop(pid);self.fail('OTHER_UNFINISHED_CONNECT',True)
  if len(body)>65536:self.fail('OTHER_UNFINISHED_CONNECT',True);return
  marker=b'<unfinished ...>'
  if marker in body:
   if resumed or not body.endswith(marker)or not body.startswith(b'connect(')or len(self.pending)>=64:self.fail('OTHER_UNFINISHED_CONNECT',True);return
   self.pending[pid]=body[:-len(marker)]
   return
  try:family,peer,abstract,connected=parsed_connect(body)
  except ValueError:self.fail('OTHER_FAMILY');return
  framed=str(pid).encode()+b' '+body
  role=unexpected_peer_role(framed,self.spec,route_fds)
  if role=='OTHER_AF_UNIX_PATH':role=unexpected_unix_path_role(peer,self.spec)
  if role in ['OTHER_FAMILY','OTHER_UNFINISHED_CONNECT']:self.fail(role);return
  # Only a complete, proven UNIX pathname can use the original exact list.
  # Unknown family/framing always fails; no role grants an exception.
  allowed=family in [b'AF_UNIX',b'AF_LOCAL']and not abstract and peer in self.spec.get('allowed_unix_sockets',[])
  if family==b'AF_UNSPEC'or connected and not allowed:self.unexpected(role)
 def finish(self):
  for _ in self.pending:self.fail('OTHER_UNFINISHED_CONNECT',True)
  self.pending.clear()

def count_unexpected_peer(line,spec,result,route_fds):
 """Host-test convenience for a single complete record; runtime pairs a stream."""
 errors=[];audit=ConnectAudit(spec,result,errors);audit.feed(line,route_fds);audit.finish()
 return len(errors)
class ExecCandidates:
 """Keep only successful exact command-path exec TIDs from anonymous audit."""
 def __init__(self,expected,result):
  if not isinstance(expected,str)or not expected.startswith('/')or any(c in expected for c in '\\"\n\r\0')or len(expected.encode())>256:raise ValueError('FIXED_EXEC_PATH_REQUIRED')
  self.expected=b'"'+expected.encode()+b'"';self.result=result;self.pending=set();self.pids=set();self.lock=threading.Lock()
  result['exec_candidate_count']=0;result['exec_candidate_parse_errors']=0
 def snapshot(self):
  with self.lock:return tuple(sorted(self.pids))
 def accept(self,pid):
  with self.lock:
   if len(self.pids)>=16 and pid not in self.pids:self.result['exec_candidate_parse_errors']+=1;return
   self.pids.add(pid);self.result['exec_candidate_count']=len(self.pids)
 def feed(self,line):
  prefix=re.match(rb'^\s*(?:\[pid\s+)?([0-9]+)\]?\s+',line)
  if not prefix:return
  pid=int(prefix.group(1));body=line[prefix.end():].strip()
  if pid<=0:return
  if body.startswith(b'<... execve resumed>'):
   if pid not in self.pending:return
   self.pending.remove(pid)
   if re.fullmatch(rb'<\.\.\. execve resumed>.*\)\s+=\s+0',body):self.accept(pid)
   elif not re.fullmatch(rb'<\.\.\. execve resumed>.*\)\s+=\s+-1\s+[A-Z0-9_]+.*',body):self.result['exec_candidate_parse_errors']+=1
   return
  match=re.match(rb'^execve\(("(?:[^"\\]|\\.)*"),',body)
  if not match or match.group(1)!=self.expected:return
  if pid in self.pending:self.pending.remove(pid);self.result['exec_candidate_parse_errors']+=1
  if body.endswith(b'<unfinished ...>'):
   if len(self.pending)>=16:self.result['exec_candidate_parse_errors']+=1
   else:self.pending.add(pid)
  elif re.fullmatch(rb'execve\(.*\)\s+=\s+0',body):self.accept(pid)
  elif not re.fullmatch(rb'execve\(.*\)\s+=\s+-1\s+[A-Z0-9_]+.*',body):self.result['exec_candidate_parse_errors']+=1
 def finish(self):
  self.result['exec_candidate_parse_errors']+=len(self.pending);self.pending.clear()

def wait_with_controller(process,timeout,result,guard_identity,controller_hook,root_identity,exec_capture=None):
 """Only polls an already owned process; a hook never changes audit decisions."""
 deadline=time.monotonic()+timeout;hook_failed=False
 result['controller_error_count']=0
 while True:
  remaining=deadline-time.monotonic()
  if remaining<=0:raise subprocess.TimeoutExpired(process.args,timeout)
  try:return process.wait(timeout=min(0.1,remaining))
  except subprocess.TimeoutExpired:
   if hook_failed:continue
   context=dict(root_identity,guard_ok=result.get('guard_ok')is True,
                child_pid_associated=result.get('child_pid_associated')is True)
   context.update(guard_identity)
   if exec_capture is not None:context['exec_candidate_pids']=exec_capture.snapshot()
   if not context['guard_ok']or not context['child_pid_associated']:continue
   if not all(type(context.get(k))is int and context[k]>0 for k in ['namespace_pid','pidns_inode']):continue
   try:controller_hook(context)
   except BaseException:
    result['controller_error_count']+=1;hook_failed=True

def run_sandbox(spec,command,timeout=60,output_handler=None,capture_public_stdout=False,controller_hook=None):
 result={'exit_code':None,'stopped':False,'forced_cleanup':False,'child_pid':None,'guard_ok':False,'audit_ok':False,'denied_mutations':0,'unsupported_async':0,'output_fatal':False,'unexpected_peers':0,'child_pid_associated':False,'denied_call_counts':{},'denied_role_counts':{}}
 result.update(unexpected_peer_role_counts=dict.fromkeys(PEER_ROLES,0),guard_count=0,audit_error_count=0,collectors_live=0)
 process=None;threads=[];fds=[];audit_error=[];guard_count=[0];captured=[];total=[0];route_fds=set();connect_audit=ConnectAudit(spec,result,audit_error);guard_identity={};exec_capture=ExecCandidates(command[0],result)
 try:
  rows=[('ro',r)for r in spec.get('ro_binds',[])]+[('rw',r)for r in spec.get('rw_binds',[])]
  protected=[Path(p).resolve()for p in spec.get('protected_paths',[])]
  for kind,row in rows:
   for key in ['source','destination']:
    p=Path(row[key])
    if not p.is_absolute()or p!=p.resolve()or p.is_symlink():raise ValueError('unsafe bind')
   if kind=='rw':
    for p in protected:
     if p.is_relative_to(Path(row['source']))or p.is_relative_to(Path(row['destination'])):raise ValueError('RW covers backup')
  cwd=Path(spec['cwd']);cwd.mkdir(parents=True,exist_ok=True)
  patch=cwd/'patch'
  if patch.is_symlink():raise ValueError('patch symlink')
  patch.mkdir(exist_ok=True)
  if any(patch.iterdir()):raise ValueError('patch not empty')
  prepared=dict(spec);prepared['parent_namespaces']={key:os.stat('/proc/self/ns/'+name).st_ino for key,name in [('user','user'),('mount','mnt'),('pid','pid')]}
  # Spec contains paths/environment, never original config contents. It is private.
  fd,path=tempfile.mkstemp(prefix='sandbox-spec-',suffix='.json',dir=cwd);os.fchmod(fd,0o600)
  with os.fdopen(fd,'w')as stream:json.dump(prepared,stream)
  info_r,info_w=os.pipe();trace_r,trace_w=os.pipe();fds=[info_r,info_w,trace_r,trace_w]
  bwrap=['/usr/bin/unshare','--user','--map-root-user','--mount','--propagation','private','--','/usr/bin/bwrap','--unshare-user','--unshare-pid','--unshare-net','--disable-userns','--assert-userns-disabled','--cap-drop','CAP_SYS_ADMIN','--die-with-parent','--info-fd',str(info_w),'--ro-bind','/','/','--proc','/proc','--dev','/dev','--size','67108864','--tmpfs','/tmp','--size','16777216','--tmpfs','/run']
  # Parent binds precede child mounts, irrespective of RO/RW type.
  for kind,row in sorted(rows,key=lambda item:len(Path(item[1]['destination']).parts)):
   bwrap += ['--ro-bind'if kind=='ro'else'--bind',row['source'],row['destination']]
  bwrap += ['--chdir',str(cwd),'--clearenv']
  for key,value in spec['env'].items():bwrap += ['--setenv',key,value]
  bwrap += ['/usr/bin/python3','-B',str(ROOT/'sandbox_guard.py'),path,'1',*command]
  # Raw decoding never dereferences strings/buffers. Conservatively count all
  # denied file-family operations; unsupported async submissions always fail.
  trace='%'+'file,write,pwrite64,writev,pwritev,pwritev2,ftruncate,fallocate,fchmod,fchown,mmap,mprotect,connect,socket,close,dup,dup2,dup3,fcntl,io_uring_setup,io_uring_enter,io_uring_register,io_setup,io_submit,io_getevents,io_cancel,io_destroy'
  argv=['/usr/bin/strace','-f','-qq','-s','256','-e','trace='+trace,'-e','raw=write,pwrite64,writev,pwritev,pwritev2,ftruncate,fallocate,fchmod,fchown,mmap,mprotect,setxattr,lsetxattr,fsetxattr,getxattr,lgetxattr,fgetxattr,listxattr,llistxattr,flistxattr','-o','/proc/self/fd/'+str(trace_w),*bwrap]
  env={'PATH':'/usr/bin:/bin','LANG':'C','PYTHONDONTWRITEBYTECODE':'1'}
  process=subprocess.Popen(argv,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,close_fds=True,pass_fds=(info_w,trace_w),start_new_session=True)
  os.close(info_w);os.close(trace_w);fds.remove(info_w);fds.remove(trace_w)
  def audit():
   try:
    with os.fdopen(trace_r,'rb')as stream:
     for line in stream:
      if len(line)>65536:audit_error.append(True);continue
      exec_capture.feed(line)
      track_route_sockets(line,route_fds)
      if b'io_uring_'in line or re.search(rb'\bio_(?:submit|setup|getevents|cancel|destroy)\(',line):result['unsupported_async']+=1
      if re.search(rb'= -1 (?:EACCES|EROFS|EPERM|EXDEV|EBUSY)\b',line):
       call=re.search(rb'\b([a-zA-Z0-9_]+)\(',line);name=call.group(1).decode()if call else''
       mutation=name in ['creat','unlink','unlinkat','rename','renameat','renameat2','mkdir','mkdirat','rmdir','link','linkat','symlink','symlinkat','mknod','mknodat','truncate','ftruncate','fallocate','chmod','fchmod','fchmodat','chown','fchown','lchown','fchownat','utime','utimes','futimesat','utimensat','setxattr','lsetxattr','fsetxattr','removexattr','lremovexattr','fremovexattr','write','writev','pwrite64','pwritev','pwritev2','openat2']
       if name in ['open','openat']:
        pattern=rb'\bopenat\([^,]*,[^,]*,\s*(0x[0-9a-f]+|[0-9]+)'if name=='openat'else rb'\bopen\([^,]*,\s*(0x[0-9a-f]+|[0-9]+)'
        flags=re.search(pattern,line);mutation=not flags or bool(int(flags.group(1),0)&(3|0x40|0x200|0x400))
       if name in ['mmap','mprotect']:
        flags=re.search(rb'\b(?:mmap|mprotect)\([^,]*,[^,]*,\s*(0x[0-9a-f]+|[0-9]+)',line);mutation=not flags or bool(int(flags.group(1),0)&2)
       if name in ['open','openat']and b'O_'in line:
        mutation=any(flag in line for flag in [b'O_WRONLY',b'O_RDWR',b'O_CREAT',b'O_TRUNC',b'O_APPEND'])or b'O_RDONLY'not in line
       if mutation:
        result['denied_mutations']+=1;result['denied_call_counts'][name]=result['denied_call_counts'].get(name,0)+1
        role='UNCLASSIFIED'
        quoted=re.search(rb'"(?:[^"\\]|\\.)*"',line)
        try:location=json.loads(quoted.group(0))if quoted else None
        except(ValueError,UnicodeError):location=None
        if isinstance(location,str):
         data_root=spec['env'].get('XDG_DATA_HOME','')+'/EKA2L1/data/'
         if location.startswith(data_root):
          suffix=location[len(data_root):];role='DATA_OTHER'
          for part,label in [('roms/','DATA_ROM'),('drives/','DATA_DRIVES'),('cache/','DATA_CACHE'),('devices.yml','DATA_CATALOG')]:
           if suffix.startswith(part):role=label;break
         elif location.rstrip('/')==data_root.rstrip('/'):role='DATA_ROOT'
         elif location.startswith(spec['env'].get('XDG_CONFIG_HOME','/missing')+'/'):role='CONFIG_TREE'
         elif location.startswith(spec['env'].get('XDG_DATA_HOME','/missing')+'/EKA2L1/'):role='BASE_OTHER'
         elif location.startswith('/var/cache/fontconfig'):role='SYSTEM_FONT_CACHE'
         elif location.startswith('/usr/'):role='SYSTEM_USR'
         elif location.startswith('/var/'):role='SYSTEM_VAR'
         elif location.startswith('/etc/'):role='SYSTEM_ETC'
         elif location.startswith('/dev/'):role='SYSTEM_DEV'
         elif location.startswith('/run/'):role='SYSTEM_RUN'
         elif location.startswith('/proc/'):role='SYSTEM_PROC'
         elif location.startswith('/root/')or location.startswith('/home/'):role='SYSTEM_HOME'
         elif location.startswith(spec['env'].get('TMPDIR','/missing')+'/')or location.startswith('/tmp/'):role='TASK_TMP'
         elif not location.startswith('/'):
          role='RELATIVE_OTHER'
          for part,label in [('cache','RELATIVE_CACHE'),('scripts','RELATIVE_SCRIPTS'),('resources','RELATIVE_RESOURCES'),('compat','RELATIVE_COMPAT'),('config','RELATIVE_CONFIG')]:
           if location.startswith(part):role=label;break
        result['denied_role_counts'][role]=result['denied_role_counts'].get(role,0)+1
      if b'ptrace'in line or b'detached'in line:audit_error.append(True)
      connect_audit.feed(line,route_fds)
    fds.remove(trace_r)
   except BaseException:audit_error.append(True)
   finally:connect_audit.finish();exec_capture.finish()
  def output():
   try:
    for line in process.stdout:
     total[0]+=len(line)
     if total[0]>16*1024*1024:audit_error.append(True);continue
     if line.startswith(b'SANDBOX-GUARD '):
      guard_count[0]+=1
      guard=json.loads(line[len(b'SANDBOX-GUARD '):]);result['guard_ok']=guard.get('guard_ok')is True
      if controller_hook is not None:
       namespace_pid=guard.get('namespace_pid');pidns_inode=guard.get('pidns_inode')
       if type(namespace_pid)is not int or namespace_pid<=0 or type(pidns_inode)is not int or pidns_inode<=0:raise ValueError('guard owned identity')
       guard_identity.update(namespace_pid=namespace_pid,pidns_inode=pidns_inode)
      code=guard.get('guard_error_code');result['guard_error_code']=code if isinstance(code,str)and re.fullmatch(r'[A-Z0-9_]{1,64}',code)else None
      continue
     result['output_fatal'] |= FATAL.search(line)is not None
     if output_handler:output_handler(line)
     if capture_public_stdout:captured.append(line)
   except BaseException:audit_error.append(True)
  def errors():
   # strace diagnostics cannot be mistaken for a valid audit. Never persist text.
   data=process.stderr.read(65537)
   if data:audit_error.append(True)
  def info():
   try:
    data=b'';value=None
    while len(data)<65536:
     part=os.read(info_r,4096)
     if not part:break
     data+=part
     try:value=json.loads(data);break
     except(ValueError,UnicodeError):pass
    os.close(info_r);fds.remove(info_r);pid=value.get('child-pid');result['child_pid']=pid
    if type(pid)is not int or pid<=0:raise ValueError('pid')
    status=Path('/proc/'+str(pid)+'/status').read_text();tracer=re.search(r'^TracerPid:\s+(\d+)',status,re.M)
    if tracer and int(tracer.group(1))==process.pid:result['child_pid_associated']=True
    current=pid
    for _ in range(16):
     tail=Path('/proc/'+str(current)+'/stat').read_text().rsplit(')',1)[1].split()
     if current==pid:result['child_starttime']=int(tail[19])
     current=int(tail[1])
     if current==process.pid:result['child_pid_associated']=True;break
     if current<=1:break
   except BaseException:audit_error.append(True)
  for fn in [audit,output,errors,info]:
   thread=threading.Thread(target=fn,daemon=True);thread.start();threads.append(thread)
  try:
   if controller_hook is None:result['exit_code']=process.wait(timeout=timeout)
   else:
    tail=Path('/proc/'+str(process.pid)+'/stat').read_text().rsplit(')',1)[1].split()
    if int(tail[2])!=process.pid:raise ValueError('owned root group')
    root_identity={'root_pid':process.pid,'root_starttime':int(tail[19]),'owned_pgid':process.pid}
    result['exit_code']=wait_with_controller(process,timeout,result,guard_identity,controller_hook,root_identity,exec_capture)
  except subprocess.TimeoutExpired:
   result['forced_cleanup']=True;os.killpg(process.pid,signal.SIGKILL);result['exit_code']=process.wait(timeout=5)
  for thread in threads:thread.join(timeout=3)
  # Do not trust a reported PID for signals. Only kill the original owned group.
  # strace -f reaps its traced tree; EOF on all pipes is also required.
  live_group=False;zombies=0
  for entry in Path('/proc').iterdir():
   if not entry.name.isdigit():continue
   try:tail=(entry/'stat').read_text().rsplit(')',1)[1].split()
   except(OSError,ValueError):continue
   if int(tail[2])==process.pid:
    if tail[0]=='Z':zombies+=1
    else:live_group=True
  result['owned_group_zombies']=zombies
  result['stopped']=process.poll()is not None and not live_group and not any(t.is_alive()for t in threads)
  result['guard_ok']=result['guard_ok']and guard_count[0]==1
  result['audit_capture_ok']=result['stopped']and result['child_pid_associated']and not audit_error
  result['audit_ok']=result['audit_capture_ok']and not result['unexpected_peers']and result['denied_mutations']==0 and result['unsupported_async']==0
  if capture_public_stdout:result['preflightstdout_text']=b''.join(captured).decode('utf-8',errors='strict')
  Path(path).unlink(missing_ok=True)
 except BaseException:
  if process and process.poll()is None:
   result['forced_cleanup']=True
   try:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=5)
   except BaseException:pass
  result['audit_ok']=False
 finally:
  result['guard_count']=guard_count[0]
  result['audit_error_count']=len(audit_error)
  result['collectors_live']=sum(thread.is_alive()for thread in threads)
  for fd in fds:
   try:os.close(fd)
   except OSError:pass
 return result
