"""Register water sweep images by measured camera rays and test annotated water.

Eye translation and time-varying waves remain after rotational registration;
the bounded pixel-error test permits them. Requires water-input log rows.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


def camera_ray_maps(text, width, height, eye="left", step=18):
    text = text[text.rfind('VRTEST water runner armed schema=21'):] if 'VRTEST water runner armed schema=21' in text else text
    pattern = re.compile(r"water-input view=(\d+) step=(\d+) eye=(\w+) row=(\d+) "
                         r"mvp=\(([^)]+)\) world=\(([^)]+)\)")
    matrices = {}
    for match in pattern.finditer(text):
        view, sample_step, sample_eye, row, mvp, world = match.groups()
        if int(sample_step) != step or sample_eye != eye:
            continue
        pair = matrices.setdefault(int(view), ({}, {}))
        if int(row) in pair[0]:
            raise ValueError("Duplicate camera row")
        pair[0][int(row)] = list(map(float, mvp.split(',')))
        pair[1][int(row)] = list(map(float, world.split(',')))
    viewport = np.array([[width/2, 0, width/2], [0, -height/2, height/2], [0, 0, 1]])
    maps = {}
    for view, (mvp, world) in matrices.items():
        if set(mvp) != set(range(4)) or set(world) != set(range(4)):
            raise ValueError("Incomplete camera rows")
        projection = np.array([mvp[i] for i in range(4)]) @ np.linalg.inv(np.array([world[i] for i in range(4)]))
        if not np.isfinite(projection).all():
            raise ValueError("Non-finite camera projection")
        maps[view] = viewport @ projection[[0, 1, 3], :3]
    if set(maps) != set(range(7)):
        raise ValueError("Exactly seven camera views are required")
    return maps


def register(image, target_map, reference_map):
    transform = target_map @ np.linalg.inv(reference_map)
    transform /= transform[2, 2]
    return image.transform(image.size, Image.Transform.PERSPECTIVE,
                           tuple(transform.ravel()[:8]), Image.Resampling.BILINEAR)


def compare_water_pixels(reference, candidate, mask):
    """RGB error in a geometrically valid water mask; black pixels are included."""
    count = int(mask.sum())
    if count < 512:
        return {"status": "unavailable", "pixels": count}
    difference = float(np.abs(reference.astype(float)-candidate.astype(float))[mask].mean())
    black = float((candidate.max(axis=2)[mask] < 16).mean())
    return {"status": "pass" if difference <= 5.0 and black < .20 else "fail",
            "pixels": count, "meanRgbError": difference, "blackFraction": black}


def analyze_alignment(root):
    """Compare all yaw/time/eye images against a reviewed reference water mask.

    The mask is tied to this run's bytes. Registration uses measured camera
    rays, never the reflection features being tested. Rotational coverage is
    determined with a white validity image so missing/black water stays tested.
    Five RGB levels after ~4px smoothing is the allowed cross-view error.
    This does not measure absolute reflection accuracy against a flat capture.
    """
    try:
        annotation = json.loads((root/'water-alignment-region.json').read_text())
        paths = { (v,s,e): root/f'OBVR-VRTest-water-view-{v}-step-{s}-{e}.bmp'
                  for v in range(7) for s in (8,18) for e in ('L','R') }
        for path in paths.values():
            if annotation['sha256'][path.name] != hashlib.sha256(path.read_bytes()).hexdigest():
                raise ValueError('Water annotation belongs to different capture bytes')
        images = {key:Image.open(path).convert('RGB') for key,path in paths.items()}
        size = images[(3,18,'L')].size
        if any(image.size != size for image in images.values()):
            raise ValueError('Capture sizes differ')
        width, height = size
        water = Image.new('L',size)
        draw = ImageDraw.Draw(water)
        for polygon in annotation['polygons']:
            if len(polygon) < 3 or any(len(p)!=2 or any(not isinstance(v,(int,float))
                    or isinstance(v,bool) or not np.isfinite(v) or v<0 or v>1 for v in p) for p in polygon):
                raise ValueError('Invalid water polygon')
            draw.polygon([(x*width,y*height) for x,y in polygon],fill=255)
        water_mask = np.asarray(water) == 255
        text = (root/'OBVR.log').read_text()
        white = Image.new('L',size,255)
        radius = max(1, round(width/250))
        comparisons, waves, details = {}, {}, {}
        for eye, label in (('left','L'),('right','R')):
            reference_maps = camera_ray_maps(text,width,height,eye,18)
            reference = images[(3,18,label)].filter(ImageFilter.GaussianBlur(radius))
            reference_pixels = np.asarray(reference)
            details[label] = float(reference_pixels.mean(axis=2)[water_mask].std()) if water_mask.any() else 0
            for view in range(7):
                pair = []
                valid_pair = water_mask.copy()
                for step in (8,18):
                    maps = camera_ray_maps(text,width,height,eye,step)
                    valid = register(white,maps[view],reference_maps[3]).filter(ImageFilter.MinFilter(radius*4+1))
                    valid_mask = water_mask & (np.asarray(valid)==255)
                    valid_pair &= valid_mask
                    aligned = register(images[(view,step,label)],maps[view],reference_maps[3])
                    pair.append(np.asarray(aligned).astype(float))
                    blurred = np.asarray(aligned.filter(ImageFilter.GaussianBlur(radius)))
                    comparisons[f'{view}-{step}-{label}'] = compare_water_pixels(reference_pixels,blurred,valid_mask)
                waves[f'{view}-{label}'] = float(np.abs(pair[0]-pair[1])[valid_pair].mean()) if valid_pair.any() else 0
        moving = sum(value >= .75 for value in waves.values())
        passed = all(item['status']=='pass' for item in comparisons.values()) and min(details.values()) >= 4 and moving >= 11
        return {'status':'pass' if passed else 'fail', 'worldAlignmentStatus':'pass' if all(
                    item['status']=='pass' for item in comparisons.values()) else 'fail',
                'reason':'registered water images compared across all yaw/time/eye samples',
                'comparisons':comparisons, 'waveDifferences':waves, 'waveMovingPairs':moving,
                'referenceContrast':details, 'rgbErrorLimit':5.0,
                'limitation':'rotation registration permits small eye-translation and wave differences; no flat-reference accuracy claim'}
    except (OSError, ValueError, KeyError, TypeError, np.linalg.LinAlgError) as error:
        return {'status':'unavailable','reason':str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    args = parser.parse_args()
    images = [Image.open(args.root / f'OBVR-VRTest-water-view-{v}-step-18-L.bmp').convert('RGB') for v in range(7)]
    maps = camera_ray_maps((args.root/'OBVR.log').read_text(), *images[0].size)
    canvas = Image.new('RGB', (1200, 1008))
    draw = ImageDraw.Draw(canvas)
    for view, source in enumerate(images):
        registered = register(source, maps[view], maps[3])
        registered.save(args.root/f'registered-{view}.png')
        canvas.paste(registered.resize((400,334)), ((view%3)*400,(view//3)*336))
        draw.text(((view%3)*400+5,(view//3)*336+5),str(view),fill='red')
    canvas.save(args.root/'registered-contact.png')


if __name__ == '__main__':
    main()
