# example of creating a set of images with magick (ImageMagick utility)
prefix=x
cols=64
for plane in U V Z;do
  for tpc in 0 1;do
    case $plane in U)rows=316;; V)rows=315;; Z)rows=240;;esac
    magick -size ${rows}x$cols gradient: -rotate 90 -depth 16 -type Grayscale \
  -evaluate divide $((65535/($cols-1))) \
  -define png:compression-level=0 \
  -define png:compression-filter=0 \
  -define png:compression-strategy=0 $prefix$plane${tpc}x$cols.png
  done
done

# use gimp to examine the image:
gimp xU0x$cols.png

