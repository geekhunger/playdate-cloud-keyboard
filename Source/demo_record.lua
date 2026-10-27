-- Deterministic recordings of the real SDK renderer. Simulator-only.
CloudKeyboardRecorder={}
local Recorder=CloudKeyboardRecorder
Recorder.__index=Recorder
local function scenario(kind)
    local events={}
    local function event(t,method,value) events[#events+1]={t,method,value} end
    local function button(t,key,duration)
        event(t,'press',key);event(t+(duration or 70),'release',key)
    end
    local t=650
    if kind=='typing' then
        for ch in ('hello'):gmatch('.') do
            local g,i
            for group,chars in ipairs(Keyboard._Model.groups) do
                local at=chars:find(ch,1,true)
                if at then g,i=group,at;break end
            end
            event(t,'direction',({'up','right','down','left'})[g]);t=t+340
            local key=i<=4 and 'b' or 'a'
            for n=1,(i-1)%4+1 do button(t,key);t=t+150 end
            t=t+900
        end
    elseif kind=='crank' then
        event(t,'direction','up');t=t+350
        for i=1,8 do event(t,'crank',1);t=t+160 end
        button(t,'b',400);t=t+1000
        event(t,'direction','right');t=t+350
        event(t,'crank',1);t=t+1100
    else
        button(t,'b',420);t=t+1100
        for ch in ('2026'):gmatch('.') do
            local i=assert(('1234567890'):find(ch,1,true))
            event(t,'direction','up');t=t+340
            for n=1,(i-1)%5+1 do button(t,i<=5 and 'b' or 'a');t=t+145 end
            t=t+900
        end
        event(t,'direction','left');t=t+600;event(t,'directionRelease','left');t=t+500
        event(t,'direction','right');t=t+600;event(t,'directionRelease','right');t=t+500
    end
    table.sort(events,function(a,b)return a[1]<b[1]end)
    return {events=events,duration=t+900,kind=kind}
end
function Recorder.new(keyboard,path)
    keyboard:close()
    local self=setmetatable({path=path,scenes={scenario('typing'),scenario('crank'),scenario('tools')},scene=0},Recorder)
    self:nextScene()
    return self
end
function Recorder:nextScene()
    self.scene=self.scene+1
    if not self.scenes[self.scene] then return false end
    self.frame,self.time,self.eventIndex=0,0,1
    self.model=Keyboard._Model.new(self.scenes[self.scene].kind=='tools' and 'ship it in ' or '')
    self.view=Keyboard._View.new({idleAnimation=true})
    return true
end
function Recorder:update()
    local current=self.scenes[self.scene]
    if not current then return true end
    local t=math.floor(self.frame*1000/25)
    while current.events[self.eventIndex] and current.events[self.eventIndex][1]<=t do
        local e=current.events[self.eventIndex]
        self.model:update(e[1])
        self.model[e[2]](self.model,e[3],e[1])
        self.eventIndex=self.eventIndex+1
    end
    self.model:update(t);self.view:draw(self.model,t)
    playdate.simulator.writeToFile(playdate.graphics.getWorkingImage(),string.format('%s/%s-%04d.png',self.path,current.kind,self.frame))
    self.frame=self.frame+1
    if t>=current.duration and not self:nextScene() then
        print('CLOUD KEYBOARD RECORDING COMPLETE');return true
    end
    return false
end
