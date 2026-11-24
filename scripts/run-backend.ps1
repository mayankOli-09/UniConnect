# Starts MySQL (if it is not running) and then the UniConnect backend.
# Development convenience only: the passwords below are for your local machine. Change them for anything real.
param(
    [string]$MySqlRoot = "D:\mysql",
    [string]$MySqlData = "D:\mysql-data"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not (Get-Process mysqld -ErrorAction SilentlyContinue)) {
    Write-Host "Starting MySQL..."
    Start-Process -FilePath "$MySqlRoot\bin\mysqld.exe" -WindowStyle Minimized -ArgumentList "--console","--basedir=$MySqlRoot","--datadir=$MySqlData","--bind-address=127.0.0.1","--mysqlx=OFF"
    Start-Sleep -Seconds 8
}

# Database
$env:MYSQL_HOST     = "127.0.0.1"
$env:MYSQL_USER     = "uniconnect"
$env:MYSQL_PASSWORD = "UniConnect#Dev2026"
$env:MYSQL_DB       = "uniconnect"
# Login tokens: a fixed secret, so people stay signed in when the backend restarts
$env:JWT_SECRET     = "dev-only-secret-change-before-deploying-0123456789"
# The scheduler and thread pool (all optional; these are the defaults)
$env:WORKER_THREADS   = "4"        # 1 to 64
$env:SCHEDULER_POLICY = "FCFS"     # FCFS, SJF, ROUND_ROBIN or PRIORITY (can also be changed live on the System monitor page)
$env:TIME_QUANTUM_MS  = "25"       # for ROUND_ROBIN
$env:QUEUE_CAPACITY   = "1000"     # tasks allowed to wait for a worker

$exe = Join-Path $root "build-win\Release\uniconnect.exe"
if (-not (Test-Path $exe)) { throw "Not built yet: $exe. Run: cmake --build build-win --config Release" }
& $exe
