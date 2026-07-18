local UEHelpers = require("UEHelpers")

IsInitialized = false

function Init()
    IsInitialized = true
end

Init()

function DumpAllFoundObjects()
    ExecuteInGameThread(
        function()
            local test = FindAllOf("SceneComponent")
            for i, component in pairs(test) do
                local owner = component:GetOwner()
                if owner:IsValid() then
                    local name = owner:GetFullName()
                    if name:match("TestPlayer") then
                        print(owner:GetClass():GetFName():ToString())
                    end
                    if not name:match("ReflectionCapture") and not name:match("WireCharacter") and not name:match("Audio") then
                        owner:SetActorHiddenInGame(false)
                        component:SetHiddenInGame(false, true)
                    end
                end
            end
        end
    )
end

local canJump = false
RegisterKeyBind(Key.F5, function ()
    canJump = not canJump
end)

RegisterKeyBind(Key.SPACE, function()
    if not canJump then return end

    local playerChar = UEHelpers.GetGameplayStatics():GetPlayerCharacter(UEHelpers.GetWorld(), 0)
    if playerChar:IsValid() then
        playerChar:LaunchCharacter({X = 0, Y = 0, Z = 1000}, false, true)
    end
end)

local savedPosition = nil
local savedRotation = nil
RegisterKeyBind(Key.F6, function ()
    local playerChar = UEHelpers.GetGameplayStatics():GetPlayerCharacter(UEHelpers.GetWorld(), 0)
    if playerChar:IsValid() then
        savedPosition = playerChar:K2_GetActorLocation()
        savedRotation = playerChar:K2_GetActorRotation()
        print(savedPosition)
        print(savedRotation)
    end
end)


RegisterKeyBind(Key.F7, function ()
    if not savedPosition or not savedRotation then return end

    local playerChar = UEHelpers.GetGameplayStatics():GetPlayerCharacter(UEHelpers.GetWorld(), 0)
    if playerChar:IsValid() then
        playerChar:K2_SetActorLocationAndRotation(savedPosition, savedRotation, false, {}, 1)
    end
end)

RegisterKeyBind(Key.F8, function()
    DumpAllFoundObjects()
end)

local wasHidden = false
local hiddenObjects = {}

RegisterInitGameStatePreHook(function ()
    hiddenObjects = {}
    wasHidden = false
    print("begin play")
end)

function CheckHasCollision(component)
    return component:GetCollisionEnabled() == 0
        or component:GetCollisionProfileName():ToString() == "NoCollision"
end

RegisterKeyBind(Key.F9, function()
    ExecuteInGameThread(function()
        if wasHidden then
            for key, component in pairs(hiddenObjects) do
                component:SetHiddenInGame(false, true)
            end

            wasHidden = false
            hiddenObjects = {}
        else
            local test = FindAllOf("StaticMeshComponent")
            for i, component in pairs(test) do
                if component:IsValid() and not component.bHiddenInGame and CheckHasCollision(component) then
                    component:SetHiddenInGame(true, false)
                    table.insert(hiddenObjects, component)
                end
            end

            print("hid " .. #hiddenObjects .. " objects")
            wasHidden = true
        end
    end
)
end)

local dilationProfiles = {1, 1.5, 2, 3}
local dilationProfileIndex = 0
RegisterKeyBind(Key.F3, function ()
    dilationProfileIndex = math.max(1, (dilationProfileIndex + 1) % (#dilationProfiles + 1))
    UEHelpers.GetGameplayStatics():SetGlobalTimeDilation(UEHelpers.GetWorld(), dilationProfiles[dilationProfileIndex])
end)

--[[RegisterKeyBind(Key.F4, function ()
    local playerChar = UEHelpers.GetGameplayStatics():GetPlayerCharacter(UEHelpers.GetWorld(), 0)
    if playerChar:IsValid() then
        local location = playerChar:K2_GetActorLocation()
        print(location)
        local volumes = FindAllOf("StreamingVolume")
        for _, volume in pairs(volumes) do
            print(volume:EncompassesPoint(location))
        end
    end
end)]]