"""Record actual SDK-rendered frames and build crisp two-color README GIFs."""
import os,pty,select,subprocess,time,sys
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
sdk=Path(os.environ['PLAYDATE_SDK_PATH'])
simulator=os.environ.get('PLAYDATE_SIMULATOR',str(sdk/'bin/Playdate Simulator.app/Contents/MacOS/Playdate Simulator'))
frames=root/'output/recording';frames.mkdir(parents=True,exist_ok=True)
subprocess.run([str(sdk/'bin/pdc'),str(root/'Source'),str(root/'build/CloudKeyboard.pdx')],check=True)
master,slave=pty.openpty()
p=subprocess.Popen([simulator,str(root/'build/CloudKeyboard.pdx'),'record='+str(frames)],stdout=slave,stderr=slave)
os.close(slave);data=b'';deadline=time.monotonic()+150
try:
 while time.monotonic()<deadline:
  if select.select([master],[],[],.3)[0]:
   data+=os.read(master,65536)
   if b'Update error:' in data:raise RuntimeError(data.decode(errors='replace')[-2500:])
   if b'CLOUD KEYBOARD RECORDING COMPLETE' in data:break
 else:raise TimeoutError('SDK recording did not finish')
finally:
 p.kill();p.wait();os.close(master);(frames/'sdk.log').write_bytes(data)
palette=[0,0,0,255,255,255]+[0]*762
for kind in ('typing','crank','tools'):
 imgs=[]
 for file in sorted(frames.glob(kind+'-*.png')):
  with Image.open(file)as im:
   frame=im.convert('L').resize((800,480),Image.Resampling.NEAREST).point(lambda v:1 if v else 0,'P')
   frame.putpalette(palette);imgs.append(frame)
 assert imgs,kind
 out=root/'docs/media'/f'{kind}.gif'
 imgs[0].save(out,save_all=True,append_images=imgs[1:],duration=40,loop=0,optimize=True,disposal=2)
 print(kind,len(imgs),'frames,',out.stat().st_size,'bytes',flush=True)
