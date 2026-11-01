#!/usr/bin/env python3
"""Inside-namespace checks, followed only by the fixed caller command."""
import ctypes,errno,json,os,stat,sys
from pathlib import Path
def main():
 os.dup2(1,2)
 spec=json.loads(Path(sys.argv[1]).read_text());fd=int(sys.argv[2]);command=sys.argv[3:]
 phase='INITIAL'
 def fail():
  os.write(fd,('SANDBOX-GUARD '+json.dumps({'guard_ok':False,'guard_error_code':phase})+'\n').encode());return 125
 try:
  phase='CAPABILITIES'
  status=dict(line.split(':',1)for line in Path('/proc/self/status').read_text().splitlines()if':'in line)
  if any(int(status[k].strip(),16)&(1<<21)for k in ['CapEff','CapPrm'])or status['NoNewPrivs'].strip()!='1':return fail()
  phase='NAMESPACE'
  for key,name in [('user','user'),('mount','mnt'),('pid','pid')]:
   if os.stat('/proc/self/ns/'+name).st_ino==spec['parent_namespaces'][key]:return fail()
  phase='MOUNT_PARSE'
  mounts=[]
  for line in Path('/proc/self/mountinfo').read_text().splitlines():
   fields=line.split();sep=fields.index('-')
   point=Path(fields[4]);mounts.append((point,set(fields[5].split(','))))
   if any(v.startswith(('shared:','master:','propagate_from:'))for v in fields[6:sep]):return fail()
  def ro(path):
   # Kernel flags describe the visible mount even when /tmp is overmounted.
   return bool(os.statvfs(path).f_flag & os.ST_RDONLY)
  phase='ROOT_READONLY'
  if not ro('/'):return fail()
  phase='PROTECTED_READONLY'
  for path in spec.get('protected_paths',[]):
   if not ro(path):return fail()
  for index,row in enumerate(spec.get('ro_binds',[])):
   phase='RO_BIND_'+str(index)
   if not ro(row['destination']):return fail()
  for index,row in enumerate(spec.get('rw_binds',[])):
   phase='RW_BIND_'+str(index)
   if ro(row['destination']):return fail()
  phase='EMPTY_PATCH'
  cwd=Path(spec['cwd']);patch=cwd/'patch'
  if patch.is_symlink()or not patch.is_dir()or any(patch.iterdir()):return fail()
  phase='INHERITED_FD_CLASS'
  for name in os.listdir('/proc/self/fd'):
   number=int(name)
   if number==fd:continue
   try:value=os.readlink('/proc/self/fd/'+name);mode=os.stat('/proc/self/fd/'+name).st_mode
   except FileNotFoundError:continue
   if not(stat.S_ISFIFO(mode)or(stat.S_ISCHR(mode)and value=='/dev/null')):return fail()
  phase='USERNS_DISABLED'
  libc=ctypes.CDLL(None,use_errno=True)
  if libc.unshare(0x10000000)!=-1 or ctypes.get_errno()not in [errno.EPERM,errno.EACCES,errno.ENOSPC,getattr(errno,'EUSERS',87)]:return fail()
  os.write(fd,b'SANDBOX-GUARD {"guard_ok":true}\n')
  if fd!=1:os.close(fd)
  phase='EXEC_COMMAND'
  os.execvpe(command[0],command,spec['env'])
 except BaseException:return fail()
if __name__=='__main__':raise SystemExit(main())
