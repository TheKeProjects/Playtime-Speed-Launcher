local TAS = {
    recording = false,
    playing = false,
    paused = true,
    frame = 0,
    inputs = {},
    player = nil,
}

-- Setup logger
local log = function(msg)
    print("[TAS] " .. msg)
end

-- Access player
function TAS:GetPlayer()
    if not self.player then
        self.player = FindFirstOf("PlayerController")
    end
    return self.player
end

-- Hook Tick
RegisterHook("/Script/Engine.PlayerController:PlayerTick", function(Context, Func, This, Args)
    if TAS.paused then return end

    TAS.frame = TAS.frame + 1

    if TAS.recording then
        -- record key states or actions here (example with Jump)
        local isJumpPressed = This:IsInputKeyDown(Enum.Key.SpaceBar) -- change key if needed
        table.insert(TAS.inputs, {frame = TAS.frame, action = "Jump", pressed = isJumpPressed})
    end

    if TAS.playing then
        for _, input in ipairs(TAS.inputs) do
            if input.frame == TAS.frame then
                TAS:SimulateInput(input.action, input.pressed)
            end
        end
    end
end)

-- Simulate input
function TAS:SimulateInput(actionName, pressed)
    local pawn = self:GetPlayer():GetPawn()
    if not pawn then return end

    local func = pawn:FindFunction(actionName)
    if func then
        pawn:ProcessEvent(func, nil)
        log("Simulated " .. actionName)
    else
        log("Function not found: " .. actionName)
    end
end

-- Frame advance
function TAS:StepOneFrame()
    self.paused = false
    Timer.After(0.016, function()  -- Wait 1 frame (approx 60 FPS)
        self.paused = true
    end)
end

-- Console commands
RegisterConsoleCommand("tas_record", function()
    TAS.recording = true
    TAS.frame = 0
    TAS.inputs = {}
    TAS.playing = false
    log("Started recording.")
end)

RegisterConsoleCommand("tas_stop", function()
    TAS.recording = false
    log("Stopped recording.")
end)

RegisterConsoleCommand("tas_play", function()
    TAS.playing = true
    TAS.frame = 0
    log("Started playback.")
end)

RegisterConsoleCommand("tas_step", function()
    TAS:StepOneFrame()
    log("Stepped one frame.")
end)

RegisterConsoleCommand("tas_pause", function()
    TAS.paused = true
    log("Paused.")
end)

RegisterConsoleCommand("tas_resume", function()
    TAS.paused = false
    log("Resumed.")
end)
