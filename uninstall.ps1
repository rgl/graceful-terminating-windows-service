$serviceName = 'graceful-terminating-windows-service'

Write-Host "Destroying the $serviceName service..."
$result = sc.exe delete $serviceName
if ($result -ne '[SC] DeleteService SUCCESS') {
    throw "sc.exe config failed with $result"
}
