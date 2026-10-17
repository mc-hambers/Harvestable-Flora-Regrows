RELATIVE_PATH = 'vendor/prebuilt/commonlibsse-ng-prebuilt-v12.1.0-all-msvc-cmake/lib/CommonLibSSE.lib'
EXPECTED_HASH = '666eb00b048390e3bbb0f39865e9774c0eb8bdad21043997d9e581933b3fbba1'
from pathlib import Path
import argparse,hashlib,lzma,os,tempfile
parser=argparse.ArgumentParser(description="Restore the matching precompiled CommonLib into an extracted complete source tree")
parser.add_argument("source_root",type=Path)
parser.add_argument("--archive",type=Path,default=Path(__file__).with_name("CommonLibSSE.lib.xz"))
args=parser.parse_args()
root=args.source_root.resolve()
if not root.is_dir(): raise SystemExit("Extract the full source archive first and pass that folder")
target=root/RELATIVE_PATH
if not target.resolve().is_relative_to(root): raise SystemExit("Invalid destination")
if target.exists():
 with target.open("rb") as f: existing=hashlib.file_digest(f,"sha256").hexdigest()
 if existing==EXPECTED_HASH:
  print("Matching library already present");raise SystemExit(0)
 raise SystemExit("A different library already exists at the destination; use a fresh source folder")
target.parent.mkdir(parents=True,exist_ok=True)
fd,name=tempfile.mkstemp(prefix="commonlib-",suffix=".tmp",dir=target.parent)
try:
 checksum=hashlib.sha256()
 with os.fdopen(fd,"wb") as output,lzma.open(args.archive,"rb") as source:
  while chunk:=source.read(4*1024*1024):
   checksum.update(chunk);output.write(chunk)
 if checksum.hexdigest()!=EXPECTED_HASH: raise SystemExit("Library checksum mismatch")
 os.replace(name,target)
 print("Restored verified precompiled CommonLib:",target)
finally:
 if Path(name).exists():Path(name).unlink()
