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

uilabel_mut="$mut/uilabel.cpp"
uiwidget_mut="$mut/uiwidget.cpp"

source_fault() {
	# $1 = fault name
	if python3 ./d2color_control.py 0 "$mut/tr_draw.c" "$mut/tr_backend.c" \
			"$uilabel_mut" "$uiwidget_mut" >/dev/null 2>&1; then
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

# 3: the census can no longer say a run's ink is absent
sed 's#"VERDICT=NO_INK"#"VERDICT=UNKNOWN"#' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
cp "$src/tr_draw.c" "$mut/tr_draw.c"
source_fault "3 the no-ink verdict"

# 4: the census claims a verdict where the clear colour could also be the ink
sed 's/INK=AMBIGUOUS /INK=KNOWN /' "$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "4 the ambiguity refusal"

# 5: the span row drops the colour the span carries
sed 's/vcol=%d,%d,%d,%d nvcol=%d rgbGen=%d const=%d,%d,%d,%d alphaGen=%d imgdesc=%s/vcol=? nvcol=%d rgbGen=%d/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "5 the span's own colour"

# 6: the readback stops counting opaque dark samples
sed 's/nd++;//' "$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "6 the dark sample count"

# 6b: the readback samples the widget's clip instead of the element's footprint
sed 's/c->quad\[2\] > c->quad\[0\] \&\& c->quad\[3\] > c->quad\[1\]/0/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "6b the element's own footprint"

# 6c: the ink floor is no longer two fifths
sed 's/#define RB_2DCOV_INK_FLOOR_NUM\t2/#define RB_2DCOV_INK_FLOOR_NUM\t0/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "6c the ink floor"

# 7: the image descriptor drops the art's own size and format
sed 's/"%dx%d\/0x%x\/%dm",/"%dx%d\/0x%x\/%dm",/' "$src/tr_backend.c" > "$mut/tr_backend.c"
sed 's/image->width, image->height, (unsigned)image->internalFormat,/image->uploadWidth, image->uploadHeight, 0u,/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "7 the image descriptor"

cp "$src/../uilib/uilabel.cpp" "$uilabel_mut"
cp "$src/../uilib/uiwidget.cpp" "$uiwidget_mut"

# 8: the draw reads the title before the label string -- retail's order is
#    label text, cvar, label, then title
sed -e 's/} else if (label.length()) {/} else if (MUTTITLE) {/' \
	-e 's/} else if (m_title.length()) {/} else if (label.length()) {/' \
	-e 's/MUTTITLE/m_title.length()/' \
	"$uilabel_mut" > "$uilabel_mut.n" && mv "$uilabel_mut.n" "$uilabel_mut"
source_fault "8 the label source order"

# 9: a plain label text lands in the widget's title instead
sed 's/    m_labelText = text;/    m_title = text;/' "$uiwidget_mut" > "$uiwidget_mut.n" \
	&& mv "$uiwidget_mut.n" "$uiwidget_mut"
source_fault "9 the plain label text field"

# 10: setTitle also writes the label text
sed 's/        m_title = title;/        m_title = title;\n        m_labelText = title;/' \
	"$uiwidget_mut" > "$uiwidget_mut.n" && mv "$uiwidget_mut.n" "$uiwidget_mut"
source_fault "10 setTitle writes the label text"

exit $rc
