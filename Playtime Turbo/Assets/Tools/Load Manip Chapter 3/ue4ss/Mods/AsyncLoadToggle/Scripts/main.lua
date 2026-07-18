local UEHelpers = require("UEHelpers")
local KSL = nil

local function EjecutarModo(cmd, mensaje)
    ExecuteInGameThread(function()
        local status, World = pcall(UEHelpers.GetWorld)
        if not status or not World or not World:IsValid() then
            World = FindFirstOf("World")
        end
        if not World or not World:IsValid() then return end

        if not KSL or not KSL:IsValid() then
            KSL = StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
        end
        if KSL and KSL:IsValid() then
            pcall(function() KSL:ExecuteConsoleCommand(World, cmd, nil) end)
        end

        local actorFresco = FindFirstOf("ModActor_C")
        if actorFresco and actorFresco:IsValid() then
            pcall(function() actorFresco:MostrarTexto(mensaje) end)
        end
    end)
end

RegisterKeyBind(Key.I, {}, function() EjecutarModo("streamlevelout /Game/Maps/GasProductionZone/Sublevel/MP_GasProductionZone_Functionality", "LOADS ARE FROZEN") end)