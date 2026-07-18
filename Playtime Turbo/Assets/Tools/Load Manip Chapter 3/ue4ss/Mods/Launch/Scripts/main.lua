-- main.lua
-- Launch mod for Poppy Playtime (UE4SS Lua)
-- Press J to launch your player upward.

local CONFIG = {
    Key = "J",              -- key to press
    LaunchStrength = 800,   -- upward velocity/impulse
    Cooldown = 0.25,        -- seconds between launches
    RequireGrounded = true, -- only launch if on ground
    LogFile = "LaunchOnKey.log"
}

----------------------------------------------------------------
-- Logger (must be defined first!)
----------------------------------------------------------------
local function getScriptFolder()
    local info = debug.getinfo(1, "S")
    if info and info.source then
        local s = info.source
        local path = s:match("^@(.+)$")
        if path then
            return path:match("^(.+)\\[^\\]+$") or ""
        end
    end
    return ""
end

local scriptFolder = getScriptFolder()
local function log(msg)
    local path = (scriptFolder ~= "" and scriptFolder.."\\" or "") .. CONFIG.LogFile
    local f = io.open(path, "a")
    if f then
        local t = os.date("%Y-%m-%d %H:%M:%S")
        f:write(string.format("[%s] %s\n", t, tostring(msg)))
        f:close()
    end
end

log("Launch mod loaded")

----------------------------------------------------------------
-- Helpers
----------------------------------------------------------------
local UE = UE4 or ue4 or _G.UE4 or _G.ue4

local function getPlayerController()
    if UE and UE.GetPlayerController then
        return UE.GetPlayerController(0)
    end
    if GetPlayerController then
        return GetPlayerController(0)
    end
    return nil
end

local function getPlayerPawn()
    local pc = getPlayerController()
    if not pc then return nil end
    if pc.GetPawn then return pc:GetPawn() end
    if pc.K2_GetPawn then return pc:K2_GetPawn() end
    return nil
end

local function isKeyDown(keyName)
    local pc = getPlayerController()
    if not pc then
        if input and input.IsKeyDown then
            return input.IsKeyDown(keyName)
        end
        return false
    end

    if pc.IsInputKeyDown then
        local ekey = (UE and UE.EKeys and UE.EKeys[keyName]) or keyName
        local ok, res = pcall(function() return pc:IsInputKeyDown(ekey) end)
        if ok then return res end
    end

    return false
end

local function tryLaunchCharacter(char, strength)
    if not char then return false end

    if char.LaunchCharacter then
        local FVector = (UE and UE.FVector) or nil
        if FVector and FVector.New then
            local up = FVector.New(0,0,strength)
            pcall(function() char:LaunchCharacter(up, true, true) end)
            return true
        else
            pcall(function() char:LaunchCharacter({X=0,Y=0,Z=strength}, true, true) end)
            return true
        end
    end

    if char.RootComponent and char.RootComponent.AddImpulse then
        local FVector = (UE and UE.FVector) or nil
        if FVector and FVector.New then
            local imp = FVector.New(0,0,strength)
            pcall(function() char.RootComponent:AddImpulse(imp, nil, true) end)
            return true
        end
    end

    if char.GetActorLocation and char.SetActorLocation then
        local ok, loc = pcall(function() return char:GetActorLocation() end)
        if ok and loc and loc.Z then
            loc.Z = loc.Z + (strength * 0.02)
            pcall(function() char:SetActorLocation(loc) end)
            return true
        end
    end

    return false
end

local lastTime = 0
local function getTime()
    if UE and UE.GetWorldTimeSeconds then return UE.GetWorldTimeSeconds() end
    if os.clock then return os.clock() end
    return 0
end

----------------------------------------------------------------
-- Tick loop
----------------------------------------------------------------
if RegisterTickCallback then
    RegisterTickCallback(function(deltaSeconds)
        local now = getTime()
        if now - lastTime < CONFIG.Cooldown then return end
        if not isKeyDown(CONFIG.Key) then return end

        local pawn = getPlayerPawn()
        if not pawn then return end

        if CONFIG.RequireGrounded then
            local movement = pawn.CharacterMovement or (pawn.GetCharacterMovement and pawn:GetCharacterMovement())
            if movement and movement.IsFalling then
                local ok, falling = pcall(function() return movement:IsFalling() end)
                if ok and falling then
                    return
                end
            end
        end

        local ok = tryLaunchCharacter(pawn, CONFIG.LaunchStrength)
        if ok then
            lastTime = now
            log("Launched player (strength=" .. tostring(CONFIG.LaunchStrength) .. ")")
            if UE and UE.PrintString then pcall(function() UE.PrintString("Launch!") end) end
        end
    end)

    log("Launch mod initialized (Key="..CONFIG.Key..", using RegisterTickCallback)")
else
    log("ERROR: No RegisterTickCallback available in this UE4SS build")
end
