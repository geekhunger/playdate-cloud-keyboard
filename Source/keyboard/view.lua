Keyboard._View = {}
local View = Keyboard._View
View.__index = View
local gfx = playdate.graphics
local images = {}
local function image(name)
    if not images[name] then
        images[name] = assert(gfx.image.new('keyboard/art/'..name), 'Missing keyboard asset: '..name)
    end
    return images[name]
end
local function round(n) return math.floor(n+0.5) end
local function clamp(n) return math.max(0, math.min(1,n)) end
local function smooth(n) n=clamp(n); return n*n*(3-2*n) end
local directions = {'up','right','down','left','up'}
local positions = {{105,72},{236,115},{105,160},{8,115},{105,72}}
local phases = {0,1.4,2.8,4.2,0}

local function glyphWidth(code)
    if code<32 or code>126 then code=63 end
    return Keyboard._glyphWidths[code-31], code
end

function View.textWidth(text)
    local width=0
    for i=1,#text do width=width+glyphWidth(text:byte(i))+1 end
    return math.max(0,width-1)
end

function View.drawText(text,x,y,white)
    gfx.setImageDrawMode(white and gfx.kDrawModeFillWhite or gfx.kDrawModeCopy)
    for i=1,#text do
        local width,code=glyphWidth(text:byte(i))
        image(string.format('glyph-%d',code)):draw(round(x),round(y))
        x=x+width+1
    end
    gfx.setImageDrawMode(gfx.kDrawModeCopy)
end

function View.new(options)
    return setmetatable({field=gfx.image.new(400,240), group=nil, p=0,
        lastTime=nil, index=0, upper=false, idle=options.idleAnimation~=false},View)
end

function View:reset()
    self.group,self.p,self.lastTime,self.index=nil,0,nil,0
end

function View:advance(model,timestamp)
    local dt=self.lastTime and math.max(0,math.min(100,timestamp-self.lastTime)) or 0
    self.lastTime=timestamp
    if not self.group and model.group then self.group=model.group; self.p=0 end
    if self.group then
        if self.group == model.group then
            self.p=math.min(1,self.p+dt/300)
            self.index=model.pending and model.index or 0
            self.upper=model.upper
        else
            self.p=math.max(0,self.p-dt/180)
            if self.p==0 then
                self.group=model.group
                self.index=model.pending and model.index or 0
                self.upper=model.upper
            end
        end
    end
end

function View:drawField(model,timestamp,alternate)
    gfx.pushContext(self.field)
    gfx.clear(gfx.kColorClear)
    local group=self.group
    local sideGroup=group==2 or group==4
    local movement=sideGroup and smooth(self.p/0.65) or 0
    local reveal=sideGroup and clamp((self.p-0.65)/0.35) or self.p
    local upper=model.upper and 1 or 0
    local dx=sideGroup and round((group==4 and 164 or -164)*movement) or 0
    local function pos(g)
        local x,y=positions[g][1],positions[g][2]
        if sideGroup then
            if g==group then x=round(x+(105-x)*movement)
            elseif (group==4 and g==2) or (group==2 and g==4) then
                x=x+round((group==4 and 154 or -154)*movement)
            end
        end
        local bob=self.idle and math.sin(timestamp/700+phases[g])*(1-smooth(self.p)) or 0
        y=y-3+round(bob)
        return x,y
    end
    local groups=alternate and {5} or {1,2,3,4}
    -- Ground shadows keep their vertical position as the cloud tiles float.
    for _,g in ipairs(groups) do
        local x=pos(g)
        local frame=(g==group and sideGroup) and round(movement*10) or 0
        local h=(g==2 or g==4) and 38 or 36
        image(string.format('g%d-shadow-%d',g,frame)):draw(x,positions[g][2]+h+1)
    end
    if alternate then
        image('g4-shadow-0'):draw(8,154)
        image('g2-shadow-0'):draw(236,154)
    end
    for _,g in ipairs(groups) do
        if g~=group then
            local x,y=pos(g)
            image(string.format('g%d-%d-idle-0',g,upper)):draw(x,y)
        end
    end
    if alternate then
        local lx,ly=pos(4)
        local rx,ry=pos(2)
        image('cursor-left'):draw(lx,ly)
        image('cursor-right'):draw(rx,ry)
    end
    image('dpad-'..(group and directions[group] or 'none')):draw(180+dx,114)
    if group and ((alternate and group==5) or (not alternate and group~=5)) then
        local x,y=pos(group)
        local height=(group==2 or group==4) and 38 or 36
        local opened=not sideGroup or movement>=1
        local index=self.index
        local side=index>0 and (index<=#Keyboard._Model.groups[group]/2 and 'left' or 'right') or nil
        local groupUpper=self.upper and 1 or 0
        if opened and reveal>0 then
            local eased=1-(1-reveal)^3
            local bx=round(x+18-48*eased)
            local ax=round(x+190-18+48*eased)
            local cy=y+height/2
            gfx.setColor(gfx.kColorBlack)
            if side=='left' then gfx.fillRect(bx+12,cy-1,math.max(0,x+3-bx-12),3)
            elseif side=='right' then gfx.fillRect(x+188,cy-1,math.max(0,ax-12-x-188),3) end
            image('button-B-'..(side=='left' and 1 or 0)):draw(bx-16,cy-16)
            image('button-A-'..(side=='right' and 1 or 0)):draw(ax-16,cy-16)
            image(string.format('g%d-%d-open-%d',group,groupUpper,index)):draw(x,y)
        else
            local frame=sideGroup and round(movement*10) or 0
            image(string.format('g%d-%d-idle-%d',group,groupUpper,frame)):draw(x,y)
        end
    end
    gfx.popContext()
end

function View:draw(model,timestamp)
    gfx.pushContext()
    gfx.setDrawOffset(0,0)
    gfx.setClipRect(0,0,400,240)
    gfx.setImageDrawMode(gfx.kDrawModeCopy)
    self:advance(model,timestamp)
    local alternate=model.alternate
    local scale=1
    if model.layerAt then
        local progress=clamp((timestamp-model.layerAt)/460)
        if progress<0.5 then
            alternate=model.layerFrom
            scale=1-smooth(progress*2)
        else scale=smooth((progress-0.5)*2) end
    end
    local phase=math.floor(timestamp/180)%4
    image(string.format('background-%d-%d-%d',model.upper and 1 or 0,alternate and 1 or 0,phase)):draw(0,0)
    self:drawField(model,timestamp,alternate)
    if scale==1 then self.field:draw(0,0)
    elseif scale>0.01 then self.field:drawScaled(round(200*(1-scale)),round(134*(1-scale)),scale) end

    for _,button in ipairs({{'a','A',367},{'b','B',11}}) do
        local key,label,x=button[1],button[2],button[3]
        local press
        if key=='a' then press=model.caseHold else press=model.toolsHold end
        local filled=0
        if press then
            filled=not press.hold and 21 or math.floor(clamp((timestamp-press.at)/350)*21)
        else
            local flash=model.buttonFeedback[key]
            if flash and timestamp>=flash and timestamp-flash<130 then filled=21 end
        end
        image(string.format('footer-%s-%d',label,filled)):draw(x,207)
    end

    -- Native bitmap glyphs, clipped horizontal scrolling and an inverting block.
    local prefix=model.text:sub(1,model.cursor)..(model.pending or '')
    local suffix=model.text:sub(model.cursor+1)
    local width=View.textWidth(prefix)
    local offset=math.max(0,width-337)
    gfx.setClipRect(22,34,354,23)
    View.drawText(prefix..suffix,23-offset,38)
    if math.floor(timestamp/420)%2==0 then
        local nextChar=suffix:sub(1,1)
        local cursorWidth=math.max(9,View.textWidth(nextChar))
        local x=23+width-offset
        gfx.setColor(gfx.kColorBlack)
        gfx.fillRect(x,37,cursorWidth,18)
        if #nextChar>0 then View.drawText(nextChar,x,38,true) end
    end
    gfx.popContext()
end
