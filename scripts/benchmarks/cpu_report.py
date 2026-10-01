#!/usr/bin/env python3
"""Turn --cpu-trace JSON into a chronological view and inclusive/self reports.

Only explicitly instrumented scopes are represented. Self time includes any
uninstrumented callees, driver waits and tracing overhead. Worker timelines stay
separate; their elapsed times must not be added to render-thread latency.
"""
import argparse
from collections import defaultdict
import csv
from html import escape
import json
import math
from pathlib import Path


def analyze(document, skip_frames=0, frame_count=None):
    names = {e['tid']: e['args']['name'] for e in document['traceEvents']
             if e.get('ph') == 'M' and e.get('name') == 'thread_name'}
    events = [dict(e) for e in document['traceEvents'] if e.get('ph') == 'X']
    for e in events:
        if not all(math.isfinite(e[k]) and e[k] >= 0 for k in ('ts', 'dur')):
            raise ValueError('Invalid event timestamp/duration')
    # Restrict to complete render frames, preserving their nested calls. For a
    # sliced report, workers crossing the boundary are excluded, never clipped
    # into an invented thread CPU measurement.
    frames = sorted((e for e in events if e['name'] in ('capture.frame', 'interactive.frame')),
                    key=lambda e: e['ts'])
    if skip_frames or frame_count is not None:
        chosen = frames[skip_frames:None if frame_count is None else skip_frames + frame_count]
        if not chosen:
            raise ValueError('No frames in requested range')
        lo, hi = chosen[0]['ts'], chosen[-1]['ts'] + chosen[-1]['dur']
        frame_threads = {e['tid'] for e in frames}
        crossing_workers = defaultdict(list)
        for e in events:
            if e['tid'] not in frame_threads and (e['ts'] < lo or e['ts'] + e['dur'] > hi):
                crossing_workers[e['tid']].append(e)
        # A child wholly inside the slice is still part of an incomplete worker
        # job. Drop that subtree so it cannot masquerade as a complete job.
        events = [e for e in events if e['ts'] >= lo and e['ts'] + e['dur'] <= hi
                  and not any(p['ts'] <= e['ts'] and e['ts'] + e['dur'] <= p['ts'] + p['dur']
                              for p in crossing_workers[e['tid']])]
    threads = defaultdict(list)
    for e in events:
        threads[e['tid']].append(e)
    rows, paths, reverse = {}, {}, {}
    for tid, group in threads.items():
        stack = []
        for e in sorted(group, key=lambda e: (e['ts'], -e['dur'])):
            while stack and e['ts'] >= stack[-1]['ts'] + stack[-1]['dur']:
                stack.pop()
            if stack and e['ts'] + e['dur'] > stack[-1]['ts'] + stack[-1]['dur'] + .001:
                raise ValueError('Crossing scopes on one thread')
            e['depth'] = len(stack)
            e['path'] = [p['name'] for p in stack] + [e['name']]
            e['self_us'] = e['dur']
            e['cpu_us'] = e.get('args', {}).get('thread_cpu_us')
            e['self_cpu_us'] = e['cpu_us']
            if e['cpu_us'] is not None and (not math.isfinite(e['cpu_us']) or e['cpu_us'] < 0):
                raise ValueError('Invalid thread CPU duration')
            if stack:
                parent = stack[-1]
                parent['self_us'] -= e['dur']
                if parent['self_cpu_us'] is not None:
                    parent['self_cpu_us'] = (parent['self_cpu_us'] - e['cpu_us']
                                             if e['cpu_us'] is not None else None)
            stack.append(e)
        for e in group:
            e['self_us'] = max(0, e['self_us'])
            if e['self_cpu_us'] is not None:
                e['self_cpu_us'] = max(0, e['self_cpu_us'])
            thread = f'{names.get(tid, "thread")} [{tid}]'
            for target, key in ((rows, (thread, e['name'])),
                                (paths, (thread, tuple(e['path'])))):
                row = target.setdefault(key, dict(calls=0, inclusive_ms=0, self_ms=0,
                                                  thread_cpu_ms=0, self_thread_cpu_ms=0))
                row['calls'] += 1
                row['inclusive_ms'] += e['dur'] / 1000
                row['self_ms'] += e['self_us'] / 1000
                for field, value in (('thread_cpu_ms', e['cpu_us']),
                                     ('self_thread_cpu_ms', e['self_cpu_us'])):
                    row[field] = row[field] + value / 1000 if row[field] is not None and value is not None else None
            # Attribute each self interval to its callee-first caller chain.
            # This partitions scope time without counting ancestors twice.
            key = (thread, tuple(reversed(e['path'])))
            reverse[key] = reverse.get(key, 0) + e['self_us'] / 1000
    return events, rows, paths, reverse


def timeline(events, title):
    lo = min(e['ts'] for e in events)
    hi = max(e['ts'] + e['dur'] for e in events)
    span = max(1, hi - lo)
    lanes, y = {}, 75
    for tid in sorted({e['tid'] for e in events}):
        for depth in range(1 + max(e['depth'] for e in events if e['tid'] == tid)):
            lanes[(tid, depth)] = y
            y += 27
        y += 15
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{y+25}" viewBox="0 0 1200 {y+25}">',
           '<rect width="100%" height="100%" fill="#f8fafc"/>',
           '<g font-family="sans-serif" font-size="12" fill="#16324f">',
           f'<text x="20" y="25" font-size="18">{escape(title)}</text>']
    for n in range(6):
        x = 130 + n * 208
        anchor = 'end' if n == 5 else 'start'
        svg += [f'<text x="{x}" y="52" text-anchor="{anchor}">{span*n/5000:.1f} ms</text>',
                f'<path d="M{x},60 V{y}" stroke="#dde4ec"/>']
    colors = ['#2563eb', '#0f766e', '#b45309', '#7c3aed', '#be123c', '#0369a1']
    for (tid, depth), lane in lanes.items():
        svg.append(f'<text x="10" y="{lane+16}">T{tid} / depth {depth}</text>')
    for e in sorted(events, key=lambda e: (e['tid'], e['depth'], e['ts'])):
        x, width = 130 + (e['ts'] - lo) / span * 1040, e['dur'] / span * 1040
        lane = lanes[(e['tid'], e['depth'])]
        label = f"{e['name']}: {e['dur']/1000:.3f} ms; self {e['self_us']/1000:.3f} ms"
        svg.append(f'<rect x="{x:.3f}" y="{lane}" width="{max(.3,width):.3f}" height="22" fill="{colors[e["depth"]%len(colors)]}"><title>{escape(label)}</title></rect>')
        if width > 60:
            labels = {'TerrainSurface::buildGeometryForEye': 'terrain.build',
                      'TerrainSurface::refineShoreline': 'shoreline.refine',
                      'Renderer::preparePlanetMeshes': 'mesh.prepare',
                      'terrain.build_land_and_water': 'land + water',
                      'GrassRenderer::draw': 'grass.draw'}
            short = labels.get(e['name'], e['name'])[:max(1, int(width/7)-2)]
            svg.append(f'<text x="{x+4:.3f}" y="{lane+15}" fill="white">{escape(short)}</text>')
    return ''.join(svg) + '</g></svg>'


def bottom_up(rows, title):
    ordered = sorted(rows.items(), key=lambda item: item[1]['self_ms'], reverse=True)[:14]
    maximum = max(.001, *(row['self_ms'] for _, row in ordered))
    height = 90 + 30 * len(ordered)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}">',
           '<rect width="100%" height="100%" fill="#f8fafc"/>',
           '<g font-family="sans-serif" font-size="13" fill="#16324f">',
           f'<text x="20" y="27" font-size="18">{escape(title)}</text>',
           '<text x="20" y="49">Self wall time (ms). Includes uninstrumented calls and waits; each thread is counted separately.</text>']
    for i, ((thread, name), row) in enumerate(ordered):
        y = 72 + i*30
        label = escape(f'{name} · {thread}')
        svg += [f'<text x="20" y="{y+15}">{label}</text>',
                f'<rect x="600" y="{y}" height="21" width="{row["self_ms"]/maximum*490:.2f}" fill="#0f766e"/>',
                f'<text x="1100" y="{y+15}">{row["self_ms"]:.2f}</text>']
    return ''.join(svg) + '</g></svg>'


def tree_html(entries, bottom=False):
    tree = {}
    for (thread, path), value in entries.items():
        node = tree.setdefault(thread, {})
        for name in path:
            node = node.setdefault(name, {})
        node[None] = value

    def render(nodes):
        out = []
        for name, children in nodes.items():
            if name is None:
                continue
            value = children.get(None)
            detail = ''
            if value is not None:
                detail = (f' — {value:.3f} ms self attributed to this caller path' if bottom else
                          f' — {value["calls"]} calls, {value["inclusive_ms"]:.3f} ms inclusive, {value["self_ms"]:.3f} ms self')
            out.append(f'<details open><summary>{escape(name + detail)}</summary><div class="branch">{render(children)}</div></details>')
        return ''.join(out)
    return render(tree)


def write_report(document, out, skip_frames=0, frame_count=None):
    events, rows, paths, reverse = analyze(document, skip_frames, frame_count)
    if not events:
        raise ValueError('Trace has no complete events')
    out.mkdir(parents=True, exist_ok=True)
    top = timeline(events, 'Top-down CPU scope timeline')
    bottom = bottom_up(rows, 'Bottom-up cost by instrumented scope')
    (out/'top-down.svg').write_text(top)
    (out/'bottom-up.svg').write_text(bottom)
    fields = ['thread', 'scope', 'calls', 'inclusive_ms', 'self_ms', 'thread_cpu_ms', 'self_thread_cpu_ms']
    with (out/'scopes.csv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for (thread, name), row in sorted(rows.items(), key=lambda item: -item[1]['self_ms']):
            writer.writerow(dict(thread=thread, scope=name, **row))
    table = '<table><tr>' + ''.join(f'<th>{escape(f)}</th>' for f in fields) + '</tr>'
    for (thread, name), row in sorted(rows.items(), key=lambda item: -item[1]['self_ms']):
        values = [thread, name] + [row[f] for f in fields[2:]]
        table += '<tr>' + ''.join(f'<td>{escape("N/A" if v is None else format(v,".3f") if isinstance(v,float) else str(v))}</td>' for v in values) + '</tr>'
    table += '</table>'
    html = f'''<!doctype html><meta charset="utf-8"><title>CPU profiling report</title>
<style>body{{font:15px system-ui;margin:32px;color:#16324f}}svg{{max-width:100%;height:auto}}.branch{{margin-left:22px}}summary{{cursor:pointer;padding:3px}}table{{border-collapse:collapse}}td,th{{text-align:left;padding:7px;border-bottom:1px solid #ddd}}</style>
<h1>CPU profiling report</h1><p>{len(events)} complete scopes. Frame selection: skip {skip_frames}, count {frame_count if frame_count is not None else 'all remaining'}.</p>
<p>Wall times include driver blocking and scheduling delays. Thread CPU times measure only the scoped thread, excluding driver workers. Inclusive values overlap their descendants; self values subtract immediate children. Uninstrumented work and trace overhead remain in self time. Separate worker lanes can overlap. No GPU execution time is inferred here. Constructor startup precedes this trace.</p>
<h2>Top-down: when scopes run</h2>{top}<h2>Top-down: call paths</h2>{tree_html(paths)}
<h2>Bottom-up: self time</h2>{bottom}{table}<h2>Bottom-up: callee to callers</h2>{tree_html(reverse, True)}'''
    (out/'report.html').write_text(html)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--skip-frames', type=int, default=0)
    parser.add_argument('--frames', type=int)
    args = parser.parse_args()
    if args.skip_frames < 0 or (args.frames is not None and args.frames < 1):
        parser.error('Frame skip must be nonnegative and count positive')
    write_report(json.loads(args.trace.read_text()), args.output_dir, args.skip_frames, args.frames)
    print(args.output_dir/'report.html')


if __name__ == '__main__':
    main()
