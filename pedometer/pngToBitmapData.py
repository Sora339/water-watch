# -*- coding:utf-8 -*-
import numpy as np
from PIL import Image
import sys
import glob

IMG_FOLDER_PATH = "./.img/*"
SAVE_FILE_PATH  = "./ImgData.h"


def main(outstr):
    img_list = sorted(glob.glob(IMG_FOLDER_PATH)) # 画像リストを取得

    # 画像がない場合は終了
    if len(img_list) == 0:
        return print("No image File!")
    
    f = open(SAVE_FILE_PATH, 'w')

    file_count = 0
    for fn in img_list:
        file_count += 1
        print("loading..." + fn)

        # 画像ファイル読み込み
        image = Image.open(fn)
        width, height = image.size

        # 最初の画像の時に画像サイズなどの情報を書き込む
        if file_count == 1:
            header_str  = "const int IMG_WIDTH  = {};\n".format(width)
            header_str += "const int IMG_HEIGHT = {};\n".format(height)
            header_str += "const int IMG_MAX    = {};\n\n".format(len(img_list))
            header_str += "const unsigned short IMG_DATA[IMG_MAX][IMG_WIDTH*IMG_HEIGHT] = {\n"
            f.write(header_str)

        # 画像の色配列情報の文字列を取得して書き込む
        image_hex = outputColorPixel(width, height, image, outstr)
        f.write("    {\n")
        f.write(image_hex)

        if file_count != len(img_list):
            f.write("    },\n")
        else:
            f.write("    }\n")

    # 最後の括弧とセミコロンを書き込んで閉じる
    f.write("};\n")
    f.close()

    print("saved " + SAVE_FILE_PATH)


# RGB値を16bit(RGB565)のテキストに変換する
def rgb2hexstr(rgb):
    col = ((rgb[0]>>3)<<11) | ((rgb[1]>>2)<<5) | (rgb[2]>>3)
    return "0x{:04X}".format(col)


# RGB値を8bit(RGB332)のテキストに変換する
def rgb8bithexstr(rgb):
    col = ((rgb[0]>>5)<<5) | ((rgb[1]>>5)<<2) | (rgb[2]>>6)
    return "0x{:02X}".format(col)


# 8, 16bpp出力
def outputColorPixel(width, height, image, outstr):
    result_str = ""

    #  パレット読み込み
    if image.mode == 'P':
        palette = np.array(image.getpalette()).reshape(-1, 3)  # n行3列に変換
        getPixel = lambda x,y: palette[image.getpixel((x, y))]
    else:
        getPixel = lambda x,y: image.getpixel((x, y))

    # 書き込み時の改行位置を計算
    line_break = 16
    while True:
        if (width % line_break) == 0:
            break
        line_break = line_break - 1

    for y in range(height):
        x_cnt = 0
        for x in range(width):
            #  行の先頭に空白を入れる
            if x_cnt == 0:
                result_str += "    "
            pixel = getPixel(x, y)
            result_str += outstr(pixel) + ","
            x_cnt = x_cnt + 1
            if x_cnt >= line_break:
                x_cnt = 0
                result_str += "\n"

    return result_str[:-2] + "\n"


if __name__ == '__main__':
    args = sys.argv

    outstr = rgb2hexstr # 16bit(RGB565の場合)
    if len(args) > 1:
        if "8" in args:
            outstr = rgb8bithexstr # 8bit(RGB332の場合)
            print("RGB332(8bit)")
        else:
            print("RGB565(16bit)")
    else:
        print("RGB565(16bit)")

    main(outstr)