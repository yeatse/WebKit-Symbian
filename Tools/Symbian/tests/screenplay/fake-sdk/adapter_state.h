#pragma once
#include "fake_symbian.h"
#include <map>
namespace fake {
struct Resource {
 RSurfaceManager::TSurfaceInfoV01 info;
 std::shared_ptr<std::vector<TUint8>> bytes;
 TInt offsets[2]{};bool registered=false;
};
struct Pending{RSurfaceUpdateSession*session;TRequestStatus*status;TInt buffer;};
inline std::map<TInt,Resource>resources;
inline std::vector<Pending>pending;
inline std::map<std::string,TInt>errors;
inline std::string info_mode,offset_mode,map_mode;
inline int create_count=0,close_count=0,submit_count=0;
inline TInt next_id=1;
inline TInt result(const char*op){event(op);auto it=errors.find(op);return it==errors.end()?KErrNone:it->second;}
inline TInt once(const char*op){TInt value=result(op);errors.erase(op);return value;}
inline Resource&resource(const TSurfaceId&id){auto it=resources.find(static_cast<TInt>(id.iInternal[0]));if(it==resources.end())throw std::runtime_error("unknown owned surface");return it->second;}
inline void Complete(TInt buffer,TInt code=KErrNone){for(auto it=pending.begin();it!=pending.end();++it){if(it->buffer==buffer){assert(it->status->Int()==KRequestPending);*it->status=code;pending.erase(it);return;}}throw std::runtime_error("no owned pending buffer");}
inline void Reset(){
 if(!resources.empty()||!pending.empty()||!CleanupStack::entries.empty())throw std::runtime_error("previous owned test not cleaned");
 errors.clear();events.clear();CActiveScheduler::items.clear();info_mode.clear();offset_mode.clear();map_mode.clear();wait_calls=0;
 create_count=close_count=submit_count=0;
 allocations_before_failure=-1;initial_status_pending=false;
}
}
