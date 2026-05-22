param([string]$RootDir = "C:\Users\Dhiren\source\repos\DhirenTheHeadlights\GoonSquad")

$dirs = @("$RootDir\Engine\Engine", "$RootDir\Game\Game", "$RootDir\Server\Server")
$files = $dirs | ForEach-Object { Get-ChildItem -Path $_ -Recurse -Include *.cpp,*.cppm -File }

function Merge-TrailingReturn([string[]]$lines) {
    $out = [System.Collections.Generic.List[string]]::new()
    $i = 0
    $changed = $false
    while ($i -lt $lines.Count) {
        $line = $lines[$i]
        # Line ends with `)` (no leading `->`); next non-blank line starts with `->`
        if ($line -match '\)\s*$' -and $i + 1 -lt $lines.Count) {
            $next = $lines[$i + 1]
            if ($next -match '^[\t ]*(->\s*.*)$') {
                $arrow = $matches[1]
                $merged = "$($line.TrimEnd()) $arrow"
                if ($merged.Length -lt 300) {
                    $out.Add($merged)
                    $i += 2
                    $changed = $true
                    continue
                }
            }
        }
        $out.Add($line)
        $i++
    }
    return @{Lines = $out; Changed = $changed}
}

$count = 0
foreach ($f in $files) {
    $lines = Get-Content $f.FullName
    if ($null -eq $lines) { continue }
    if ($lines -is [string]) { $lines = @($lines) }
    $result = Merge-TrailingReturn $lines
    if ($result.Changed) {
        [System.IO.File]::WriteAllLines($f.FullName, $result.Lines)
        $count++
    }
}
Write-Output "Merged trailing-return arrows in $count files"
