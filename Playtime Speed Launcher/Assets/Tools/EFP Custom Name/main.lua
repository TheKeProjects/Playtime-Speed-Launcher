--[[
    Playtime Speed Launcher - Custom Name (Escape From Playtime, old versions)

    Old builds run on OnlineSubsystemNull, so your in-game name is "<PC NAME>-<CODE>".
    This mod:
      1. Asks the server to rename you (APlayerController::ServerChangeName), so the other
         players see your custom name too, and sets it on your own PlayerState.
      2. Replaces "<PC NAME>-<CODE>" with your custom name in every text widget you see
         (menus, lobby, nameplates, scoreboard...).
      3. Styles and animates those texts on your screen: colour, size, shadow, outline, bold
         and an animation (rainbow, pulse, wave...). The style is only visible to you.

    Settings live in config.ini next to this folder's Scripts\ (written by the launcher) and are
    re-read every 2 seconds, so changes made in the launcher apply while the game is running.
]]

local UEHelpers = nil
pcall(function() UEHelpers = require("UEHelpers") end)

-- ─────────────────────────────── Config ───────────────────────────────

local function ScriptDir()
    local ok, info = pcall(debug.getinfo, 1, "S")
    if ok and info and info.source then
        local src = info.source:gsub("^@", "")
        local dir = src:match("^(.*)[/\\][^/\\]+$")
        if dir then return dir end
    end
    return nil
end

local function ConfigCandidates()
    local list = {}
    local dir = ScriptDir()
    if dir then table.insert(list, dir .. "\\..\\config.ini") end
    table.insert(list, "Mods\\PlaytimeCustomName\\config.ini")
    table.insert(list, "ue4ss\\Mods\\PlaytimeCustomName\\config.ini")
    return list
end

local CONFIG_PATHS = ConfigCandidates()

local function ReadFile(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local s = f:read("*a")
    f:close()
    return s
end

local function Trim(s) return (s:gsub("^%s+", ""):gsub("%s+$", "")) end

local function ParseHex(hex, fallback)
    hex = (hex or ""):gsub("#", "")
    if #hex ~= 6 or not hex:match("^%x+$") then return fallback end
    return { tonumber(hex:sub(1, 2), 16) / 255, tonumber(hex:sub(3, 4), 16) / 255, tonumber(hex:sub(5, 6), 16) / 255 }
end

local function ParseConfig(text)
    local c = {
        Name = "", Color = { 1, 1, 1 }, Color2 = { 0.2, 0.8, 1 }, Animation = "none", Speed = 1.0,
        Size = 1.0, Shadow = false, Outline = false, Bold = false, Style = true,
        Typeface = "", Italic = false, Case = "typed", Spacing = false,
    }
    for line in (text or ""):gmatch("[^\r\n]+") do
        if not line:match("^%s*[;#%[]") then
            local k, v = line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
            if k then
                if k == "Name" then c.Name = v
                elseif k == "Color" then c.Color = ParseHex(v, c.Color)
                elseif k == "Color2" then c.Color2 = ParseHex(v, c.Color2)
                elseif k == "Animation" then c.Animation = v:lower()
                elseif k == "Speed" then c.Speed = tonumber(v) or 1.0
                elseif k == "Size" then c.Size = tonumber(v) or 1.0
                elseif k == "Shadow" then c.Shadow = v == "1"
                elseif k == "Outline" then c.Outline = v == "1"
                elseif k == "Bold" then c.Bold = v == "1"
                elseif k == "Style" then c.Style = v ~= "0"
                elseif k == "Typeface" then c.Typeface = v
                elseif k == "Italic" then c.Italic = v == "1"
                elseif k == "Upper" then if v == "1" then c.Case = "upper" end   -- older launchers
                elseif k == "Case" then c.Case = v:lower()
                elseif k == "Spacing" then c.Spacing = v == "1"
                end
            end
        end
    end
    c.Name = Trim(c.Name)
    if c.Typeface == "" and c.Bold then c.Typeface = "Bold" end
    if c.Typeface:lower() == "regular" then c.Typeface = "" end   -- the game's own typeface
    if c.Speed < 0.1 then c.Speed = 0.1 elseif c.Speed > 4 then c.Speed = 4 end
    if c.Size < 0.5 then c.Size = 0.5 elseif c.Size > 2 then c.Size = 2 end
    return c
end

local cfg = ParseConfig(nil)
local cfgRaw = nil

-- Animations that move / grow the text get room: the font is made bigger (the layout box grows
-- with it) and the render scale shrinks it back, so pulse / wobble / shake don't get cut.
local MOVING = { pulse = true, wave = true, shake = true, wobble = true, glitch = true, fire = true }
local function Headroom() return MOVING[cfg.Animation] and 1.3 or 1.0 end


-- ─────────────────────────────── Helpers ───────────────────────────────

local function Valid(o)
    if o == nil then return false end
    local ok, v = pcall(function() return o:IsValid() end)
    return ok and v
end

local function Addr(o)
    local ok, a = pcall(function() return o:GetAddress() end)
    if ok then return a end
    return tostring(o)
end

local function TextOf(tb)
    local ok, s = pcall(function() return tb.Text:ToString() end)
    if ok and type(s) == "string" then return s end
    return nil
end

local KTL = nil
local function MakeText(s)
    local ok, t = pcall(function() return FText(s) end)
    if ok and t then return t end
    if not Valid(KTL) then KTL = StaticFindObject("/Script/Engine.Default__KismetTextLibrary") end
    if Valid(KTL) then
        local ok2, t2 = pcall(function() return KTL:Conv_StringToText(s) end)
        if ok2 then return t2 end
    end
    return nil
end

local function SetText(tb, s)
    local t = MakeText(s)
    if t then pcall(function() tb:SetText(t) end) end
end

local function Lin(c) return c ^ 2.2 end   -- sRGB → linear (FLinearColor)

local function Lerp3(a, b, k) return { a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k, a[3] + (b[3] - a[3]) * k } end

local function Hsv(h, s, v)
    local i = math.floor(h * 6)
    local f = h * 6 - i
    local p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    i = i % 6
    if i == 0 then return { v, t, p } elseif i == 1 then return { q, v, p } elseif i == 2 then return { p, v, t }
    elseif i == 3 then return { p, q, v } elseif i == 4 then return { t, p, v } end
    return { v, p, q }
end

-- ─────────────────────────── Who am I (original name) ───────────────────────────

local hostname = (os.getenv("COMPUTERNAME") or ""):lower()
local originalNames = {}      -- lower-case names seen on our PlayerState before renaming ("<PC>-<CODE>")
local previousNames = {}      -- earlier custom names (exact match only: they can be plain words)

local function IsCustom(lower) return cfg.Name ~= "" and lower == cfg.Name:lower() end

local function LooksLikeNullId(lower)
    if #hostname >= 2 and lower:find(hostname, 1, true) then return true end
    return #lower >= 10 and lower:find("%-%x%x%x%x") ~= nil
end

-- Finds "<PC NAME>...-<CODE>" (or a known original name) in `s`; returns start, end (bytes)
local function FindOriginal(s)
    local lower = s:lower()
    for name, _ in pairs(originalNames) do
        local a, b = lower:find(name, 1, true)
        if a then
            -- The PlayerState keeps only the first 20 characters: the rest of the code follows it
            local tail = lower:sub(b + 1):match("^[%w%-_]*") or ""
            return a, b + #tail
        end
    end
    if #hostname >= 2 then
        local from = 1
        while true do
            local a = lower:find(hostname, from, true)
            if not a then break end
            local before = a > 1 and lower:sub(a - 1, a - 1) or ""
            if not before:match("[%w]") then
                local b = a + #hostname - 1
                local rest = lower:sub(b + 1):match("^[%w%-_]*")
                -- The Null id always has a "-<code>" after the PC name
                if rest and rest:find("-", 1, true) then return a, b + #rest end
            end
            from = a + 1
        end
    end
    return nil
end

-- ─────────────────────────────── Rename ───────────────────────────────

local renameTries = {}   -- PlayerState address → { count, last }

local function LocalController()
    if UEHelpers then
        local ok, pc = pcall(UEHelpers.GetPlayerController)
        if ok and Valid(pc) then return pc end
    end
    local pcs = FindAllOf("PlayerController")
    if pcs then
        for _, pc in ipairs(pcs) do
            if Valid(pc) then
                local ok, isLocal = pcall(function() return pc:IsLocalController() end)
                if ok and isLocal then return pc end
            end
        end
    end
    return nil
end

local function RenameSelf(now)
    if cfg.Name == "" then return end
    local pc = LocalController()
    if not pc then return end
    local ps = nil
    pcall(function() ps = pc.PlayerState end)
    if not Valid(ps) then return end
    local current = nil
    pcall(function() current = ps:GetPlayerName():ToString() end)
    if not current then pcall(function() current = ps.PlayerNamePrivate:ToString() end) end
    if not current or current == cfg.Name then return end
    local lower = current:lower()
    -- Only Null ids ("<PC NAME>-<CODE>"): a plain default like "Player" would match unrelated texts
    if current ~= "" and not IsCustom(lower) and not previousNames[lower] and LooksLikeNullId(lower) then originalNames[lower] = true end

    local key = Addr(ps)
    local tries = renameTries[key] or { count = 0, last = -100 }
    renameTries[key] = tries
    if tries.count >= 6 or now - tries.last < 3 then return end
    tries.count = tries.count + 1
    tries.last = now
    -- Server RPC: the server renames our PlayerState and replicates it to everyone
    pcall(function() pc:ServerChangeName(cfg.Name) end)
    -- Host: replicates straight away. Client: our own copy shows it until the server answers
    pcall(function() ps:SetPlayerName(cfg.Name) end)
end

-- ─────────────────────────── Text widgets ───────────────────────────

local styled = {}     -- address → entry of the texts showing our name (with the widget's path)

-- Objects are never kept from one tick to the next: the game destroys widgets (loading screens,
-- menus) and UE4SS's IsValid() can't always tell a freed object, so touching a kept one crashes.
-- Texts are looked up fresh (FindAllOf) and the styled ones re-found by path every tick.
local textClasses = { TextBlock = true, CommonTextBlock = true }   -- TextBlock and the subclasses seen
pcall(function()
    NotifyOnNewObject("/Script/UMG.TextBlock", function(obj)
        local ok, name = pcall(function() return obj:GetClass():GetFName():ToString() end)
        if ok and type(name) == "string" and not textClasses[name] then textClasses[name] = true end
    end)
end)

local function PathOf(o)
    local ok, full = pcall(function() return o:GetFullName() end)
    if not ok or type(full) ~= "string" then return nil end
    return full:match("^%S+%s+(.+)$")
end

-- The object at `path` right now, nil when it's gone (or another object took its place)
local function Resolve(path, addr)
    if not path then return nil end
    local ok, o = pcall(StaticFindObject, path)
    if not ok or not Valid(o) then return nil end
    if addr and Addr(o) ~= addr then return nil end
    return o
end

-- Logs the first failure of each operation in UE4SS.log (Win64\ue4ss\UE4SS.log)
local reported = {}
local function Try(tag, fn)
    local ok, err = pcall(fn)
    if not ok and not reported[tag] then
        reported[tag] = true
        print("[PlaytimeCustomName] " .. tag .. " failed: " .. tostring(err) .. "\n")
    end
    return ok
end

-- Struct values are written straight into the widget's memory and pushed with setters that take
-- plain numbers: UE5 vector structs (doubles) passed as Lua tables don't reach the game.
local function SaveOriginal(tb)
    local o = {}
    pcall(function()
        local c = tb.ColorAndOpacity
        o.color = { R = c.SpecifiedColor.R, G = c.SpecifiedColor.G, B = c.SpecifiedColor.B, A = c.SpecifiedColor.A }
        o.colorRule = c.ColorUseRule
    end)
    pcall(function() o.shadowOffset = { X = tb.ShadowOffset.X, Y = tb.ShadowOffset.Y } end)
    pcall(function()
        local sc = tb.ShadowColorAndOpacity
        o.shadowColor = { R = sc.R, G = sc.G, B = sc.B, A = sc.A }
    end)
    pcall(function()
        local r = tb.RenderTransform
        o.transform = { tx = r.Translation.X, ty = r.Translation.Y, sx = r.Scale.X, sy = r.Scale.Y, angle = r.Angle }
    end)
    pcall(function() o.opacity = tb.RenderOpacity end)
    pcall(function() o.policy = tb.TextTransformPolicy end)
    pcall(function()
        local f = tb.Font
        o.font = {
            typeface = f.TypefaceFontName, skew = f.SkewAmount, spacing = f.LetterSpacing, size = f.Size,
            outline = f.OutlineSettings.OutlineSize,
            oc = { R = f.OutlineSettings.OutlineColor.R, G = f.OutlineSettings.OutlineColor.G,
                   B = f.OutlineSettings.OutlineColor.B, A = f.OutlineSettings.OutlineColor.A },
        }
    end)
    return o
end

-- The font fields are edited in the widget's memory; Slate only re-reads them through SetFont.
-- SetFontMaterial / SetFontOutlineMaterial call SetFont(Font) inside with the whole in-memory
-- font, so passing them the material the font already has pushes our edits without building the
-- struct in Lua (UE4SS can't push its object references from a table).
local function ObjOrNil(o)
    if Valid(o) then return o end
    return nil
end

local fontMethod = nil   -- the first way that worked, used from then on
local function PushFont(tb)
    local methods = {
        { "SetFontMaterial", function() tb:SetFontMaterial(tb.Font.FontMaterial) end },
        { "SetFontMaterial(nil)", function() tb:SetFontMaterial(nil) end },
        { "SetFontOutlineMaterial", function() tb:SetFontOutlineMaterial(tb.Font.OutlineSettings.OutlineMaterial) end },
        { "SetFontOutlineMaterial(nil)", function() tb:SetFontOutlineMaterial(nil) end },
    }
    for _, m in ipairs(methods) do
        if fontMethod == nil or fontMethod == m[1] then
            if Try(m[1], m[2]) then
                if fontMethod == nil then print("[PlaytimeCustomName] font pushed with " .. m[1] .. "\n") end
                fontMethod = m[1]
                return
            end
        end
    end
    Try("SetFont", function()
        local f = tb.Font
        local ol = f.OutlineSettings
        tb:SetFont({
            FontObject = ObjOrNil(f.FontObject), FontMaterial = ObjOrNil(f.FontMaterial),
            TypefaceFontName = f.TypefaceFontName,
            Size = f.Size, LetterSpacing = f.LetterSpacing, SkewAmount = f.SkewAmount,
            OutlineSettings = {
                OutlineSize = ol.OutlineSize, OutlineMaterial = ObjOrNil(ol.OutlineMaterial),
                bSeparateFillAlpha = ol.bSeparateFillAlpha, bApplyOutlineToDropShadows = true,
                OutlineColor = { R = ol.OutlineColor.R, G = ol.OutlineColor.G, B = ol.OutlineColor.B, A = ol.OutlineColor.A },
            },
        })
    end)
end

local function ApplyFont(tb, o)
    if not o.font then return end
    Try("font fields", function()
        local f = tb.Font
        f.Size = o.font.size * cfg.Size * Headroom()
        local tf = cfg.Typeface:lower()
        local bold = tf == "bold" or tf == "black"
        local face = cfg.Italic and (bold and "BoldItalic" or "Italic") or (bold and "Bold" or nil)
        if face then f.TypefaceFontName = FName(face) end
        if cfg.Spacing then f.LetterSpacing = 150 end
        if cfg.Outline then
            local ol = f.OutlineSettings
            ol.OutlineSize = math.max(2, math.floor(o.font.size / 12 + 0.5))
            local oc = ol.OutlineColor
            oc.R = 0
            oc.G = 0
            oc.B = 0
            oc.A = 1
            pcall(function() ol.bApplyOutlineToDropShadows = true end)
        end
    end)
    PushFont(tb)
end

local function RestoreFont(tb, o)
    if not o.font then return end
    Try("font restore", function()
        local f = tb.Font
        f.Size = o.font.size
        f.TypefaceFontName = o.font.typeface
        f.SkewAmount = o.font.skew
        f.LetterSpacing = o.font.spacing
        f.OutlineSettings.OutlineSize = o.font.outline
        local oc = f.OutlineSettings.OutlineColor
        oc.R = o.font.oc.R
        oc.G = o.font.oc.G
        oc.B = o.font.oc.B
        oc.A = o.font.oc.A
    end)
    PushFont(tb)
end

local flip = false
local function SetTransform(tb, tx, ty, sx, sy, angle)
    -- A tiny angle change every call, so Slate never skips the update as "the same transform"
    flip = not flip
    if flip then angle = angle + 0.001 end
    Try("transform", function()
        local r = tb.RenderTransform
        r.Translation.X = tx
        r.Translation.Y = ty
        r.Scale.X = sx
        r.Scale.Y = sy
        tb:SetRenderTransformAngle(angle)   -- pushes the whole RenderTransform to Slate
    end)
end

local function SetShadow(tb, x, y, color)
    Try("shadow offset", function()
        tb.ShadowOffset.X = x
        tb.ShadowOffset.Y = y
        if not pcall(function() tb:SetShadowOffset(tb.ShadowOffset) end) then tb:SetShadowOffset({ X = x, Y = y }) end
    end)
    Try("shadow color", function() tb:SetShadowColorAndOpacity(color) end)
end

local function Restore(tb, e, restoreText)
    if not Valid(tb) then return end
    local o = e.orig
    if o.color then pcall(function() tb:SetColorAndOpacity({ SpecifiedColor = o.color, ColorUseRule = o.colorRule or 0 }) end) end
    if e.shadowApplied and o.shadowOffset and o.shadowColor then SetShadow(tb, o.shadowOffset.X, o.shadowOffset.Y, o.shadowColor) end
    if o.transform then
        local t = o.transform
        SetTransform(tb, t.tx, t.ty, t.sx, t.sy, t.angle)
    end
    if o.opacity then pcall(function() tb:SetRenderOpacity(o.opacity) end) end
    if e.caseApplied and o.policy then pcall(function() tb:SetTextTransformPolicy(o.policy) end) end
    if e.fontApplied then RestoreFont(tb, o) end
    if restoreText and e.originalText then SetText(tb, e.originalText) end
    local comp = Resolve(e.compPath, e.compAddr)
    if comp then pcall(function() comp:RequestRedraw() end) end
end

-- Does `tb` show our name? Replaces the original name with the custom one when it does.
local function Track(addr, tb, text)
    local e = styled[addr]
    if e then
        if text == e.shownText then return e end
        -- The widget was reused for something else (list rows are recycled)
        Restore(tb, e, false)
        styled[addr] = nil
    end
    if cfg.Name == "" then return nil end
    local trimmed = Trim(text)
    local shown = nil
    local originalText = nil
    if IsCustom(trimmed:lower()) then
        shown = text
        if cfg.Style and trimmed ~= cfg.Name then
            originalText = text
            shown = text:gsub(trimmed:gsub("%p", "%%%0"), (cfg.Name:gsub("%%", "%%%%")), 1)
            SetText(tb, shown)
            local now = TextOf(tb)
            if now then shown = now end
        end
    elseif previousNames[trimmed:lower()] then
        originalText = text
        shown = cfg.Name
        SetText(tb, shown)
        local now = TextOf(tb)
        if now then shown = now end
    else
        local a, b = FindOriginal(text)
        if not a then return nil end
        originalText = text
        shown = text:sub(1, a - 1) .. cfg.Name .. text:sub(b + 1)
        SetText(tb, shown)
        local now = TextOf(tb)
        if now then shown = now end
    end
    local path = PathOf(tb)
    if not path then return nil end
    e = { orig = SaveOriginal(tb), shownText = shown, originalText = originalText, fontApplied = false, path = path }
    styled[addr] = e
    return e
end

-- Every text widget, fresh from FindAllOf and used only within this tick. Reading a text makes a
-- copy through UE4SS, so only new widgets are read on every scan; the ones already checked are
-- read again every 5th scan (their text can change later), and styled ones always.
local seenText = {}   -- address → text when last checked ("" = none)
local scanRound = 0
local inherits = nil  -- FindAllOf("TextBlock") also returns the subclasses (then one query is enough)

local function Scan()
    scanRound = scanRound + 1
    local full = scanRound % 5 == 0
    local seen = {}
    local classes = textClasses
    if inherits then classes = { TextBlock = true } end
    for cls, _ in pairs(classes) do
        local all = FindAllOf(cls)
        if all then
            if inherits == nil and cls == "TextBlock" then
                inherits = false
                for _, tb in ipairs(all) do
                    local ok, name = pcall(function() return tb:GetClass():GetFName():ToString() end)
                    if ok and name ~= "TextBlock" then inherits = true; break end
                end
                print("[PlaytimeCustomName] " .. #all .. " text widgets; subclasses included: " .. tostring(inherits) .. "\n")
            end
            for _, tb in ipairs(all) do
                local addr = Addr(tb)
                if not seen[addr] then
                    seen[addr] = true
                    if (styled[addr] or full or seenText[addr] == nil) and Valid(tb) then
                        local text = TextOf(tb) or ""
                        if styled[addr] or text ~= seenText[addr] then
                            seenText[addr] = text
                            if text ~= "" then Track(addr, tb, text)
                            elseif styled[addr] then Restore(tb, styled[addr], false); styled[addr] = nil end
                        end
                    end
                end
            end
        end
    end
    for addr, _ in pairs(seenText) do if not seen[addr] then seenText[addr] = nil end end
    for addr, _ in pairs(styled) do if not seen[addr] then styled[addr] = nil end end
    -- The copies UE4SS made hold game memory Lua doesn't see: free them now
    collectgarbage("collect")
end

-- ─────────────────────────────── Effects ───────────────────────────────

local function Effect(t)
    local s = cfg.Speed
    local fx = { color = cfg.Color, alpha = 1, dx = 0, dy = 0, scale = 1, angle = 0 }
    local a = cfg.Animation
    if a == "rainbow" then
        fx.color = Hsv((t * 0.25 * s) % 1, 0.75, 1)
    elseif a == "gradient" then
        fx.color = Lerp3(cfg.Color, cfg.Color2, (math.sin(t * 2.2 * s) + 1) / 2)
    elseif a == "pulse" then
        local k = (math.sin(t * 4 * s) + 1) / 2
        fx.scale = 1 + 0.10 * k
        fx.color = Lerp3(cfg.Color, { 1, 1, 1 }, 0.25 * k)
    elseif a == "blink" then
        fx.alpha = 0.3 + 0.7 * (((math.sin(t * 5 * s) + 1) / 2) ^ 0.4)
    elseif a == "wave" then
        fx.dy = math.sin(t * 4 * s) * 4
        fx.angle = math.cos(t * 4 * s) * 2
    elseif a == "shake" then
        fx.dx = (math.random() * 2 - 1) * 1.6 * math.min(s, 2)
        fx.dy = (math.random() * 2 - 1) * 1.6 * math.min(s, 2)
        fx.angle = (math.random() * 2 - 1) * 2
    elseif a == "wobble" then
        fx.angle = math.sin(t * 3 * s) * 4
        fx.scale = 1 + 0.04 * math.sin(t * 6 * s)
    elseif a == "glitch" then
        if math.random() < 0.12 * math.min(s, 2) then
            fx.dx = (math.random() * 2 - 1) * 5
            fx.color = math.random() < 0.5 and cfg.Color2 or { 1, 0.15, 0.35 }
            fx.alpha = 0.75
        end
    elseif a == "fire" then
        local n = (math.sin(t * 9 * s) + math.sin(t * 13.7 * s + 1.3) + 2) / 4
        fx.color = Lerp3({ 1, 0.25, 0.05 }, { 1, 0.85, 0.2 }, n * 0.8 + math.random() * 0.2)
        fx.dy = -n * 1.5
    elseif a == "neon" then
        local flick = math.random() < 0.027 and 0.35 or 1
        fx.alpha = flick * (0.85 + 0.15 * math.sin(t * 8 * s))
        fx.color = Lerp3(cfg.Color, { 1, 1, 1 }, 0.15)
    end
    return fx
end

-- Nameplates are WidgetComponents: the widget is drawn into a texture that's only redrawn now and
-- then, so the animation changes the widget but never reaches the screen. Find the component that
-- shows `tb` and make it redraw every frame.
local function FindWidgetComponent(tb)
    local owners = {}
    local o = tb
    for _ = 1, 8 do
        local ok, outer = pcall(function() return o:GetOuter() end)
        if not ok or not Valid(outer) then break end
        owners[Addr(outer)] = true
        o = outer
    end
    local comps = FindAllOf("WidgetComponent")
    if not comps then return nil end
    for _, comp in ipairs(comps) do
        if Valid(comp) then
            local w = nil
            pcall(function() w = comp:GetUserWidgetObject() end)
            if not Valid(w) then pcall(function() w = comp.Widget end) end
            if Valid(w) and owners[Addr(w)] then return comp end
        end
    end
    return nil
end

local function KeepRedrawing(e, tb, animated)
    if e.compSearched == nil then
        e.compSearched = true
        local comp = FindWidgetComponent(tb)
        if comp then
            e.compPath = PathOf(comp)
            e.compAddr = Addr(comp)
            local manual, redraw = "?", "?"
            pcall(function() manual = tostring(comp.bManuallyRedraw) end)
            pcall(function() redraw = tostring(comp.RedrawTime) end)
            print("[PlaytimeCustomName] name is on a WidgetComponent (manual redraw=" .. manual .. ", redraw time=" .. redraw .. ")\n")
            if animated then
                Try("SetManuallyRedraw", function() comp:SetManuallyRedraw(false) end)
                Try("SetRedrawTime", function() comp:SetRedrawTime(1 / 30) end)   -- the animation's 30 fps
            end
        else
            print("[PlaytimeCustomName] name is not on a WidgetComponent\n")
        end
        if comp then Try("RequestRedraw", function() comp:RequestRedraw() end) end
    end
end

local function ApplyStyle(tb, e, fx)
    if not cfg.Style then return end
    -- No animation: everything is set once
    if cfg.Animation == "none" and e.applied then return end
    e.applied = true
    local c = fx.color
    Try("color", function()
        tb:SetColorAndOpacity({ SpecifiedColor = { R = Lin(c[1]), G = Lin(c[2]), B = Lin(c[3]), A = 1 }, ColorUseRule = 0 })
    end)
    Try("opacity", function() tb:SetRenderOpacity((e.orig.opacity or 1) * fx.alpha) end)
    if cfg.Shadow and not e.shadowApplied then
        SetShadow(tb, 2, 2, { R = 0, G = 0, B = 0, A = 0.8 })
        e.shadowApplied = true
    end
    local t = e.orig.transform or { tx = 0, ty = 0, sx = 1, sy = 1, angle = 0 }
    local k = fx.scale / Headroom()   -- the size itself is in the font
    SetTransform(tb, t.tx + fx.dx, t.ty + fx.dy, t.sx * k, t.sy * k, t.angle + fx.angle)
    -- ETextTransformPolicy: 0 None (as typed), 1 ToLower, 2 ToUpper
    local policy = cfg.Case == "upper" and 2 or cfg.Case == "lower" and 1 or 0
    local current = nil
    pcall(function() current = tb.TextTransformPolicy end)
    if current ~= policy then
        Try("letter case", function() tb:SetTextTransformPolicy(policy) end)
        e.caseApplied = true
    end
    if not e.fontApplied and (cfg.Size ~= 1 or Headroom() ~= 1 or cfg.Outline or cfg.Typeface ~= "" or cfg.Italic or cfg.Spacing) then
        ApplyFont(tb, e.orig)
        e.fontApplied = true
    end
end

local lastCount = -1
local ticks, lastReport, reports = 0, 0, 0
local function Animate(t)
    if cfg.Name == "" then return end
    local count = 0
    for _ in pairs(styled) do count = count + 1 end
    if count ~= lastCount then
        lastCount = count
        print("[PlaytimeCustomName] styling " .. count .. " text(s)\n")
    end
    local fx = Effect(t)
    ticks = ticks + 1
    local report = (reports < 6 and t - lastReport >= 5) or t - lastReport >= 60   -- then once a minute
    if report then lastReport = t; reports = reports + 1 end
    for addr, e in pairs(styled) do
        local tb = Resolve(e.path, addr)
        if not tb then
            styled[addr] = nil   -- gone: never touched again
        else
            -- The game wrote the original name again (texts refreshed by the UI): put ours back
            if e.originalText and TextOf(tb) == e.originalText then SetText(tb, e.shownText) end
            ApplyStyle(tb, e, fx)
            if cfg.Style then KeepRedrawing(e, tb, cfg.Animation ~= "none") end
            if report then
                -- Heartbeat for the first 30 s: is the loop running, and do the values reach the widget?
                local sx, angle, skew = "?", "?", "?"
                pcall(function() sx = string.format("%.3f", tb.RenderTransform.Scale.X) end)
                pcall(function() angle = string.format("%.2f", tb.RenderTransform.Angle) end)
                pcall(function() skew = string.format("%.2f", tb.Font.SkewAmount) end)
                print(string.format("[PlaytimeCustomName] lua memory %.0f KB\n", collectgarbage("count")))
                print(string.format("[PlaytimeCustomName] t=%.1f ticks=%d anim=%s wanted scale=%.3f -> widget scale=%s angle=%s skew=%s visible=%s\n",
                    t, ticks, cfg.Animation, cfg.Size * fx.scale, sx, angle, skew, tostring(select(2, pcall(function() return tb:IsVisible() end)))))
            end
        end
    end
end

-- ─────────────────────────────── Loops ───────────────────────────────

-- A map / loading screen destroys the widgets: drop every reference before they go away, the new
-- ones are found again by the next scan
local function ForgetAll()
    styled = {}
    seenText = {}
    renameTries = {}
end
pcall(function() RegisterLoadMapPreHook(function() ForgetAll() end) end)
pcall(function() RegisterInitGameStatePreHook(function() ForgetAll() end) end)

local function ReloadConfig()
    local raw = nil
    for _, p in ipairs(CONFIG_PATHS) do
        raw = ReadFile(p)
        if raw then break end
    end
    if raw == cfgRaw then return false end
    cfgRaw = raw
    -- New settings: put every styled widget back as it was, then they are found again
    local oldName = cfg.Name
    for addr, e in pairs(styled) do
        local tb = Resolve(e.path, addr)
        if tb then Restore(tb, e, true) end
    end
    styled = {}
    seenText = {}   -- every text is looked at again with the new name
    cfg = ParseConfig(raw)
    -- Texts still showing the previous custom name (the server already renamed us) switch too
    if oldName ~= "" then previousNames[oldName:lower()] = true end
    if cfg.Name ~= "" then previousNames[cfg.Name:lower()] = nil; originalNames[cfg.Name:lower()] = nil end
    renameTries = {}
    return true
end

local startedAt = os.clock()
local lastScan, lastRename, lastConfig = -100, -100, -100

ReloadConfig()

LoopAsync(33, function()   -- 30 fps
    ExecuteInGameThread(function()
        local now = os.clock() - startedAt
        if now - lastConfig >= 2 then lastConfig = now; pcall(ReloadConfig) end
        if now - lastRename >= 1 then lastRename = now; pcall(RenameSelf, now) end
        if now - lastScan >= 2 then lastScan = now; pcall(Scan) end
        pcall(Animate, now)
        collectgarbage("step", 0)
    end)
    return false
end)

print("[PlaytimeCustomName] loaded\n")
