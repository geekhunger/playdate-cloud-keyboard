-- Example host: copy keyboard.lua and keyboard/ into another project's Source/.
import 'CoreLibs/graphics'
import 'keyboard'

local pd = playdate
local gfx = pd.graphics
pd.display.setRefreshRate(30) -- Host-owned; the library does not change refresh rate.
local savedText = ''
local result = nil
local keyboard = Keyboard.new({
    onDone = function(text) savedText=text; result='Done' end,
    onCancel = function() result='Cancelled' end,
})
local function openKeyboard(text)
    local ok,err=keyboard:show(text)
    assert(ok,err)
end
function pd.AButtonDown()
    if not keyboard:isVisible() then openKeyboard(savedText) end
end
function pd.BButtonDown()
    if not keyboard:isVisible() then openKeyboard('') end
end
local function drawResult()
    gfx.clear(gfx.kColorWhite)
    gfx.drawText(result or 'Keyboard',20,22)
    gfx.drawTextInRect(savedText,20,55,360,130)
    gfx.drawText('A Edit     B New input',20,212)
end
openKeyboard('')
local recording=nil
if pd.isSimulator then
    for _,arg in ipairs(pd.argv or {}) do recording=arg:match('^record=(.+)$') or recording end
end
if recording then import 'demo_record' end
local recorder=recording and CloudKeyboardRecorder.new(keyboard,recording) or nil
function pd.update()
    if recorder then
        if recorder:update() then recorder=nil;openKeyboard('') end
        return
    end
    if keyboard:isVisible() then keyboard:update();keyboard:draw() else drawResult() end
end
