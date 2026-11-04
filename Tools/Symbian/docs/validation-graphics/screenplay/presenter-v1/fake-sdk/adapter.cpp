#include "adapter_state.h"
TInt RSurfaceManager::Open(){return fake::result("manager-open");}
void RSurfaceManager::Close(){fake::event("manager-close");}
TInt RSurfaceManager::CreateSurface(const TSurfaceCreationAttributesBuf&p,TSurfaceId&id){
 TInt e=fake::result("create");if(e)return e;
 assert(p().iBuffers==2&&p().iAlignment==4&&p().iMappable&&!p().iContiguous);
 ++fake::create_count;fake::Resource r;
 r.info.iSize=p().iSize;r.info.iBuffers=p().iBuffers;r.info.iPixelFormat=p().iPixelFormat;r.info.iMappable=p().iMappable;
 r.info.iCacheAttrib=p().iCacheAttrib;r.info.iStride=p().iStride+16;
 TInt bytes=r.info.iStride*r.info.iSize.iHeight;r.offsets[0]=32;r.offsets[1]=bytes+48;
 r.bytes=std::make_shared<std::vector<TUint8>>(bytes*2+96,0x5a);
 id.iInternal[0]=fake::next_id++;fake::resources.emplace(static_cast<TInt>(id.iInternal[0]),std::move(r));return KErrNone;
}
TInt RSurfaceManager::SurfaceInfo(const TSurfaceId&id,TInfoBuf&out){
 TInt e=fake::result("info");if(e)return e;out()=fake::resource(id).info;
 if(fake::info_mode=="size")out().iSize.iWidth++;if(fake::info_mode=="buffers")out().iBuffers=1;
 if(fake::info_mode=="format")out().iPixelFormat=EUidPixelFormatXRGB_8888;if(fake::info_mode=="mappable")out().iMappable=false;
 if(fake::info_mode=="short-stride")out().iStride=4;if(fake::info_mode=="unaligned-stride")out().iStride++;
 if(fake::info_mode=="large-stride")out().iStride=KMaxTInt&~3;return KErrNone;
}
TInt RSurfaceManager::GetBufferOffset(const TSurfaceId&id,TInt b,TInt&out){
 TInt e=fake::result("offset");if(e)return e;assert(b>=0&&b<2);out=fake::resource(id).offsets[b];
 if(fake::offset_mode=="negative")out=-4;if(fake::offset_mode=="unaligned")++out;
 if(fake::offset_mode=="overlap")out=fake::resource(id).offsets[0];
 if(fake::offset_mode=="outside")out=static_cast<TInt>(fake::resource(id).bytes->size());return KErrNone;
}
TInt RSurfaceManager::MapSurface(const TSurfaceId&id,RChunk&chunk){
 TInt e=fake::result("map");if(e)return e;chunk.bytes=fake::resource(id).bytes;
 if(fake::map_mode=="empty")chunk.bytes=std::make_shared<std::vector<TUint8>>();return KErrNone;
}
TInt RSurfaceManager::SynchronizeCache(const TSurfaceId&id,TInt b,TSyncOperation op){
 fake::resource(id);assert(b>=0&&b<2&&op==ESyncBeforeNonCPURead);return fake::result("sync");
}
TInt RSurfaceManager::CloseSurface(const TSurfaceId&id){
 fake::event("close-surface");++fake::close_count;assert(fake::resources.erase(static_cast<TInt>(id.iInternal[0]))==1);return KErrNone;
}
TInt RWsSession::RegisterSurface(TInt screen,const TSurfaceId&id){assert(screen==0);TInt e=fake::result("register");if(!e)fake::resource(id).registered=true;return e;}
void RWsSession::UnregisterSurface(TInt screen,const TSurfaceId&id){assert(screen==0);fake::event("unregister");fake::resource(id).registered=false;}
TInt RWsSession::Finish(){return fake::once("finish");}
TInt RWindow::GetBackgroundSurface(TSurfaceConfiguration&c)const{
 TInt e=fake::result("get-bg");if(e)return e;if(current.IsNull())return KErrNotFound;c.id=current;return KErrNone;
}
TInt RWindow::SetBackgroundSurface(const TSurfaceConfiguration&c,TBool){TInt e=fake::result("set-bg");if(e)return e;current=c.id;return KErrNone;}
TInt RWindow::SetBackgroundSurface(const TSurfaceId&id){TSurfaceConfiguration c;c.id=id;return SetBackgroundSurface(c,false);}
void RWindow::RemoveBackgroundSurface(TBool){fake::event("remove-bg");current=TSurfaceId();}
TInt RSurfaceUpdateSession::Connect(TInt){TInt e=fake::result("updates-connect");if(!e)connected=true;return e;}
void RSurfaceUpdateSession::Close(){fake::event("updates-close");assert(!staged);for(const auto&p:fake::pending)assert(p.session!=this);connected=false;}
void RSurfaceUpdateSession::NotifyWhenAvailable(TRequestStatus&s){fake::event("notify");assert(connected&&s.Int()==KRequestPending&&!staged);staged=&s;}
TInt RSurfaceUpdateSession::SubmitUpdate(TInt screen,const TSurfaceId&id,TInt b,const TRegion*){
 assert(connected&&screen==KAllScreens&&staged&&fake::resource(id).registered);TInt e=fake::result("submit");
 if(e){*staged=e;staged=nullptr;return e;}++fake::submit_count;fake::pending.push_back({this,staged,b});staged=nullptr;return KErrNone;
}
void RSurfaceUpdateSession::CancelAllUpdateNotifications(){
 fake::event("cancel-all");if(staged){*staged=KErrCancel;staged=nullptr;}
 for(auto it=fake::pending.begin();it!=fake::pending.end();){if(it->session!=this){++it;continue;}assert(it->status->Int()==KRequestPending);*it->status=KErrCancel;it=fake::pending.erase(it);}
}
