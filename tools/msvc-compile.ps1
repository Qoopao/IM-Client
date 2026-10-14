param(
    [Parameter(Position = 0, Mandatory = $true)]
    [string]$Compiler,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CompilerArguments
)

& $Compiler @CompilerArguments 2>&1 | ForEach-Object {
    $line = $_.ToString()
    if ($line -match '^[^:]+:[^:]+:\s+((?:[A-Za-z]:[\\/]|\\\\).+)$') {
        Write-Output ("Note: including file: " + $Matches[1])
    } else {
        Write-Output $line
    }
}
exit $LASTEXITCODE
