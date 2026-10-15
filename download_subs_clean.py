import urllib.request
import json
import re
import xml.etree.ElementTree as ET

vids = [
    ('MpSVU3t1J38', 'Part 1 Overview'),
    ('NvNtfvvK60E', 'Part 2 Custom OTA Firmware'),
    ('-LuA11BlPKY', 'Part 3 Smart Home Display')
]

headers = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
}

for vid, label in vids:
    print(f'Fetching {label} ({vid})...')
    url = f'https://www.youtube.com/watch?v={vid}'
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req) as resp:
            html = resp.read().decode('utf-8', errors='ignore')
        
        idx = html.find('captionTracks')
        if idx == -1:
            print(f'  No captionTracks in HTML for {vid}')
            continue
        
        start = html.find('[', idx)
        end = html.find(']', start) + 1
        tracks = json.loads(html[start:end])
        print(f'  Found {len(tracks)} tracks for {vid}')
        
        for t in tracks:
            lang = t.get('languageCode')
            base_url = t.get('baseUrl')
            print(f'  Downloading track {lang}: {base_url[:80]}...')
            req_tr = urllib.request.Request(base_url, headers=headers)
            with urllib.request.urlopen(req_tr) as tr_resp:
                xml_raw = tr_resp.read().decode('utf-8', errors='ignore')
            
            # Save raw XML
            xml_file = f'subtitles_{vid}_{lang}.xml'
            with open(xml_file, 'w', encoding='utf-8') as fh:
                fh.write(xml_raw)
            print(f'  Wrote XML {xml_file} ({len(xml_raw)} bytes)')
            
            # Parse text and timestamps
            root = ET.fromstring(xml_raw)
            lines = []
            for child in root:
                start_time = child.attrib.get('start', '0')
                dur = child.attrib.get('dur', '0')
                txt = child.text or ''
                txt = txt.replace('&#39;', "'").replace('&quot;', '"').replace('&amp;', '&').replace('\n', ' ')
                lines.append(f'[{float(start_time):06.1f}s] {txt}')
            
            txt_file = f'subtitles_{vid}_{lang}.txt'
            with open(txt_file, 'w', encoding='utf-8') as fh:
                fh.write('\n'.join(lines))
            print(f'  Wrote TXT {txt_file} ({len(lines)} lines)')
            break
    except Exception as e:
        print(f'  Error for {vid}: {e}')
