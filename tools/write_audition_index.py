#!/usr/bin/env python3
"""Write a self-contained local listening index for packaged real-engine WAVs."""
from __future__ import annotations
import html
import json
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
FAMILIES = ['Bloom Bark', 'Formant Snarl', 'Upper Chop', 'Split Maul', 'Crushed Teeth', 'Harmonic Rip']
MODES = ['Native procedural', 'Source image', 'Hybrid blend']
TUNINGS = ['12 EDO', '19 EDO', '24 EDO', '13-step tritave']


def write_index(destination: Path) -> int:
    entries = []
    recipes = json.loads((ROOT/'presets/factory.json').read_text())
    for index, recipe in enumerate(recipes):
        path = destination/'auditions'/f'preset-{index+1:02d}.wav'
        if path.is_file():
            p = recipe['patch']
            entries.append(dict(path=path, group='Factory sounds', name=recipe['name'],
                description=recipe['description'], tags=[FAMILIES[p['family']], MODES[p['mode']], TUNINGS[p['tuning']]]))
    songs = destination/'fune-songs'/'manifest.json'
    if songs.is_file():
        for song in json.loads(songs.read_text())['songs']:
            path = songs.parent/song['audio']
            if path.is_file():
                entries.append(dict(path=path, group='Fune songs', name=song['name'],
                    description=song['description'], tags=[f"{song['bpm']} BPM", 'Eight scenes', 'Actual Fune sequence']))
    for path in sorted((destination/'architecture-comparison').glob('*.wav')):
        number, _, identity = path.stem.partition('-')
        source_names = ['Locked Bloom', *FAMILIES]
        source_name = source_names[int(number)] if number.isdigit() and int(number) < len(source_names) else number
        name = source_name + ' / ' + identity.replace('-', ' ').replace('_', ' ')
        entries.append(dict(path=path, group='Source comparisons', name=name,
            description='Reference and implementation comparison. The source filenames retain their comparison identities.',
            tags=['Fixed comparison gain']))
    if not entries: return 0
    counts = {group: sum(e['group'] == group for e in entries)
              for group in ['Factory sounds', 'Fune songs', 'Source comparisons']}
    buttons = '<button class="filter active" data-group="all" aria-pressed="true">All recordings</button>'
    for group, count in counts.items():
        if count:
            buttons += f'<button class="filter" data-group="{group}" aria-pressed="false">{group} <span>{count}</span></button>'
    cards = []
    for entry in entries:
        uri = quote(entry['path'].relative_to(destination).as_posix(), safe='/')
        tags = ''.join(f'<span>{html.escape(tag)}</span>' for tag in entry['tags'])
        search = html.escape(' '.join([entry['name'], entry['description'], *entry['tags']]).lower(), quote=True)
        cards.append(f'''<article data-group="{entry['group']}" data-search="{search}">
<div class="category">{entry['group']}</div><h2>{html.escape(entry['name'])}</h2>
<p>{html.escape(entry['description'])}</p><div class="tags">{tags}</div>
<audio controls preload="none" src="{uri}" aria-label="{html.escape(entry['name'], quote=True)}"></audio>
<a class="download" href="{uri}" download>Save WAV</a></article>''')
    page = '''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ZaaggenZ-FM1 — Listen</title><style>
:root{color-scheme:dark;--ink:#eaf1f4;--muted:#abbec5;--accent:#6addd3;--panel:#18222b}
*{box-sizing:border-box}body{margin:0;background:#10171d;color:var(--ink);font:16px/1.5 system-ui,Segoe UI,sans-serif}
main{max-width:1220px;margin:auto;padding:44px 28px 60px}header{border-bottom:1px solid #33434d;padding-bottom:26px}
.eyebrow{color:var(--accent);font-size:12px;font-weight:700;letter-spacing:.18em}h1{font-size:clamp(34px,6vw,64px);line-height:1.06;margin:12px 0 18px;letter-spacing:-.04em}
header p{max-width:760px;color:var(--muted);margin:0}nav{display:flex;gap:9px;flex-wrap:wrap;margin:26px 0 18px}
button,input{font:inherit;border:1px solid #465863;border-radius:7px;background:var(--panel);color:var(--ink)}button{padding:9px 14px;cursor:pointer}
button span{color:var(--muted);margin-left:4px}button.active{background:var(--accent);color:#102027;border-color:var(--accent)}button.active span{color:#234149}
button:focus-visible,input:focus-visible,a:focus-visible,audio:focus-visible{outline:3px solid #8dece3;outline-offset:3px}
.search-row{display:flex;gap:16px;align-items:center;margin-bottom:22px;flex-wrap:wrap}label{color:var(--muted)}input{padding:11px 14px;flex:1;min-width:220px}#count{color:var(--muted);font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,320px),1fr));gap:16px}article{background:var(--panel);border:1px solid #2e404b;border-radius:12px;padding:23px;display:flex;flex-direction:column}
article[hidden]{display:none}.category{font-size:11px;text-transform:uppercase;letter-spacing:.12em;color:var(--accent)}h2{font-size:22px;margin:8px 0 9px;line-height:1.25}
article p{color:var(--muted);font-size:14px;margin:0 0 18px;flex:1}.tags{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:18px}.tags span{font-size:11px;color:#b8cbd3;border:1px solid #3d515d;border-radius:4px;padding:3px 6px}
audio{width:100%;height:42px}.download{margin-top:12px;font-size:12px;color:var(--accent);align-self:flex-start;text-decoration:none}.download:hover{text-decoration:underline}
footer{color:var(--muted);font-size:13px;border-top:1px solid #33434d;margin-top:32px;padding-top:20px;max-width:940px}#empty{color:var(--muted);padding:24px 0}
</style></head><body><main><header><div class="eyebrow">ZAAGGENZ / FM-1</div><h1>Hear the instrument.</h1>
<p>Choose a sound, explore a different character, then try it in the instrument. Factory sounds use the actual C engine; songs run through Fune. Source comparisons also include the authenticated desktop reference and comparison models. Playback works locally from this extracted folder.</p>
</header><nav aria-label="Recording categories">''' + buttons + '''</nav>
<div class="search-row"><label for="search">Find a sound</label><input id="search" type="search" placeholder="Try snarl, pedal, native or tritave" autocomplete="off"><span id="count" role="status"></span></div>
<section class="grid" aria-label="Audio recordings">''' + '\n'.join(cards) + '''</section><p id="empty" hidden>No matching sounds. Try another family or clear the search.</p>
<footer>Levels are preserved across each rendering workflow; files are not individually loudness-normalized. Start at a comfortable playback volume. Source comparisons include phase-sensitive reference tests as well as musical alternatives. These are software renders. The playing and installation guides in the package explain source behavior, tuning and hardware evaluation.</footer>
</main><script>
const cards=Array.from(document.querySelectorAll('article')),search=document.querySelector('#search');let group='all';
function filter(){const query=search.value.toLowerCase().trim();let visible=0;for(const card of cards){card.hidden=(group!=='all'&&card.dataset.group!==group)||!card.dataset.search.includes(query);if(!card.hidden)visible++;else card.querySelector('audio').pause();}document.querySelector('#count').textContent=visible+' recordings';document.querySelector('#empty').hidden=visible!==0;}
for(const button of document.querySelectorAll('.filter'))button.addEventListener('click',()=>{group=button.dataset.group;for(const b of document.querySelectorAll('.filter')){const active=b===button;b.classList.toggle('active',active);b.setAttribute('aria-pressed',String(active));}filter();});
search.addEventListener('input',filter);document.addEventListener('play',event=>{if(event.target.tagName==='AUDIO')for(const audio of document.querySelectorAll('audio'))if(audio!==event.target)audio.pause();},true);filter();
</script></body></html>
'''
    (destination/'Listen.html').write_bytes(page.encode('utf-8'))
    return len(entries)


if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('destination',type=Path)
    print(write_index(parser.parse_args().destination), 'recordings indexed')
