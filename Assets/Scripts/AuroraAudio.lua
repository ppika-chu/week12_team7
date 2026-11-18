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

    -- 네온 글씨: 첫 글자(G)와 마지막 '!'만 magicsprinkle, 그 사이는 띄엄띄엄 spare를 작게.
    -- 글씨 파티클 한 루프 = EmitterDelay 7.6 + Duration 12.0. T = 글씨 파티클 안에서 그 글자를 쓰기 시작하는 시각
    Text = {
        Delay = 7.6,
        Duration = 12.003,
        Sprinkle = Dir .. "snd_magicsprinkle.wav",
        Spare = Dir .. "snd_spare.wav",
        SprinkleVolume = 0.3,
        SpareVolume = 0.16,
        Letters = {
            { T = 0.40, Sound = "Sprinkle" },               -- G (GAMETECH)
            { T = 1.31, Sound = "Spare", Pitch = 1.00 },    -- M
            { T = 2.18, Sound = "Spare", Pitch = 1.06 },    -- T
            { T = 2.85, Sound = "Spare", Pitch = 1.12 },    -- C
            { T = 3.69, Sound = "Spare", Pitch = 1.18 },    -- L (LAB)
            { T = 4.28, Sound = "Spare", Pitch = 1.24 },    -- B
            { T = 4.95, Sound = "Spare", Pitch = 0.94 },    -- F (FIGHTING!)
            { T = 5.51, Sound = "Spare", Pitch = 1.00 },    -- G
            { T = 6.61, Sound = "Spare", Pitch = 1.08 },    -- T
            { T = 7.58, Sound = "Spare", Pitch = 1.16 },    -- G
            { T = 8.21, Sound = "Sprinkle", Volume = 0.45 }, -- ! 마무리는 조금 크게
        },
    },
}

local State = { Time = 0.0, Bgm = 0, Shows = {}, TextLoopStart = 0.0, TextIndex = 1 }

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
    State.TextLoopStart = 0.0
    State.TextIndex = 1
end

-- 글씨를 쓰는 박자에 맞춰 반짝이는 소리
local function TickText()
    local C = Config.Text
    local Local = State.Time - State.TextLoopStart - C.Delay
    local L = C.Letters[State.TextIndex]
    if L and Local >= L.T then
        local Handle
        if L.Sound == "Sprinkle" then
            Handle = PlaySound(C.Sprinkle, false, L.Volume or C.SprinkleVolume)
        else
            Handle = PlaySound(C.Spare, false, L.Volume or C.SpareVolume)
        end
        if Handle ~= 0 and L.Pitch then
            SetSoundPitch(Handle, L.Pitch) -- 단어를 써 나갈수록 음이 조금씩 올라간다
        end
        State.TextIndex = State.TextIndex + 1
    end
    if State.Time - State.TextLoopStart >= C.Delay + C.Duration then
        State.TextLoopStart = State.TextLoopStart + C.Delay + C.Duration
        State.TextIndex = 1
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
    TickText()
end

function EndPlay()
    StopAllSounds()
end
