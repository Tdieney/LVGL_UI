import os
import subprocess
import time
from PIL import Image

EDGE_PATH = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROTOTYPE_DIR = os.path.join(ROOT_DIR, "docs", "prototype")
INDEX_PATH = os.path.join(PROTOTYPE_DIR, "index.html")

def capture(url, out_path):
    temp_shot = out_path + ".temp.png"
    args = [
        EDGE_PATH,
        "--headless=new",
        "--disable-gpu",
        "--hide-scrollbars",
        "--force-device-scale-factor=1",
        "--window-size=800,480",
        f"--screenshot={temp_shot}",
        url
    ]
    subprocess.run(args, check=True)
    time.sleep(0.5)
    im = Image.open(temp_shot)
    if im.size != (800, 480):
        im = im.crop((0, 0, 800, 480))
    im.save(out_path, "PNG")
    if os.path.exists(temp_shot):
        os.remove(temp_shot)
    print(f"Captured {out_path} ({im.size})")

def main():
    base_url = "file:///" + INDEX_PATH.replace("\\", "/")
    targets = [
        ("home.png", f"{base_url}?capture=1&page=home&scenario=moderate&bars=4"),
        ("trends.png", f"{base_url}?capture=1&page=trends&scenario=moderate&bars=4"),
        ("devices.png", f"{base_url}?capture=1&page=devices&scenario=moderate&bars=4"),
        ("device-settings.png", f"{base_url}?capture=1&page=devices&scenario=moderate&bars=4&dialog=1"),
        ("offline.png", f"{base_url}?capture=1&page=home&scenario=offline&bars=0"),
        ("home-degraded.png", f"{base_url}?capture=1&page=home&scenario=moderate&bars=2"),
    ]
    for fname, url in targets:
        out = os.path.join(PROTOTYPE_DIR, fname)
        capture(url, out)

if __name__ == "__main__":
    main()
