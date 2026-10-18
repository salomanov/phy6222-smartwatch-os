import sys
import json
from pathlib import Path
from graphify.extract import collect_files, extract

code_files = []
detect = json.loads(Path('graphify-out/.graphify_detect.json').read_text(encoding='utf-8'))
for f in detect.get('files', {}).get('code', []):
    p = Path(f)
    code_files.extend(collect_files(p) if p.is_dir() else [p])

print(f"Starting AST extraction for {len(code_files)} code files...")
result = extract(code_files, cache_root=Path('.'), root=Path('.'))
Path('graphify-out/.graphify_ast.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
print(f"AST done: {len(result['nodes'])} nodes, {len(result['edges'])} edges")
