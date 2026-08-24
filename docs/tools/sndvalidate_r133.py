#!/usr/bin/env python3
"""r133-sound3d -- does the port's fixed OpenAL listener frame reproduce retail?

Every link is byte-derived this session:

  cgame   CG_CalcViewValues @0x00044640 (Mac, raw 0x00044745A fneg on the yaw
          word feeding the AnglesToAxis block at 0x00044745A-0x00044747E):
              SoundAxis = AnglesToAxis({pitch, -yaw, roll})    cg+12440
  engine  S_MILES_Respatialize @0x001F2CC0:
              M(v) = (-v.x, v.z, -v.y)          positions, 0x001F2D10/0x001F2EC0
              AIL_set_3D_orientation(h, -a0, a6, -a3,  -a2, a8, -a5)
                                                0x001F2D6C-0x001F2D98
                  => FRONT = (-F0, U0, -L0),  ABOVE = (-F2, U2, -L2)
              pan vector (-a1, a7, -a4)         0x001F338C-0x001F33DC
  driver  msssoft.m3d @0x224019B0 (listener branch of the orientation handler,
          7-arg stdcall, ndisasm this session):
              FRONT[0..2] = (X,Y,Z)              -> 0x22415654/58/5c
              ABOVE[0..2] = (Xa,Ya,Za)           -> 0x22415660/64/68
              RIGHT[0]    = Z*Ya - Y*Za          -> 0x22413c40
              RIGHT[1]    = X*Za - Z*Xa          -> 0x22413c44
              RIGHT[2]    = Y*Xa - X*Ya          -> 0x22413c48
              i.e. RIGHT = ABOVE x FRONT  (the plain RH cross, negated termwise)
          msssoft.m3d @0x2240111B-0x224011BD (3D azimuth):
              azimuth = atan2(dot(RIGHT,dir), dot(FRONT,dir))

The port feeds OpenAL at = M^T(FRONT), up = M^T(ABOVE); OpenAL's own side is
cross(at, up) = det(MT) . MT(FRONT x ABOVE) = MT(ABOVE x FRONT) = MT(RIGHT),
which is exactly retail's side expressed in world coordinates.
"""
import math, struct

def f(h): return struct.unpack('>f', bytes.fromhex(h))[0]
# klingon-base-routeB frame 156: the # retail snap row and the # retail playsnd
# row for ent=32 "sound/ships/klingon/kling_beep4.wav" on the same frame.
head = (f('450e6e0d'), f('44e938e4'), f('43801434') + 85.0)
snd  = (f('450d9ca4'), f('455c899a'), f('429b23d7'))
d0 = [snd[i] - head[i] for i in range(3)]

def M(v):  return (-v[0], v[2], -v[1])    # engine -> Miles      (S_MILES_Respatialize)
def MT(v): return (-v[0], -v[2], v[1])    # Miles  -> engine     (M is orthogonal, det -1)

def A2A(yaw, pitch, roll):                # degrees; axis[1] is EF2's LEFT (SDK q_mathsys.c:56)
    sy,cy = math.sin(math.radians(yaw)),   math.cos(math.radians(yaw))
    sp,cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sr,cr = math.sin(math.radians(roll)),  math.cos(math.radians(roll))
    return ([cp*cy, cp*sy, -sp],
            [sr*sp*cy+cr*(-sy), sr*sp*sy+cr*cy, sr*cp],
            [cr*sp*cy+(-sr)*(-sy), cr*sp*sy+(-sr)*cy, cr*cp])

dot   = lambda a,b: sum(a[i]*b[i] for i in range(3))
cross = lambda a,b: [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def retail_az(yaw, pitch, roll, d):
    F,L,U = A2A(-yaw, pitch, roll)                      # SoundAxis, as cgame builds it
    FRONT = (-F[0], U[0], -L[0])                        # 0x001F2D6C-98 fp1..fp3
    ABOVE = (-F[2], U[2], -L[2])                        #                fp4..fp6
    PANV  = (-F[1], U[1], -L[1])                        # 0x001F338C-DC
    RIGHT = cross(ABOVE, FRONT)                         # msssoft 0x224019F8
    dM    = M(d)
    az  = math.degrees(math.atan2(dot(RIGHT, dM), dot(FRONT, dM)))
    pan = max(0.0, 0.5*(1 - dot(PANV, dM)/math.sqrt(dot(dM, dM))))*127
    return az, pan

def port_az(yaw, pitch, roll, d, fixed=True):
    F,L,U = A2A(-yaw, pitch, roll)
    if fixed:                                            # what this workstream landed
        at  = (F[0], L[0], U[0])                         # X column = M^T(FRONT)
        up  = (F[2], L[2], U[2])                         # Z column = M^T(ABOVE)
        panv = (F[1], L[1], U[1])                        # Y column
    else:                                                # what the port did before
        at, up, panv = F, U, L                           # the ROWS
    side = cross(at, up)                                 # OpenAL derives its own side
    az  = math.degrees(math.atan2(dot(d, side), dot(d, at)))
    pan = max(0.0, 0.5*(1 - dot(panv, d)/math.sqrt(dot(d, d))))*127
    return az, pan

if __name__ == '__main__':
    print("klingon-base-routeB f=156, ent=32 kling_beep4.wav, viewangles (2.72, 74.35, 0)")
    print(f"{'yaw':>7} {'pitch':>6} {'roll':>5} | {'retail az':>10} {'fixed az':>10} {'old az':>9} | {'retail pan':>10} {'fixed pan':>10} {'old pan':>8}")
    w_new = w_old = wp_new = wp_old = 0.0
    for yaw in range(0, 360, 15):
        for pitch in (0, 2.72, 10, 30, 45):
            for roll in (0, 5, 15):
                ar, pr = retail_az(yaw, pitch, roll, d0)
                an, pn = port_az(yaw, pitch, roll, d0, True)
                ao, po = port_az(yaw, pitch, roll, d0, False)
                w_new   = max(w_new,   abs((an-ar+540) % 360 - 180)); wp_new = max(wp_new, abs(pn-pr))
                w_old   = max(w_old,   abs((ao-ar+540) % 360 - 180)); wp_old = max(wp_old, abs(po-pr))
                if pitch in (0, 45) and roll == 0:
                    print(f"{yaw:7d} {pitch:6.2f} {roll:5.1f} | {ar:10.4f} {an:10.4f} {ao:9.2f} | {pr:10.4f} {pn:10.4f} {po:8.2f}")
    print()
    print(f"FIXED : worst |azimuth| diff = {w_new:.3e} deg ; worst |pan| diff = {wp_new:.3e} /127   (600 combos)")
    print(f"OLD   : worst |azimuth| err  = {w_old:.2f} deg ; worst |pan| err  = {wp_old:.2f} /127")
