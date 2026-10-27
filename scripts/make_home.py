"""Build an embedded-bitmap SWF for SkyUI's custom-content loader (Pillow)."""
from pathlib import Path
from io import BytesIO
import struct
from PIL import Image
root=Path(__file__).resolve().parents[1]
class Bits:
 def __init__(self):self.data=[]
 def put(self,value,width):self.data.extend((value>>i)&1 for i in range(width-1,-1,-1))
 def bytes(self):
  data=self.data+[0]*((-len(self.data))%8)
  return bytes(sum(data[i+j]<<(7-j) for j in range(8)) for i in range(0,len(data),8))
def rect(x,y):
 b=Bits();n=max(x,y).bit_length()+1;b.put(n,5)
 for value in (0,x,0,y):b.put(value,n)
 return b.bytes()
def tag(code,data=b''):
 return struct.pack('<H',(code<<6)|min(len(data),63))+(struct.pack('<I',len(data)) if len(data)>=63 else b'')+data
image=Image.open(root/'artwork/home.png').convert('RGB').resize((704,396),Image.Resampling.LANCZOS)
jpeg=BytesIO();image.save(jpeg,format='JPEG',quality=95,subsampling=0)
# Bitmap coordinates are pixels; the shape uses twips (20 per pixel).
m=Bits();m.put(1,1);m.put(22,5);m.put(20*65536,22);m.put(20*65536,22);m.put(0,1);m.put(0,5)
shape=struct.pack('<H',2)+rect(14080,7920)+b'\x01\x41'+struct.pack('<H',1)+m.bytes()+b'\x00'
b=Bits();b.put(1,4);b.put(0,4)
b.put(0,1);b.put(5,5);b.put(1,5);b.put(0,1);b.put(0,1);b.put(1,1)
for dx,dy in [(14080,0),(0,7920),(-14080,0),(0,-7920)]:
 value=dx or dy;n=abs(value).bit_length()+1
 b.put(1,1);b.put(1,1);b.put(n-2,4);b.put(0,1);b.put(int(dx==0),1);b.put(value&((1<<n)-1),n)
b.put(0,6);shape+=b.bytes()
body=rect(14080,7920)+struct.pack('<HH',24*256,1)
body+=tag(9,b'\x00\x00\x00')+tag(21,struct.pack('<H',1)+jpeg.getvalue())+tag(32,shape)
body+=tag(26,b'\x06'+struct.pack('<HH',1,2)+b'\x00')+tag(1)+tag(0)
output=root/'Data/Interface/PickpocketOverhaul/home.swf';output.parent.mkdir(parents=True,exist_ok=True)
output.write_bytes(b'FWS'+bytes([8])+struct.pack('<I',len(body)+8)+body)
# Validate header, image payload and complete tag boundaries.
data=output.read_bytes();assert struct.unpack_from('<I',data,4)[0]==len(data)
assert jpeg.getvalue() in data
print(output,len(data),'bytes')
