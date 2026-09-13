-- Archangel MTA:SA payload (reconstructed).
-- The original 16KB embedded Lua (DAT_100ae610 in DLL.dll) is not recoverable
-- from the decompiled dump; this script re-implements the same pipeline:
--   onClientRender -> collect players (screen pos via getScreenFromWorldPosition)
--   -> ar_pushPlayer() (C side renders ESP) and apply features via MTA client API.
-- The original also had a start/stop toggle bound to a game key (startTaran /
-- _taran_enabled fragments are visible in the decompiled code) - kept here as
-- a keybind (default "f8", change in settings.cfg: tarankey = <key>).

local me = localPlayer
local enabled = true

local function toggleTaran()
    enabled = not enabled
    if enabled then
        outputChatBox("Archangel: ENABLED", 0, 200, 200, true)
    else
        outputChatBox("Archangel: DISABLED", 200, 200, 0, true)
    end
end

-- original fragment: bindKey("...", ..., "down", startTaran)
-- key name is configurable: settings.cfg -> tarankey = <mta key name> (default "f8")
local taranKey = (type(archangel_taranKey) == "string") and archangel_taranKey or "f8"
local ok = pcall(bindKey, taranKey, root, toggleTaran, "down")
if not ok then pcall(bindKey, "f8", root, toggleTaran, "down") end

local function frame()
    if not enabled then return end
    ar_frameBegin()

    local mx, my, mz = getElementPosition(me)
    if mx and my and mz then
        local players = getElementsByType("player")
        for i = 1, #players do
            local pl = players[i]
            local x, y, z = getElementPosition(pl)
            if x and y and z then
                local sx, sy, vis = getScreenFromWorldPosition(x, y, z)
                if sx and sy and vis then
                    local d = getDistanceBetweenPoints3D(mx, my, mz, x, y, z)
                    local hp = getPedHealth(pl)
                    if not hp then hp = 100 end
                    ar_pushPlayer(sx, sy, d, getElementName(pl), (pl == me) and 1 or 0, hp)
                end
            end
        end
    end

    local st = ar_state()
    if not st then return end

    if st.god == 1 then
        setPedInvincible(me, true)
    else
        setPedInvincible(me, false)
    end

    if st.armor == 1 then
        local a = getPedArmor(me)
        if a and a < 100 then setPedArmor(me, 100) end
    end

    if st.health == 1 then
        local h = getPedHealth(me)
        if h and h < 100 then setPedHealth(me, 100) end
    end

    if st.speed == 1 and st.speedmul and st.speedmul > 1.0 then
        local rot = getPedRotation(me)
        if rot then
            local a = math.rad(rot)
            local v = 6.0 * st.speedmul
            setElementVelocity(me, math.sin(a) * v, 0, -math.cos(a) * v)
        end
    end

    if st.noclip == 1 and mx then
        setElementVelocity(me, 0, 0, 0)
        setElementPosition(me, mx, my + 0.02, mz)
    end
end

addEventHandler("onClientRender", root, frame)
outputChatBox("Archangel payload loaded (toggle: F8)", 0, 200, 200, true)
