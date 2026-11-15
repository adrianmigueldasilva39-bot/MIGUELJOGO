import struct
def make_sfo():
    title = "Slither.io"
    with open("PARAM.SFO", "wb") as f:
        f.write(b'\x00PSF\x01\x01\x00\x00')
        f.write(b'\x00\x00\x00\x00\x00\x00\x00\x00')
        f.write(title.encode() + b'\x00')
    print("PARAM.SFO criado: " + title)
if __name__ == "__main__":
    make_sfo()