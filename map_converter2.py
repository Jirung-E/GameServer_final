# 컬러를 반영해서 변환
import os
import sys
from PIL import Image


def convert_map(input_path, output_path):
    map_contents = []

    with Image.open(input_path) as img:
        img = img.convert("RGBA")   # png
        width, height = img.size
        pixels = img.load()

        for y in range(height):
            content = ""    # 줄별로 문자열로 저장
            
            for x in range(width):
                r, g, b, a = pixels[x, y]
                if r == 0 and g == 0 and b == 0:
                    content += '#'   # 돌
                elif r == 255 and g == 255 and b == 255:
                    content += ' '   # 빈 공간
                elif r == 0 and g == 0 and b == 255:
                    content += 'W'   # Water
                elif r == 0 and g == 255 and b == 0:
                    content += 'S'   # Spawn
                else:
                    content += '#'   # 돌로 취급

            map_contents.append(content)

    with open(output_path, 'w') as f:
        for line in map_contents:
            print(''.join(line))  # 줄별로 출력
            f.write(''.join(line) + '\n')


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python map_converter.py <input_map_path> <output_map_path>")
        sys.exit(1)
    
    input_map_path = sys.argv[1]
    file_name = os.path.basename(input_map_path)

    # 파일 이름에서 확장자 제거
    file_name = os.path.splitext(file_name)[0]
    output_map_path = os.path.join(os.path.dirname(input_map_path), f"{file_name}.mapcontents")

    convert_map(input_map_path, output_map_path)

    print("Done")