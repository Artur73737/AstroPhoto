param([string]$Panel='Android pairing', [string]$Output='out/remote-pairing-test.png')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AstroPanelCapture {
    public delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect bounds);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
}
'@
$null=[AstroPanelCapture]::SetProcessDPIAware()
$processIds=@(Get-Process AstroCanonController | Select-Object -ExpandProperty Id)
$script:targetWindow=[IntPtr]::Zero
$callback=[AstroPanelCapture+WindowCallback]{param($window,$data)
    [uint32]$owner=0;$null=[AstroPanelCapture]::GetWindowThreadProcessId($window,[ref]$owner)
    if($owner -in $processIds -and [AstroPanelCapture]::IsWindowVisible($window)){
        $title=[Text.StringBuilder]::new(256);$null=[AstroPanelCapture]::GetWindowText($window,$title,256)
        if($title.ToString().StartsWith($Panel)){$script:targetWindow=$window;return $false}
    }
    return $true
}
$null=[AstroPanelCapture]::EnumWindows($callback,[IntPtr]::Zero)
if($script:targetWindow -eq [IntPtr]::Zero){throw "Panel not found: $Panel"}
$bounds=[AstroPanelCapture+Rect]::new();$null=[AstroPanelCapture]::GetWindowRect($script:targetWindow,[ref]$bounds)
$null=[AstroPanelCapture]::SetWindowPos($script:targetWindow,[IntPtr](-1),0,0,0,0,0x13)
Start-Sleep -Milliseconds 200
$bitmap=[Drawing.Bitmap]::new($bounds.right-$bounds.left,$bounds.bottom-$bounds.top)
$graphics=[Drawing.Graphics]::FromImage($bitmap)
try{$graphics.CopyFromScreen($bounds.left,$bounds.top,0,0,$bitmap.Size);$bitmap.Save([IO.Path]::GetFullPath($Output),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose();$null=[AstroPanelCapture]::SetWindowPos($script:targetWindow,[IntPtr](-2),0,0,0,0,0x13)}
Write-Output "Captured requested application panel to $Output"
