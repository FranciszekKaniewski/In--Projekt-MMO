[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$taskServerDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskDotenvPath = Join-Path $taskServerDirectory '.env'
$taskEnv = @{}
foreach ($taskLine in (Get-Content -LiteralPath $taskDotenvPath -Encoding utf8)) {
    if ($taskLine -cmatch '^\s*(GAME_HOST|GAME_TLS_CERTFILE|GAME_TLS_KEYFILE|GAME_CAFILE)\s*=(.*)$') {
        $taskName = $Matches[1]
        $taskValue = $Matches[2].Trim()
        if ($taskValue -match '^(["''])(.*)\1$') { $taskValue = $Matches[2] }
        $taskEnv[$taskName] = $taskValue
    }
}
foreach ($taskName in @('GAME_HOST', 'GAME_TLS_CERTFILE', 'GAME_TLS_KEYFILE', 'GAME_CAFILE')) {
    if ([string]::IsNullOrWhiteSpace($taskEnv[$taskName])) {
        throw "Missing $taskName in $taskDotenvPath."
    }
}
$taskCertificatePath = [System.IO.Path]::GetFullPath($taskEnv.GAME_TLS_CERTFILE, $taskServerDirectory)
$taskKeyPath = [System.IO.Path]::GetFullPath($taskEnv.GAME_TLS_KEYFILE, $taskServerDirectory)
$taskCaPath = [System.IO.Path]::GetFullPath($taskEnv.GAME_CAFILE, $taskServerDirectory)
$taskOutputPaths = @($taskCertificatePath, $taskKeyPath, $taskCaPath)
if (($taskOutputPaths | Sort-Object -Unique).Count -ne 3) { throw 'TLS output paths must be distinct.' }
foreach ($taskPath in $taskOutputPaths) {
    if (Test-Path -LiteralPath $taskPath) {
        throw "TLS file already exists: $taskPath. Choose a new path in .env."
    }
}
foreach ($taskPath in $taskOutputPaths) {
    [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($taskPath))
}
$taskRsa = [System.Security.Cryptography.RSA]::Create(3072)
$taskCaRsa = [System.Security.Cryptography.RSA]::Create(3072)
try {
    $taskCaRequest = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
        'CN=SDL Game Local Development CA', $taskCaRsa, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
        [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $taskCaRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($true, $false, 0, $true))
    $taskCaRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
            [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyCertSign, $true))
    $taskCaRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509SubjectKeyIdentifierExtension]::new($taskCaRequest.PublicKey, $false))
    $taskCaCertificate = $taskCaRequest.CreateSelfSigned(
        [DateTimeOffset]::UtcNow.AddDays(-1), [DateTimeOffset]::UtcNow.AddYears(10))
    $taskRequest = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
        'CN=localhost', $taskRsa, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
        [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $taskSan = [System.Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
    $taskSan.AddDnsName('localhost')
    $taskSan.AddIpAddress([System.Net.IPAddress]::Loopback)
    $taskSan.AddIpAddress([System.Net.IPAddress]::IPv6Loopback)
    $taskHostAddress = $null
    if ([System.Net.IPAddress]::TryParse($taskEnv.GAME_HOST, [ref]$taskHostAddress)) {
        if ($taskHostAddress -ne [System.Net.IPAddress]::Loopback -and
            $taskHostAddress -ne [System.Net.IPAddress]::IPv6Loopback) {
            $taskSan.AddIpAddress($taskHostAddress)
        }
    } elseif ($taskEnv.GAME_HOST -ne 'localhost') {
        $taskSan.AddDnsName($taskEnv.GAME_HOST)
    }
    $taskRequest.CertificateExtensions.Add($taskSan.Build())
    $taskRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false, $false, 0, $true))
    $taskRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
            [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature, $true))
    $taskEku = [System.Security.Cryptography.OidCollection]::new()
    [void]$taskEku.Add([System.Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.1'))
    $taskRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($taskEku, $false))
    $taskRequest.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509SubjectKeyIdentifierExtension]::new($taskRequest.PublicKey, $false))
    $taskSerial = [byte[]]::new(16)
    [System.Security.Cryptography.RandomNumberGenerator]::Fill($taskSerial)
    $taskCertificate = $taskRequest.Create($taskCaCertificate,
        [DateTimeOffset]::UtcNow.AddDays(-1), [DateTimeOffset]::UtcNow.AddYears(5), $taskSerial)
    try {
        [System.IO.File]::WriteAllText($taskCertificatePath, $taskCertificate.ExportCertificatePem())
        [System.IO.File]::WriteAllText($taskKeyPath, $taskRsa.ExportPkcs8PrivateKeyPem())
        [System.IO.File]::WriteAllText($taskCaPath, $taskCaCertificate.ExportCertificatePem())
    } finally { $taskCertificate.Dispose() }
} finally {
    $taskRsa.Dispose()
    $taskCaRsa.Dispose()
    if ($null -ne $taskCaCertificate) { $taskCaCertificate.Dispose() }
}
Write-Output "Created local development certificate: $taskCertificatePath"
