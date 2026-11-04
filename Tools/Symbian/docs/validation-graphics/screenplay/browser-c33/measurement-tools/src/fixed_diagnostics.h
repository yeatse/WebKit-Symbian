/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <cstdio>
struct FixedDiagnostics {
 const char*phase="ARGS";
 unsigned proc_identity_checks=0,proc_identity_passes=0;
 bool xres_extension_present=false,xres_version_supported=false;
 unsigned query_client_ids_calls=0,query_client_ids_successes=0,query_client_ids_values=0,query_client_ids_owner_matches=0,windows_examined=0,declared_pid_matches=0,wm_delete_candidate_count=0,render_marker_count=0,marker_all_nodes=0,marker_nonleaf_nodes=0;
};
static FixedDiagnostics diagnostics;
static void increment(unsigned&v,unsigned n=1){v=n>=65536-v?65536:v+n;}
static void print_diagnostics(){
 std::printf("\"diagnostics\":{\"phase\":\"%s\",\"proc_identity_checks\":%u,\"proc_identity_passes\":%u,\"xres_extension_present\":%s,\"xres_version_supported\":%s,\"query_client_ids_calls\":%u,\"query_client_ids_successes\":%u,\"query_client_ids_values\":%u,\"query_client_ids_owner_matches\":%u,\"windows_examined\":%u,\"declared_pid_matches\":%u,\"wm_delete_candidate_count\":%u,\"render_marker_count\":%u,\"marker_all_nodes\":%u,\"marker_nonleaf_nodes\":%u}",diagnostics.phase,diagnostics.proc_identity_checks,diagnostics.proc_identity_passes,diagnostics.xres_extension_present?"true":"false",diagnostics.xres_version_supported?"true":"false",diagnostics.query_client_ids_calls,diagnostics.query_client_ids_successes,diagnostics.query_client_ids_values,diagnostics.query_client_ids_owner_matches,diagnostics.windows_examined,diagnostics.declared_pid_matches,diagnostics.wm_delete_candidate_count,diagnostics.render_marker_count,diagnostics.marker_all_nodes,diagnostics.marker_nonleaf_nodes);
}
static int rejected(const char*status,const char*error,bool wm=false){
 if(wm)std::printf("{\"wm_close_sent\":false,\"error_code\":\"%s\",",error);
 else std::printf("{\"status\":\"%s\",\"error_code\":\"%s\",",status,error);
 print_diagnostics();std::puts("}");return 2;
}
