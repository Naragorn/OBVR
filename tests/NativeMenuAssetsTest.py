"""Validate engine XML structure after substituting Oblivion's built-in entities."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
def parse(path):
    return ET.fromstring(re.sub(r'&(\w+);', lambda m: {'true':'1','false':'0','GenericMenu':'1011','no_click_past':'2'}.get(m[1],m[0]),path.read_text()))
base=ROOT/'assets/menus'
onboarding=parse(base/'generic/OBVR_Onboarding.xml')
settings=parse(base/'generic/OBVR_Settings.xml')
assert sorted(int(x.text) for x in onboarding.iter('id'))==[9101,9102]
assert '(OBVR)' in onboarding.find('.//text[@name="title"]/string').text
assert 'Decide later' not in (base/'generic/OBVR_Onboarding.xml').read_text()
expected=[9201,9202,9299]+list(range(9300,9321))
assert sorted(int(x.text) for x in settings.iter('id'))==sorted(expected)
assert settings.find('.//rect[@name="close"]/id').text=='9299'
for i in range(7):
    row=settings.find(f'.//rect[@name="row{i}"]')
    for name in ['label','value','restart']:
        assert row.find(f'text[@name="{name}"]/string') is not None
for root in [onboarding,settings]:
    for txt in root.iter('text'):
        for channel in ['red','green','blue']:
            trait=txt.find(channel)
            assert trait is not None and len(trait)==0, 'Text color must never change on hover'
    for inc in root.iter('include'):
        name=inc.attrib['src']
        if name=='generic_background.xml': continue # supplied by the game
        assert (base/'prefabs'/name).is_file(),name
highlight=parse(base/'prefabs/OBVR/button_highlight.xml')
assert highlight.find('visible/copy').attrib['trait']=='mouseover'
assert {x.text.strip() for x in highlight.iter('filename')}=={'Menus\\Dialog\\dialog_selection_full.dds','Menus\\Dialog\\dialog_selection_cut.dds'}
print('Native XML: IDs, row bindings, includes, static text colors and hover textures verified')

# Height alone crops Oblivion image textures. Preserve the full 64px native
# highlight at the chosen zoom, with the cap on the same baseline.
for piece in highlight.iter('image'):
    assert float(piece.findtext('height')) == 64 * float(piece.findtext('zoom')) / 100
assert float(highlight.find('image/width').text) == 104 * float(highlight.findtext('zoom')) / 100
assert highlight.find('image/y/copy').attrib == {'src':'parent()','trait':'y'}
for root in [onboarding,settings]:
    for rect in root.iter('rect'):
        if any(i.attrib['src']=='OBVR/button_highlight.xml' for i in rect.findall('include')):
            assert float(highlight.findtext('y')) + float(highlight.findtext('height')) <= float(rect.findtext('height'))
print('Highlight: full native texture height, cap alignment and all button bounds verified')

assert onboarding.find('.//rect[@name="classic"]/text[@name="heading"]/string').text=="Keyboard/Gamepad + VR"
assert onboarding.find('.//rect[@name="motion"]/text[@name="heading"]/string').text=="Full VR"
