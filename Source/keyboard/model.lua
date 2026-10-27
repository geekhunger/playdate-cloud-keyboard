-- Input state only: no drawing, filesystem, display or global event changes.
Keyboard._Model = {}
local Model = Keyboard._Model
Model.__index = Model
Model.groups = {"abcdefgh", "ijklmnop", "qrstuvwx", "yz,.!?-_", "1234567890"}

function Model.new(text, cursor)
    return setmetatable({text=text or "", cursor=cursor or #(text or ""),
        upper=false, alternate=false, group=nil, index=0, pending=nil,
        pendingSide=nil, groupAt=0, pendingAt=0, caseHold=nil, toolsHold=nil,
        layerAt=nil, layerFrom=false, buttonFeedback={}, cursorHold=nil,
        lastDeleteTapAt=nil}, Model)
end

function Model:insert(character)
    if self.maxLength and #self.text+#character>self.maxLength then return false end
    self.lastDeleteTapAt=nil
    self.text = self.text:sub(1,self.cursor) .. character .. self.text:sub(self.cursor+1)
    self.cursor = self.cursor + #character
    return true
end

function Model:commit(keepGroup)
    if self.pending then self:insert(self.pending) end
    self.pending, self.pendingSide, self.index = nil, nil, 0
    if not keepGroup then self.group = nil end
end

function Model:delete()
    if self.cursor > 0 then
        self.text = self.text:sub(1,self.cursor-1) .. self.text:sub(self.cursor+1)
        self.cursor = self.cursor - 1
    end
end

function Model:direction(direction, timestamp)
    self.lastDeleteTapAt=nil
    self.cursorHold=nil
    if self.layerAt and timestamp-self.layerAt < 460 then return end
    if self.alternate then
        if direction == "down" then return end -- deliberately unassigned
        if direction ~= "up" then
            if not self.group then
                self.cursor = math.max(0, math.min(#self.text,
                    self.cursor + (direction == "left" and -1 or 1)))
                self.cursorHold={direction=direction,nextAt=timestamp+250}
            end
            return
        end
    end
    self:commit(false)
    self.group = self.alternate and 5 or ({up=1,right=2,down=3,left=4})[direction]
    self.groupAt = timestamp
end

function Model:directionRelease(direction)
    if self.cursorHold and self.cursorHold.direction==direction then self.cursorHold=nil end
end

function Model:tap(side, timestamp)
    if not self.group then return end
    local chars = Model.groups[self.group]
    local half = #chars//2
    if self.pending and self.pendingSide ~= side then self:commit(true) end
    if not self.pending then
        self.index = side == "left" and 1 or half+1
    elseif side == "left" then
        self.index = self.index % half + 1
    else
        self.index = half + 1 + ((self.index-half) % half)
    end
    local char = chars:sub(self.index,self.index)
    self.pending = self.upper and char:upper() or char
    self.pendingSide, self.pendingAt = side, timestamp
end

function Model:crank(steps, timestamp)
    if steps == 0 or not self.group then return end
    local chars = Model.groups[self.group]
    if not self.pending then self.index = steps > 0 and 0 or 1 end
    self.index = ((self.index-1+steps) % #chars)+1
    local char = chars:sub(self.index,self.index)
    self.pending = self.upper and char:upper() or char
    self.pendingSide, self.pendingAt = "crank", timestamp
end

function Model:press(key, timestamp)
    local press = {at=timestamp, group=self.group, handled=false}
    if not self.group then
        if key=='a' then press.hold='case'
        elseif self.lastDeleteTapAt and timestamp-self.lastDeleteTapAt>=0 and timestamp-self.lastDeleteTapAt<=500 then
            press.hold='delete'
        else press.hold='tools' end
    elseif self.pending then press.hold='invert' end
    if key=='a' or self.group then self.lastDeleteTapAt=nil end
    if key == "a" then self.caseHold=press else self.toolsHold=press end
end

function Model:release(key, timestamp)
    local press
    if key == "a" then press=self.caseHold else press=self.toolsHold end
    if press and press.hold and not press.handled and timestamp-press.at >= 350 then
        self:update(timestamp)
    end
    if key == "a" then self.caseHold=nil else self.toolsHold=nil end
    if press and not press.handled then self.buttonFeedback[key]=timestamp end
    if not press or press.handled then return end
    if press.group then
        if self.group == press.group then self:tap(key == "a" and "right" or "left", timestamp) end
    elseif key == "a" then self:insert(" ")
    else self:delete(); self.lastDeleteTapAt=timestamp end
end

function Model:update(timestamp)
    local a, b = self.caseHold, self.toolsHold
    for _,press in pairs({a=a,b=b}) do
        if not press.handled and timestamp-press.at>=350 then
            if press.hold=='invert' and self.group==press.group and self.pending then
                self.pending=self.upper and self.pending:lower() or self.pending:upper()
                self:commit(false)
                press.handled=true
            elseif press.hold=='case' then
                self.upper,press.handled=not self.upper,true
            elseif press.hold=='tools' then
                self.layerFrom,self.layerAt=self.alternate,timestamp
                self.alternate,press.handled=not self.alternate,true
                self.cursorHold=nil
                self.lastDeleteTapAt=nil
                self:commit(false)
            elseif press.hold=='delete' and not self.group then
                self:delete()
                self.lastDeleteTapAt=nil
                press.handled=true
                press.nextDeleteAt=timestamp+100
            end
        end
        if press.hold=='delete' and press.handled and press.nextDeleteAt and not self.group and timestamp>=press.nextDeleteAt then
            local count=math.min(4,math.floor((timestamp-press.nextDeleteAt)/100)+1)
            for i=1,count do self:delete() end
            press.nextDeleteAt=timestamp+100
        end
    end
    local cursorHold=self.cursorHold
    if cursorHold and self.alternate and not self.group and timestamp>=cursorHold.nextAt then
        local steps=math.min(4,math.floor((timestamp-cursorHold.nextAt)/100)+1)
        local delta=cursorHold.direction=='left' and -steps or steps
        self.cursor=math.max(0,math.min(#self.text,self.cursor+delta))
        cursorHold.nextAt=timestamp+100
    end
    -- Never consume an in-flight A/B press just before its release.
    if (a and a.group) or (b and b.group) then return end
    if self.pending and timestamp-self.pendingAt >= 750 then self:commit(false)
    elseif self.group and not self.pending and timestamp-self.groupAt >= 750 then
        self.group = nil
    end
end
