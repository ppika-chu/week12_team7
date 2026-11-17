param(
    [string]$SymbolStore,                                   # 예: \\172.21.10.88\symbols  (생략하면 고르게 함)
    [string]$Module = 'HitoriEditor.exe',                   # '지정한 모듈만 로드' 목록에 넣을 모듈
    [string]$SettingsFile                                   # 시험용: 이 settings.json만 고친다 (VS 검색·실행 확인 생략)
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\SymbolStoreCommon.ps1"   # Fail, Connect-NewShare, Select-SymbolStore

# ---- 1. VS 2026 설정 파일 찾기 ----
# VS 2026은 옵션을 %LOCALAPPDATA%\Microsoft\VisualStudio\18.0_<설치 ID>\settings.json에 저장한다.
if ($SettingsFile) {
    $Targets = @($SettingsFile)
} else {
    $VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VsWhere)) { Fail 'Visual Studio 설치 정보를 찾을 수 없습니다 (vswhere.exe 없음).' }

    $Instances = @(& $VsWhere -version '[18.0,19.0)' -products * -prerelease -format json | Out-String | ConvertFrom-Json | ForEach-Object { $_ })
    if ($Instances.Count -eq 0) { Fail 'Visual Studio 2026이 설치되어 있지 않습니다.' }

    $Targets = @()
    foreach ($Instance in $Instances) {
        $Dir = "$env:LOCALAPPDATA\Microsoft\VisualStudio\18.0_$($Instance.instanceId)"
        if (Test-Path $Dir) {
            $Targets += Join-Path $Dir 'settings.json'
        } else {
            Write-Host "$($Instance.displayName): 아직 한 번도 실행하지 않아 설정 폴더가 없습니다. 한 번 실행한 뒤 다시 하세요." -ForegroundColor Yellow
        }
    }
    if ($Targets.Count -eq 0) { Fail '고칠 Visual Studio 2026 설정 파일이 없습니다.' }

    # VS는 종료할 때 설정을 저장하므로, 켜져 있으면 고친 내용이 덮어써진다
    if (Get-Process devenv -ErrorAction SilentlyContinue) {
        Fail 'Visual Studio가 실행 중입니다. 모두 닫은 뒤 다시 실행하세요.'
    }
}

# ---- 2. 심볼 저장소 고르기 ----
if (-not $SymbolStore) { $SymbolStore = Select-SymbolStore }
$SymbolStore = $SymbolStore.TrimEnd('\')
if (-not (Test-Path $SymbolStore)) { Fail "심볼 저장소에 접근할 수 없습니다: $SymbolStore" }

# 배열 설정에 { location = 값 } 항목이 없으면 추가하고, 있으면 켠다
function Add-LocationEntry($Json, [string]$Key, [string]$Location) {
    $Items = @($Json.$Key | Where-Object { $_ })
    $Found = $Items | Where-Object { $_.location -eq $Location }
    if ($Found) {
        foreach ($Item in $Found) { $Item.enabled = $true }
        Write-Host "    = $Key 에 이미 있음: $Location"
    } else {
        $Items += [pscustomobject]@{ enabled = $true; location = $Location }
        Write-Host "    + $Key 에 추가: $Location"
    }
    $Json | Add-Member -NotePropertyName $Key -NotePropertyValue ([object[]]$Items) -Force
}

# ---- 3. 설정 고치기 ----
foreach ($Target in $Targets) {
    Write-Host ''
    Write-Host "설정 파일 : $Target"

    # VS는 파일 맨 위에 /* Visual Studio Settings File */ 주석을 넣는다.
    # Windows PowerShell의 JSON 파서는 주석을 못 읽으므로 떼어 냈다가 쓸 때 다시 붙인다.
    $Header = '/* Visual Studio Settings File */'
    $HasBom = $false
    if (Test-Path $Target) {
        $Bytes  = [IO.File]::ReadAllBytes($Target)
        $HasBom = $Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF
        $Text   = [IO.File]::ReadAllText($Target, [Text.Encoding]::UTF8).TrimStart([char]0xFEFF)
        if ($Text -match '^\s*(/\*[\s\S]*?\*/)') {
            $Header = $Matches[1]
            $Text   = $Text.Substring($Matches[0].Length)
        }
        try {
            $Json = if ($Text.Trim()) { $Text | ConvertFrom-Json } else { [pscustomobject]@{} }
        } catch {
            Fail "설정 파일을 JSON으로 읽지 못했습니다(주석이 있으면 읽지 못합니다). 손대지 않고 종료합니다: $Target"
        }
        Copy-Item $Target "$Target.bak" -Force
        Write-Host "    백업     : $Target.bak"
    } else {
        $Json = [pscustomobject]@{}
    }

    Add-LocationEntry $Json 'debugging.symbols.location.searchPaths' $SymbolStore
    Add-LocationEntry $Json 'debugging.symbols.load.includedModules' $Module
    $Json | Add-Member -NotePropertyName 'debugging.general.useSourceServer' -NotePropertyValue $true -Force
    Write-Host '    = debugging.general.useSourceServer : true'

    $Out = $Header + "`r`n" + ($Json | ConvertTo-Json -Depth 32)
    [IO.File]::WriteAllText($Target, $Out, (New-Object Text.UTF8Encoding($HasBom)))
}

Write-Host ''
Write-Host '완료했습니다. Visual Studio를 다시 열면 적용됩니다.' -ForegroundColor Green
