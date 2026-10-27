-- Focused checks for the current changes; no full state matrix.
Keyboard={}
dofile('Source/keyboard/model.lua')
local Model=Keyboard._Model
local count=0
local function eq(a,b,label)
 assert(a==b,(label or '')..': '..tostring(a)..' ~= '..tostring(b)); count=count+1
end
for _,key in ipairs({'a','b'}) do
 for _,upper in ipairs({false,true}) do
  for group,chars in ipairs(Model.groups) do
   for _,index in ipairs({1,#chars//2+1}) do
    local m=Model.new('test ')
    m.upper,m.group=upper,group
    m:tap(index==1 and 'left' or 'right',0)
    local ch=chars:sub(index,index)
    m:press(key,100); m:update(449)
    eq(m.text,'test ','hold not early')
    m:update(450)
    eq(m.text,'test '..(upper and ch:lower() or ch:upper()),key..' temporary case')
    eq(m.upper,upper,'global case preserved')
    eq(m.group,nil,'hold accepts character')
    m:release(key,451)
    eq(m.text,'test '..(upper and ch:lower() or ch:upper()),'release does not type again')
   end
  end
 end
end
for _,direction in ipairs({'left','right'}) do
 local m=Model.new('abcdefghij',5); m.alternate=true
 local delta=direction=='left' and -1 or 1
 m:direction(direction,100); eq(m.cursor,5+delta,'cursor tap')
 m:update(349); eq(m.cursor,5+delta,'cursor repeat delay')
 m:update(350); eq(m.cursor,5+2*delta,'cursor first repeat')
 m:update(450); eq(m.cursor,5+3*delta,'cursor next repeat')
 m:directionRelease(direction); m:update(9000)
 eq(m.cursor,5+3*delta,'release stops repeat')
end
local m=Model.new('abcdefghij')
m:press('b',0);m:release('b',50);eq(m.text,'abcdefghi','first delete tap')
m:press('b',150);m:update(499);eq(m.text,'abcdefghi','second press before hold')
m:update(500);eq(m.text,'abcdefgh','double press hold deletes')
eq(m.alternate,false,'double press hold does not change layer')
m:update(600);eq(m.text,'abcdefg','delete repeats')
m:release('b',610);m:update(5000);eq(m.text,'abcdefg','delete stops on release')
m=Model.new('abcdefghij');m:press('b',0);m:update(350)
eq(m.alternate,true,'direct B hold changes layer');eq(m.text,'abcdefghij','direct hold does not delete')
m:release('b',351)
m=Model.new('abc');m:press('b',0);m:release('a',10)
eq(m.text,'abc','unmatched A release cannot use B press')
-- New Clear/Done/Cancel menu contract, with no host menu mutation.
local clock=0;local stack={};local menu={items={}}
function menu:getMenuItems()return self.items end
function menu:addMenuItem(title,cb)local item={title=title,callback=cb};table.insert(self.items,item);return item end
function menu:removeMenuItem(item)for i,v in ipairs(self.items)do if v==item then table.remove(self.items,i);return end end end
playdate={graphics={},getCurrentTimeMilliseconds=function()return clock end,getSystemMenu=function()return menu end,
 inputHandlers={push=function(h)stack[#stack+1]=h end,pop=function()table.remove(stack)end}}
import=function(path)
 if path=='keyboard/view' then Keyboard._View={new=function()return{reset=function()end}end}
 elseif path=='keyboard/model' or path=='keyboard/art_metrics' then dofile('Source/'..path..'.lua')end
end
dofile('Source/keyboard.lua')
local k=Keyboard.new();local host=menu:addMenuItem('Host',function()end)
eq(k:show('abc'),false,'requires three free menu slots');eq(menu.items[1],host,'host item preserved')
menu:removeMenuItem(host);eq(k:show('abc'),true)
eq(menu.items[1].title,'Clear');eq(menu.items[2].title,'Done');eq(menu.items[3].title,'Cancel')
k.model.upper=true;k.model.alternate=true;k.model.group=5;k.model:tap('right',0)
menu.items[1].callback()
eq(k:getText(),'','Clear removes text and preview');eq(k.model.cursor,0);eq(k.model.group,nil)
eq(k.model.upper,true);eq(k.model.alternate,true);eq(k:isVisible(),true,'Clear keeps keyboard open')
k:cancel();eq(k:getText(),'abc','Cancel still restores original');eq(#menu.items,0);eq(#stack,0)
eq(k:show('abcdefghij',5),true);k.model.alternate=true
k.handler.rightButtonDown();k:update();eq(k.model.cursor,6)
k.handler.rightButtonUp();k:update();eq(k.model.cursorHold,nil,'direction release reaches model')
k:close()
eq(k:show(''),true)
k.handler.upButtonDown();k:update()
k.handler.cranked(30.0);k:update();eq(k.model.pending,nil,'below new crank threshold')
k.handler.cranked(.1);k:update();eq(k.model.pending,'a','slightly more sensitive crank')
k.handler.upButtonDown();k.handler.cranked(-30.1);k:update()
eq(k.model.pending,'h','sensitive reverse crank wraps')
k:close()
local limited=Model.new('ab');limited.maxLength=3
assert(limited:insert('c'));assert(not limited:insert('d'));assert(limited.text=='abc' and limited.cursor==3)
limited:delete();assert(limited:insert('d') and limited.text=='abd')
local capped=Keyboard.new({maxLength=2})
assert(not capped:show('abc'));assert(capped:show('ab'))
capped:clear();assert(capped.model.maxLength==2);capped:close()
print('PASS '..count..' focused changed-input checks')
