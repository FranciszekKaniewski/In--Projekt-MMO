param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../priv/tls'))

$ErrorActionPreference = 'Stop'
$taskCertificateDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$taskCertificatePath = Join-Path $taskCertificateDirectory 'server.crt'
$taskKeyPath = Join-Path $taskCertificateDirectory 'server.key'
$taskCaPath = Join-Path $taskCertificateDirectory 'ca.crt'
if ((Test-Path -LiteralPath $taskCertificatePath) -or (Test-Path -LiteralPath $taskKeyPath) -or (Test-Path -LiteralPath $taskCaPath)) {
    throw 'Certificate or key already exists. Choose a new output directory.'
}
New-Item -ItemType Directory -Path $taskCertificateDirectory -Force | Out-Null
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
