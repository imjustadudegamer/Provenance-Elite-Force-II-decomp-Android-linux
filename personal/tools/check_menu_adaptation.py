#!/usr/bin/env python3
"""Static check of the handheld settings adaptation against the shipped .urc.

Reads the extracted shipped menus (a read-only reference outside the tree) and
applies the same row table the engine applies at load, then reports which widget
each row disables.  A row that matches nothing, or a built widget whose name
already exists in the shipped data, is reported as a problem.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# The extracted shipped menus, a read-only reference the repo does not carry.
# Point EF2_MENU_REF at them when they do not sit beside the checkout.
MENUS = os.environ.get("EF2_MENU_REF") or os.path.join(
    ROOT, "personal", "menuwork", "loc_Eng_ui")

ROWS = [
    ("video",         "$$VideoMode$$",              "r_mode",                      "reslist",     None),
    ("video",         "$$ColorDepth$$",             "r_texturebits",               "colorlist",   None),
    ("video",         "$$FullScreen$$",             "r_fullscreen",                None,          None),
    ("advancedvideo", "$$FullScreenAntialiasing$$", "r_arb_multisample",           None,          None),
    ("advancedvideo", "$$NvidiaAntiAliasing$$",     "r_ext_nv_multisample_filter", None,          None),
    ("audio",         "$$SoundDriver$$",            "s_milesdriver",               "sounddriver", None),
    ("GameOptions",   "$$DataRate$$",               "rate",                        None,          None),
    ("GameOptions",   "$$LowBandwidth$$",           "mp_lowBandwidth",             None,          None),
    ("controls",      None,                         None,                          None,          "menu/mp/bar-fix"),
]

BIND = ["WeaponBindList", "AttackLookBindList", "MovementBindList", "GameplayBindList",
        "WeaponBind", "AttackLookBind", "MovementBind", "GameplayBind", "PressKey"]

BUILT = ["TouchSectionTitle", "TouchSectionBox", "TouchLook", "TouchLookLabel", "TouchScale",
         "TouchScaleLabel", "TouchOpacity", "TouchOpacityLabel", "TouchCrouchToggle"]

FILES = {"video": "menu_video.urc", "advancedvideo": "menu_advancedvideo.urc",
         "audio": "menu_audio.urc", "GameOptions": "menu_gameoption.urc",
         "controls": "menu_controls.urc"}


def parse(path):
    text = open(path, encoding="utf-8", errors="surrogateescape").read()
    out = []
    for m in re.finditer(r"^resource[ \t]*\n(\w+)[ \t]*\n\{(.*?)^\}", text, re.S | re.M):
        body = m.group(2)

        def grab(key, body=body):
            hit = re.search(r"^[ \t]*%s[ \t]+(.*)$" % key, body, re.M | re.I)
            return hit.group(1).strip().strip('"') if hit else None

        out.append({"class": m.group(1), "name": grab("name"), "title": grab("title"),
                    "cvar": grab("linkcvar"), "shader": grab("shader"),
                    "stuff": grab("stuffcommand"), "rect": grab("rect")})
    return out


def main():
    problems = []
    for menu, filename in FILES.items():
        widgets = parse(os.path.join(MENUS, filename))
        print("=== %s (%s): %d resources" % (menu, filename, len(widgets)))
        for row in ROWS:
            if row[0] != menu:
                continue
            _, token, cvar, name, shader = row
            hits = [w for w in widgets
                    if (name and w["name"] == name)
                    or (cvar and w["cvar"] and w["cvar"].lower() == cvar.lower())
                    or (shader and w["shader"] and w["shader"].lower() == shader.lower())
                    or (token and w["title"] == token)]
            if not hits:
                problems.append("no match: %s %s" % (menu, token or cvar or name or shader))
            for hit in hits:
                print("   hide %-10s name=%-14r title=%-28r cvar=%-22r rect=%s"
                      % (hit["class"], hit["name"], hit["title"], hit["cvar"],
                         (hit["rect"] or "").strip()))
        if menu == "controls":
            for named in BIND:
                hits = [w for w in widgets if w["name"] == named]
                if not hits:
                    problems.append("no match: controls %s" % named)
                for hit in hits:
                    print("   hide %-10s name=%-18r rect=%s"
                          % (hit["class"], hit["name"], (hit["rect"] or "").strip()))
        for widget in widgets:
            if widget["name"] in BUILT:
                problems.append("name collision: %s in %s" % (widget["name"], filename))
        if menu == "video":
            gate = [w for w in widgets if w["stuff"] and "promptadvancedvideo" in w["stuff"]]
            print("   repoint: %d widget(s) -> %s" % (len(gate), [(w["title"], w["stuff"]) for w in gate]))
            if len(gate) != 1:
                problems.append("repoint matched %d widgets" % len(gate))
        if menu == "GameOptions":
            orphan = [w for w in widgets if w["class"].lower() == "pulldownmenucontainer"
                      and (w["rect"] or "").split()[:3] == ["586", "231", "16"]]
            print("   hide by rect: %s" % [(w["class"], (w["rect"] or "").strip()) for w in orphan])
            if len(orphan) != 1:
                problems.append("rect hide matched %d widgets" % len(orphan))
        for widget in widgets:
            if widget["class"].lower() == "pulldownmenucontainer":
                print("   [pulldown] name=%-14r rect=%s" % (widget["name"], (widget["rect"] or "").strip()))

    print()
    if problems:
        print("PROBLEMS:")
        for problem in problems:
            print("  ", problem)
        return 1
    print("ALL ROWS MATCHED, NO NAME COLLISIONS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
