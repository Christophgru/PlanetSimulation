"""Real X11 input against the production GLFW loop in the private probe."""
import json
import os
from pathlib import Path
import subprocess
import time


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


class Session:
    def __init__(self, probe, root, output, flags=(), environment=None, control=None):
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        self.trace_path = output / 'frames.jsonl'
        self.trace_path.unlink(missing_ok=True)
        self.control_path = output / 'control.json'
        write_json(self.control_path, control or {})
        self.log = (output / 'run.log').open('w')
        self.frames = []
        self.offset = 0
        self.read_error = None
        env = {**os.environ, **(environment or {}),
               'PLANET_NATIVE_TRACE': str(self.trace_path),
               'PLANET_NATIVE_CONTROL': str(self.control_path)}
        self.process = subprocess.Popen([str(probe.resolve()), *flags], cwd=root,
                                        stdout=self.log, stderr=subprocess.STDOUT, env=env)
        self.window = None

    def text(self):
        return (self.output / 'run.log').read_text()

    def read(self):
        if self.trace_path.exists():
            with self.trace_path.open('rb') as source:
                source.seek(self.offset)
                data = source.read()
            # Growing-file snapshots can expose transient bytes inside a record.
            # Commit only decoded complete rows; retry the remainder next poll.
            # close() compares every observed frame with the finished trace.
            self.read_error = None
            for row in data.splitlines(keepends=True):
                if not row.endswith(b'\n'):
                    break
                try:
                    frame = json.loads(row)
                except (ValueError, UnicodeDecodeError) as error:
                    self.read_error = str(error)
                    break
                assert frame['frame'] == len(self.frames) + 1, frame
                self.frames.append(frame)
                self.offset += len(row)
        return self.frames

    def wait(self, predicate, message='Native condition did not complete', timeout=40):
        end = time.monotonic() + timeout
        start = len(self.frames)
        while time.monotonic() < end:
            for frame in self.read()[start:]:
                if predicate(frame):
                    return frame
            start = len(self.frames)
            assert self.process.poll() is None, f'Native exit {self.process.returncode}:\n' + self.text()
            time.sleep(.025)
        raise AssertionError(message + '\nTrace read error: ' + str(self.read_error) + '\n' + self.text()[-6000:])

    def focus(self):
        self.wait(lambda frame: True, 'No native frames')
        found = subprocess.check_output(['xdotool', 'search', '--onlyvisible', '--pid', str(self.process.pid)])
        self.window = found.splitlines()[0].decode()
        subprocess.run(['xdotool', 'windowfocus', '--sync', self.window], check=True)

    def key(self, value):
        subprocess.run(['xdotool', 'key', value], check=True)

    def down(self, value):
        subprocess.run(['xdotool', 'keydown', value], check=True)

    def up(self, value):
        subprocess.run(['xdotool', 'keyup', value], check=True)

    def control(self, value):
        write_json(self.control_path, value)
        return self.wait(lambda frame: frame['controls'] == value, 'Probe control was not observed')

    def close(self):
        if self.process.poll() is None:
            # A close event may stop the loop before another presentation row.
            # Join the process, then validate its complete trace and exit audit.
            write_json(self.control_path, {'close': True})
            self.process.wait(timeout=8)
        self.read()
        assert self.offset == self.trace_path.stat().st_size, self.read_error
        final_frames = [json.loads(row) for row in self.trace_path.read_bytes().splitlines()]
        assert final_frames == self.frames, 'Live observations differ from the finished native trace'
        assert self.process.returncode == 0, self.text()
        assert 'Native audit:' in self.text(), self.text()
        self.log.close()

    def abort(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.log.close()


def validate(frames, managed=True):
    rendered = []
    for frame in frames:
        assert frame['backend'] == ('compute' if managed else 'cpu'), frame
        assert frame['publication']['managed'] == managed, frame
        assert all(frame['worker'][k] <= 1 for k in ('running', 'queued', 'ready')), frame
        assert all(frame['gl'][k] == 0 for k in ('blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries')), frame['gl']
        if not managed or frame['publication']['loading']:
            continue
        rendered.append(frame)
        publication = frame['publication']
        assert publication['peak_reserved_bytes'] <= 1024**3
        for consumer in publication['consumers']:
            assert consumer['epoch'] == frame['reload']['epoch'], consumer
            assert consumer['land'] == consumer['grass'] == consumer['contacts'], consumer
            assert consumer['land_revision'] == consumer['grass_revision'] == consumer['main_revision'], consumer
            if consumer['shadows_enabled']:
                assert consumer['shadow_revision'] == consumer['land_revision'], consumer
            assert consumer['reflection_revision'] in (0, consumer['land_revision']), consumer
            assert consumer['grass_draw_revision'] in (0, consumer['grass_revision']), consumer
            if consumer['water_enabled']:
                assert consumer['water_revision'] == consumer['water_draw_revision'], consumer
        if frame['mode'] == 3 and frame.get('pose'):
            assert publication['astronaut_contact_revision'] == publication['consumers'][frame['selected']]['land_revision'], frame
    assert rendered or not managed
    if managed:
        assert frames[-1]['gl']['polls'] > 0
    return {'frames': len(frames), 'rendered': len(rendered), 'gl': frames[-1]['gl'],
            'peak_logical_bytes': max((f['publication'].get('peak_reserved_bytes', 0) for f in frames), default=0)}
