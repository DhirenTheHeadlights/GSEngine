param([string]$RootDir = "C:\Users\Dhiren\source\repos\DhirenTheHeadlights\GoonSquad")

$dirs = @("$RootDir\Engine\Engine", "$RootDir\Game\Game", "$RootDir\Server\Server")
$files = $dirs | ForEach-Object { Get-ChildItem -Path $_ -Recurse -Include *.cpp,*.cppm -File }

function Flatten-Defs([string[]]$lines) {
    $out = [System.Collections.Generic.List[string]]::new()
    $i = 0
    $changed = $false
    while ($i -lt $lines.Count) {
        $line = $lines[$i]
        # Match a line ending with `(` (no `)` on same line), followed by indented args, then `) ... {`
        if ($line -match '^([\t ]*\S.*)\($' -and ($line -notmatch '\)')) {
            $prefix = $matches[1]
            # Scan forward for closing line
            $j = $i + 1
            $argLines = @()
            $found = $false
            $closeLine = $null
            $maxScan = [Math]::Min($i + 30, $lines.Count - 1)
            for (; $j -le $maxScan; $j++) {
                $cur = $lines[$j]
                if ($cur -match '^[\t ]*\)([^{;\(]*)\{\s*$') {
                    $closeLine = $cur
                    $found = $true
                    break
                }
                # Must be an indented arg line (not blank, no opening paren that opens nested scope)
                if ($cur -match '^[\t ]+\S' -and $cur -notmatch '[{};]\s*$') {
                    $argLines += $cur.Trim()
                } else {
                    break
                }
            }
            if ($found -and $argLines.Count -gt 0) {
                # Build flattened line
                $argsJoined = $argLines -join ' '
                # Trim trailing comma (clang-format reformat will re-add if needed)
                $trailing = if ($closeLine -match '^[\t ]*\)([^{;\(]*)\{\s*$') { $matches[1] } else { '' }
                $newLine = "$prefix($argsJoined)$trailing{"
                # Sanity: only flatten if line is "reasonable" (< 300 chars)
                if ($newLine.Length -lt 300) {
                    $out.Add($newLine)
                    $i = $j + 1
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
    $result = Flatten-Defs $lines
    if ($result.Changed) {
        [System.IO.File]::WriteAllLines($f.FullName, $result.Lines)
        $count++
    }
}
Write-Output "Flattened $count files"
