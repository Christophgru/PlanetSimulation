"""Read the renderer's non-interlaced 8-bit RGB/RGBA PNGs without dependencies."""
import struct
import zlib


def read_png(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, compressed = 8, bytearray()
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + size]
        if kind == b'IHDR':
            width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', chunk)
            assert depth == 8 and color in (2, 6)
            assert compression == filtering == interlace == 0
            channels = 3 if color == 2 else 4
        elif kind == b'IDAT':
            compressed.extend(chunk)
        elif kind == b'IEND':
            break
        offset += size + 12
    raw = zlib.decompress(compressed)
    stride = width * channels
    assert len(raw) == height * (stride + 1)
    pixels, previous = bytearray(), bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind = raw[start]
        assert 0 <= kind <= 4
        row = bytearray(raw[start + 1:start + stride + 1])
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            if kind == 1:
                predictor = left
            elif kind == 2:
                predictor = up
            elif kind == 3:
                predictor = (left + up) // 2
            elif kind == 4:
                p = left + up - corner
                distances = (abs(p - left), abs(p - up), abs(p - corner))
                predictor = (left, up, corner)[distances.index(min(distances))]
            else:
                predictor = 0
            row[i] = (row[i] + predictor) & 255
        pixels.extend(row)
        previous = row
    return width, height, channels, pixels
