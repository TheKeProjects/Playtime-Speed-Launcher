local UEHelpers = require("UEHelpers")

-- Fling strength
local flingStrength = 1800 -- adjust as needed

-- Keybind to fling upward
RegisterKeyBind(Key.F1, function()
    ExecuteInGameThread(function()
        local world = UEHelpers.GetWorld()
        local player = UEHelpers.GetGameplayStatics():GetPlayerCharacter(world, 0)

        if not (player and player:IsValid()) then
            print("Player not found or not valid!")
            return
        end

        -- Get current velocity if needed (optional)
        local currentVelocity = player:GetVelocity()

        -- Only fling upward
        local velocity = {
            X = -200,
            Y = 0,
            Z = flingStrength
        }

        -- Launch the player straight up
        player:LaunchCharacter(velocity, true, true)
        print(string.format("Flinging upward with velocity Z=%.2f", velocity.Z))
    end)
end)
