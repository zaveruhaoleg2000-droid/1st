# GRAY ZONE: 1989 - Vulkan driver probe (Windows 11, no SDK required).
# Only calls Vulkan Loader entry points. Does not install/change drivers.
$ErrorActionPreference = 'Stop'
$report = Join-Path $PSScriptRoot 'gpu_probe.txt'
$lines = New-Object 'System.Collections.Generic.List[string]'
function Say([string]$message) {
    [void]$lines.Add($message)
    Write-Host $message
}
Say 'GRAY ZONE: 1989 - Windows Vulkan probe v0.0.22'
Say 'This checks the Vulkan Loader and GPU driver; it does not render a frame.'
try {
    Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class GZVulkanProbe {
    [StructLayout(LayoutKind.Sequential)]
    public struct AppInfo {
        public uint sType;
        public IntPtr pNext, pApplicationName;
        public uint applicationVersion;
        public IntPtr pEngineName;
        public uint engineVersion, apiVersion;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct InstanceInfo {
        public uint sType;
        public IntPtr pNext;
        public uint flags;
        public IntPtr pApplicationInfo;
        public uint enabledLayerCount;
        public IntPtr ppEnabledLayerNames;
        public uint enabledExtensionCount;
        public IntPtr ppEnabledExtensionNames;
    }
    [DllImport("vulkan-1.dll", CallingConvention=CallingConvention.Winapi)]
    public static extern int vkEnumerateInstanceVersion(out uint version);
    [DllImport("vulkan-1.dll", CallingConvention=CallingConvention.Winapi)]
    public static extern int vkCreateInstance(ref InstanceInfo info, IntPtr allocator, out IntPtr instance);
    [DllImport("vulkan-1.dll", CallingConvention=CallingConvention.Winapi)]
    public static extern int vkEnumeratePhysicalDevices(IntPtr instance, out uint count, IntPtr devices);
    [DllImport("vulkan-1.dll", CallingConvention=CallingConvention.Winapi)]
    public static extern void vkDestroyInstance(IntPtr instance, IntPtr allocator);
}
'@
    try {
        [uint32]$version = 0
        $versionResult = [GZVulkanProbe]::vkEnumerateInstanceVersion([ref]$version)
        if ($versionResult -eq 0) {
            Say ('Vulkan Loader version: {0}.{1}.{2}' -f ($version -shr 22), (($version -shr 12) -band 1023), ($version -band 4095))
        } else {
            Say "Vulkan Loader query error: $versionResult"
        }
    } catch [System.EntryPointNotFoundException] {
        Say 'Vulkan Loader version: 1.0 (version query not exported)'
    }
    $app = New-Object GZVulkanProbe+AppInfo
    $app.sType = 0
    $app.pApplicationName = [Runtime.InteropServices.Marshal]::StringToHGlobalAnsi('GZ Vulkan Probe')
    $app.applicationVersion = 1
    $app.pEngineName = [Runtime.InteropServices.Marshal]::StringToHGlobalAnsi('GZ89')
    $app.engineVersion = 1
    $app.apiVersion = 0x00400000
    $pApp = [Runtime.InteropServices.Marshal]::AllocHGlobal([Runtime.InteropServices.Marshal]::SizeOf([type][GZVulkanProbe+AppInfo]))
    $instance = [IntPtr]::Zero
    try {
        [Runtime.InteropServices.Marshal]::StructureToPtr($app,$pApp,$false)
        $info = New-Object GZVulkanProbe+InstanceInfo
        $info.sType = 1
        $info.pApplicationInfo = $pApp
        $result = [GZVulkanProbe]::vkCreateInstance([ref]$info,[IntPtr]::Zero,[ref]$instance)
        Say "vkCreateInstance result: $result"
        if ($result -eq 0 -and $instance -ne [IntPtr]::Zero) {
            [uint32]$count = 0
            $enumeration = [GZVulkanProbe]::vkEnumeratePhysicalDevices($instance,[ref]$count,[IntPtr]::Zero)
            Say "vkEnumeratePhysicalDevices result: $enumeration"
            Say "Vulkan physical device count: $count"
            if ($enumeration -eq 0 -and $count -gt 0) { Say 'RESULT: Vulkan instance and device enumeration work. GPU 3D still requires game testing.' }
            else { Say 'RESULT: Vulkan initialized but no GPU device was enumerated.' }
        } elseif ($result -eq -9) {
            Say 'RESULT: VK_ERROR_INCOMPATIBLE_DRIVER (-9). Update GPU driver or install Vulkan-capable hardware.'
        } else {
            Say 'RESULT: Vulkan Instance creation failed.'
        }
    } finally {
        if ($instance -ne [IntPtr]::Zero) { [GZVulkanProbe]::vkDestroyInstance($instance,[IntPtr]::Zero) }
        [Runtime.InteropServices.Marshal]::FreeHGlobal($pApp)
        [Runtime.InteropServices.Marshal]::FreeHGlobal($app.pApplicationName)
        [Runtime.InteropServices.Marshal]::FreeHGlobal($app.pEngineName)
    }
} catch [System.DllNotFoundException] {
    Say 'RESULT: vulkan-1.dll not found. Vulkan runtime / graphics driver is missing.'
} catch {
    Say ('RESULT: Vulkan probe failed: ' + $_.Exception.Message)
} finally {
    $lines | Set-Content -Encoding UTF8 $report
    Write-Host "Report saved: $report"
}
