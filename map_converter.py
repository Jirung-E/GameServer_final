# png 맵 파일을 서버에서 사용할 수 있도록 변환하는 스크립트
import os
import sys
from PIL import Image

def convert_map(input_path, output_path):
    bit_buffer = 0
    bit_count = 0
    byte_content = bytearray()

    with Image.open(input_path) as img:
        img = img.convert("RGBA")   # png
        width, height = img.size
        pixels = img.load()

        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                if r == 0 and g == 0 and b == 0:
                    # 검은색이면 0
                    bit = 0
                else:
                    # 아니면 1
                    bit = 1
                bit_buffer = (bit_buffer << 1) | bit
                bit_count += 1

                if bit_count == 8:
                    byte_content.append(bit_buffer)
                    bit_buffer = 0
                    bit_count = 0

        # 마지막 남은 비트 처리 (빈 비트는 오른쪽에 0으로 채움)
        if bit_count > 0:
            bit_buffer <<= (8 - bit_count)
            byte_content.append(bit_buffer)

    with open(output_path, 'wb') as f:
        f.write(byte_content)
        

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python map_converter.py <input_map_path> <output_map_path>")
        sys.exit(1)
    
    input_map_path = sys.argv[1]
    file_name = os.path.basename(input_map_path)

    # 파일 이름에서 확장자 제거
    file_name = os.path.splitext(file_name)[0]
    output_map_path = os.path.join(os.path.dirname(input_map_path), f"{file_name}.map")

    convert_map(input_map_path, output_map_path)

    print("Done")