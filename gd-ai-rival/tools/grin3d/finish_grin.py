# Turns the raw three.js renders into the mod's Grin images:
# soft edges for the face and roar, a cropped body, a glowing small icon.
import sys
import numpy as np
from PIL import Image, ImageFilter

src, out = sys.argv[1], sys.argv[2]

def vignette(name, cx, cy, rx, ry, inner=.62, outer=1.0, dim=True):
    im = np.asarray(Image.open(f'{src}/{name}.png').convert('RGBA')).astype(np.float32)
    h, w = im.shape[:2]
    y, x = np.mgrid[0:h, 0:w]
    d = np.sqrt(((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2)
    k = np.clip((outer - d) / (outer - inner), 0, 1)
    k = k * k * (3 - 2 * k)
    if dim: im[..., :3] *= (0.25 + 0.75 * k)[..., None]
    im[..., 3] *= k
    return Image.fromarray(im.clip(0, 255).astype(np.uint8))

face = vignette('face', 335, 300, 330, 360)
roar = vignette('roar', 335, 320, 330, 370)
face.save(f'{out}/grin-face.png', optimize=True)
roar.save(f'{out}/grin-roar.png', optimize=True)

# The body, cropped to the creature
full = Image.open(f'{src}/full.png').convert('RGBA').crop((36, 121, 488, 781))
full.save(f'{out}/grin-full.png', optimize=True)

# Small icon for the anger bar and chat bubble: the head, brightened, with a
# red glow so it reads on dark backgrounds
head = vignette('face', 335, 285, 175, 225, inner=.8, dim=False).crop((125, 60, 545, 480))
arr = np.asarray(head).astype(np.float32)
arr[..., :3] = 255 * (arr[..., :3] / 255) ** 0.7
head = Image.fromarray(arr.clip(0, 255).astype(np.uint8)).resize((112, 112), Image.LANCZOS)
canvas = Image.new('RGBA', (128, 128), (0, 0, 0, 0))
canvas.alpha_composite(head, (8, 8))
alpha = canvas.getchannel('A').filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(3))
glow = Image.new('RGBA', canvas.size, (255, 40, 20, 0))
glow.putalpha(alpha.point(lambda a: int(a * .85)))
icon = Image.alpha_composite(glow, canvas)
icon.save(f'{out}/grin-icon.png', optimize=True)
print('ok')
