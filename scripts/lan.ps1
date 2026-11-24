# Makes UniConnect reachable from other laptops on the same Wi-Fi or LAN.
# Run it ONCE in a PowerShell window opened with "Run as administrator".
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Host "Please run this in a PowerShell window opened with 'Run as administrator'." -ForegroundColor Yellow; exit 1 }

# Allow incoming connections to the web page (port 5173) on private and domain networks.
if (-not (Get-NetFirewallRule -DisplayName "UniConnect web (5173)" -ErrorAction SilentlyContinue)) {
    New-NetFirewallRule -DisplayName "UniConnect web (5173)" -Direction Inbound -Protocol TCP -LocalPort 5173 -Action Allow -Profile Private,Domain | Out-Null
    Write-Host "Firewall rule added."
} else { Write-Host "Firewall rule already exists." }

# Windows blocks incoming connections on networks marked "Public".
$public = Get-NetConnectionProfile | Where-Object { $_.NetworkCategory -eq "Public" }
foreach ($n in $public) {
    Write-Host "Your network '$($n.Name)' is marked Public, so other laptops cannot connect." -ForegroundColor Yellow
    Write-Host "To mark it Private (only do this on a network you trust):  Set-NetConnectionProfile -Name '$($n.Name)' -NetworkCategory Private"
}

Write-Host ""
Write-Host "Other laptops on the same network can open one of these addresses:" -ForegroundColor Green
Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notlike "169.254.*" -and $_.IPAddress -ne "127.0.0.1" -and $_.InterfaceAlias -notmatch "vEthernet|Loopback|VirtualBox|VMware|Bluetooth" } |
    ForEach-Object { Write-Host ("  http://{0}:5173    ({1})" -f $_.IPAddress, $_.InterfaceAlias) }
Write-Host ""
Write-Host "Keep this laptop's backend and frontend running. Each person signs in with their own account."
