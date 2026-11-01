"""Root reviews the untouched native username query while its empty FEP stays open."""
from pathlib import Path
from urllib.parse import urlsplit
import ctypes as C
import json,os,re,struct,time,uuid
from mask_review import require,sha,relative_file,capture_fep
SCHEMA='auth-public-native-heading-v2'
GATE_KEYS={'schema','run','scheme','case','pid','window','geometry','image','image_sha256','expected_heading','expected_origin','private_username_input_started','empty_username','state','fep_window','fep_original_geometry','fep_review_geometry','content_rect','screen_geometry'}
APPROVAL_KEYS={'schema','run','scheme','case','pid','window','geometry','gate_sha256','image_sha256','observed_heading','observed_origin','observed_empty_username'}
PROOF_KEYS={'pid','window','geometry','gate','gate_sha256','image','image_sha256','approval','approval_sha256','approved','fep_window','fep_original_geometry','fep_review_geometry','empty_fep_kept_open','empty_fep_restored_before_private_input'}

def rect(value):
 return isinstance(value,list) and len(value)==4 and all(type(n)is int for n in value) and 0<value[2]<=8192 and 0<value[3]<=8192

def check_gate(path):
 path=Path(path);require(path.is_file() and not path.is_symlink(),'heading gate missing');g=json.loads(path.read_text());require(isinstance(g,dict) and set(g)==GATE_KEYS,'heading gate shape changed')
 require(g['schema']==SCHEMA and re.fullmatch(r'[A-Za-z0-9_-]{8,80}',g['run']) and g['scheme'] in ('basic','digest'),'heading identity invalid')
 require(g['case'] in ('success','retry','cancel-user','cancel-password','navigate-user','navigate-password','exit-user','exit-password'),'heading case invalid')
 require(type(g['pid'])is int and g['pid']>0 and all(re.fullmatch(r'0x[0-9a-f]+',g[k]) for k in ('window','fep_window')) and g['window']!=g['fep_window'],'heading owner invalid')
 for k in ('geometry','fep_original_geometry','fep_review_geometry','content_rect','screen_geometry'):require(rect(g[k]),'heading geometry invalid')
 m,f,o,c,s=[g[k] for k in ('geometry','fep_review_geometry','fep_original_geometry','content_rect','screen_geometry')]
 require(f[2:]==o[2:] and f[0]>=m[0]+c[0]+c[2] and s[0]<=f[0] and s[1]<=f[1] and f[0]+f[2]<=s[0]+s[2] and f[1]+f[3]<=s[1]+s[3],'review FEP obscures content or leaves screen')
 require(0<=c[0] and 0<=c[1] and c[0]+c[2]<=m[2] and c[1]+c[3]<=m[3],'guest content outside main')
 require(g['expected_heading']=='HTTP sign in' and re.fullmatch(r'http://localhost:[0-9]{4,5}',g['expected_origin']),'heading public labels invalid')
 require(g['private_username_input_started']is False and g['empty_username']is True and g['state']=='WAITING_ROOT_HEADING_REVIEW','heading not untouched public query')
 require(g['image']=='empty-native-query.png','unknown heading image');image=relative_file(path.parent,g['image']);data=image.read_bytes();require(sha(image)==g['image_sha256'],'heading image changed')
 require(24<=len(data)<=8*1024*1024 and data[:8]==b'\x89PNG\r\n\x1a\n' and data[12:16]==b'IHDR' and list(struct.unpack('>II',data[16:24]))==m[2:],'heading PNG geometry changed');return g,image

def check_approval(path,gate_path,gate):
 a=json.loads(Path(path).read_text());expected={k:gate[k] for k in ('schema','run','scheme','case','pid','window','geometry')};expected.update(gate_sha256=sha(gate_path),image_sha256=gate['image_sha256'],observed_heading=gate['expected_heading'],observed_origin=gate['expected_origin'],observed_empty_username=True)
 require(isinstance(a,dict) and set(a)==APPROVAL_KEYS and a==expected,'root heading approval does not match this image');return a

def approve(path,pid,image_sha256,origin):
 path=Path(path);g,_=check_gate(path);require(pid==g['pid'] and image_sha256==g['image_sha256'] and origin==g['expected_origin'],'reviewed heading PID/image/origin differs')
 a={k:g[k] for k in ('schema','run','scheme','case','pid','window','geometry')};a.update(gate_sha256=sha(path),image_sha256=image_sha256,observed_heading=g['expected_heading'],observed_origin=origin,observed_empty_username=True)
 target=path.parent/'approval.json';temp=target.with_name('.approval-'+uuid.uuid4().hex+'.tmp')
 try:
  with os.fdopen(os.open(temp,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600),'w') as f:json.dump(a,f,indent=2);f.write('\n')
  os.link(temp,target)
 finally:temp.unlink(missing_ok=True)

def move_window(device,window,geometry):
 # Ordinary X11 geometry change on the already-owned Qt dialog; no guest or
 # editor state is changed. No shell or process-global input state is used.
 fn=device.x.XMoveWindow;fn.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_int];fn.restype=C.c_int
 require(fn(device.display,window,*geometry[:2])!=0,'XMoveWindow failed');device.x.XSync(device.display,0)

def perform_heading(device,key_window,ui,ready,pid,root,proofs,record,wait,timeout=30,capture=capture_fep,move=move_window,content_rect=None):
 require(not proofs,'heading gate is first username only');require(rect(content_rect),'measured guest content rectangle required')
 w=wait(device.fep_window,'initial empty username FEP');original=device.geometry(w);require(original==ui['fep_geometry'],'initial empty username FEP geometry changed');main=device.check();window=device.window;screen=device.geometry(device.root)
 review=[screen[0]+screen[2]-original[2],original[1],*original[2:]]
 require(review[0]>=main[0]+content_rect[0]+content_rect[2] and review[1]>=screen[1] and review[1]+review[3]<=screen[1]+screen[3],'screen has no clear public FEP review area')
 def same(geometry):
  require(device.check()==main and device.window==window and device.fep_window()==w and device.geometry(w)==geometry,'same-PID empty FEP/main changed while awaiting review');device.check_focus(w)
 same(original);device.click_window(w,original,*ui['edit_point']);key_window(w,'a','Control_L');key_window(w,'BackSpace');record('public-heading-empty-username-cleared')
 restored=False
 try:
  move(device,w,review);wait(lambda:device.geometry(w)==review,'empty FEP moved for heading review');same(review);record('public-heading-empty-fep-moved',fep_window=hex(w),original_geometry=original,review_geometry=review)
  time.sleep(1.0);same(review) # Let the Qt owner repaint the exposed native query before capture.
  root=Path(root);directory=root/'native-heading-review';directory.mkdir(mode=0o700,exist_ok=False);image=directory/'empty-native-query.png';capture(window,image)
  g={k:ready[k] for k in ('run','scheme','case')};g.update(schema=SCHEMA,pid=pid,window=hex(window),geometry=main,image=image.name,image_sha256=sha(image),expected_heading='HTTP sign in',expected_origin='http://'+urlsplit(ready['url']).netloc,private_username_input_started=False,empty_username=True,state='WAITING_ROOT_HEADING_REVIEW',fep_window=hex(w),fep_original_geometry=original,fep_review_geometry=review,content_rect=content_rect,screen_geometry=screen);gp=directory/'gate.json';gp.write_text(json.dumps(g,indent=2)+'\n');check_gate(gp);ap=directory/'approval.json'
  proof=dict(pid=pid,window=hex(window),geometry=main,gate=str(gp.relative_to(root)),gate_sha256=sha(gp),image=str(image.relative_to(root)),image_sha256=sha(image),approval=str(ap.relative_to(root)),approval_sha256=None,approved=False,fep_window=hex(w),fep_original_geometry=original,fep_review_geometry=review,empty_fep_kept_open=True,empty_fep_restored_before_private_input=False);proofs.append(proof);record('public-heading-captured',**{k:proof[k] for k in ('pid','window','geometry','gate','image_sha256')})
  print('HEADING_REVIEW '+str(gp)+' pid='+str(pid)+' image_sha256='+proof['image_sha256'],flush=True);deadline=time.monotonic()+timeout
  while not ap.exists():same(review);require(time.monotonic()<deadline,'public heading review timeout; no private username entered');time.sleep(.1)
  same(review);check_gate(gp);check_approval(ap,gp,g)
  move(device,w,original);wait(lambda:device.geometry(w)==original,'same empty FEP restored after heading review');same(original);restored=True
  device.click_window(w,original,*ui['edit_point']);key_window(w,'a','Control_L');key_window(w,'BackSpace')
  proof.update(approval_sha256=sha(ap),approved=True,empty_fep_restored_before_private_input=True);record('public-heading-approved-and-empty-fep-restored',gate_sha256=proof['gate_sha256'],approval_sha256=proof['approval_sha256'])
 finally:
  if not restored and device.fep_window()==w:
   move(device,w,original);wait(lambda:device.geometry(w)==original,'empty FEP restored after failed heading review')

def validate_heading(interaction,root):
 failures=[];incomplete=[];images=set();proofs=interaction.get('heading_reviews');records=interaction.get('records',[])
 if not isinstance(proofs,list):return ['native heading proof list absent'],[],images
 if len(proofs)!=1:incomplete.append('exactly one current empty native heading review required')
 for p in proofs:
  try:
   require(isinstance(p,dict) and set(p)==PROOF_KEYS,'heading proof shape changed');gp=relative_file(root,p['gate']);g,image=check_gate(gp);images.add(image.resolve());require(sha(gp)==p['gate_sha256'],'heading gate changed');require(all(g[k]==interaction.get(k) for k in ('run','scheme','case','pid')),'heading run/owner differs');require(all(p[k]==g[k] for k in ('pid','window','geometry','fep_window','fep_original_geometry','fep_review_geometry')),'heading proof owner differs');require(relative_file(root,p['image'])==image and p['image_sha256']==g['image_sha256'],'heading proof image differs')
   if p['approved']is not True or p['empty_fep_kept_open']is not True or p['empty_fep_restored_before_private_input']is not True:incomplete.append('native heading image not reviewed/empty FEP restored');continue
   ap=relative_file(root,p['approval']);require(ap.parent==gp.parent and ap.name=='approval.json','heading approval path differs');check_approval(ap,gp,g);require(sha(ap)==p['approval_sha256'],'heading approval changed')
   def positions(event):return [i for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')==event]
   cleared=positions('public-heading-empty-username-cleared');moved=positions('public-heading-empty-fep-moved');captured=positions('public-heading-captured');approved=positions('public-heading-approved-and-empty-fep-restored');started=positions('private-username-input-begins')+positions('empty-username-submission-after-heading');submitted=[i for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')=='editor-submitted' and r.get('role')=='username']
   require(all(len(x)==1 for x in (cleared,moved,captured,approved)) and started and submitted and cleared[0]<moved[0]<captured[0]<approved[0]<min(started)<min(submitted),'heading approval not before first username input');c=records[captured[0]];a=records[approved[0]];mv=records[moved[0]];require(all(c.get(k)==p[k] for k in ('pid','window','geometry','gate','image_sha256')) and all(a.get(k)==p[k] for k in ('gate_sha256','approval_sha256')) and mv.get('fep_window')==p['fep_window'] and mv.get('original_geometry')==p['fep_original_geometry'] and mv.get('review_geometry')==p['fep_review_geometry'],'heading records differ from proof')
  except (OSError,ValueError,TypeError,KeyError,RuntimeError):failures.append('invalid current public native heading evidence')
 return failures,incomplete,images
