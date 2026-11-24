# One command to start everything: MySQL, the backend and the frontend, then open the browser.
# Usage (from the project folder):  powershell -ExecutionPolicy Bypass -File .\scripts\start-all.ps1
param(
    [string]$MySqlRoot = "D:\mysql",
    [string]$MySqlData = "D:\mysql-data"
)
$root = Split-Path -Parent $PSScriptRoot

Write-Host "Starting the backend (and MySQL) in a new window..."
Start-Process powershell -ArgumentList "-NoExit","-ExecutionPolicy","Bypass","-File","`"$PSScriptRoot\run-backend.ps1`"","-MySqlRoot","`"$MySqlRoot`"","-MySqlData","`"$MySqlData`""
Start-Sleep -Seconds 14

Write-Host "Starting the frontend in a new window..."
Start-Process powershell -WorkingDirectory "$root\frontend" -ArgumentList "-NoExit","-Command","npm run dev"
Start-Sleep -Seconds 6

Start-Process "http://localhost:5173"
Write-Host "Opened http://localhost:5173. Close the two new windows (or press Ctrl+C in them) to stop."
