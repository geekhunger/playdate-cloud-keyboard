-- Public API. Copy this file and the keyboard/ folder into any Playdate Source/.
import 'CoreLibs/graphics'
Keyboard = Keyboard or {}
import 'keyboard/art_metrics'
import 'keyboard/model'
import 'keyboard/view'

local Instance = {}
Instance.__index = Instance

function Keyboard.new(options)
    options=options or {}
    local self=setmetatable({options=options,visible=false,events={},crank=0},Instance)
    self.crankSensitivity=options.crankSensitivity or 1.496
    assert(type(self.crankSensitivity)=='number' and self.crankSensitivity>0,'crankSensitivity must be positive')
    self.view=Keyboard._View.new(options)
    self.handler={}
    local function queue(kind,value)
        if self.visible then self.events[#self.events+1]={kind,value,playdate.getCurrentTimeMilliseconds()} end
    end
    for _,direction in ipairs({'up','right','down','left'}) do
        local key=direction
        self.handler[key..'ButtonDown']=function() queue('direction',key) end
        self.handler[key..'ButtonUp']=function() queue('directionRelease',key) end
    end
    self.handler.AButtonDown=function() queue('press','a') end
    self.handler.AButtonUp=function() queue('release','a') end
    self.handler.BButtonDown=function() queue('press','b') end
    self.handler.BButtonUp=function() queue('release','b') end
    self.handler.cranked=function(change) queue('cranked',change) end
    return self
end

function Instance:show(text,cursor)
    if self.visible then return false,'Keyboard is already visible' end
    if Keyboard._active then return false,'Another keyboard owns the input focus' end
    text=text or ''
    if type(text)~='string' or text:find('[^ -~]') then
        return false,'This keyboard currently supports printable ASCII text'
    end
    if cursor~=nil and (type(cursor)~='number' or cursor%1~=0 or cursor<0 or cursor>#text) then
        return false,'Cursor must be an integer from 0 to the text length'
    end
    local limit=self.options.maxLength
    if limit~=nil and (type(limit)~='number' or limit%1~=0 or limit<0) then
        return false,'maxLength must be a nonnegative integer'
    end
    if limit and #text>limit then return false,'Initial text exceeds maxLength' end
    local menu=playdate.getSystemMenu()
    if #menu:getMenuItems()>0 then return false,'Keyboard needs three free System Menu slots' end
    self.model=Keyboard._Model.new(text,cursor)
    self.model.maxLength=limit
    self.initialText=text
    self.events,self.crank={},0
    self.view:reset()
    local clear,clearErr=menu:addMenuItem('Clear',function() self:clear() end)
    if not clear then return false,clearErr end
    local done,err=menu:addMenuItem('Done',function() self:done() end)
    if not done then menu:removeMenuItem(clear); return false,err end
    local cancel,err2=menu:addMenuItem('Cancel',function() self:cancel() end)
    if not cancel then menu:removeMenuItem(done); menu:removeMenuItem(clear); return false,err2 end
    self.menu,self.clearItem,self.doneItem,self.cancelItem=menu,clear,done,cancel
    self.visible=true
    Keyboard._active=self
    playdate.inputHandlers.push(self.handler,true)
    return true
end

function Instance:isVisible() return self.visible end
function Instance:getText() return self.model and self.model.text or '' end

function Instance:clear()
    if not self.visible then return end
    local upper,alternate=self.model.upper,self.model.alternate
    self.model=Keyboard._Model.new('')
    self.model.maxLength=self.options.maxLength
    self.model.upper,self.model.alternate=upper,alternate
    self.events,self.crank={},0
    self.view:reset()
end

function Instance:close()
    if not self.visible then return end
    self.visible=false
    self.menu:removeMenuItem(self.clearItem)
    self.menu:removeMenuItem(self.doneItem)
    self.menu:removeMenuItem(self.cancelItem)
    self.clearItem,self.doneItem,self.cancelItem=nil,nil,nil
    playdate.inputHandlers.pop()
    self.events,self.crank={},0
    Keyboard._active=nil
end

function Instance:done()
    if not self.visible then return end
    self:processEvents()
    self.model:commit(false)
    local text=self.model.text
    self:close()
    if self.options.onDone then self.options.onDone(text,self) end
    return text
end

function Instance:cancel()
    if not self.visible then return end
    local original=self.initialText
    self.model=Keyboard._Model.new(original)
    self:close()
    if self.options.onCancel then self.options.onCancel(self) end
end

function Instance:processEvents()
    for _,event in ipairs(self.events) do
        local kind,value,timestamp=event[1],event[2],event[3]
        if kind=='cranked' then
            if self.model.group then
                local tick=360/#Keyboard._Model.groups[self.model.group]/self.crankSensitivity
                self.crank=self.crank+value
                local steps=self.crank>=0 and math.floor(self.crank/tick) or math.ceil(self.crank/tick)
                self.crank=self.crank-steps*tick
                self.model:crank(steps,timestamp)
            else self.crank=0 end
        else
            self.model[kind](self.model,value,timestamp)
            if kind=='direction' then self.crank=0 end
        end
    end
    self.events={}
end

function Instance:update()
    if not self.visible then return false end
    self:processEvents()
    self.model:update(playdate.getCurrentTimeMilliseconds())
    return true
end

function Instance:draw()
    if self.visible then self.view:draw(self.model,playdate.getCurrentTimeMilliseconds()) end
end
