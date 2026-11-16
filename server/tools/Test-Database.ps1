$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
$taskPreviousDatabase = $env:POSTGRES_TEST_DB
$taskPreviousTests = $env:SERVER_DB_TESTS
$taskTestDatabase = 'sdl_auth_test_' + [Guid]::NewGuid().ToString('N')
$taskCreated = $false
$taskTestExitCode = 1
try {
    $taskDbUser = & docker compose exec -T db printenv POSTGRES_USER
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($taskDbUser)) {
        throw 'Could not read POSTGRES_USER from the running database container.'
    }
    if ($taskTestDatabase -notmatch '^sdl_auth_test_[a-f0-9]{32}$') { throw 'Invalid test database name.' }
    & docker compose exec -T db psql -U $taskDbUser -d postgres -v ON_ERROR_STOP=1 -c "CREATE DATABASE $taskTestDatabase"
    if ($LASTEXITCODE -ne 0) { throw 'Could not create an isolated test database.' }
    $taskCreated = $true
    foreach ($taskMigration in @('db/migrations/001_create_users.sql', 'db/migrations/002_create_characters.sql')) {
        Get-Content -Raw -Encoding UTF8 -LiteralPath $taskMigration |
            & docker compose exec -T db psql -U $taskDbUser -d $taskTestDatabase -v ON_ERROR_STOP=1
        if ($LASTEXITCODE -ne 0) { throw "Test migration failed: $taskMigration" }
    }
    $env:POSTGRES_TEST_DB = $taskTestDatabase
    $env:SERVER_DB_TESTS = '1'
    & rebar3 eunit
    $taskTestExitCode = $LASTEXITCODE
} finally {
    if ($taskCreated) {
        & docker compose exec -T db psql -U $taskDbUser -d postgres -v ON_ERROR_STOP=1 -c "DROP DATABASE $taskTestDatabase WITH (FORCE)"
        if ($LASTEXITCODE -ne 0) { Write-Warning "Remove test database manually: $taskTestDatabase" }
    }
    $env:POSTGRES_TEST_DB = $taskPreviousDatabase
    $env:SERVER_DB_TESTS = $taskPreviousTests
    Pop-Location
}
if ($taskTestExitCode -ne 0) { throw 'Database integration tests failed.' }
