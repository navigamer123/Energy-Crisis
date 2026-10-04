<#
.SYNOPSIS
    Checks the translation files assets/lang/*.lang against the reference language.

.DESCRIPTION
    Format of a .lang file (see docs/I18N.md):
      - UTF-8 without BOM (a BOM is tolerated and reported as a warning);
      - one entry per line: "key = text", split at the FIRST '=', key and text trimmed;
      - lines starting with '#' are comments, blank lines are ignored;
      - keys: lowercase ASCII segments [a-z0-9_] separated by '.', e.g. "menu.play";
      - escapes in text: \n (newline) and \\ (backslash);
      - placeholders {0}, {1}, ...; "{{" and "}}" are literal braces.

    Errors (exit code 1):
      - malformed line (no '=' or a bad key), duplicate key, empty value,
        missing "lang.name", text that is not valid UTF-8;
      - for every non-reference file: missing keys, extra keys and keys whose
        set of {N} placeholders differs from the reference.
    Warnings (do not change the exit code):
      - UTF-8 BOM, unknown escape sequence, stray '{' or '}'.

.PARAMETER LangDir
    Folder with the .lang files. Default: <script folder>/../assets/lang

.PARAMETER Reference
    Language code of the reference file. Default: bg (assets/lang/bg.lang)

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_lang.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_lang.ps1 -LangDir assets/lang -Reference bg
#>
[CmdletBinding()]
param(
    [string]$LangDir = '',
    [string]$Reference = 'bg'
)

# Windows PowerShell 5.1 compatible: no ?:, no ??, no &&.
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$KeyPattern = '^[a-z0-9_]+(\.[a-z0-9_]+)*$'
# Tokens of a text, scanned left to right: literal "{{" / "}}", a placeholder {N}, or a lone brace.
$BracePattern = '\{\{|\}\}|\{([0-9]+)\}|[{}]'
$EscapePattern = '\\(.|$)'

function Write-Err([string]$text)  { Write-Host ('  ERROR   ' + $text) -ForegroundColor Red }
function Write-Warn([string]$text) { Write-Host ('  warning ' + $text) -ForegroundColor Yellow }

function Get-Excerpt([string]$text) {
    $t = $text.Trim()
    if ($t.Length -gt 60) { $t = $t.Substring(0, 57) + '...' }
    return '"' + $t + '"'
}

function Test-HasBom([string]$path) {
    $fs = [System.IO.File]::OpenRead($path)
    try {
        $buf = New-Object byte[] 3
        $n = $fs.Read($buf, 0, 3)
        return ($n -eq 3 -and $buf[0] -eq 0xEF -and $buf[1] -eq 0xBB -and $buf[2] -eq 0xBF)
    } finally {
        $fs.Dispose()
    }
}

# Parses one .lang file. Prints its own format problems and returns
# @{ Entries = Dictionary[key -> entry]; Order = List[key]; Errors = int; Warnings = int }.
function Read-LangFile([string]$path) {
    $name = [System.IO.Path]::GetFileName($path)
    $result = @{
        Entries  = [System.Collections.Generic.Dictionary[string, object]]::new([System.StringComparer]::Ordinal)
        Order    = [System.Collections.Generic.List[string]]::new()
        Errors   = 0
        Warnings = 0
    }

    if (Test-HasBom $path) {
        Write-Warn ($name + ': UTF-8 BOM (tolerated; save the file as UTF-8 without BOM)')
        $result.Warnings++
    }

    $lines = [System.IO.File]::ReadAllLines($path, [System.Text.Encoding]::UTF8)
    if ($lines.Length -gt 0 -and $lines[0].Length -gt 0 -and $lines[0][0] -eq [char]0xFEFF) {
        $lines[0] = $lines[0].Substring(1)
    }

    for ($i = 0; $i -lt $lines.Length; $i++) {
        $lineNo = $i + 1
        $raw = $lines[$i]
        $trimmed = $raw.Trim()
        if ($trimmed.Length -eq 0) { continue }
        if ($trimmed.StartsWith('#')) { continue }

        if ($raw.IndexOf([char]0xFFFD) -ge 0) {
            Write-Err ('line ' + $lineNo + ': invalid UTF-8 (was the file saved as Windows-1251?)')
            $result.Errors++
        }

        $eq = $raw.IndexOf('=')
        if ($eq -lt 0) {
            Write-Err ('line ' + $lineNo + ": malformed line, no '=': " + (Get-Excerpt $raw))
            $result.Errors++
            continue
        }
        $key = $raw.Substring(0, $eq).Trim()
        $value = $raw.Substring($eq + 1).Trim()

        if ($key -cnotmatch $KeyPattern) {
            Write-Err ('line ' + $lineNo + ": malformed line, bad key '" + $key + "' (allowed: a-z 0-9 _ and '.' between segments)")
            $result.Errors++
            continue
        }

        if ($result.Entries.ContainsKey($key)) {
            Write-Err ('line ' + $lineNo + ": duplicate key '" + $key + "' (first at line " + $result.Entries[$key].Line + ')')
            $result.Errors++
            continue
        }

        if ($value.Length -eq 0) {
            Write-Err ('line ' + $lineNo + ": empty value for '" + $key + "'")
            $result.Errors++
        }

        # Placeholders and stray braces.
        $set = New-Object 'System.Collections.Generic.SortedSet[int]'
        $stray = $false
        foreach ($m in [regex]::Matches($value, $BracePattern)) {
            if ($m.Groups[1].Success) {
                $n = 0
                if ([int]::TryParse($m.Groups[1].Value, [ref]$n)) { [void]$set.Add($n) }
            } elseif ($m.Value.Length -eq 1) {
                $stray = $true
            }
        }
        if ($stray) {
            Write-Warn ('line ' + $lineNo + ": stray '{' or '}' in '" + $key + "' (use {{ and }} for literal braces)")
            $result.Warnings++
        }

        # Escapes: only \n and \\ are known.
        foreach ($m in [regex]::Matches($value, $EscapePattern)) {
            $c = $m.Groups[1].Value
            if ($c.Length -eq 0) {
                Write-Warn ('line ' + $lineNo + ": trailing '\' in '" + $key + "' (write \\ for a backslash)")
                $result.Warnings++
            } elseif ($c -cne 'n' -and $c -ne '\') {
                Write-Warn ('line ' + $lineNo + ": unknown escape '\" + $c + "' in '" + $key + "' (known: \n and \\)")
                $result.Warnings++
            }
        }

        $ph = (@($set) | ForEach-Object { '{' + $_ + '}' }) -join ','
        $result.Entries[$key] = [pscustomobject]@{ Line = $lineNo; Value = $value; Placeholders = $ph }
        $result.Order.Add($key)
    }

    if (-not $result.Entries.ContainsKey('lang.name')) {
        Write-Err ($name + ": missing 'lang.name' (the language name shown in the menu)")
        $result.Errors++
    }

    return $result
}

function Format-Placeholders([string]$ph) {
    if ($ph.Length -eq 0) { return '(none)' }
    return $ph
}

# --- main --------------------------------------------------------------------

# Returns the exit code: 0 if every file is clean, 1 otherwise.
function Invoke-CheckLang([string]$dir, [string]$code) {
    if ($dir.Length -eq 0) {
        $scriptDir = Split-Path -Parent $script:MyInvocation.MyCommand.Path
        $dir = Join-Path $scriptDir '..\assets\lang'
    }
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        Write-Host ('check_lang: folder not found: ' + $dir) -ForegroundColor Red
        return 1
    }
    $dir = (Resolve-Path -LiteralPath $dir).ProviderPath

    $refName = $code + '.lang'
    $refPath = Join-Path $dir $refName
    if (-not (Test-Path -LiteralPath $refPath -PathType Leaf)) {
        Write-Host ('check_lang: reference file not found: ' + $refPath) -ForegroundColor Red
        return 1
    }

    Write-Host ('check_lang: ' + $dir + ' (reference: ' + $refName + ')')

    $totalErrors = 0
    $totalWarnings = 0
    $failed = New-Object System.Collections.Generic.List[string]

    # 1) The reference file on its own.
    Write-Host ''
    Write-Host ('== ' + $refName + ' (reference)')
    $ref = Read-LangFile $refPath
    $totalErrors += $ref.Errors
    $totalWarnings += $ref.Warnings
    $status = 'OK'
    if ($ref.Errors -gt 0) { $status = 'FAIL'; $failed.Add($refName) }
    Write-Host ('  ' + $refName + ': ' + $ref.Entries.Count + ' keys, ' + $ref.Errors + ' error(s), ' + $ref.Warnings + ' warning(s) -> ' + $status)

    # 2) Every other *.lang file against the reference.
    $others = @(Get-ChildItem -LiteralPath $dir -Filter '*.lang' -File | Where-Object { $_.Name -ne $refName } | Sort-Object Name)
    foreach ($f in $others) {
        Write-Host ''
        Write-Host ('== ' + $f.Name + ' (vs ' + $refName + ')')
        $cur = Read-LangFile $f.FullName
        $errors = $cur.Errors
        $warnings = $cur.Warnings

        $missing = 0
        foreach ($k in $ref.Order) {
            # A missing lang.name is already reported by Read-LangFile.
            if ($k -ceq 'lang.name') { continue }
            if (-not $cur.Entries.ContainsKey($k)) {
                Write-Err ("missing key '" + $k + "' (" + $refName + ':' + $ref.Entries[$k].Line + ')')
                $missing++
            }
        }

        $extra = 0
        $phDiff = 0
        foreach ($k in $cur.Order) {
            $e = $cur.Entries[$k]
            if (-not $ref.Entries.ContainsKey($k)) {
                Write-Err ('line ' + $e.Line + ": extra key '" + $k + "' (not in " + $refName + ')')
                $extra++
                continue
            }
            $refPh = $ref.Entries[$k].Placeholders
            if ($e.Placeholders -cne $refPh) {
                Write-Err ('line ' + $e.Line + ": placeholders differ for '" + $k + "': " + $code + ' ' + (Format-Placeholders $refPh) + ', ' + $f.BaseName + ' ' + (Format-Placeholders $e.Placeholders))
                $phDiff++
            }
        }

        $errors += $missing + $extra + $phDiff
        $totalErrors += $errors
        $totalWarnings += $warnings
        $status = 'OK'
        if ($errors -gt 0) { $status = 'FAIL'; $failed.Add($f.Name) }
        Write-Host ('  ' + $f.Name + ': ' + $cur.Entries.Count + ' keys, ' + $missing + ' missing, ' + $extra + ' extra, ' + $phDiff + ' placeholder mismatch(es), ' + $errors + ' error(s), ' + $warnings + ' warning(s) -> ' + $status)
    }

    Write-Host ''
    $fileCount = 1 + $others.Count
    if ($totalErrors -gt 0) {
        Write-Host ('check_lang: ' + $fileCount + ' file(s), ' + $totalErrors + ' error(s), ' + $totalWarnings + ' warning(s) -> FAIL (' + ($failed -join ', ') + ')') -ForegroundColor Red
        return 1
    }
    Write-Host ('check_lang: ' + $fileCount + ' file(s), 0 errors, ' + $totalWarnings + ' warning(s) -> OK') -ForegroundColor Green
    return 0
}

# Print UTF-8 (keys are ASCII, but excerpts of malformed lines may be Cyrillic).
$oldEncoding = $null
try {
    $oldEncoding = [Console]::OutputEncoding
    [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
} catch {
    $oldEncoding = $null
}

$exitCode = 1
try {
    $exitCode = Invoke-CheckLang $LangDir $Reference
} finally {
    if ($null -ne $oldEncoding) {
        try { [Console]::OutputEncoding = $oldEncoding } catch { }
    }
}
exit $exitCode
