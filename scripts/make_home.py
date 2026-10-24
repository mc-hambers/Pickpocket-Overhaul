"""Convert the supplied thumbnail into the SkyUI Home page texture."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
out=root/'Data/Interface/PickpocketOverhaul/home.dds'
out.parent.mkdir(parents=True,exist_ok=True)
Image.open(root/'artwork/home.png').convert('RGB').resize((704,396),Image.Resampling.LANCZOS).save(out,pixel_format='DXT1')
