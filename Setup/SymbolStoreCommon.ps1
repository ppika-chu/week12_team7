# IndexSymbols.ps1, SetupDebugger.ps1이 함께 쓰는 함수 모음. 직접 실행하지 않고 . 으로 불러 쓴다.

function Fail([string]$Message) {
    Write-Host $Message -ForegroundColor Red
    exit 1
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
    # Start-Process로 실행해야 net use의 출력이 함수 반환값에 섞이지 않는다
    $Process = Start-Process net -ArgumentList 'use', "`"$Path`"", "/user:`"$User`"", '*', '/persistent:yes' -NoNewWindow -Wait -PassThru
    if ($Process.ExitCode -ne 0) {
        Fail '연결하지 못했습니다. 같은 서버에 다른 계정으로 이미 연결되어 있으면(오류 1219) 호스트 이름 대신 IP로 다시 시도하세요.'
    }
    return $Path
}

# 심볼 저장소를 내 PC / 기존 연결 / 새 연결 중에서 번호로 고르게 하고, 고른 경로를 반환한다.
function Select-SymbolStore {
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

    $Store = $Choices[$Index - 1]
    if (-not $Store) { $Store = Connect-NewShare }
    return $Store
}
