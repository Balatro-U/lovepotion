R"luastring"--(
-- DO NOT REMOVE THE ABOVE LINE. It is used to load this file as a C++ string.
-- There is a matching delimiter at the bottom of the file.

--[[
Copyright (c) 2006-2024 LOVE De        -- Pump events

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
--]]

local love = require("love")function love.createhandlers()
    -- Standard callback handlers.
    love.handlers = setmetatable({
        keypressed = function(b, s, r)
            if love.keypressed then return love.keypressed(b, s, r) end
        end,
        keyreleased = function(b, s)
            if love.keyreleased then return love.keyreleased(b, s) end
        end,
        textinput = function(t)
            if love.textinput then return love.textinput(t) end
        end,
        textedited = function(t, s, l)
            if love.textedited then return love.textedited(t, s, l) end
        end,
        mousemoved = function(x, y, dx, dy, t)
            if love.mousemoved then return love.mousemoved(x, y, dx, dy, t) end
        end,
        mousepressed = function(x, y, b, t, c)
            if love.mousepressed then return love.mousepressed(x, y, b, t, c) end
        end,
        mousereleased = function(x, y, b, t, c)
            if love.mousereleased then return love.mousereleased(x, y, b, t, c) end
        end,
        wheelmoved = function(x, y, px, py, dir)
            if love.wheelmoved then return love.wheelmoved(x, y, px, py, dir) end
        end,
        touchpressed = function(id, x, y, dx, dy, p, t, m)
            if love.touchpressed then return love.touchpressed(id, x, y, dx, dy, p, t, m) end
        end,
        touchreleased = function(id, x, y, dx, dy, p, t, m)
            if love.touchreleased then return love.touchreleased(id, x, y, dx, dy, p, t, m) end
        end,
        touchmoved = function(id, x, y, dx, dy, p, t, m)
            if love.touchmoved then return love.touchmoved(id, x, y, dx, dy, p, t, m) end
        end,
        joystickpressed = function(j, b)
            if love.joystickpressed then return love.joystickpressed(j, b) end
        end,
        joystickreleased = function(j, b)
            if love.joystickreleased then return love.joystickreleased(j, b) end
        end,
        joystickaxis = function(j, a, v)
            if love.joystickaxis then return love.joystickaxis(j, a, v) end
        end,
        joystickhat = function(j, h, v)
            if love.joystickhat then return love.joystickhat(j, h, v) end
        end,
        gamepadpressed = function(j, b)
            if love.gamepadpressed then return love.gamepadpressed(j, b) end
        end,
        gamepadreleased = function(j, b)
            if love.gamepadreleased then return love.gamepadreleased(j, b) end
        end,
        gamepadaxis = function(j, a, v)
            if love.gamepadaxis then return love.gamepadaxis(j, a, v) end
        end,
        joystickadded = function(j)
            if love.joystickadded then return love.joystickadded(j) end
        end,
        joystickremoved = function(j)
            if love.joystickremoved then return love.joystickremoved(j) end
        end,
        joysticksensorupdated = function(j, sensorType, x, y, z)
            if love.joysticksensorupdated then return love.joysticksensorupdated(j, sensorType, x, y, z) end
        end,
        focus = function(f)
            if love.focus then return love.focus(f) end
        end,
        mousefocus = function(f)
            if love.mousefocus then return love.mousefocus(f) end
        end,
        visible = function(v)
            if love.visible then return love.visible(v) end
        end,
        exposed = function()
            if love.exposed then return love.exposed() end
        end,
        occluded = function()
            if love.occluded then return love.occluded() end
        end,
        quit = function()
            return
        end,
        threaderror = function(t, err)
            if love.threaderror then return love.threaderror(t, err) end
        end,
        resize = function(w, h)
            if love.resize then return love.resize(w, h) end
        end,
        filedropped = function(f, x, y)
            if love.filedropped then return love.filedropped(f, x, y) end
        end,
        directorydropped = function(dir, x, y)
            if love.directorydropped then return love.directorydropped(dir, x, y) end
        end,
        dropbegan = function()
            if love.dropbegan then return love.dropbegan() end
        end,
        dropmoved = function(x, y)
            if love.dropmoved then return love.dropmoved(x, y) end
        end,
        dropcompleted = function(x, y)
            if love.dropcompleted then return love.dropcompleted(x, y) end
        end,
        lowmemory = function()
            if love.lowmemory then love.lowmemory() end
            collectgarbage()
            collectgarbage()
        end,
        displayrotated = function(display, orient)
            if love.displayrotated then return love.displayrotated(display, orient) end
        end,
        localechanged = function()
            if love.localechanged then return love.localechanged() end
        end,
        audiodisconnected = function(sources)
            if not love.audiodisconnected or not love.audiodisconnected(sources) then
                love.audio.setPlaybackDevice()
            end
        end,
        sensorupdated = function(sensorType, x, y, z)
            if love.sensorupdated then return love.sensorupdated(sensorType, x, y, z) end
        end,
    }, {
        __index = function(self, name)
            error("Unknown event: " .. name)
        end,
    })
end

-----------------------------------------------------------
-- Default callbacks.
-----------------------------------------------------------

---Gets the stereoscopic 3D value of the 3D slide on Nintendo 3DS
---@param screen string The current screen
---@return number | nil depth The stereoscopic 3D value (0.0 - 1.0)
local function get_stereoscopic_depth(screen)
    if love._console ~= "3DS" then return end
    local depth = love.graphics.getDepth()
    return screen ~= "bottom" and (screen == "left" and depth or -depth) or 0
end

-- Helper function for stereoscopic depth (missing in original)
local function get_stereoscopic_depth(display_name)
    -- For now, return 0 depth for all displays
    -- This can be enhanced later for 3D displays
    return 0
end

function love.run()
    print("DEBUG: love.run() starting...")
    
    if love.load then 
        print("DEBUG: Calling love.load()...")
        love.load(love.parsedGameArguments, love.rawGameArguments) 
        print("DEBUG: love.load() completed")
    end

    -- We don't want the first frame's dt to include time taken by love.load.
    if love.timer then love.timer.step() end

    print("DEBUG: Starting main loop function...")
    
    -- Test error trigger counter for runtime testing
    local frame_count = 0
    local test_error_triggered = false
    
    -- Main loop time.
    return function()
        -- Process events.
        if love.event then
            print("DEBUG: About to pump events (frame " .. frame_count .. ")")
            love.event.pump()
            print("DEBUG: Event pump completed, about to poll events")
            for name, a, b, c, d, e, f, g, h in love.event.poll() do
                print("DEBUG: Got event: " .. tostring(name))
                if name == "quit" then
                    if not love.quit or not love.quit() then
                        return a or 0, b
                    end
                end
                love.handlers[name](a, b, c, d, e, f, g, h)
            end
            print("DEBUG: Event polling completed")
        end

        -- Update dt, as we'll be passing it to update
        local dt = love.timer and love.timer.step() or 0
        
        -- Frame counter for debugging
        frame_count = frame_count + 1
        print("DEBUG: Starting frame " .. frame_count .. " with dt=" .. tostring(dt))
        if frame_count == 10 or frame_count == 30 or frame_count == 50 then
            print("Frame counter: " .. frame_count .. " (console: " .. tostring(love._console_name) .. ")")
        end

        -- Call update and draw
        print("DEBUG: About to call love.update()")
        if love.update then love.update(dt) end -- will pass 0 if love.timer is disabled
        print("DEBUG: love.update() completed")

        if love.graphics and love.graphics.isActive() then
            print("DEBUG: About to start graphics rendering")
            local display_count = love.window.getDisplayCount()
            for display_index = 1, display_count do
                local display_name = love.window.getDisplayName(display_index)
                love.graphics.setActiveScreen(display_name)

                love.graphics.origin()

                love.graphics.clear(love.graphics.getBackgroundColor())
                local stereoscopic_depth = get_stereoscopic_depth(display_name)

                if love.draw then love.draw(display_name, stereoscopic_depth) end
                love.graphics.copyCurrentScanBuffer()
            end
            if love._debug_build then print("DEBUG: About to present graphics") end
            love.graphics.present()
            if love._debug_build then print("DEBUG: Graphics present completed") end
        end
        if love._debug_build then print("DEBUG: About to sleep") end
        if love.timer then love.timer.sleep(0.001) end
        if love._debug_build then print("DEBUG: Sleep completed, frame " .. frame_count .. " finished") end
    end
end

local debug, print, tostring, error = debug, print, tostring, error

function love.threaderror(t, err)
    error("Thread error (" .. tostring(t) .. ")\n\n" .. err, 0)
end

local utf8 = require("utf8")

local function error_printer(msg, layer)
    -- Get the full traceback immediately
    local trace = debug.traceback("Error: " .. tostring(msg), 1 + (layer or 1))
    
    -- Add Wii U specific logging for error_printer
    if love._console_name == "cafe" and love._debug_build then
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("=== ERROR_PRINTER DETAILED LOG ===\n")
            logFile:write("Raw message: " .. tostring(msg) .. "\n")
            logFile:write("Layer: " .. tostring(layer) .. "\n")
            logFile:write("Full trace with context:\n" .. tostring(trace) .. "\n")
            logFile:write("================================\n")
            logFile:flush()
            logFile:close()
        end
    end
    
    -- Print to console (this may or may not work depending on system state)
    print(trace:gsub("\n[^\n]+$", ""))
    
    if love._console_name == "cafe" and love._debug_build then
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("error_printer print() call completed\n")
            logFile:flush()
            logFile:close()
        end
    end
end

function love.errhand(msg)
    msg = tostring(msg)

    -- PRIORITY: Immediately log the full error to file before doing ANYTHING else
    -- Multiple fallback attempts to ensure the error gets logged somewhere
    local logged = false
    
    if love._console_name == "cafe" and love._debug_build then
        -- Try primary log location
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("\n" .. string.rep("=", 80) .. "\n")
            logFile:write("CRITICAL ERROR DETECTED AT " .. tostring(os.date()) .. "\n")
            logFile:write(string.rep("=", 80) .. "\n")
            logFile:write("ERROR MESSAGE:\n" .. tostring(msg) .. "\n")
            logFile:write(string.rep("-", 40) .. "\n")
            
            -- Get and log the full traceback immediately
            local trace = debug.traceback()
            logFile:write("FULL TRACEBACK:\n" .. tostring(trace) .. "\n")
            logFile:write(string.rep("-", 40) .. "\n")
            
            -- Log system state
            logFile:write("SYSTEM STATE:\n")
            logFile:write("- Window exists: " .. tostring(love.window ~= nil) .. "\n")
            logFile:write("- Graphics exists: " .. tostring(love.graphics ~= nil) .. "\n")
            logFile:write("- Event exists: " .. tostring(love.event ~= nil) .. "\n")
            
            if love.window then
                logFile:write("- Window open: " .. tostring(love.window.isOpen()) .. "\n")
            end
            if love.graphics then
                logFile:write("- Graphics created: " .. tostring(love.graphics.isCreated()) .. "\n")
                logFile:write("- Graphics active: " .. tostring(love.graphics.isActive()) .. "\n")
            end
            
            logFile:write(string.rep("=", 80) .. "\n\n")
            logFile:flush()
            logFile:close()
            logged = true
        end
        
        -- If primary logging failed, try backup location
    if love._debug_build and not logged then
            local backupLog = io.open("fs:/vol/save/error_backup.log", "a") 
            if backupLog then
                backupLog:write("BACKUP ERROR LOG - PRIMARY FAILED\n")
                backupLog:write("Error: " .. tostring(msg) .. "\n")
                backupLog:write("Trace: " .. tostring(debug.traceback()) .. "\n")
                backupLog:flush()
                backupLog:close()
                logged = true
            end
        end
        
        -- If file logging completely failed, at least try to print to console
    if love._debug_build and not logged then
            print("CRITICAL ERROR LOGGING FAILED")
            print("Error: " .. tostring(msg))
            print("Trace: " .. tostring(debug.traceback()))
        end
    end

    error_printer(msg, 2)

    if love._console_name == "cafe" and love._debug_build then
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("error_printer() completed\n")
            logFile:write("Checking modules: window=" .. tostring(love.window ~= nil) .. 
                         ", graphics=" .. tostring(love.graphics ~= nil) .. 
                         ", event=" .. tostring(love.event ~= nil) .. "\n")
            logFile:flush()
            logFile:close()
        end
    end

    if not love.window or not love.graphics or not love.event then
    if love._console_name == "cafe" and love._debug_build then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("CRITICAL: Essential modules missing - cannot display error screen\n")
                logFile:write("Window: " .. tostring(love.window ~= nil) .. "\n")
                logFile:write("Graphics: " .. tostring(love.graphics ~= nil) .. "\n") 
                logFile:write("Event: " .. tostring(love.event ~= nil) .. "\n")
                logFile:write("Error handling will terminate here, but error is logged above.\n")
                logFile:flush()
                logFile:close()
            end
        end
        return
    end

    -- Wrap the entire graphics error display in a protected call
    local success, errorScreenFunction = pcall(function()
        return love.errhand_create_error_screen(msg)
    end)
    
    if success and errorScreenFunction then
        return errorScreenFunction
    else
        -- Graphics error display failed, log this and return
        if love._console_name == "cafe" then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("CRITICAL: Error screen creation failed\n")
                logFile:write("Failure reason: " .. tostring(errorScreenFunction or "unknown") .. "\n")
                logFile:write("Error handling will terminate, but full error is logged above.\n")
                logFile:flush()
                logFile:close()
            end
        end
        return
    end
end

function love.errhand_create_error_screen(msg)

    if not love.graphics.isCreated() or not love.window.isOpen() then
        if love._console_name == "cafe" and love._debug_build then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("Graphics not created or window not open, trying to set mode\n")
                logFile:flush()
                logFile:close()
            end
        end
        
        local success, status = pcall(love.window.setMode, 800, 600)
        if not success or not status then
            if love._console_name == "cafe" and love._debug_build then
                local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
                if logFile then
                    logFile:write("EARLY RETURN: Failed to set window mode\n")
                    logFile:flush()
                    logFile:close()
                end
            end
            return
        end
    end

    if love._console_name == "cafe" and love._debug_build then
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("About to reset graphics and continue with error display\n")
            logFile:flush()
            logFile:close()
        end
    end

    -- Reset state.
    if love.mouse then
        love.mouse.setVisible(true)
        love.mouse.setGrabbed(false)
        love.mouse.setRelativeMode(false)
        if love.mouse.isCursorSupported() then
            love.mouse.setCursor()
        end
    end
    if love.joystick then
        -- Stop all joystick vibrations.
        for i, v in ipairs(love.joystick.getJoysticks()) do
            v:setVibration()
        end
    end
    if love.audio then love.audio.stop() end

    love.graphics.reset()
    
    -- Use larger font for Wii U since it's displayed on TV and needs to be readable
    local font_size = 15
    if love._console_name == "cafe" and love._debug_build then
        font_size = 32  -- Much larger for TV display
        
        local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
        if logFile then
            logFile:write("Using larger font size for Wii U: " .. font_size .. "\n")
            logFile:flush()
            logFile:close()
        end
    end
    
    love.graphics.setFont(love.graphics.newFont(font_size))

    love.graphics.setColor(1, 1, 1)

    local trace = debug.traceback()

    love.graphics.origin()

    local sanitizedmsg = {}
    for char in msg:gmatch(utf8.charpattern) do
        table.insert(sanitizedmsg, char)
    end
    local sanitizedmsg_str = table.concat(sanitizedmsg)

    local err = {}

    table.insert(err, "Error\n")
    table.insert(err, sanitizedmsg_str)

    if #sanitizedmsg_str ~= #msg then
        table.insert(err, "Invalid UTF-8 string in error message.")
    end

    table.insert(err, "\n")

    for l in trace:gmatch("(.-)\n") do
        if not l:match("boot.lua") then
            l = l:gsub("stack traceback:", "Traceback\n")
            table.insert(err, l)
        end
    end

    local p = table.concat(err, "\n")

    p = p:gsub("\t", "")
    p = p:gsub("%[string \"(.-)\"%]", "%1")

    local function draw()
    if love._console_name == "cafe" and love._debug_build then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("=== ERROR SCREEN DRAW() CALLED ===\n")
                logFile:write("Graphics active: " .. tostring(love.graphics.isActive()) .. "\n")
                logFile:flush()
                logFile:close()
            end
        end
        
        if not love.graphics.isActive() then 
            if love._console_name == "cafe" and love._debug_build then
                local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
                if logFile then
                    logFile:write("EARLY RETURN: Graphics not active\n")
                    logFile:flush()
                    logFile:close()
                end
            end
            return 
        end
        
        local pos = 70
        
    if love._console_name == "cafe" and love._debug_build then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("About to draw blue error screen\n")
                logFile:write("Error text length: " .. string.len(p) .. "\n")
                logFile:write("Error text preview: " .. string.sub(p, 1, 100) .. "\n")
                logFile:write("Screen dimensions: " .. love.graphics.getWidth() .. "x" .. love.graphics.getHeight() .. "\n")
                logFile:flush()
                logFile:close()
            end
        end
        
        love.graphics.clear(89 / 255, 157 / 255, 220 / 255)
        love.graphics.printf(p, pos, pos, love.graphics.getWidth() - pos)
        love.graphics.present()
        
    if love._console_name == "cafe" and love._debug_build then
            local logFile = io.open("fs:/vol/external01/simple_debug.log", "a")
            if logFile then
                logFile:write("Blue error screen draw completed\n")
                logFile:flush()
                logFile:close()
            end
        end
    end

    local fullErrorText = p
    local function copyToClipboard()
        if not love.system then return end
        love.system.setClipboardText(fullErrorText)
        p = p .. "\nCopied to clipboard!"
    end

    if love.system then
        p = p .. "\n\nPress Ctrl+C or tap to copy this error"
    end

    return function()
        love.event.pump()

        for e, a, b, c in love.event.poll() do
            if e == "quit" then
                return 1
            elseif e == "keypressed" and a == "escape" then
                return 1
            elseif e == "keypressed" and a == "c" and love.keyboard.isDown("lctrl", "rctrl") then
                copyToClipboard()
            elseif e == "touchpressed" then
                local name = love.window.getTitle()
                if #name == 0 or name == "Untitled" then name = "Game" end
                local buttons = { "OK", "Cancel" }
                if love.system then
                    buttons[3] = "Copy to clipboard"
                end
                local pressed = love.window.showMessageBox("Quit " .. name .. "?", "", buttons)
                if pressed == 1 then
                    return 1
                elseif pressed == 3 then
                    copyToClipboard()
                end
            end
        end

        draw()

        if love.timer then
            love.timer.sleep(0.1)
        end
    end
end

-- DO NOT REMOVE THE NEXT LINE. It is used to load this file as a C++ string.
--)luastring"--"
