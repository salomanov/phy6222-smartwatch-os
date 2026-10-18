import urllib.request
import json
import re
import html

vids = [
    ('MpSVU3t1J38', 'Video 1'),
    ('NvNtfvvK60E', 'Video 2'),
    ('-LuA11BlPKY', 'Video 3')
]

headers = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    'Accept-Language': 'en-US,en;q=0.9'
}

for vid, label in vids:
    url = f'https://www.youtube.com/watch?v={vid}'
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=15) as resp:
            page = resp.read().decode('utf-8', errors='ignore')
        
        m_title = re.search(r'<title>(.*?)</title>', page)
        title = m_title.group(1) if m_title else vid
        print(f'=== {label}: {title} ===')
        
        idx = page.find('captionTracks')
        if idx != -1:
            start = page.find('[', idx)
            end = page.find(']', start) + 1
            cap_data = json.loads(page[start:end])
            for track in cap_data:
                lang = track.get('languageCode')
                b_url = track.get('baseUrl')
                req_t = urllib.request.Request(b_url, headers=headers)
                with urllib.request.urlopen(req_t, timeout=15) as tr_resp:
                    xml_data = tr_resp.read().decode('utf-8', errors='ignore')
                
                texts = re.findall(r'<text[^>]*>(.*?)</text>', xml_data)
                clean_text = ' '.join(texts)
                clean_text = html.unescape(clean_text)
                out_name = f'youtube_{vid}_{lang}.txt'
                with open(out_name, 'w', encoding='utf-8') as fh:
                    fh.write(clean_text)
                print(f'  Saved subtitles ({lang}): {out_name} ({len(clean_text)} chars)')
                break
        else:
            print('  No captions found.')
    except Exception as e:
        print(f'  Error: {e}')
