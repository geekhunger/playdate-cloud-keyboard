"""Code-native pixel artwork primitives for Cloud Keyboard."""

from pathlib import Path

from PIL import Image, ImageDraw

ASSETS=Path(__file__).parent/'assets'

class BitmapFont:
    def __init__(self, png, cell_w, cell_h, tracking=1):
        self.sheet = Image.open(png).convert('RGBA')
        self.cell_w = cell_w
        self.cell_h = cell_h
        self.tracking = tracking

    def mask(self, text):
        glyphs = []
        width = 0
        for ch in text:
            code = ord(ch)
            if code < 32 or code > 126:
                ch, code = '-', 45
            idx = code - 32
            x = (idx % 16) * self.cell_w
            y = (idx // 16) * self.cell_h
            tile = self.sheet.crop((x, y, x + self.cell_w, y + self.cell_h))
            alpha = tile.getchannel('A')
            bbox = alpha.getbbox()
            if bbox:
                glyph = alpha.crop((0, 0, bbox[2], self.cell_h))
                advance = max(2, bbox[2])
            else:
                glyph = Image.new('L', (4, self.cell_h), 0)
                advance = 4
            glyphs.append((glyph, advance))
            width += advance + self.tracking
        width = max(1, width - self.tracking)
        out = Image.new('L', (width, self.cell_h), 0)
        x = 0
        for glyph, advance in glyphs:
            out.paste(glyph, (x, 0), glyph)
            x += advance + self.tracking
        return out

FONT = BitmapFont(ASSETS / 'Oklahoma-Bold-table-12-15.png', 12, 15, 1)

def text(draw, xy, value, anchor='la', invert=False, font=FONT):
    mask = font.mask(value)
    x, y = xy
    if 'm' in anchor:
        x -= mask.width // 2
    elif 'r' in anchor:
        x -= mask.width
    if 'm' == anchor[-1:]:
        y -= mask.height // 2
    elif 'b' == anchor[-1:]:
        y -= mask.height
    color = 1 if invert else 0
    draw.bitmap((int(x), int(y)), mask, fill=color)
    return mask.width

def notched(draw, box, fill=1, outline=0, notch=5, width=2):
    x0, y0, x1, y1 = box
    pts = [(x0 + notch, y0), (x1 - notch, y0), (x1, y0 + notch),
           (x1, y1 - notch), (x1 - notch, y1), (x0 + notch, y1),
           (x0, y1 - notch), (x0, y0 + notch)]
    draw.polygon(pts, fill=fill)
    draw.line(pts + [pts[0]], fill=outline, width=width, joint='curve')

def soft_box(draw, box, fill=1, outline=0, width=2, radius=12, shadow=False):
    x0, y0, x1, y1 = box
    if shadow:
        draw.rounded_rectangle((x0 + 3, y0 + 3, x1 + 3, y1 + 3), radius=radius, fill=0)
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def icon_dpad(draw, cx, cy, style, active=None):
    # Original geometry retained exactly; erase only the overlapping seam.
    if style == 'soft':
        draw.rounded_rectangle((cx - 7, cy - 19, cx + 7, cy + 19), radius=5, fill=1, outline=0, width=2)
        draw.rounded_rectangle((cx - 19, cy - 7, cx + 19, cy + 7), radius=5, fill=1, outline=0, width=2)
        draw.rectangle((cx - 5, cy - 7, cx + 5, cy + 7), fill=1)
    elif style == 'ticket':
        notched(draw, (cx - 7, cy - 19, cx + 7, cy + 19), 1, 0, 3, 2)
        notched(draw, (cx - 19, cy - 7, cx + 19, cy + 7), 1, 0, 3, 2)
        draw.rectangle((cx - 5, cy - 7, cx + 5, cy + 7), fill=1)
    else:
        draw.rectangle((cx - 6, cy - 20, cx + 6, cy + 20), fill=1, outline=0, width=2)
        draw.rectangle((cx - 20, cy - 6, cx + 20, cy + 6), fill=1, outline=0, width=2)
        draw.rectangle((cx - 4, cy - 6, cx + 4, cy + 6), fill=1)
    if active == 'up':
        draw.rectangle((cx - 5, cy - 18, cx + 5, cy - 7), fill=0)
    elif active == 'left':
        draw.rectangle((cx - 18, cy - 5, cx - 7, cy + 5), fill=0)
    elif active == 'right':
        draw.rectangle((cx + 7, cy - 5, cx + 18, cy + 5), fill=0)

def button(draw, cx, cy, label, invert=False):
    fill, ink = (0, 1) if invert else (1, 0)
    draw.ellipse((cx - 10, cy - 10, cx + 10, cy + 10), fill=fill, outline=ink, width=2)
    text(draw, (cx, cy - 7), label, anchor='ma', invert=invert)

def large_button(draw, cx, cy, label, invert=False, ghost=False):
    radius = 16
    fill, ink = (0, 1) if invert else (1, 0)
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=fill, outline=ink, width=2)
    if ghost:
        for y in range(cy - 11, cy + 12, 4):
            for x in range(cx - 11 + ((y // 4) % 2) * 2, cx + 12, 4):
                if (x - cx) ** 2 + (y - cy) ** 2 < 165:
                    draw.point((x, y), fill=0)
    text(draw, (cx, cy - 7), label, anchor='ma', invert=invert)

def icon_label(draw, x, y, icon, label, right=False):
    if icon == 'space':
        draw.rounded_rectangle((x, y + 4, x + 21, y + 17), radius=3, fill=1, outline=0, width=2)
        draw.line((x + 6, y + 13, x + 15, y + 13), fill=0, width=2)
    elif icon == 'delete':
        draw.polygon([(x, y + 10), (x + 6, y + 4), (x + 21, y + 4),
                      (x + 21, y + 17), (x + 6, y + 17)], fill=0)
        draw.line((x + 10, y + 7, x + 16, y + 13), fill=1, width=2)
        draw.line((x + 16, y + 7, x + 10, y + 13), fill=1, width=2)
    elif icon == 'case':
        text(draw, (x, y + 3), 'Aa')
    elif icon == 'tools':
        draw.rectangle((x + 2, y + 4, x + 16, y + 18), outline=0, width=2)
        draw.line((x + 6, y + 1, x + 6, y + 7), fill=0, width=2)
        draw.line((x + 12, y + 1, x + 12, y + 7), fill=0, width=2)
    text(draw, (x + (26 if not right else -4), y + 3), label, anchor='la' if not right else 'ra')

def cursor_icon(draw, x, y, direction):
    # Same solid block as the text-field cursor, plus a stepped arrow.
    block_x=x+23 if direction=='left' else x
    draw.rectangle((block_x,y-2,block_x+8,y+15),fill=0) # Native input cursor: 9 x 18 px.
    points = [(2, 7), (9, 0), (9, 4), (19, 4),
              (19, 9), (9, 9), (9, 13)]
    if direction == 'right':
        points = [(31 - px, py) for px, py in points]
    draw.polygon([(x + px, y + py) for px, py in points], fill=0)
