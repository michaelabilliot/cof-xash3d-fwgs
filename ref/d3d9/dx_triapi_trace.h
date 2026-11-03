// Opt-in, bounded crash evidence. Never repairs or dereferences damaged storage.
#pragma once
struct dx_tri_trace_entry_t {
    DWORD tick,thread; unsigned serial; const char* event;
    int mode,quad; bool active,screen;
    uintptr_t world[3],ui[3],expanded[3];
};
static dx_tri_trace_entry_t g_triTrace[256]{};
static unsigned g_triTraceSerial=0;
static SRWLOCK g_triTraceLock=SRWLOCK_INIT;
template<class T> static void R_TriTraceVector(const std::vector<T>& vector,uintptr_t (&words)[3]) {
    // This renderer uses MSVC's release three-pointer vector ABI. Copying the
    // representation avoids subtracting corrupt pointers just to diagnose them.
    static_assert(sizeof(vector)==sizeof(words),"TriAPI diagnostic requires release vector ABI");
    memcpy(words,&vector,sizeof(words));
}
static bool R_TriTraceVectorValid(const uintptr_t (&p)[3],size_t stride) {
    if(!p[0])return !p[1]&&!p[2];
    return p[0]<=p[1]&&p[1]<=p[2]&&(p[1]-p[0])%stride==0&&(p[2]-p[0])%stride==0;
}
static void R_TriTraceWriteLocked(const char* reason) {
    const HANDLE file=CreateFileA("cof_triapi_trace.log",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    char text[512];DWORD written;
    int length=snprintf(text,sizeof(text),"TriAPI diagnostic: %s; last 256 lifecycle records; tick/thread correlate with cof_inventory.log\r\n",reason);
    WriteFile(file,text,static_cast<DWORD>(length),&written,nullptr);
    const unsigned count=std::min(g_triTraceSerial,256u),first=g_triTraceSerial-count;
    for(unsigned i=first;i<g_triTraceSerial;++i) {
        const auto& e=g_triTrace[i%256];
        length=snprintf(text,sizeof(text),"%u tick=%lu thread=%lu %s active=%d screen=%d mode=%d quad=%d world=%p/%p/%p ui=%p/%p/%p expanded=%p/%p/%p\r\n",
            e.serial,e.tick,e.thread,e.event,e.active,e.screen,e.mode,e.quad,
            reinterpret_cast<void*>(e.world[0]),reinterpret_cast<void*>(e.world[1]),reinterpret_cast<void*>(e.world[2]),
            reinterpret_cast<void*>(e.ui[0]),reinterpret_cast<void*>(e.ui[1]),reinterpret_cast<void*>(e.ui[2]),
            reinterpret_cast<void*>(e.expanded[0]),reinterpret_cast<void*>(e.expanded[1]),reinterpret_cast<void*>(e.expanded[2]));
        WriteFile(file,text,static_cast<DWORD>(length),&written,nullptr);
    }
    FlushFileBuffers(file);CloseHandle(file);
}
static void R_TriTraceFlush(const char* reason) {
    if(dx_triapi_debug.value==0.f)return;
    AcquireSRWLockExclusive(&g_triTraceLock);R_TriTraceWriteLocked(reason);ReleaseSRWLockExclusive(&g_triTraceLock);
}
static void R_TriTrace(const char* event,bool force=false) {
    if(!force&&dx_triapi_debug.value==0.f)return;
    dx_tri_trace_entry_t e{};e.tick=GetTickCount();e.thread=GetCurrentThreadId();e.event=event;
    e.mode=g_triApi.primitiveMode;e.quad=g_triApi.screenQuadCount;e.active=g_triApi.active;e.screen=g_triApi.screenSpace;
    R_TriTraceVector(g_triApi.vertices,e.world);R_TriTraceVector(g_triApi.screenVertices,e.ui);R_TriTraceVector(g_triApi.expandedScreenVertices,e.expanded);
    const bool invalid=e.quad<0||e.quad>4||!R_TriTraceVectorValid(e.world,sizeof(brush_vertex_t))||
        !R_TriTraceVectorValid(e.ui,sizeof(vertex2d_t))||!R_TriTraceVectorValid(e.expanded,sizeof(vertex2d_t));
    AcquireSRWLockExclusive(&g_triTraceLock);e.serial=g_triTraceSerial;g_triTrace[g_triTraceSerial++%256]=e;
    if(force||invalid)R_TriTraceWriteLocked(force?event:"FIRST INVALID VECTOR/QUAD STATE");
    ReleaseSRWLockExclusive(&g_triTraceLock);
    if(invalid||force) {
        // Preserve the first bad state for the engine's existing crash handler.
        // Continuing or clearing a corrupted vector would hide the writer.
        RaiseException(EXCEPTION_ARRAY_BOUNDS_EXCEEDED,EXCEPTION_NONCONTINUABLE,0,nullptr);
    }
}
