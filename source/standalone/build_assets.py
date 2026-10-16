"""Generate deterministic gzip C assets; output is a build artifact, not OTA."""
from pathlib import Path
import gzip
import zopfli.gzip
import hashlib
import json
import re
import argparse
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--ota-test',action='store_true');options=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'standalone/results/sdk'
if options.ota_test:OUT=ROOT/'standalone/results/network-ota-test/sdk'
OUT.mkdir(parents=True,exist_ok=True)
html=(ROOT/'standalone/ui/index.html').read_text(encoding='utf-8')
html=html.replace('<script src="firmware.js"></script>','').replace('<script src="app.js"></script>','<script src="live.js"></script>')
# Device boot starts in setup AP; show setup even before API/JS has loaded.
html=html.replace('aria-selected="true" aria-controls="device"','aria-selected="false" aria-controls="device"')
html=html.replace('aria-selected="false" aria-controls="wifi"','aria-selected="true" aria-controls="wifi"')
html=html.replace('<section id="device" class="active"','<section id="device"')
html=html.replace('<section id="wifi" role=','<section id="wifi" class="active" role=')
if options.ota_test:
    panel='''<section id="firmware" role="tabpanel" aria-labelledby="tab-firmware"><div class="panel">
<h2 data-i18n="firmware">펌웨어</h2><div class="sensor"><span data-i18n="currentVersion">현재 버전</span><b id="current-firmware-version">2.0.14</b></div>
<div class="sensor"><span>Build ID</span><b id="current-build-id">—</b></div>
<p id="ota-hint" class="muted"></p><form id="firmware-form">
<label id="ota-file-label" for="firmware-picker"></label><input id="firmware-file" type="file" accept=".fwr" hidden>
<div class="file-picker"><button id="firmware-picker" type="button" data-i18n="chooseFirmwareFile">파일 선택</button><span id="firmware-selected" class="muted" aria-live="polite"></span></div>
<div class="row"><button id="firmware-verify" type="button" hidden></button><button id="firmware-update" class="primary" disabled></button></div></form>
<div id="firmware-status" class="status" role="status" aria-live="polite"></div></div></section>'''
    html=re.sub(r'<section id="firmware".*?</section>',lambda _:panel,html,flags=re.S)
    html=html.replace('<script src="live.js"></script>','<script src="live.js"></script><script src="ota-live.js"></script>')
# Device assets must not briefly display PC sample readings before JavaScript loads.
for field, value in {'ip':'—','mac':'—','online':'—','connected-ssid':'—',
                     'rssi':'— dBm','uptime':'—','total':'— W','current':'— A','all':'—'}.items():
    html, count = re.subn(r'(<(?:b|span) id="'+re.escape(field)+r'">)[^<]*(</(?:b|span)>)',
                         lambda match: match[1]+value+match[2], html)
    assert count == 1, 'Missing device placeholder: '+field
html=html.replace('<b>1.551 kWh</b>','<b>— kWh</b>').replace('<b>220.0 V</b>','<b>— V</b>')
html, count = re.subn(r'<div class="row"><button id="hold".*?</div>', '', html)
assert count == 1, 'Missing preview-only setup simulation'
for field in ('all-on','all-off','firmware-picker','firmware-verify'):
    html, count = re.subn(r'(<button id="'+field+r'")', r'\1 disabled', html)
    assert count == 1, 'Missing unavailable control: '+field
html=re.sub(r'<title>.*?</title>', '<title>MTTL-W01</title>',html,flags=re.S)
html=re.sub(r'<p class="note".*?</p>', '',html,flags=re.S)
html=re.sub(r'<span class="badge".*?</span>', '',html,flags=re.S)
html=html.replace('.note,','')
for before,after in [('data-i18n="scan"','data-i18n="realScan"'),('data-i18n="connect"','data-i18n="realConnect"'),('data-i18n="saveMock"','data-i18n="saveDevice"'),('Wi-Fi 검색 모의','Wi-Fi 검색'),('연결 모의','연결'),('저장 모의','기기에 저장')]:
    html=html.replace(before,after)
assert not any(sample in html for sample in ('192.0.2.10','02:00:00:00:00:01','SIMULATED_AP','1.551 kWh','220.0 V'))
html=re.sub(r'<!--.*?-->','',html,flags=re.S)
html=re.sub(r'>\s+<','><',html)
html=re.sub(r'<style>(.*?)</style>',lambda m:'<style>'+re.sub(r'\s*([{}:;,])\s*',r'\1',m[1])+'</style>',html,flags=re.S)
sources=[('/', 'text/html; charset=utf-8', html.encode()),
         ('/i18n.js','application/javascript; charset=utf-8',(ROOT/'standalone/ui/i18n.js').read_bytes()),
         ('/live.js','application/javascript; charset=utf-8',(ROOT/'standalone/ui/live.js').read_bytes())]
# Bundle editor with live controller to share gzip dictionary and avoid another
# request/callback. Preview keeps its separate script for local-file use.
html=html.replace('<script src="scheduler.js"></script>','')
html=html.replace('<script src="timer.js"></script>','')
sources[0]=('/', 'text/html; charset=utf-8', html.encode())
sources[-1]=('/live.js','application/javascript; charset=utf-8',(ROOT/'standalone/ui/scheduler.js').read_bytes()+b'\n'+(ROOT/'standalone/ui/timer.js').read_bytes()+b'\n'+sources[-1][2])
if options.ota_test:
    sources.append(('/ota-live.js','application/javascript; charset=utf-8',(ROOT/'standalone/ui/ota-live.js').read_bytes()))
# Preview-only translations do not belong in the RAM-resident device image.
# Retain literal keys in HTML/JS, plus dynamically constructed state keys.
messages=json.loads(subprocess.check_output(['node','-e',
    'console.log(JSON.stringify(require(process.argv[1]).messages))',
    str(ROOT/'standalone/ui/i18n.js')],text=True,encoding='utf-8'))
messages['ko']['saveDevice']='기기에 저장';messages['en']['saveDevice']='Save to device'
# The broad literal-key scan also sees element IDs such as 'scan'/'online'.
# Those are not calls to preview translations; explicitly exclude their text.
preview_keys={'title','simulation','note','online','hold','holdHint','scan','connect','saveMock','enabled','wifiConnected','firmwareHint','updateMock','progressLabel','firmwareStep2','firmwareStep3','inspectionPassed','uploading','rechecking','rebooting','updateComplete','log_started','log_wifi','log_fwComplete','deviceIntegration','liveNote'}
messages={lang:{key:value for key,value in values.items() if key not in preview_keys} for lang,values in messages.items()}
used='\n'.join(data.decode('utf-8') for path,kind,data in sources if path!='/i18n.js')
messages={lang:{key:value for key,value in values.items() if any(quote+key+quote in used for quote in ('"',"'")) or key.startswith('netState')}
          for lang,values in messages.items()}
# Wasel: English-only device UI. Korean costs image RAM (the image runs from SRAM) that the
# LED features need; the Korean preview files in ui/ are unchanged. WASEL_KOREAN=1 keeps it.
import os
ENGLISH_ONLY=os.environ.get('WASEL_KOREAN')!='1'
HANGUL=re.compile('[\uac00-\ud7a3\u3131-\u318e]')
if ENGLISH_ONLY:
    messages={'en':messages['en']}
    def english_js(text):
        text=re.sub(r"(?<![.\w])language(\(\))?\s*===\s*(['\"])en\2",'!0',text)
        def blank(m):return "''" if HANGUL.search(m[0]) else m[0]
        return re.sub(r"'(?:[^'\\\n]|\\.)*'|\"(?:[^\"\\\n]|\\.)*\"|`(?:[^`\\]|\\.)*`",blank,text)
    def english_html(text):
        en=messages['en']
        text=re.sub(r'(<([a-z0-9]+)[^>]*data-i18n="([A-Za-z0-9]+)"[^>]*>)([^<]*)',lambda m:m[1]+(en.get(m[3],'') if HANGUL.search(m[4]) else m[4]),text)
        text=re.sub(r'>([^<]*)<',lambda m:'><' if HANGUL.search(m[1]) else m[0],text)
        text=re.sub(r'([a-z-]+)="([^"]*)"',lambda m:m[1]+'=""' if HANGUL.search(m[2]) else m[0],text)
        text=text.replace('lang="ko"','lang="en"')
        return text.replace('</style>','#language{display:none}</style>',1)
    sources=[(path,kind,english_html(data.decode('utf-8')).encode() if path=='/' else english_js(data.decode('utf-8')).encode() if path.endswith('.js') and path!='/i18n.js' else data) for path,kind,data in sources]
compact='globalThis.MttlI18n={messages:'+json.dumps(messages,ensure_ascii=False,separators=(',',':'))+',text:(language,key,args={})=>{const value=MttlI18n.messages['+('"en"' if ENGLISH_ONLY else 'language==="en"?"en":"ko"')+'][key];if(value===undefined)throw Error("Missing translation: "+key);return value.replace(/\\{(\\w+)\\}/g,(_,name)=>String(args[name]??""));}};'
sources=[(path,kind,compact.encode() if path=='/i18n.js' else data) for path,kind,data in sources]
if ENGLISH_ONLY:assert not any(HANGUL.search(data.decode('utf-8')) for path,kind,data in sources),'Korean left in device assets'
lines=['/* Generated gzip UI assets. Do not edit by hand. */','#include <stddef.h>',
       'typedef struct { const char *path,*type; const unsigned char *data; size_t size; } mttl_asset;']
meta=[]
for i,(path,kind,data) in enumerate(sources):
    # Drop only a leading explanatory JS block comment from device assets.
    if path.endswith(".js"):
        data=re.sub(rb"^/\*.*?\*/\s*", b"", data, count=1, flags=re.S)
        data=re.sub(rb"^\s*//[^\r\n]*",b"",data,flags=re.M)
        data=subprocess.check_output(['node',str(ROOT/'standalone/minify_ui.cjs')],input=data)
    if options.ota_test:
        text=data.decode('utf-8').lower()
        assert not any(word in text for word in ('초안','모의','흐름 확인','draft','simulat','experimental')), 'Preview text in device asset: '+path
    zipped=zopfli.gzip.compress(data,numiterations=30)
    assert gzip.decompress(zipped)==data
    lines.append('static const unsigned char mttl_asset_%d[]={%s};'%(i,','.join(map(str,zipped))))
    meta.append({'path':path,'raw_bytes':len(data),'gzip_bytes':len(zipped),'sha256':hashlib.sha256(data).hexdigest()})
assert sum(x['gzip_bytes'] for x in meta)<20000,'UI asset budget exceeded'
lines.append('static const mttl_asset mttl_assets[]={'+','.join('{"%s","%s",mttl_asset_%d,sizeof(mttl_asset_%d)}'%(p,k,i,i) for i,(p,k,d) in enumerate(sources))+'};')
(OUT/'ui_assets.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(OUT/'ui_assets.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
print(json.dumps(meta,indent=2))
