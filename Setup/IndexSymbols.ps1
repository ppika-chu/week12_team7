param(
    [string]$SymbolStore,                                   # 예: \\172.21.10.88\symbols  (생략하면 이 PC에서 찾음)
    [string]$PdbPath,                                       # 생략하면 Build\Bin에서 찾음
    [string]$ExePath                                        # 생략하면 PDB 옆의 같은 이름 exe
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\SymbolStoreCommon.ps1"   # Fail, Connect-NewShare, Select-SymbolStore

# 후보를 소개한다. 하나면 바로 쓰고, 여러 개면 번호로 고르게 한다(Enter = 1번).
function Select-Candidate([string]$Title, [object[]]$Items) {
    Write-Host ''
    if ($Items.Count -eq 1) {
        Write-Host "$Title : $($Items[0].Label)"
        return $Items[0].Value
    }
    Write-Host "$Title :"
    for ($i = 0; $i -lt $Items.Count; $i++) {
        Write-Host ('  [{0}] {1}' -f ($i + 1), $Items[$i].Label)
    }
    while ($true) {
        $Answer = Read-Host '번호 선택 [기본값: 1]'
        if (-not $Answer) { return $Items[0].Value }
        $Index = 0
        if ([int]::TryParse($Answer, [ref]$Index) -and $Index -ge 1 -and $Index -le $Items.Count) {
            return $Items[$Index - 1].Value
        }
        Write-Host "1부터 $($Items.Count) 사이의 번호를 입력하세요."
    }
}

function Resolve-RepoPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $Repo $Path
}

$Repo   = (git rev-parse --show-toplevel) -replace '/', '\'
$Commit = git rev-parse HEAD

# ---- 0. 디버깅 도구 찾기: Windows SDK 설치 경로(KitsRoot10)는 레지스트리에 기록되어 있다 ----
$KitsRoots = @(
    'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots',
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots'
) | ForEach-Object { (Get-ItemProperty $_ -Name KitsRoot10 -ErrorAction SilentlyContinue).KitsRoot10 }
$KitsRoots += "${env:ProgramFiles(x86)}\Windows Kits\10\"   # 레지스트리에 없을 때 기본 위치

$Debuggers = $KitsRoots | Where-Object { $_ } | ForEach-Object { Join-Path $_ 'Debuggers\x64' } |
    Where-Object { (Test-Path "$_\symstore.exe") -and (Test-Path "$_\srcsrv\srctool.exe") -and (Test-Path "$_\srcsrv\pdbstr.exe") } |
    Select-Object -First 1
if (-not $Debuggers) {
    Fail @'
symstore.exe / srctool.exe / pdbstr.exe를 찾지 못했습니다.
Windows SDK의 "Debugging Tools for Windows" 구성 요소를 설치하세요:
  설정 -> 앱 -> 설치된 앱 -> Windows Software Development Kit -> 수정 -> 변경
  -> "Debugging Tools for Windows" 체크 -> 변경
'@
}
$SymStore = "$Debuggers\symstore.exe"
$SrcTool  = "$Debuggers\srcsrv\srctool.exe"
$PdbStr   = "$Debuggers\srcsrv\pdbstr.exe"

# ---- 1. 심볼 저장소: 내 PC / 기존 연결 / 새 연결 중 하나를 번호로 고른다 ----
if (-not $SymbolStore) { $SymbolStore = Select-SymbolStore }
if (-not (Test-Path $SymbolStore)) { Fail "심볼 저장소에 접근할 수 없습니다: $SymbolStore" }

# ---- 2. PDB: Build\Bin 아래에서 같은 이름의 exe와 짝을 이루는 PDB (최근 빌드 순) ----
if (-not $PdbPath) {
    $Pdbs = @(Get-ChildItem (Join-Path $Repo 'Build\Bin') -Recurse -Filter *.pdb -ErrorAction SilentlyContinue |
        Where-Object { Test-Path ([IO.Path]::ChangeExtension($_.FullName, '.exe')) } |
        Sort-Object LastWriteTime -Descending)
    if ($Pdbs.Count -eq 0) {
        Fail 'Build\Bin에 exe와 짝을 이루는 PDB가 없습니다. 먼저 빌드하세요.'
    }
    $PdbPath = Select-Candidate '빌드 폴더에서 찾은 PDB' @($Pdbs | ForEach-Object {
        [pscustomobject]@{ Label = ('{0}  ({1:yyyy-MM-dd HH:mm})' -f $_.FullName.Substring($Repo.Length + 1), $_.LastWriteTime); Value = $_.FullName }
    })
}
$Pdb = Resolve-RepoPath $PdbPath
if (-not (Test-Path $Pdb)) { Fail "PDB를 찾을 수 없습니다: $Pdb" }

# ---- 3. exe: 고른 PDB 옆의 같은 이름 exe ----
if (-not $ExePath) {
    $ExePath = [IO.Path]::ChangeExtension($Pdb, '.exe')
    if (-not (Test-Path $ExePath)) { Fail "PDB와 짝인 exe가 없습니다: $ExePath" }
    Write-Host ''
    Write-Host "PDB와 짝인 exe : $($ExePath.Substring($Repo.Length + 1))"
}
$Exe = Resolve-RepoPath $ExePath
if (-not (Test-Path $Exe)) { Fail "exe를 찾을 수 없습니다: $Exe" }
Write-Host ''

# ---- 4. GitHub 원격 주소에서 소스 다운로드 주소 만들기 ----
# https://github.com/owner/repo.git 또는 git@github.com:owner/repo.git 에서 owner/repo 추출
$Remote = git remote get-url origin
if ($Remote -notmatch 'github\.com[:/](.+?)(\.git)?$') { Fail "GitHub 원격 주소가 아닙니다: $Remote" }
$Slug    = $Matches[1]
$Product = $Slug -replace '.*/', ''
$Alias   = "https://raw.githubusercontent.com/$Slug/$Commit"

# git이 추적하는 파일 목록 (소문자 경로 -> 실제 대소문자 경로)
$Tracked = @{}
git -c core.quotepath=false ls-files | ForEach-Object { $Tracked[$_.ToLower()] = $_ }

# PDB에 기록된 소스 중 저장소 안에 있고 git이 추적하는 것만 고른다
$Files = & $SrcTool -r $Pdb | ForEach-Object {
    if ($_.StartsWith($Repo, 'OrdinalIgnoreCase')) {
        $Rel = $_.Substring($Repo.Length + 1).Replace('\', '/').ToLower()
        if ($Tracked.ContainsKey($Rel)) { "$_*$($Tracked[$Rel])" }
    }
}
if (-not $Files) { Fail '저장소 소스를 PDB에서 찾지 못했습니다.' }

@(
    'SRCSRV: ini ------------------------------------------------'
    'VERSION=2'
    'VERCTRL=http'
    'SRCSRV: variables ------------------------------------------'
    "HTTP_ALIAS=$Alias"
    'HTTP_EXTRACT_TARGET=%HTTP_ALIAS%/%var2%'
    'SRCSRVTRG=%HTTP_EXTRACT_TARGET%'
    'SRCSRV: source files ---------------------------------------'
    $Files
    'SRCSRV: end ------------------------------------------------'
) | Set-Content -Encoding ascii "$Repo\Build\srcsrv.txt"

# ---- 5. PDB에 스트림 써넣기 ----
& $PdbStr -w -p:$Pdb -s:srcsrv -i:"$Repo\Build\srcsrv.txt"
if ($LASTEXITCODE -ne 0) { exit 1 }

# ---- 6. 심볼 서버에 PDB와 exe 등록 (exe는 다른 PC에서 콜스택 해제에 필요) ----
& $SymStore add /f $Pdb /s $SymbolStore /t $Product
if ($LASTEXITCODE -ne 0) { exit 1 }

& $SymStore add /f $Exe /s $SymbolStore /t $Product
if ($LASTEXITCODE -ne 0) { exit 1 }
