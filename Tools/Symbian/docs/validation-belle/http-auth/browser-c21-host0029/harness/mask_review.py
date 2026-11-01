"""Only public password probes may be pictured, before private input."""
from pathlib import Path
import hashlib,json,os,re,struct,subprocess,time,uuid

SCHEMA='auth-public-password-mask-v1'
PROBE='maskcheck'  # Public fixed text; nine lowercase X11 keysyms.
GATE_KEYS={'schema','run','scheme','case','instance','pid','window','geometry','public_probe','image','image_sha256','credential_input_started','state'}
APPROVAL_KEYS={'schema','run','scheme','case','instance','pid','window','geometry','gate_sha256','image_sha256','observed_masked','reviewed_public_probe'}
PROOF_KEYS={'instance','pid','window','geometry','gate','gate_sha256','image','image_sha256','approval','approval_sha256','approved','cleared_before_private_input'}

def require(value,message):
    if not value:raise RuntimeError(message)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def expected_probes(case):
    return 3 if case=='retry' else 1 if case=='success' or case.endswith('password') else 0

def relative_file(root,value):
    require(isinstance(value,str) and value and not Path(value).is_absolute(),'probe path must be relative')
    require(all(p not in ('','..','.') for p in Path(value).parts),'invalid probe path')
    root=Path(root).resolve();path=root/value
    require(path.resolve().is_relative_to(root),'probe evidence escaped its run')
    current=path
    while current!=root:
        require(not current.is_symlink(),'symlinked probe evidence refused');current=current.parent
    require(path.is_file(),'probe evidence file missing')
    return path

def check_gate(path):
    path=Path(path);require(path.is_file() and not path.is_symlink(),'public probe gate missing')
    gate=json.loads(path.read_text());require(isinstance(gate,dict) and set(gate)==GATE_KEYS,'public probe gate shape changed')
    require(gate['schema']==SCHEMA and gate['public_probe']==PROBE,'unknown public probe contract')
    require(re.fullmatch(r'[A-Za-z0-9_-]{8,80}',gate['run']) is not None,'invalid probe run')
    require(gate['scheme'] in ('basic','digest'),'invalid probe scheme')
    require(gate['case'] in ('success','retry','cancel-password','navigate-password','exit-password'),'unexpected probe case')
    require(type(gate['instance']) is int and 1<=gate['instance']<=expected_probes(gate['case']),'invalid probe instance')
    require(type(gate['pid']) is int and gate['pid']>0 and re.fullmatch(r'0x[0-9a-f]+',gate['window']) is not None,'invalid probe owner')
    geometry=gate['geometry']
    require(isinstance(geometry,list) and len(geometry)==4 and all(type(n)is int for n in geometry) and 0<geometry[2]<=8192 and 0<geometry[3]<=8192,'invalid probe geometry')
    require(gate['credential_input_started'] is False and gate['state']=='WAITING_ROOT_MASK_REVIEW','probe not captured before credential input')
    require(gate['image']=='public-probe.png','only the public FEP probe image is accepted')
    image=relative_file(path.parent,gate['image']);require(sha(image)==gate['image_sha256'],'public probe image changed')
    data=image.read_bytes()
    require(24<=len(data)<=8*1024*1024 and data[:8]==b'\x89PNG\r\n\x1a\n' and data[12:16]==b'IHDR' and list(struct.unpack('>II',data[16:24]))==geometry[2:],'probe image dimensions differ from FEP')
    return gate,image

def check_approval(path,gate_path,gate):
    value=json.loads(Path(path).read_text());require(isinstance(value,dict) and set(value)==APPROVAL_KEYS,'probe approval shape changed')
    expected={k:gate[k] for k in ('schema','run','scheme','case','instance','pid','window','geometry')}
    expected.update(gate_sha256=sha(gate_path),image_sha256=gate['image_sha256'],observed_masked=True,reviewed_public_probe=PROBE)
    require(value==expected,'root review does not match this exact public FEP image')
    return value

def approve(gate_path,pid,image_sha256):
    gate_path=Path(gate_path);gate,_=check_gate(gate_path)
    require(pid==gate['pid'] and image_sha256==gate['image_sha256'],'root PID or reviewed image SHA differs')
    value={k:gate[k] for k in ('schema','run','scheme','case','instance','pid','window','geometry')}
    value.update(gate_sha256=sha(gate_path),image_sha256=image_sha256,observed_masked=True,reviewed_public_probe=PROBE)
    target=gate_path.parent/'approval.json';temporary=target.with_name('.approval-'+uuid.uuid4().hex+'.tmp')
    try:
        with os.fdopen(os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600),'w') as stream:json.dump(value,stream,indent=2);stream.write('\n')
        os.link(temporary,target)  # Atomic publication; old approvals are refused.
    finally:temporary.unlink(missing_ok=True)
    return gate['instance']

def capture_fep(window,path):
    result=subprocess.run(['import','-window',hex(window),str(path)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
    require(result.returncode==0,'public FEP probe capture failed')

def perform_probe(device,key_window,ui,ready,pid,evidence_root,proofs,record,timeout=30,capture=capture_fep):
    """No value parameter: only PROBE is eligible for a screenshot."""
    require(1<=timeout<=30,'public mask review must remain bounded')
    window=device.fep_window();require(window is not None,'password FEP absent before public probe')
    geometry=device.geometry(window);require(geometry==ui['fep_geometry'],'password FEP geometry changed')
    def still_same():
        device.check();require(device.fep_window()==window and device.geometry(window)==geometry,'password FEP changed while awaiting review');device.check_focus(window)
    def clear():
        still_same();device.click_window(window,geometry,*ui['edit_point']);key_window(window,'a','Control_L');key_window(window,'BackSpace')
    clear()  # Discard an unknown initial draft before any possible image.
    record('public-mask-probe-cleared-before-capture',role='password')
    for character in PROBE:key_window(window,character)
    still_same();root=Path(evidence_root);instance=len(proofs)+1
    directory=root/'password-mask-reviews'/('%02d'%instance);directory.mkdir(parents=True,exist_ok=False,mode=0o700)
    image=directory/'public-probe.png';capture(window,image)  # FEP only, no main/credential image.
    gate={k:ready[k] for k in ('run','scheme','case')}
    gate.update(schema=SCHEMA,instance=instance,pid=pid,window=hex(window),geometry=geometry,public_probe=PROBE,image=image.name,image_sha256=sha(image),credential_input_started=False,state='WAITING_ROOT_MASK_REVIEW')
    gate_path=directory/'gate.json';gate_path.write_text(json.dumps(gate,indent=2)+'\n');check_gate(gate_path)
    approval_path=directory/'approval.json'
    proof=dict(instance=instance,pid=pid,window=hex(window),geometry=geometry,gate=str(gate_path.relative_to(root)),gate_sha256=sha(gate_path),image=str(image.relative_to(root)),image_sha256=sha(image),approval=str(approval_path.relative_to(root)),approval_sha256=None,approved=False,cleared_before_private_input=False)
    proofs.append(proof);record('public-mask-probe-captured',instance=instance,gate=proof['gate'],image_sha256=proof['image_sha256'],pid=pid,window=hex(window),geometry=geometry)
    print('MASK_REVIEW '+str(gate_path)+' pid='+str(pid)+' image_sha256='+proof['image_sha256'],flush=True)
    deadline=time.monotonic()+timeout
    while not approval_path.exists():
        still_same();require(time.monotonic()<deadline,'public mask review timeout; no credential entered in this FEP');time.sleep(.1)
    still_same();check_gate(gate_path);check_approval(approval_path,gate_path,gate)
    clear()  # Root reviewed this image; discard probe before private input.
    proof.update(approval_sha256=sha(approval_path),approved=True,cleared_before_private_input=True)
    record('public-mask-probe-approved-and-cleared',instance=instance,gate_sha256=proof['gate_sha256'],approval_sha256=proof['approval_sha256'])

def validate_probe_evidence(interaction,evidence_root,*,allowed_public_images=None):
    failures=[];incomplete=[];expected=expected_probes(interaction.get('case',''));proofs=interaction.get('password_mask_reviews')
    if interaction.get('credential_stage_screenshots') is not False:failures.append('credential screenshot prohibition not explicit')
    if interaction.get('public_probe_only_screenshots') is not True:failures.append('public-only probe policy absent')
    if not isinstance(proofs,list):return {'status':'FAIL','failures':failures+['probe list absent'],'incomplete':[],'expected':expected,'approved':0}
    if interaction.get('screenshots_taken') is not bool(proofs or interaction.get('heading_reviews')):failures.append('actual screenshot flag differs from probe evidence')
    if len(proofs)!=expected:incomplete.append('expected %d per-instance password probe reviews, found %d'%(expected,len(proofs)))
    records=interaction.get('records',[])
    event_positions={}
    if not isinstance(records,list):failures.append('native input records invalid');records=[]
    for i,record in enumerate(records):
        if isinstance(record,dict):event_positions.setdefault(record.get('event'),[]).append(i)
    captured=event_positions.get('public-mask-probe-captured',[])
    cleared=event_positions.get('public-mask-probe-approved-and-cleared',[])
    started=sorted(event_positions.get('private-password-input-begins',[])+event_positions.get('empty-password-submission-after-probe',[]))
    submitted=[i for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')=='editor-submitted' and r.get('role')=='password']
    if all(len(x)==expected for x in (captured,cleared,started,submitted)):
        if any(not(captured[i]<cleared[i]<started[i]<submitted[i]) for i in range(expected)):
            failures.append('password input occurred before its probe review and clearing')
    elif len(proofs)==expected and all(p.get('approved') is True for p in proofs if isinstance(p,dict)):
        failures.append('per-instance probe/input ordering records incomplete')
    allowed_images=set(allowed_public_images or ())
    approved=0
    for number,proof in enumerate(proofs,1):
        try:
            require(isinstance(proof,dict) and set(proof)==PROOF_KEYS,'password proof shape changed');require(proof['instance']==number,'probe instances not sequential')
            gate_path=relative_file(evidence_root,proof['gate']);gate,image=check_gate(gate_path);require(sha(gate_path)==proof['gate_sha256'],'probe gate changed')
            allowed_images.add(image.resolve())
            require(all(gate[k]==interaction.get(k) for k in ('run','scheme','case','pid')),'probe identity differs from native input')
            require(all(proof[k]==gate[k] for k in ('instance','pid','window','geometry')),'probe owner/geometry differs')
            require(relative_file(evidence_root,proof['image'])==image and proof['image_sha256']==gate['image_sha256'],'probe image proof differs')
            if all(len(x)==expected for x in (captured,cleared,started,submitted)):
                c=records[captured[number-1]];a=records[cleared[number-1]];b=records[started[number-1]]
                require(all(c.get(k)==proof[k] for k in ('instance','pid','window','geometry','image_sha256','gate')),'capture record belongs to another probe')
                require(a.get('instance')==number and a.get('gate_sha256')==proof['gate_sha256'] and a.get('approval_sha256')==proof['approval_sha256'] and b.get('instance')==number,'clearing/input record belongs to another probe')
            if proof['approved'] is not True or proof['cleared_before_private_input'] is not True:
                incomplete.append('password probe instance %d not reviewed and cleared'%number);continue
            approval_path=relative_file(evidence_root,proof['approval']);require(approval_path.parent==gate_path.parent and approval_path.name=='approval.json','approval outside probe instance')
            check_approval(approval_path,gate_path,gate);require(sha(approval_path)==proof['approval_sha256'],'password approval changed');approved+=1
        except (OSError,ValueError,TypeError,KeyError,RuntimeError):failures.append('invalid public probe evidence for instance %d'%number)
    image_types={'.png','.jpg','.jpeg','.bmp','.gif','.webp'}
    unexpected=[p for p in Path(evidence_root).rglob('*') if p.is_file() and p.suffix.lower() in image_types and p.resolve() not in allowed_images]
    if unexpected:failures.append('image outside the validated public heading/password probe set present in run evidence')
    return {'status':'FAIL' if failures else 'INCOMPLETE' if incomplete else 'PASS','failures':failures,'incomplete':incomplete,'expected':expected,'approved':approved}
