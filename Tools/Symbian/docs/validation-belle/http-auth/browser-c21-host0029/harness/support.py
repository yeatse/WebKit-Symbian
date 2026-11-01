from pathlib import Path
import hashlib,json,re,importlib.util
ROOT=Path(__file__).resolve().parent
CASES=('success','cancel-user','cancel-password','retry','navigate-user','navigate-password','exit-user','exit-password')
SCHEMES=('basic','digest')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load_module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def require(value,message):
 if not value:raise RuntimeError(message)
def checked_inputs():
 pins=json.loads((ROOT/'pins.json').read_text());checked={}
 for name,item in pins['files'].items():
  p=Path(item['path']);require(p.is_file() and sha(p)==item['sha256'],'pinned public input changed: '+name);checked[name]=dict(item,bytes=p.stat().st_size)
 return checked

def geometry(value):return isinstance(value,list) and len(value)==4 and all(type(n)is int for n in value) and 0<value[2]<=8192 and 0<value[3]<=8192
def point(value,size):return isinstance(value,list) and len(value)==2 and all(type(n)is int for n in value) and 0<=value[0]<size[0] and 0<=value[1]<size[1]
def check_calibration(path,scheme,case):
 value=json.loads(Path(path).read_text());needs=[];pins=json.loads((ROOT/'pins.json').read_text())['files']
 require(isinstance(value,dict) and set(value)<={'calibrated','pins','main_geometry','start_point','start_point_overrides','username','password','exit_launch','note','content_rect'},'calibration accepts only public geometry/pin/role fields')
 for role in ('username','password'):
  fields=value.get(role,{})
  require(isinstance(fields,dict) and set(fields)<={'fep_geometry','edit_point','submit_point','native_editor_point','ok_point','cancel_point','widget','public_mask_fixture_reviewed'},'editor calibration contains unexpected fields')
 for key,name in [('browser_sha256','browser'),('host_sha256','host'),('fep_sha256','fep')]:
  require(value.get('pins',{}).get(key)==pins[name]['sha256'],'calibration belongs to another public input: '+key)
 if value.get('calibrated') is not True:needs.append('root calibrated=true after actual public non-credential UI review')
 main=value.get('main_geometry')
 if not geometry(value.get('content_rect')):needs.append('actual browser content rectangle')
 if not geometry(main):needs.append('actual main geometry')
 start=value.get('start_point_overrides',{}).get(scheme+'/'+case) or value.get('start_point')
 if not geometry(main) or not point(start,main[2:]):needs.append('actual Start point for '+scheme+'/'+case)
 roles=['username']+(['password'] if case in ('success','retry') or case.endswith('password') else [])
 for role in roles:
  fields=value.get(role,{})
  fep=fields.get('fep_geometry')
  if not geometry(fep):needs.append(role+' actual FEP geometry')
  for key in ('edit_point','submit_point'):
   if not geometry(fep) or not point(fields.get(key),fep[2:]):needs.append(role+' '+key)
  for key in ('ok_point','cancel_point'):
   if not geometry(main) or not point(fields.get(key),main[2:]):needs.append(role+' native '+key)
  optional=fields.get('native_editor_point')
  if role=='username' and optional is None:needs.append('username native editor point required after heading Escape')
  if optional is not None and (not geometry(main) or not point(optional,main[2:])):needs.append(role+' actual native editor point or null')
  if role=='username' and fields.get('widget')!='QPlainTextEdit':needs.append('username public calibration identifies QPlainTextEdit')
  if role=='password':
   if fields.get('widget')!='QLineEdit.Password':needs.append('password public calibration identifies QLineEdit.Password')
   if fields.get('public_mask_fixture_reviewed') is not True:needs.append('root public fixed non-credential mask fixture review')
 if case.startswith('exit'):
  launch=value.get('exit_launch',{})
  if launch.get('calibrated') is not True:needs.append('official EndTask helper launch UI calibration')
  size=launch.get('dialog_size')
  valid_size=isinstance(size,list) and len(size)==2 and all(type(n)is int and 0<n<=8192 for n in size)
  for key in ('menu','launch_item'):
   if not geometry(main) or not point(launch.get(key),main[2:]):needs.append('exit '+key)
  for key in ('path_field','launch_button'):
   if not valid_size or not point(launch.get(key),size):needs.append('exit '+key)
  if not valid_size:needs.append('actual Launch process dialog size')
 return value,start,needs
