# Run dist/cof.exe for a fixed time with the console log on, then close it.
#   powershell -File tools/run_game.ps1 -Seconds 75 -Map c_start -Log <out.txt> [-Shots <dir>] [-Every 15]
# The log (qconsole.log, written by -condebug) is copied to -Log. With
# -Shots, a half size screenshot of the desktop is saved every -Every seconds.
param(
	[int]$Seconds = 60,
	[string]$Map = "c_start",
	[string]$Log = "",
	[string]$Extra = "",
	[string]$Shots = "",
	[int]$Every = 15
)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
function Save-Shot([string]$path) {
	$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
	$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
	$small = New-Object System.Drawing.Bitmap $bmp, ([int]($b.Width / 2)), ([int]($b.Height / 2))
	$small.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
	$g.Dispose(); $bmp.Dispose(); $small.Dispose()
}
$d = Join-Path (Split-Path -Parent $PSScriptRoot) "dist"
Remove-Item (Join-Path $d "qconsole.log") -ErrorAction SilentlyContinue
$cmd = "-game cryoffear -dev -condebug -window -w 1024 -h 768 +developer 2 +map $Map $Extra"
$p = Start-Process -FilePath (Join-Path $d "cof.exe") -ArgumentList $cmd -WorkingDirectory $d -PassThru
$t = 0
while ($t -lt $Seconds -and -not $p.HasExited) {
	$step = [Math]::Min($Every, $Seconds - $t)
	Start-Sleep -Seconds $step
	$t += $step
	if ($Shots -ne "") { Save-Shot (Join-Path $Shots ("shot_{0:D3}.png" -f $t)) }
}
$alive = -not $p.HasExited
$code = if ($p.HasExited) { $p.ExitCode } else { "-" }
# close the window first so the engine shuts down and flushes qconsole.log
if (-not $p.HasExited) { $null = $p.CloseMainWindow(); $null = $p.WaitForExit(15000) }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
if ($Log -ne "") { Copy-Item (Join-Path $d "qconsole.log") $Log }
"alive after $t s: $alive (exit code $code)"
