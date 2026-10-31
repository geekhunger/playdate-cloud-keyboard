"""Build exact, unscaled 1-bit sprites for Cloud Keyboard.

Run with a Python environment containing Pillow. No online assets required.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageChops
import artwork as art

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Source' / 'keyboard' / 'art'
OUT.mkdir(parents=True, exist_ok=True)
GROUPS = ['abcdefgh', 'ijklmnop', 'qrstuvwx', 'yz,.!?-_', '1234567890']


def sprite(name, size, paint):
    # Painting onto both backgrounds derives a binary transparency mask.
    white = Image.new('1', size, 1)
    black = Image.new('1', size, 0)
    paint(ImageDraw.Draw(white))
    paint(ImageDraw.Draw(black))
    alpha = ImageChops.invert(ImageChops.difference(white, black).convert('L'))
    rgba = white.convert('RGBA')
    rgba.putalpha(alpha)
    rgba.save(OUT / (name + '.png'), optimize=True)


def cloud_points(width,height,seed=0):
    radius=height/2
    span=width-height
    def edge(extra):
        # Each little rounded lobe has its own width, height and peak position.
        state=seed+extra+271
        def rand():
            nonlocal state
            state=(state*1103515245+12345)&0x7fffffff
            return state
        values=[0.]*(round(span)+1)
        start=0
        while start<span:
            length=min(5+rand()%6,round(span)-start)
            height=.25+(rand()%140)/100
            peak=.3+(rand()%41)/100
            for k in range(length+1):
                t=k/length
                u=t/peak if t<=peak else (1-t)/(1-peak)
                values[start+k]=height*math.sin(math.pi*u/2)**2
            start+=length
        return values
    top,bottom=edge(0),edge(97)
    points=[]
    for n in range(round(span)+1):
        x=radius+n
        y=top[n]
        points.append((round(x),round(y)))
    for n in range(1,49):
        angle=-math.pi/2+math.pi*n/48
        points.append((round(width-radius+radius*math.cos(angle)),round(radius+radius*math.sin(angle))))
    for n in range(round(span)-1,-1,-1):
        points.append((round(radius+n),round(height-bottom[n])))
    for n in range(1,49):
        angle=math.pi/2+math.pi*n/48
        points.append((round(radius+radius*math.cos(angle)),round(radius+radius*math.sin(angle))))
    return points


def plate(draw, width, height, characters, expanded=False, index=0):
    split = width // 2
    half = len(characters) // 2
    box = (0, 0, width, height)
    points=cloud_points(width,height,sum(ord(ch) for ch in characters.lower())%17)
    mask=Image.new('1',(width+4,height+4),0)
    ImageDraw.Draw(mask).polygon(points,fill=1)
    draw.bitmap((0,0),mask,fill=1)
    draw.line(points+[points[0]],fill=0,width=2,joint='curve')
    if not expanded:
        art.text(draw, (split // 2, 10), characters[:half], anchor='ma')
        art.text(draw, ((split + width) // 2, 10), characters[half:], anchor='ma')
        return
    positions = [21, 43, 63, 83, split+20, split+41, split+62, split+82]
    if half == 5:
        positions = [14, 31, 48, 65, 82, split+14, split+31, split+48, split+65, split+82]
    for i, (ch, cx) in enumerate(zip(characters, positions), 1):
        inverted = False
        if i == index:
            cy = height // 2
            draw.ellipse((cx-10, cy-10, cx+10, cy+10), fill=0)
            inverted = True
        art.text(draw, (cx, 10), ch, anchor='ma', invert=inverted)


for g, chars in enumerate(GROUPS, 1):
    height = 38 if g in (2, 4) else 36
    for frame in range(11 if g in (2,4) else 1):
        width=round(156+34*frame/10) if g in (2,4) else 190
        def shadow(draw,w=width):
            for sy in range(5):
                for sx in range(w+4):
                    distance=((sx-(w/2+3))/((w-18)/2))**2+((sy-2)/2.5)**2
                    density=.5*max(0,1-distance)**.7
                    threshold=((sx*73+sy*151+sx*sy*17)%256+.5)/256
                    if threshold<density:
                        draw.point((sx,sy),fill=0)
        sprite(f'g{g}-shadow-{frame}',(width+4,5),shadow)
    for upper in (0, 1):
        characters = chars.upper() if upper else chars
        for frame in range(11 if g in (2, 4) else 1):
            width = round(156 + 34 * frame / 10) if g in (2, 4) else 190
            sprite(f'g{g}-{upper}-idle-{frame}', (width+4, height+4),
                   lambda d, w=width: plate(d, w, height, characters))
        for index in range(len(chars)+1):
            sprite(f'g{g}-{upper}-open-{index}', (194, height+4),
                   lambda d, idx=index: plate(d, 190, height, characters, True, idx))

for letter in ('A', 'B'):
    for selected in (0, 1):
        sprite(f'button-{letter}-{selected}', (33,33),
               lambda d, c=letter, s=selected: art.large_button(d, 16,16,c,invert=bool(s),ghost=not s))
    idle=Image.new('1',(23,23),1)
    art.button(ImageDraw.Draw(idle),11,11,letter)
    active=Image.new('1',(23,23),1)
    ad=ImageDraw.Draw(active)
    ad.ellipse((1,1,21,21),fill=0,outline=0,width=2)
    art.text(ad,(11,4),letter,anchor='ma',invert=True)
    for rows in range(22):
        frame=idle.copy()
        if rows:
            top=22-rows
            frame.paste(active.crop((0,top,23,22)),(0,top))
        frame.save(OUT/f'footer-{letter}-{rows}.png',optimize=True)

for direction in ('none','up','right','down','left'):
    def paint_dpad(draw, direction=direction):
        art.icon_dpad(draw,20,20,'soft',None if direction=='none' else direction)
        if direction=='down':
            draw.rectangle((15,27,25,38), fill=0)
    sprite('dpad-'+direction,(41,41),paint_dpad)

for direction in ('left','right'):
    def cursor_cloud(draw,direction=direction):
        points=cloud_points(156,38,11 if direction=='left' else 23)
        mask=Image.new('1',(160,42),0)
        ImageDraw.Draw(mask).polygon(points,fill=1)
        draw.bitmap((0,0),mask,fill=1)
        draw.line(points+[points[0]],fill=0,width=2,joint='curve')
        if direction=='left':
            art.cursor_icon(draw,22,12,direction)
            art.text(draw,(64,11),'CURSOR')
        else:
            art.text(draw,(22,11),'CURSOR')
            art.cursor_icon(draw,106,12,direction)
    sprite('cursor-'+direction,(160,42),cursor_cloud)

for upper in (0,1):
    for alternate in (0,1):
        for phase in range(4):
            image = Image.new('1',(400,240),1)
            d = ImageDraw.Draw(image)
            # Original drawMultiBackground motion: +1px diagonally / 180ms,
            # four phases, staggered rows. Clip behind the current input/footer.
            for y in range(21+phase,202,4):
                if y<27: continue
                row_offset=((y//4)%2)*2
                for x in range(row_offset,400,4):
                    sx=(x+phase)%400
                    d.point((sx,y),fill=0)
            art.soft_box(d,(9,27,390,63),1,0,4,16,False)
            d.rectangle((0,202,399,239),fill=1)
            art.button(d,22,218,'B')
            art.button(d,378,218,'A')
            art.icon_label(d,39,203,'delete','DELETE')
            art.text(d,(39,220),'HOLD')
            # A compact keycap indicates the target layer, not the word TOOLS.
            label=('ABC' if upper else 'abc') if alternate else '123'
            d.rounded_rectangle((73,219,101,234),radius=3,fill=1,outline=0,width=1)
            art.text(d,(87,219),label,anchor='ma')
            art.icon_label(d,335,203,'space','SPACE',right=True)
            art.text(d,(361,220),'HOLD abc' if upper else 'HOLD ABC',anchor='ra')
            image.save(OUT/f'background-{upper}-{alternate}-{phase}.png',optimize=True)
    old=OUT/f'background-{upper}.png'
    if old.exists(): old.unlink()

# Crop glyph cells using the native glyph widths and one-pixel tracking.
widths = []
for code in range(32,127):
    ch = chr(code)
    mask = art.FONT.mask(ch)
    widths.append(mask.width)
    glyph = Image.new('RGBA', (12,15), (0,0,0,0))
    black = Image.new('RGBA', mask.size, (0,0,0,255))
    glyph.paste(black,(0,0),mask)
    glyph.save(OUT/f'glyph-{code}.png',optimize=True)
(ROOT/'Source'/'keyboard'/'art_metrics.lua').write_text(
    '-- Generated by tools/build_artwork.py; ASCII 32..126, one-pixel tracking.\n'
    'Keyboard._glyphWidths = {' + ','.join(map(str,widths)) + '}\n')
print(f'Built {len(list(OUT.glob("*.png")))} 1-bit sprite assets in {OUT}')
