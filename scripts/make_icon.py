from PIL import Image, ImageDraw

S = 256
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

d.rounded_rectangle([8, 8, S - 8, S - 8], radius=52, fill=(15, 17, 21, 255), outline=(79, 140, 255, 255), width=8)

d.arc([92, 50, 164, 140], start=180, end=360, fill=(232, 234, 237, 255), width=16)
d.rounded_rectangle([60, 94, 196, 214], radius=24, fill=(79, 140, 255, 255))

d.rectangle([122, 118, 134, 158], fill=(255, 255, 255, 255))
d.polygon([(102, 154), (154, 154), (128, 190)], fill=(255, 255, 255, 255))

img.save("res/backupstore.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
img.save("res/backupstore.png")
print("icon written")
