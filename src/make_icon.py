import struct

def create_ico(filename):
    sizes = [16, 32, 48, 64]
    images_data = []
    
    for s in sizes:
        # Create uncompressed 32-bit BGRA bitmap
        bih_size = 40
        width = s
        height = s * 2 # In ICO format, height in BITMAPINFOHEADER is doubled for XOR + AND mask
        planes = 1
        bpp = 32
        compression = 0
        img_size = s * s * 4
        
        bih = struct.pack('<IIIHHIIIIII', bih_size, width, height, planes, bpp, compression, img_size, 0, 0, 0, 0)
        
        # Generate pixel data: Neon cyan lightning bolt with dark background
        pixels = bytearray()
        for y in range(s):
            for x in range(s):
                # Distance from center
                cx, cy = s / 2.0, s / 2.0
                dx, dy = (x - cx) / cx, (y - cy) / cy
                dist = (dx*dx + dy*dy)**0.5
                
                # Lightning bolt shape approximation
                norm_y = y / float(s)
                norm_x = x / float(s)
                
                is_bolt = False
                if norm_y < 0.5:
                    if abs(norm_x - (0.65 - norm_y * 0.4)) < 0.18:
                        is_bolt = True
                else:
                    if abs(norm_x - (0.55 - (norm_y - 0.5) * 0.4)) < 0.18:
                        is_bolt = True
                
                if is_bolt:
                    # Neon cyan / electric blue
                    b = 255
                    g = 240
                    r = 0
                    a = 255
                elif dist < 0.95:
                    # Deep dark obsidian rounded card
                    b = 32
                    g = 20
                    r = 14
                    a = 255
                elif dist < 1.0:
                    # Cyan outline border
                    b = 255
                    g = 200
                    r = 0
                    a = 200
                else:
                    # Transparent
                    b = g = r = a = 0
                
                pixels.extend([b, g, r, a])
        
        # AND mask (1 bit per pixel, 32-bit aligned per row)
        row_bytes = ((s + 31) // 32) * 4
        and_mask = bytearray(row_bytes * s) # All zeros = fully opaque per alpha channel
        
        images_data.append((s, bih + bytes(pixels) + bytes(and_mask)))
    
    # Write ICO header
    # ICONDIR: 2 bytes reserved (0), 2 bytes type (1=icon), 2 bytes image count
    header = struct.pack('<HHH', 0, 1, len(sizes))
    
    # Calculate offsets
    entries = []
    current_offset = 6 + (len(sizes) * 16)
    
    for s, data in images_data:
        data_len = len(data)
        # ICONDIRENTRY: width, height, colors(0), reserved(0), planes(1), bpp(32), size, offset
        w_byte = 0 if s >= 256 else s
        h_byte = 0 if s >= 256 else s
        entry = struct.pack('<BBBBHHII', w_byte, h_byte, 0, 0, 1, 32, data_len, current_offset)
        entries.append(entry)
        current_offset += data_len
    
    with open(filename, 'wb') as f:
        f.write(header)
        for e in entries:
            f.write(e)
        for _, data in images_data:
            f.write(data)

create_ico(r'C:\Users\Lenovo\.gemini\antigravity\scratch\laptop-optimizer\src\app_icon.ico')
print('Icon generated successfully')
