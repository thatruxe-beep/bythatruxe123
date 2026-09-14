-- Archangel Lua Injector - built-in payload (payload/script.lua)
-- Runs inside the game's Lua 5.1 state right after the DLL injects.
-- luainj_log(msg) is registered by the DLL -> writes to Archangel\luainj.log
pcall(function()
    if luainj_log then luainj_log("built-in payload started") end
    -- load the user script if present (CWD = game dir)
    for _, path in ipairs({ "Archangel\\script.lua", "script.lua" }) do
        local f = io.open(path, "r")
        if f then
            local src = f:read("*a")
            f:close()
            local fn, cerr = loadstring(src, "@" .. path)
            if not fn then
                if luainj_log then
                    luainj_log("compile error in " .. path .. ": " .. tostring(cerr))
                end
                return
            end
            local ok, perr = pcall(fn)
            if ok then
                if luainj_log then luainj_log("user script loaded: " .. path) end
            else
                if luainj_log then
                    luainj_log("runtime error in " .. path .. ": " .. tostring(perr))
                end
            end
            return
        end
    end
    if luainj_log then
        luainj_log("no Archangel\\script.lua found - idle (put your lua there to run it)")
    end
end)
