$serviceName = 'graceful-terminating-windows-service'

.\install.ps1

Write-Host "Starting the $serviceName service..."
$result = sc.exe start $serviceName
if ($result -like '* STATE *') {
    while ($true) {
        $result = sc.exe query $serviceName
        if ($result.Trim() -eq 'STATE              : 4  RUNNING') {
            break
        }
        Start-Sleep -Seconds 1
    }
} elseif ($result -ne '[SC] StartService SUCCESS') {
    throw "sc.exe config failed with $result"
}

Write-Host 'Slepping a bit before stopping the service...'
Start-Sleep -Seconds 2

Write-Host "Stopping the $serviceName service..."
$result = sc.exe stop $serviceName
if ($result -like '* STATE *') {
    while ($true) {
        $result = sc.exe query $serviceName
        if ($result.Trim() -eq 'STATE              : 1  STOPPED') {
            break
        }
        Start-Sleep -Seconds 1
    }
} elseif ($result -ne '[SC] ControlService SUCCESS') {
    throw "sc.exe config failed with $result"
}

.\uninstall.ps1
