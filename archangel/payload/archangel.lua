-- Archangel MTA:SA payload (reconstructed).
-- The original 16KB embedded Lua (DAT_100ae610 in DLL.dll) is not recoverable
-- from the decompiled dump; this script re-implements the same pipeline:
--   onClientRender -> collect players (screen pos via getScreenFromWorldPosition)
--   -> ar_pushPlayer() (C side renders ESP) and apply features via MTA client API.
-- The cheat is active while injected; features are toggled from the ImGui menu.

local me = localPlayer

local function frame()
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
outputChatBox("Archangel loaded", 0, 200, 200, true)
