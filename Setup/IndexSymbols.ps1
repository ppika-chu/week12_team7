param(
    [string]$SymbolStore,                                   # 예: \\172.21.10.88\symbols  (생략하면 이 PC에서 찾음)
    [string]$PdbPath,                                       # 생략하면 Build\Bin에서 찾음
    [string]$ExePath                                        # 생략하면 PDB 옆의 같은 이름 exe
)
$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Host $Message -ForegroundColor Red
    exit 1
}

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

# 새 공유 폴더에 연결한다. 이미 접근되면 로그인을 건너뛴다.
function Connect-NewShare {
    Write-Host ''
    $Path = (Read-Host '공유 폴더 경로 (예: \\192.168.0.10\symbols)').Trim().TrimEnd('\')
    if (-not $Path.StartsWith('\\')) { Fail '\\서버주소\공유이름 형식으로 입력하세요.' }

    if (Test-Path $Path) { return $Path }

    $User = Read-Host '아이디 (예: DESKTOP-OE96TKL\Week12_Team7)'
    # 비밀번호 자리에 *를 주면 net use가 입력을 가려서 직접 묻는다 (스크립트 변수에 남지 않음)
    # /persistent:yes 로 연결하면 다음 실행부터 '기존 연결'에 나타난다
    net use $Path /user:$User * /persistent:yes
    if ($LASTEXITCODE -ne 0) {
        Fail '연결하지 못했습니다. 같은 서버에 다른 계정으로 이미 연결되어 있으면(오류 1219) 호스트 이름 대신 IP로 다시 시도하세요.'
    }
    return $Path
}

$Repo   = (git rev-parse --show-toplevel) -replace '/', '\'
$Commit = git rev-parse HEAD
$Tools  = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\srcsrv"

# ---- 1. 심볼 저장소: 내 PC / 기존 연결 / 새 연결 중 하나를 번호로 고른다 ----
if (-not $SymbolStore) {
    # 내 PC: 이 PC의 공유 폴더 중 symstore가 만든 000Admin 폴더가 있는 것
    $Local = @(Get-SmbShare -Special $false -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and (Test-Path (Join-Path $_.Path '000Admin')) } |
        ForEach-Object { [pscustomobject]@{ Label = "\\$env:COMPUTERNAME\$($_.Name)  ($($_.Path))"; Value = "\\$env:COMPUTERNAME\$($_.Name)" } })

    # 기존 연결: net use 목록 중 상태가 OK이고 000Admin이 있는 것
    # (끊어진 연결은 Test-Path가 오래 기다리므로 처음부터 제외)
    $Mapped = @(Get-SmbMapping -ErrorAction SilentlyContinue |
        Where-Object { $_.Status -eq 'OK' -and (Test-Path (Join-Path $_.RemotePath '000Admin')) } |
        ForEach-Object { [pscustomobject]@{ Label = $_.RemotePath; Value = $_.RemotePath } })

    # 새 연결: Value가 빈 문자열이면 경로와 아이디를 받아 연결한다
    $New = @([pscustomobject]@{ Label = '다른 공유 폴더 경로 입력'; Value = '' })

    $Choices = @()   # 화면에 붙인 번호 순서대로 Value를 담는다
    Write-Host ''
    Write-Host '심볼 저장소 선택'
    foreach ($Group in @(
        @{ Title = '내 PC';     Items = $Local;  Empty = '(000Admin이 있는 공유 폴더 없음)' },
        @{ Title = '기존 연결'; Items = $Mapped; Empty = '(000Admin이 있는 연결 없음)' },
        @{ Title = '새 연결';   Items = $New;    Empty = '' }
    )) {
        Write-Host ''
        Write-Host "  $($Group.Title)"
        if ($Group.Items.Count -eq 0) { Write-Host "    $($Group.Empty)"; continue }
        foreach ($Item in $Group.Items) {
            $Choices += $Item.Value
            Write-Host ('    [{0}] {1}' -f $Choices.Count, $Item.Label)
        }
    }
    Write-Host ''

    while ($true) {
        $Answer = Read-Host '번호 선택 [기본값: 1]'
        $Index = 1
        if ($Answer -and -not [int]::TryParse($Answer, [ref]$Index)) { $Index = 0 }
        if ($Index -ge 1 -and $Index -le $Choices.Count) { break }
        Write-Host "1부터 $($Choices.Count) 사이의 번호를 입력하세요."
    }

    $SymbolStore = $Choices[$Index - 1]
    if (-not $SymbolStore) { $SymbolStore = Connect-NewShare }
}
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
$Files = & "$Tools\srctool.exe" -r $Pdb | ForEach-Object {
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
& "$Tools\pdbstr.exe" -w -p:$Pdb -s:srcsrv -i:"$Repo\Build\srcsrv.txt"
if ($LASTEXITCODE -ne 0) { exit 1 }

# ---- 6. 심볼 서버에 PDB와 exe 등록 (exe는 다른 PC에서 콜스택 해제에 필요) ----
& "$Tools\..\symstore.exe" add /f $Pdb /s $SymbolStore /t $Product
if ($LASTEXITCODE -ne 0) { exit 1 }

& "$Tools\..\symstore.exe" add /f $Exe /s $SymbolStore /t $Product
if ($LASTEXITCODE -ne 0) { exit 1 }
