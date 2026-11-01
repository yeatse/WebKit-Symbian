#!/usr/bin/env python3
"""Read-only-root namespace runner; raw audit data exists in anonymous pipes only."""
import json,os,re,signal,stat,subprocess,tempfile,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FATAL=re.compile(rb'panicked|KERN-EXEC|USER-EXEC|segmentation fault|access violation|SIGSEGV|SIGABRT|Unable to launch process',re.I)
def run_sandbox(spec,command,timeout=60,output_handler=None,capture_public_stdout=False):
 result={'exit_code':None,'stopped':False,'forced_cleanup':False,'child_pid':None,'guard_ok':False,'audit_ok':False,'denied_mutations':0,'unsupported_async':0,'output_fatal':False,'unexpected_peers':0,'child_pid_associated':False,'denied_call_counts':{},'denied_role_counts':{}}
 process=None;threads=[];fds=[];audit_error=[];guard_count=[0];captured=[];total=[0]
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
  trace='%'+'file,write,pwrite64,writev,pwritev,pwritev2,ftruncate,fallocate,fchmod,fchown,mmap,mprotect,connect,io_uring_setup,io_uring_enter,io_uring_register,io_setup,io_submit,io_getevents,io_cancel,io_destroy'
  argv=['/usr/bin/strace','-f','-qq','-s','256','-e','trace='+trace,'-e','raw=write,pwrite64,writev,pwritev,pwritev2,ftruncate,fallocate,fchmod,fchown,mmap,mprotect,setxattr,lsetxattr,fsetxattr,getxattr,lgetxattr,fgetxattr,listxattr,llistxattr,flistxattr','-o','/proc/self/fd/'+str(trace_w),*bwrap]
  env={'PATH':'/usr/bin:/bin','LANG':'C','PYTHONDONTWRITEBYTECODE':'1'}
  process=subprocess.Popen(argv,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,close_fds=True,pass_fds=(info_w,trace_w),start_new_session=True)
  os.close(info_w);os.close(trace_w);fds.remove(info_w);fds.remove(trace_w)
  def audit():
   try:
    with os.fdopen(trace_r,'rb')as stream:
     for line in stream:
      if len(line)>65536:audit_error.append(True);continue
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
      if b'connect('in line:
       match=re.search(rb'sun_path=("(?:[^"\\]|\\.)*")',line)
       try:peer=json.loads(match.group(1))if match else None
       except(ValueError,UnicodeError):peer=None
       if re.search(rb'= (?:0|.*EINPROGRESS)\b',line)and peer not in spec.get('allowed_unix_sockets',[]):result['unexpected_peers']+=1
    fds.remove(trace_r)
   except BaseException:audit_error.append(True)
  def output():
   try:
    for line in process.stdout:
     total[0]+=len(line)
     if total[0]>16*1024*1024:audit_error.append(True);continue
     if line.startswith(b'SANDBOX-GUARD '):
      guard_count[0]+=1
      guard=json.loads(line[len(b'SANDBOX-GUARD '):]);result['guard_ok']=guard.get('guard_ok')is True
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
  try:result['exit_code']=process.wait(timeout=timeout)
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
  for fd in fds:
   try:os.close(fd)
   except OSError:pass
 return result
