@echo off
rem Start the GhidraMCP headless server (bethington/ghidra-mcp, plugin jar in
rem the Ghidra Extensions folder) on 127.0.0.1:8089. The MCP bridge in
rem .mcp.json talks to it. Usage: tools\ghidra_mcp_server.bat [extra args]
rem The server holds the lock on re\CoF.gpr while it runs. Scripts are
rem enabled (localhost only) so "python tools\build.py target" can run
rem DelinkUnits.java through the server instead of headless Ghidra.
setlocal
if not defined GHIDRA_DIR set "GHIDRA_DIR=I:\Tools\ghidra_12.1.4_PUBLIC"
if not defined GHIDRA_MCP_MAXMEM set "GHIDRA_MCP_MAXMEM=8G"
set "GHIDRA_MCP_ALLOW_SCRIPTS=1"
set "VMARG_LIST=-XX:ParallelGCThreads=4 -Djava.awt.headless=true"
call "%GHIDRA_DIR%\support\launch.bat" fg jdk GhidraMCP-Headless "%GHIDRA_MCP_MAXMEM%" "%VMARG_LIST%" com.xebyte.headless.GhidraMCPHeadlessServer --bind 127.0.0.1 --port 8089 %*
