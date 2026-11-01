'use strict';
(async () => {
 const run = new URL(location.href).searchParams.get('run');
 if (!/^[A-Za-z0-9_-]{8,64}$/.test(run || '')) throw Error('fresh run required');
 const prefix='quota-diagnostic-'+run+'-', MiB=1024*1024, block=new Uint8Array(MiB); block[0]=17;block[MiB-1]=239;
 const cases=[], opened=[];
 const trace=(kind,value)=>console.log('IDB_DIAG_EVENT '+JSON.stringify({run,kind,value}));
 const deadline=setTimeout(()=>console.log('IDB_DIAG_TIMEOUT run='+run),25000);
 const req=r=>new Promise((resolve,reject)=>{r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
 async function open(name){const r=indexedDB.open(prefix+name,1);r.onupgradeneeded=()=>r.result.createObjectStore('data');const db=await req(r);opened.push(db);return db;}
 async function remove(name){for(const db of opened)if(db.name===prefix+name)db.close();await req(indexedDB.deleteDatabase(prefix+name));}
 function put(db,key,value,label){return new Promise(resolve=>{const tx=db.transaction('data','readwrite'),r=tx.objectStore('data').put(value,key);let requestError='';r.onerror=()=>{requestError=r.error.name;trace(label+' request-error',{name:r.error.name,message:r.error.message});};r.onsuccess=()=>trace(label+' request-success',true);tx.onerror=()=>{};tx.onabort=()=>resolve({state:'abort',requestError,txError:tx.error&&tx.error.name,txMessage:tx.error&&tx.error.message});tx.oncomplete=()=>resolve({state:'complete',requestError,txError:tx.error&&tx.error.name});});}
 async function get(db,key){return req(db.transaction('data').objectStore('data').get(key));}
 const text=blob=>new Promise((resolve,reject)=>{const r=new FileReader;r.onload=()=>resolve(r.result);r.onerror=()=>reject(r.error);r.readAsText(blob);});
 async function test(name,fn){try{cases.push({name,result:await fn()});}catch(e){cases.push({name,error:String(e),stack:e.stack});}trace('case',cases[cases.length-1]);}
 await test('34MiB Blob admission request and transaction errors',async()=>{const db=await open('blob');const seed=await put(db,'key',new Blob(['old-blob']),'seed');const source=new Blob(new Array(34).fill(block));const result=await put(db,'key',source,'oversized');const stored=await get(db,'key');const storedText=stored&&stored.size<100?await text(stored):null;const sourceFirst=await text(source.slice(0,1));const out={seed,result,sourceSize:source.size,sourceFirst:sourceFirst.charCodeAt(0),storedSize:stored&&stored.size,storedText,originalCondition:result.state==='abort'&&result.txError==='QuotaExceededError'};await remove('blob');return out;});
 await test('parallel same-origin pending reservations',async()=>{const dbs=await Promise.all([open('pending-a'),open('pending-b')]),txs=dbs.map(db=>db.transaction('data','readwrite'));const added=[0,0],errors=[],outcomes=[];let stopped=false,quotaError='',unexpectedCommit=false;
 const done=txs.map((tx,i)=>new Promise(resolve=>{tx.onerror=()=>{};tx.onabort=()=>{outcomes[i]={state:'abort',error:tx.error&&tx.error.name};resolve();};tx.oncomplete=()=>{unexpectedCommit=true;outcomes[i]={state:'complete',error:tx.error&&tx.error.name};resolve();};}));
 function stop(){if(stopped)return;stopped=true;for(const tx of txs)try{tx.abort();}catch(e){trace('abort-call-error',String(e));}}
 function step(i){if(stopped)return;const store=txs[i].objectStore('data'),r=added[i]<20?store.put(block,added[i]):store.get(0);r.onerror=event=>{event.preventDefault();quotaError=r.error.name;errors.push({index:i,name:r.error.name,message:r.error.message,added:added.slice()});trace('pending-request-error',errors[errors.length-1]);stop();};r.onsuccess=()=>{if(added[i]<20)++added[i];if(added[0]===20&&added[1]===20)stop();else step(i);};}
 step(0);step(1);await Promise.all(done);const counts=await Promise.all(dbs.map(db=>req(db.transaction('data').objectStore('data').count())));const originalCondition=!unexpectedCommit&&quotaError==='QuotaExceededError'&&added[0]>0&&added[1]>0&&added[0]+added[1]>=24&&added[0]+added[1]<32;await remove('pending-a');await remove('pending-b');return{added,errors,outcomes,counts,quotaError,unexpectedCommit,originalCondition};});
 clearTimeout(deadline);for(const db of opened)db.close();console.log('IDB_DIAG_RESULT '+JSON.stringify({run,origin:location.origin,cases}));console.log('IDB_DIAG_DONE run='+run+' cases='+cases.length);
})().catch(e=>console.log('IDB_DIAG_FATAL '+String(e)+'\n'+e.stack));
