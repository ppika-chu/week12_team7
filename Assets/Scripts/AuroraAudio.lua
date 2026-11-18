-- AuroraCurtain 씬 사운드: Winter BGM + 불꽃놀이 효과음.
-- 불꽃 파티클의 한 루프 = EmitterDelay + EmitterDuration (지연이 매 루프 들어간다).
-- 로켓은 루프마다 Delay 시점에 올라가고, Rise 초 뒤 하늘에서 터진다.

local Dir = "Assets/Sounds/Aurora/"
local Config = {
    Bgm = Dir .. "Winter.mp3",
    BgmVolume = 0.7,
    BgmFadeIn = 2.0, -- 시퀀서 페이드 인과 같은 길이
    Launch = Dir .. "fireworks_launch.wav",
    Twinkle = Dir .. "fireworks_twinkle.wav",
    TwinkleFar = Dir .. "fireworks_twinkle_far.wav",
    -- 씬의 Fireworks 액터와 같은 값 (Delay = 액터 EmitterDelay, Duration·Rise = 파티클 에셋)
    Fireworks = {
        { Delay = 2.0, Duration = 4.2, Rise = 1.3, Far = false, Volume = 0.8 }, -- Gold left
        { Delay = 2.6, Duration = 5.1, Rise = 1.3, Far = false, Volume = 0.7 }, -- Emerald right
        { Delay = 3.3, Duration = 5.5, Rise = 1.2, Far = true,  Volume = 0.9 }, -- Cyan high
        { Delay = 4.0, Duration = 4.8, Rise = 1.4, Far = false, Volume = 0.6 }, -- Rose right
        { Delay = 4.6, Duration = 6.0, Rise = 1.3, Far = true,  Volume = 0.7 }, -- Violet left
    },
    LaunchVolume = 0.35,
}

local State = { Time = 0.0, Bgm = 0, Shows = {} }

local function PlayVaried(Path, Volume)
    local Handle = PlaySound(Path, false, Volume * (0.85 + math.random() * 0.3))
    if Handle ~= 0 then
        SetSoundPitch(Handle, 0.9 + math.random() * 0.2) -- 같은 소리가 반복돼도 조금씩 다르게
    end
end

function BeginPlay()
    State.Time = 0.0
    State.Bgm = PlaySound(Config.Bgm, true, 0.0)
    if State.Bgm ~= 0 then
        FadeSound(State.Bgm, Config.BgmVolume, Config.BgmFadeIn)
    else
        print("[AuroraAudio] BGM을 열지 못함: " .. Config.Bgm)
    end
    State.Shows = {}
    for i, F in ipairs(Config.Fireworks) do
        State.Shows[i] = { NextLaunch = F.Delay, NextBurst = F.Delay + F.Rise }
    end
end

function Tick(dt)
    State.Time = State.Time + dt
    for i, F in ipairs(Config.Fireworks) do
        local S = State.Shows[i]
        local Period = F.Delay + F.Duration
        if State.Time >= S.NextLaunch then
            PlayVaried(Config.Launch, Config.LaunchVolume)
            S.NextLaunch = S.NextLaunch + Period
        end
        if State.Time >= S.NextBurst then
            PlayVaried(F.Far and Config.TwinkleFar or Config.Twinkle, F.Volume)
            S.NextBurst = S.NextBurst + Period
        end
    end
end

function EndPlay()
    StopAllSounds()
end
