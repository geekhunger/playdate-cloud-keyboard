#!/usr/bin/env python3
"""Export exact, variable-width 1-bit emoji artwork from a theme manifest.

The JSON pixel rows are the source of truth. No font rasterization, scaling
filters, or image quantization are involved in the native sprites.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path


def load_set(path):
    data=json.loads(path.read_text())
    if data.get('schema_version')!=1: raise ValueError('Unsupported set schema')
    if not re.fullmatch(r'[a-z][a-z0-9-]*',data.get('id','')): raise ValueError('Invalid set ID')
    height=data['cell_height']
    if not 1<=height<=14 or not 0<=data['baseline']<height: raise ValueError('Invalid text metrics')
    seen_ids=set(); seen_sequences=set()
    for glyph in data['glyphs']:
        key=glyph['id']; sequence=glyph['unicode']; rows=glyph['pixels']
        if not re.fullmatch(r'[a-z][a-z0-9-]*',key) or key in seen_ids: raise ValueError('Invalid or repeated glyph ID')
        if not sequence or sequence in seen_sequences: raise ValueError('Empty or repeated Unicode sequence')
        if len(sequence.encode('utf-8'))>160: raise ValueError('Sequence exceeds keyboard item budget')
        if len(rows)!=height or not rows: raise ValueError('Wrong glyph height')
        width=len(rows[0])
        if not 1<=width<=64 or any(len(row)!=width or set(row)-set('.#') for row in rows): raise ValueError('Invalid pixel grid')
        if not any('#' in row for row in rows): raise ValueError('Empty glyph')
        if not width<=glyph['advance']<=255: raise ValueError('Invalid glyph advance')
        seen_ids.add(key);seen_sequences.add(sequence)
    return data


def bitmap(glyph,ink=(0,0,0,255)):
    from PIL import Image

    rows=glyph['pixels']; image=Image.new('RGBA',(len(rows[0]),len(rows)),(0,0,0,0))
    for y,row in enumerate(rows):
        for x,pixel in enumerate(row):
            if pixel=='#':image.putpixel((x,y),ink)
    return image


def label_font(size):
    from PIL import ImageFont

    for path in ('/System/Library/Fonts/Supplemental/Arial.ttf','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        if Path(path).exists(): return ImageFont.truetype(path,size)
    return ImageFont.load_default(size=size)


def contact_sheet(data,scale=5,dark=False):
    from PIL import Image, ImageDraw

    columns=3; cell_w=360; cell_h=146; margin=32
    count=len(data['glyphs']); rows=(count+columns-1)//columns
    bg=(31,31,29) if dark else (255,255,255); fg=(255,255,255) if dark else (0,0,0)
    image=Image.new('RGB',(columns*cell_w+2*margin,rows*cell_h+2*margin),bg)
    draw=ImageDraw.Draw(image); font=label_font(19); small=label_font(14)
    for i,glyph in enumerate(data['glyphs']):
        x=margin+(i%columns)*cell_w; y=margin+(i//columns)*cell_h
        native=bitmap(glyph,fg+(255,))
        zoom=native.resize((native.width*scale,native.height*scale),Image.Resampling.NEAREST)
        image.paste(zoom,(x+(cell_w-zoom.width)//2,y+8),zoom)
        title=f"{i+1:02}  {glyph['label']}"
        tw=draw.textbbox((0,0),title,font=font)[2]
        draw.text((x+(cell_w-tw)//2,y+90),title,font=font,fill=fg)
        tw=draw.textbbox((0,0),'1:1',font=small)[2]
        total=tw+10+native.width
        bx=x+(cell_w-total)//2
        draw.text((bx,y+120),'1:1',font=small,fill=fg)
        image.paste(native,(bx+tw+10,y+120),native)
    return image


def export(data,out):
    from PIL import Image

    out.mkdir(parents=True,exist_ok=True);sprites=out/'sprites';sprites.mkdir(exist_ok=True)
    manifest={k:v for k,v in data.items() if k!='glyphs'};manifest['glyphs']=[]
    atlas_width=max(len(g['pixels'][0]) for g in data['glyphs'])
    atlas=Image.new('RGBA',(atlas_width,data['cell_height']*len(data['glyphs'])),(0,0,0,0))
    for i,glyph in enumerate(data['glyphs']):
        native=bitmap(glyph); dest=sprites/(glyph['id']+'.png');native.save(dest)
        # Verify native files have only fully opaque ink or full transparency.
        reopened=Image.open(dest).convert('RGBA')
        assert set(reopened.getdata())<={(0,0,0,255),(0,0,0,0)}
        atlas.paste(native,(0,i*data['cell_height']))
        entry={k:v for k,v in glyph.items() if k!='pixels'}
        entry.update(width=native.width,height=native.height,atlas_y=i*data['cell_height'],
                     file='sprites/'+dest.name,sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
        manifest['glyphs'].append(entry)
    atlas.save(out/'atlas.png')
    contact_sheet(data).save(out/'overview.png')
    contact_sheet(data,dark=True).save(out/'overview-dark.png')
    (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    print(f"{data['id']}: {len(data['glyphs'])} sprites, {atlas_width}×{data['cell_height']} maximum cell, exact 1-bit RGBA")


def export_c(data,path):
    """Emit immutable native data; keep JSON ordering and every approved pixel."""
    symbol=data['id'].removeprefix('brb-').replace('-','_')
    prefix='brb_emoji_'+symbol
    guard=prefix.upper()+'_DATA_H'
    packed=bytearray(); entries=[]; aliases=[]
    canonical={glyph['unicode'] for glyph in data['glyphs']}
    for index,glyph in enumerate(data['glyphs']):
        offset=len(packed); bits=''.join(glyph['pixels'])
        for start in range(0,len(bits),8):
            chunk=bits[start:start+8].ljust(8,'.')
            packed.append(sum(1<<(7-bit) for bit,value in enumerate(chunk) if value=='#'))
        entries.append((glyph,offset))
        # Emoji presentation selector is optional for the same complete scalar.
        if glyph['unicode'].endswith('\ufe0f'):
            alias=glyph['unicode'][:-1]
            if alias and alias not in canonical: aliases.append((alias,index))
        for alias in glyph.get('aliases',[]):
            if not alias or len(alias.encode('utf-8'))>160:raise ValueError('Invalid emoji alias')
            if alias in canonical or any(existing==alias for existing,_ in aliases):
                raise ValueError('Conflicting emoji alias')
            aliases.append((alias,index))
    if len(packed)>65535 or len(entries)>65535: raise ValueError('Native set exceeds compact index budget')
    quote=lambda value:json.dumps(value,ensure_ascii=False)
    lines=['// Generated from canonical theme JSON by tools/emoji/build_set.py; do not hand edit.',
           '#ifndef '+guard,'#define '+guard,
           '',f'static const uint8_t {prefix}_pixels[] = {{']
    for start in range(0,len(packed),16):
        lines.append('    '+','.join(f'0x{byte:02x}' for byte in packed[start:start+16])+',')
    lines+=['};','',f'static const BrbEmojiGlyph {prefix}_glyphs[] = {{']
    for glyph,offset in entries:
        lines.append('    {'+', '.join((quote(glyph['id']),quote(glyph['unicode']),str(offset),
            str(len(glyph['unicode'].encode('utf-8'))),str(len(glyph['pixels'][0])),str(glyph['advance'])))+'},')
    lines+=['};','']
    if aliases:
        lines.append(f'static const BrbEmojiAlias {prefix}_aliases[] = {{')
        for alias,index in aliases:
            lines.append('    {'+', '.join((quote(alias),str(index),str(len(alias.encode('utf-8')))))+'},')
        lines+=['};','']
    inventory=''.join(glyph['unicode'] for glyph in data['glyphs'])
    lines+=[f'static const char {prefix}_keyboard[] = {quote(inventory)};',
            '',f'static const BrbEmojiSet {prefix} = {{',
            f'    .id={quote(data["id"])}, .version={data["version"]},',
            f'    .cell_height={data["cell_height"]}, .baseline={data["baseline"]},',
            f'    .glyphs={prefix}_glyphs, .count=sizeof({prefix}_glyphs)/sizeof({prefix}_glyphs[0]),',
            f'    .pixels={prefix}_pixels, .pixels_size=sizeof({prefix}_pixels),']
    if aliases:lines.append(f'    .aliases={prefix}_aliases, .alias_count=sizeof({prefix}_aliases)/sizeof({prefix}_aliases[0]),')
    lines+=[f'    .keyboard_utf8={prefix}_keyboard, .keyboard_bytes=sizeof({prefix}_keyboard)-1',
            '};','','#endif','']
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines))
    print(f'{data["id"]}: {len(entries)} native glyphs, {len(packed)} packed pixel bytes, {len(aliases)} aliases')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('set',type=Path);parser.add_argument('out',type=Path)
    parser.add_argument('--c-header',type=Path);parser.add_argument('--c-only',action='store_true')
    args=parser.parse_args();data=load_set(args.set)
    if not args.c_only:export(data,args.out)
    if args.c_header:export_c(data,args.c_header)
    elif args.c_only:parser.error('--c-only requires --c-header')
