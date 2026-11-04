#!/usr/bin/env python3
"""Map only successful exact-host exec candidates captured from the owned tracer.

No task/children traversal, global PID scan, cmdline or environ access. All public
 diagnostics are fixed counts. The original strict process-group rule is retained.
"""
from pathlib import Path

class OwnershipError(RuntimeError):pass
ERROR_CODES=frozenset(('OWNED_ANCESTRY_BOUND', 'OWNED_ANCESTRY_START_CHANGED', 'OWNED_ANCESTRY_UNPROVEN', 'OWNED_EXECUTABLE_CHANGED', 'OWNED_EXEC_CANDIDATE_BOUND', 'OWNED_EXEC_CANDIDATE_GONE', 'OWNED_EXEC_CANDIDATE_INVALID', 'OWNED_EXEC_CANDIDATE_UNREADABLE', 'OWNED_INVALID_PROCESS_ID', 'OWNED_NAMESPACE_CHAIN_BROKEN', 'OWNED_NAMESPACE_CHANGED', 'OWNED_NAMESPACE_ID_INVALID', 'OWNED_NAMESPACE_ID_MISSING', 'OWNED_NAMESPACE_INIT_UNPROVEN', 'OWNED_NAMESPACE_NOT_DISTINCT', 'OWNED_NAMESPACE_PID_CHANGED', 'OWNED_PID_MAPPING_NOT_UNIQUE', 'OWNED_PROCESS_GROUP_CHANGED', 'OWNED_PROCESS_IDENTITY_CHANGED', 'OWNED_PROCESS_PREDATES_ROOT', 'OWNED_PROCESS_SNAPSHOT_FAILED', 'OWNED_PROCESS_STAT_INVALID', 'OWNED_ROOT_IDENTITY_CHANGED'))

COUNT_KEYS=('known_exec_candidates','pgrp_matches','start_matches','namespace_matches','nspid_matches','exe_matches','namespace_init_chain_matches','fully_verified_matches')

def stat(pid):
 pid=int(pid)
 if pid<=0:raise OwnershipError('OWNED_INVALID_PROCESS_ID')
 raw=Path('/proc/'+str(pid)+'/stat').read_text();tail=raw.rsplit(')',1)[1].split()
 if len(tail)<20:raise OwnershipError('OWNED_PROCESS_STAT_INVALID')
 return {'pid':pid,'ppid':int(tail[1]),'pgid':int(tail[2]),'starttime':int(tail[19])}

def _snapshot(pid,with_executable=False):
 row=stat(pid);base=Path('/proc')/str(pid)
 row['namespace_inode']=(base/'ns/pid').stat().st_ino
 for line in(base/'status').read_text().splitlines():
  if line.startswith('NSpid:'):
   ids=[int(value)for value in line.split(':',1)[1].split()]
   if not ids or len(ids)>16 or ids[0]!=pid or any(value<=0 for value in ids):raise OwnershipError('OWNED_NAMESPACE_ID_INVALID')
   row['namespace_pid']=ids[-1];break
 else:raise OwnershipError('OWNED_NAMESPACE_ID_MISSING')
 if with_executable:row['executable']=(base/'exe').resolve(strict=True)
 return row

def _chain(leaf,root_pid,root_starttime,owned_pgid):
 chain=[leaf];seen={leaf['pid']}
 for _ in range(63):
  if chain[-1]['pid']==root_pid:return chain
  parent_pid=chain[-1]['ppid']
  if parent_pid<=0 or parent_pid in seen:raise OwnershipError('OWNED_ANCESTRY_UNPROVEN')
  parent=_snapshot(parent_pid)
  if parent['pgid']!=owned_pgid:raise OwnershipError('OWNED_PROCESS_GROUP_CHANGED')
  if parent['starttime']<root_starttime or chain[-1]['starttime']<parent['starttime']:raise OwnershipError('OWNED_ANCESTRY_START_CHANGED')
  seen.add(parent_pid);chain.append(parent)
 if chain[-1]['pid']==root_pid:return chain
 raise OwnershipError('OWNED_ANCESTRY_BOUND')

def _prove(chain,root_pid,root_starttime,owned_pgid,pidns_inode,counts):
 anchor=chain[-1]
 if anchor['pid']!=root_pid or anchor['starttime']!=root_starttime or anchor['pgid']!=owned_pgid:raise OwnershipError('OWNED_ROOT_IDENTITY_CHANGED')
 if anchor['namespace_inode']==pidns_inode:raise OwnershipError('OWNED_NAMESPACE_NOT_DISTINCT')
 init=[index for index,row in enumerate(chain)if row['namespace_inode']==pidns_inode and row['namespace_pid']==1]
 if len(init)!=1:raise OwnershipError('OWNED_NAMESPACE_INIT_UNPROVEN')
 boundary=init[0]
 if any(row['namespace_inode']!=pidns_inode for row in chain[:boundary+1])or any(row['namespace_inode']==pidns_inode for row in chain[boundary+1:]):raise OwnershipError('OWNED_NAMESPACE_CHAIN_BROKEN')
 counts['namespace_init_chain_matches']+=1
 for row in chain:
  if _snapshot(row['pid'],'executable'in row)!=row:raise OwnershipError('OWNED_PROCESS_IDENTITY_CHANGED')
 return True

def map_owned_namespace_pid(root_pid,root_starttime,owned_pgid,pidns_inode,namespace_pid,expected_executable,*,diagnostics=None,candidate_pids=()):
 counts=dict.fromkeys(COUNT_KEYS,0)
 if diagnostics is not None:diagnostics.clear();diagnostics.update(counts)
 root_pid,root_starttime,owned_pgid,pidns_inode,namespace_pid=map(int,(root_pid,root_starttime,owned_pgid,pidns_inode,namespace_pid))
 if not isinstance(candidate_pids,(tuple,list,set,frozenset))or len(candidate_pids)>32:raise OwnershipError('OWNED_EXEC_CANDIDATE_BOUND')
 if any(type(pid)is not int or pid<=0 for pid in candidate_pids):raise OwnershipError('OWNED_EXEC_CANDIDATE_INVALID')
 known=set(candidate_pids);counts['known_exec_candidates']=len(known)
 try:
  root=stat(root_pid)
  if root['starttime']!=root_starttime or root['pgid']!=owned_pgid:raise OwnershipError('OWNED_ROOT_IDENTITY_CHANGED')
  expected_executable=Path(expected_executable).resolve(strict=True);matches=[];last_error=None
  for pid in known:
   try:
    row=_snapshot(pid,True)
    if row['pgid']!=owned_pgid:raise OwnershipError('OWNED_PROCESS_GROUP_CHANGED')
    counts['pgrp_matches']+=1
    if row['starttime']<root_starttime:raise OwnershipError('OWNED_PROCESS_PREDATES_ROOT')
    counts['start_matches']+=1
    if row['namespace_inode']!=pidns_inode:raise OwnershipError('OWNED_NAMESPACE_CHANGED')
    counts['namespace_matches']+=1
    if row['namespace_pid']!=namespace_pid:raise OwnershipError('OWNED_NAMESPACE_PID_CHANGED')
    counts['nspid_matches']+=1
    if row['executable']!=expected_executable:raise OwnershipError('OWNED_EXECUTABLE_CHANGED')
    counts['exe_matches']+=1
    chain=_chain(row,root_pid,root_starttime,owned_pgid)
    _prove(chain,root_pid,root_starttime,owned_pgid,pidns_inode,counts)
    counts['fully_verified_matches']+=1
    matches.append({'pid':row['pid'],'ppid':row['ppid'],'pgid':row['pgid'],'starttime':row['starttime'],'namespace_pid':namespace_pid,'pidns_inode':pidns_inode})
   except FileNotFoundError:last_error=OwnershipError('OWNED_EXEC_CANDIDATE_GONE')
   except PermissionError:last_error=OwnershipError('OWNED_EXEC_CANDIDATE_UNREADABLE')
   except OSError:last_error=OwnershipError('OWNED_PROCESS_SNAPSHOT_FAILED')
   except OwnershipError as error:last_error=error
  current=stat(root_pid)
  if current['starttime']!=root_starttime or current['pgid']!=owned_pgid:raise OwnershipError('OWNED_ROOT_IDENTITY_CHANGED')
  if len(matches)!=1:
   if not matches and last_error is not None:raise last_error
   raise OwnershipError('OWNED_PID_MAPPING_NOT_UNIQUE')
  return matches[0]
 finally:
  if diagnostics is not None:diagnostics.clear();diagnostics.update(counts)
