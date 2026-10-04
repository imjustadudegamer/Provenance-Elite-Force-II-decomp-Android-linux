#!/bin/sh
# The 2D COLOUR verdict: green on the shipping tree and the shipped art, red on
# every planted fault -- in the model (a black resolution, an unresolvable or
# mismatched bound image, a text run that emits no quad, a coverage verdict
# that reports pixels where the readback says clear) and in the shipping text
# itself.  Never launches.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./d2color_control.py 0; then
	echo "  FAIL  the shipping tree and the shipped data are not green"
	rc=1
fi

for m in 1 2 3 4 5 6 7 8 9 10 11 12; do
	if python3 ./d2color_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  model fault $m was not detected"
		rc=1
	else
		echo "  ok    model fault $m detected"
	fi
done

mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

source_fault() {
	# $1 = fault name
	if python3 ./d2color_control.py 0 "$mut/tr_draw.c" "$mut/tr_backend.c" >/dev/null 2>&1; then
		echo "  FAIL  source fault $1 was not detected"
		rc=1
	else
		echo "  ok    source fault $1 detected"
	fi
}

# 1: the front end writes the walk's 2D colour again
sed 's#^\tD2_End( D2P_setcolor, t0 );#\tbackEnd.color2D[0] = r_color2D[0];\n\tD2_End( D2P_setcolor, t0 );#' \
	"$src/tr_draw.c" > "$mut/tr_draw.c"
cp "$src/tr_backend.c" "$mut/tr_backend.c"
source_fault "1 the front-end 2D colour write"

# 2: the text colour entry point reads the walk's colour from the front end
sed 's#color\[3\] = r_color2D\[3\] / 255.0;#color[3] = backEnd.color2D[3] / 255.0;#' \
	"$src/tr_draw.c" > "$mut/tr_draw.c"
source_fault "2 the front-end text colour read"

# 3: the census can no longer say an element drew black
sed 's#"VERDICT=DREW_BLACK"#"VERDICT=UNKNOWN"#' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
cp "$src/tr_draw.c" "$mut/tr_draw.c"
source_fault "3 the black verdict"

# 4: the black verdict no longer requires every lit sample to be black
sed 's/black == nonclear/black >= 0/' "$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "4 the black verdict's condition"

# 5: the span row drops the colour the span carries
sed 's/vcol=%d,%d,%d,%d nvcol=%d rgbGen=%d const=%d,%d,%d,%d alphaGen=%d imgdesc=%s/vcol=? nvcol=%d rgbGen=%d/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "5 the span's own colour"

# 6: the readback stops counting opaque black
sed 's/\*black)++;//' "$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "6 the black sample count"

# 7: the image descriptor drops the art's own size and format
sed 's/"%dx%d\/0x%x\/%dm",/"%dx%d\/0x%x\/%dm",/' "$src/tr_backend.c" > "$mut/tr_backend.c"
sed 's/image->width, image->height, (unsigned)image->internalFormat,/image->uploadWidth, image->uploadHeight, 0u,/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "7 the image descriptor"

exit $rc
