#!/usr/bin/env python3
# Makes materials/VGUI/splitscreen/creator_credit.vtf: "Split Screen mod by" + the T3chnobone logo
# for the main menu (drawn by CGESSCreatorCredit in ge_splitscreen_menu.cpp).
#   make_creator_credit.py <logo.png on black> <header template .vtf> <out.vtf> [font.ttf]
# The header template is any 7.2 VTF (e.g. VGUI/hud/paintscreen/painteffect_c1.vtf); its size,
# flags and format are rewritten. Needs Pillow and SciPy. Font: Verdana Bold (DejaVu Sans Bold works).
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage as nd
import numpy as np, struct, sys

def cut_logo(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float)
    l = a.max(axis=2)
    fg = l > 110
    lab, n = nd.label(fg); sizes = nd.sum(fg, lab, range(1, n + 1))
    keep = np.isin(lab, 1 + np.where(sizes > 400)[0])
    # black outlines and the gun (upper right) are dark; take dark pixels close to the bright parts
    dt = nd.distance_transform_edt(~keep)
    yy, xx = np.mgrid[:l.shape[0], :l.shape[1]]
    gun = (xx > 880) & (yy < 420)
    zone = np.where(gun, dt < 80, dt < 40)
    m = keep | (zone & (l < 18))
    m = nd.binary_opening(m, iterations=2) | keep
    m = nd.binary_dilation(m, iterations=2)
    m = nd.binary_fill_holes(m)
    lab2, n2 = nd.label(m); s2 = nd.sum(m, lab2, range(1, n2 + 1)); m = np.isin(lab2, 1 + np.where(s2 > 20000)[0])
    m = nd.binary_closing(m, iterations=6); m = nd.binary_fill_holes(m)
    alpha = Image.fromarray((m * 255).astype('uint8')).filter(ImageFilter.GaussianBlur(2))
    sh = Image.fromarray((nd.binary_dilation(m, iterations=14) * 150).astype('uint8')).filter(ImageFilter.GaussianBlur(10))
    al = np.maximum(np.asarray(alpha).astype(float), np.asarray(sh).astype(float))
    w = np.asarray(alpha).astype(float) / 255
    o = Image.fromarray(np.dstack([a * w[..., None], al]).astype('uint8'), 'RGBA')
    return o.crop(o.getbbox())

W,H=1024,256
LOGO_H=210
logo=cut_logo(sys.argv[1])
lw=int(logo.width*LOGO_H/logo.height); logo=logo.resize((lw,LOGO_H),Image.LANCZOS)
font=ImageFont.truetype(sys.argv[4] if len(sys.argv) > 4 else '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',40)
txt="Split Screen mod by"
tb=font.getbbox(txt); tw=tb[2]-tb[0]; th=tb[3]-tb[1]
pad=8
lx=W-pad-lw; ly=(H-LOGO_H)//2
tx=lx-tw-14; ty=ly+LOGO_H//2-th//2-tb[1]
img=Image.new('RGBA',(W,H),(0,0,0,0))
sh=Image.new('RGBA',(W,H),(0,0,0,0)); ImageDraw.Draw(sh).text((tx,ty),txt,font=font,fill=(0,0,0,255),stroke_width=8,stroke_fill=(0,0,0,255))
sh=sh.filter(ImageFilter.GaussianBlur(8)); img.alpha_composite(sh); img.alpha_composite(sh)
d=ImageDraw.Draw(img); d.text((tx+3,ty+3),txt,font=font,fill=(0,0,0,255)); d.text((tx,ty),txt,font=font,fill=(255,255,255,255))
img.alpha_composite(logo,(lx,ly))
img.save(sys.argv[3] + '.png')
print('logo box in tex', lx, ly, lw, LOGO_H)
# VTF 7.2, BGRA8888, one mip, no thumbnail
hdr=bytearray(open(sys.argv[2],'rb').read(80))
struct.pack_into('<HHI',hdr,16,W,H,0x2000|0x4|0x8|0x100|0x200)
struct.pack_into('<I',hdr,52,12)
hdr[56]=1
px=img.tobytes(); bgra=bytearray(len(px))
bgra[0::4]=px[2::4]; bgra[1::4]=px[1::4]; bgra[2::4]=px[0::4]; bgra[3::4]=px[3::4]
open(sys.argv[3],'wb').write(bytes(hdr)+bytes(bgra))
