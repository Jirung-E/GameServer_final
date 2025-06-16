# 이미지로 되어있는 map에 랜덤하게 장애물 설치

import os
import sys
from PIL import Image
import random

def put_obstacle_on_map(input_path, output_path):
    with Image.open(input_path) as img:
        img = img.convert("RGBA")   # png
        width, height = img.size
        pixels = img.load()

        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                # 하얀부분을 랜덤하게 검은색으로 변경
                if r == 255 and g == 255 and b == 255:
                    # 0.5% 확률로 장애물 설치
                    if random.random() < 0.001:
                        pixels[x, y] = (0, 0, 0, 255)

        img.save(output_path, format="PNG")
        

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python map_dot.py <input_map_path>")
        sys.exit(1)
    
    input_map_path = sys.argv[1]
    file_name = os.path.basename(input_map_path)

    # 파일 이름에서 확장자 제거
    file_name = os.path.splitext(file_name)[0]
    output_map_path = os.path.join(os.path.dirname(input_map_path), f"{file_name}_with_obstacles.png")

    put_obstacle_on_map(input_map_path, output_map_path)

    print("Done")