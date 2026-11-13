/* Forced include for reference builds only (tools/refbuild.py), never for
   src/. Makes pre-2005 code compile with the modern headers:
   - <cmath> declares the float overloads of abs() that old code relies on;
   - client code: windows.h is included first with the SDK's HSPRITE handle
     renamed, so the HLSDK "typedef int HSPRITE" does not collide. */
#ifdef __cplusplus
#include <cmath>
#include <stdlib.h>
#endif
#ifdef CLIENT_DLL
#define HSPRITE WINDOWS_HSPRITE
#include <windows.h>
#undef HSPRITE
#endif
