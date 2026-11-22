#!/usr/bin/env python3
"""Developer-only: regenerate the bundled guide font subset (requires fontTools).
Normal GetDAQ builds use the tracked OTF and do not need this Python package.
"""
from pathlib import Path
import html,re
from fontTools import subset
from fontTools.ttLib import TTFont
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'docs/manual'
text=''.join(html.unescape(re.sub('<[^>]+>','',p.read_text())) for p in folder.glob('GetDAQ-Quick-Guide-*.html'))+'中文 使用说明 English 0123456789'
font=TTFont(folder/'fonts/NotoSansCJKsc-Regular.otf',recalcTimestamp=False)
options=subset.Options();options.name_IDs=['*'];options.name_legacy=True;options.name_languages=['*']
subsetter=subset.Subsetter(options=options);subsetter.populate(unicodes=set(map(ord,text)));subsetter.subset(font)
family='GetDAQ Guide Sans'
for entry in font['name'].names:
 if entry.nameID in (1,4,6,16):
  value={1:family,4:family+' Regular',6:'GetDAQGuideSans-Regular',16:family}[entry.nameID];entry.string=value.encode(entry.getEncoding(),errors='replace')
if 'CFF ' in font:
 cff=font['CFF '].cff;cff.fontNames=['GetDAQGuideSans-Regular'];cff.topDictIndex[0].FamilyName=family;cff.topDictIndex[0].FullName=family+' Regular'
output=folder/'fonts/GetDAQGuideSans.otf';font.save(output);print('Generated',output.name,output.stat().st_size,'bytes')
