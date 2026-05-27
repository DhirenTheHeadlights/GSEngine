param([string]$RootDir = "C:\Users\Dhiren\source\repos\DhirenTheHeadlights\GoonSquad")

$dirs = @("$RootDir\Engine\Engine", "$RootDir\Game\Game", "$RootDir\Server\Server")
$files = $dirs | ForEach-Object { Get-ChildItem -Path $_ -Recurse -Include *.cpp,*.cppm -File }

# Walk the file once, finding `Identifier<` at end of line followed by a single
# indented block ending with `>` on its own line. Collapse them. Only handles a
# single nesting level per pass; multi-pass collapses nested cases.

function Flatten-Templates([string[]]$lines) {
    $out = [System.Collections.Generic.List[string]]::new()
    $i = 0
    $changed = $false
    while ($i -lt $lines.Count) {
        $line = $lines[$i]
        # Line ends with `<` and looks like a template opener (Identifier or `template <`)
        if ($line -match '<$' -and $line -notmatch '<<$') {
            $j = $i + 1
            $argLines = @()
            $maxScan = [Math]::Min($i + 20, $lines.Count - 1)
            $closeIdx = -1
            for (; $j -le $maxScan; $j++) {
                $cur = $lines[$j]
                # Closing line: `>...` possibly with stuff after (e.g. `>::type`, `>& subject,`)
                if ($cur -match '^[\t ]*>') {
                    $closeIdx = $j
                    break
                }
                # Must be an indented content line with no obvious structural elements
                if ($cur -match '^[\t ]+\S' -and $cur -notmatch '[{};]\s*$' -and $cur -notmatch '<$') {
                    $argLines += $cur.Trim()
                } else {
                    break
                }
            }
            if ($closeIdx -ge 0 -and $argLines.Count -gt 0) {
                $closeLine = $lines[$closeIdx]
                # Extract the part after the leading `>` (trim leading whitespace and the `>`)
                if ($closeLine -match '^[\t ]*>(.*)$') {
                    $afterGt = $matches[1]
                    $argsJoined = $argLines -join ' '
                    $newLine = "$line$argsJoined>$afterGt"
                    if ($newLine.Length -lt 300) {
                        $out.Add($newLine)
                        $i = $closeIdx + 1
                        $changed = $true
                        continue
                    }
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
    $changedFile = $false
    # Multiple passes to handle nested templates
    for ($pass = 0; $pass -lt 5; $pass++) {
        $result = Flatten-Templates $lines
        if (-not $result.Changed) { break }
        $lines = $result.Lines.ToArray()
        $changedFile = $true
    }
    if ($changedFile) {
        [System.IO.File]::WriteAllLines($f.FullName, $lines)
        $count++
    }
}
Write-Output "Flattened templates in $count files"
