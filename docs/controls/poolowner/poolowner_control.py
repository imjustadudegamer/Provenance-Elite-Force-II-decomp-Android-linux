#!/usr/bin/env python3
"""
poolowner_control.py -- command-pool ownership and the one-lock-per-pool rule.

The armed run reported, at vkAllocateCommandBuffers:

    THREADING ERROR : object of type VkCommandPool is simultaneously used in
    current thread ... and thread ...

Vulkan 1.4 s6.2 "Command Pools" (vkspec.txt:14741-14745) is the rule:

    Command pools are externally synchronized, meaning that a command pool must
    not be used concurrently in multiple threads.  That includes use via
    recording commands on any command buffers allocated from the pool, as well
    as operations that allocate, free, and reset command buffers or the pool
    itself.

and the per-call Host Synchronization clauses repeat it (vkspec.txt:15000,
:15067, :15160, :15277, :15340, :15440, and :18948-18953 for recording).

The defect was structural: one upload pool reached from two threads, guarded by
two independent mutexes (a transaction lock for the one-shot transfers, the
queue lock for the arena bake's transfer buffer) which do not exclude each
other, while the bake's buffer records ACROSS a frame.  The port now gives each
thread its own pool and looks the lock up from the POOL handle, never the queue.

This control censuses the shipping text for that shape:

  * no direct qvk.AllocateCommandBuffers / FreeCommandBuffers / DestroyCommandPool
    outside the one owner API, and no qvk.ResetCommandPool anywhere;
  * every pool the renderer creates is registered with exactly one lock;
  * VK_Pool_AllocBuffers / VK_Pool_FreeBuffers take the pool's own lock and never
    the queue lock;
  * every `commandPool =` assignment is a per-thread pool or the walk's frame
    pool, and the frame-spanning arena recorder takes the calling thread's pool;
  * the pool table is private to vk_renderthread.c, and the render thread is
    joined before the pools are destroyed.

Planted faults break each in turn (argv[1]); the optional argv[2:6] override the
five sources the census reads, which is how run.sh mutates the shipping text.

Build/run: docs/controls/poolowner/run.sh  (never launches the game)
"""
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
VK = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan")
TR_INIT = os.path.join(VK, "tr_init.c")

DEFAULT_FILES = {
    "renderthread": os.path.join(VK, "vk_renderthread.c"),
    "vbo": os.path.join(VK, "vk_vbo.c"),
    "frame": os.path.join(VK, "vk_frame.c"),
    "texture": os.path.join(VK, "vk_texture.c"),
    "trinit": TR_INIT,
}

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def body(text, header):
    i = text.find(header)
    if i < 0:
        return ""
    j = text.find("\n}\n", i)
    return text[i:j + 3] if j > 0 else text[i:]


def lane_map(files):
    """Every C/C++ source in the Vulkan lane, with the overrides applied."""
    m = {}
    for name in sorted(os.listdir(VK)):
        if name.endswith((".c", ".cpp", ".h")):
            m[name] = os.path.join(VK, name)
    for key, path in files.items():
        m[os.path.basename(DEFAULT_FILES[key])] = path
    return m


def census(files):
    lane = lane_map(files)
    rt = files["renderthread"]
    vbo = files["vbo"]
    frame = files["frame"]

    # --- 1. no bypass of the one owner API --------------------------------
    for name, path in sorted(lane.items()):
        text = read(path)
        if name != "vk_renderthread.c":
            check("qvk.AllocateCommandBuffers(" not in text,
                  "%s calls qvk.AllocateCommandBuffers outside the owner API" % name)
            check("qvk.FreeCommandBuffers(" not in text,
                  "%s calls qvk.FreeCommandBuffers outside the owner API" % name)
            check("qvk.DestroyCommandPool(" not in text,
                  "%s destroys a command pool outside the owner API" % name)
        check("qvk.ResetCommandPool" not in text,
              "%s resets a whole command pool (no pool reset is needed)" % name)
        if name != "vk_vbo.c":
            check("qvk.ResetCommandBuffer" not in text,
                  "%s resets a command buffer outside the walk's own frame pool" % name)

    # --- 2. the owner API holds exactly one lock --------------------------
    alloc = body(read(rt), "VkResult VK_Pool_AllocBuffers( VkCommandPool pool,")
    check(bool(alloc), "VK_Pool_AllocBuffers is gone")
    check("VK_Pool_LockOf( pool )" in alloc,
          "VK_Pool_AllocBuffers no longer takes the pool's own lock")
    check("qvk.AllocateCommandBuffers" in alloc,
          "VK_Pool_AllocBuffers is not where the allocate happens")
    check("VK_Queue_Mutex" not in alloc and "vk_queueMutex" not in alloc,
          "VK_Pool_AllocBuffers takes the queue lock as well (two locks, one pool)")

    free = body(read(rt), "void VK_Pool_FreeBuffers( VkCommandPool pool,")
    check(bool(free), "VK_Pool_FreeBuffers is gone")
    check("VK_Pool_LockOf( pool )" in free,
          "VK_Pool_FreeBuffers no longer takes the pool's own lock")
    check("qvk.FreeCommandBuffers" in free,
          "VK_Pool_FreeBuffers is not where the free happens")
    check("VK_Queue_Mutex" not in free and "vk_queueMutex" not in free,
          "VK_Pool_FreeBuffers takes the queue lock as well (two locks, one pool)")

    for label, fn in (("Alloc", alloc), ("Free", free)):
        check(fn.count("SDL_LockMutex") == 1 and fn.count("SDL_UnlockMutex") == 1,
              "VK_Pool_%sBuffers does not take exactly one lock" % label)

    # --- 3. every created pool is registered with its lock ----------------
    creates = 0
    for name, path in sorted(lane.items()):
        text = read(path)
        for m in re.finditer(r"qvk\.CreateCommandPool\(", text):
            creates += 1
            window = text[m.start():m.start() + 1200]
            check("VK_Pool_Register(" in window,
                  "%s creates a command pool that is never registered" % name)
    check(creates == 2,
          "expected two pool creations (the walk's frame pool + the per-thread pool), saw %d" % creates)

    # --- 4. the per-thread pool is keyed by the calling thread ------------
    up = body(read(rt), "VkCommandPool VK_Upload_Pool( void )")
    check(bool(up), "VK_Upload_Pool is gone")
    check("SDL_ThreadID()" in up, "VK_Upload_Pool no longer keys the pool by thread")
    check("s_poolOwners[i].owner == me" in up,
          "VK_Upload_Pool does not look the pool up by owner thread")
    check("VK_Pool_Register( created, me )" in up,
          "VK_Upload_Pool does not register the new pool to this thread")
    check("VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO" in up,
          "VK_Upload_Pool is not where a one-shot pool is created")

    reg = body(read(rt), "qboolean VK_Pool_Register( VkCommandPool pool,")
    check(bool(reg), "VK_Pool_Register is gone")
    check("SDL_CreateMutex()" in reg, "VK_Pool_Register does not create the pool's lock")
    check("slot->owner = owner" in reg, "VK_Pool_Register does not record the owner thread")

    lockfn = body(read(rt), "static SDL_mutex *VK_Pool_LockOf( VkCommandPool pool )")
    check(bool(lockfn), "VK_Pool_LockOf is gone")
    check("slot->owner != 0 && slot->owner != (unsigned long)SDL_ThreadID()" in lockfn,
          "VK_Pool_LockOf no longer convicts a cross-thread use")
    check(lockfn.count("slot->lock") == 1,
          "VK_Pool_LockOf is not the single accessor of the pool lock")

    # --- 5. the command-pool assignment census ----------------------------
    allowed = ("VK_Upload_Pool()", "vk.commandPool")
    for name, path in sorted(lane.items()):
        text = read(path)
        for m in re.finditer(r"(?<!vk\.)commandPool\s*=\s*([^;]+);", text):
            expr = m.group(1).strip()
            check(expr in allowed,
                  "%s assigns commandPool = %s (only %s are owners)"
                  % (name, expr, " / ".join(allowed)))

    # --- 6. the frame-spanning recorder owns the calling thread's pool -----
    xfer = body(read(vbo), "static VkCommandBuffer VK_StaticXferCmd( void )")
    check(bool(xfer), "VK_StaticXferCmd is gone")
    check("VK_Upload_Pool()" in xfer,
          "the arena transfer buffer is not allocated from the calling thread's pool")
    check("vk.commandPool" not in xfer,
          "the arena transfer buffer is allocated from the walk's frame pool")

    # the frame pool is handed to the walk's own buffers, explicitly
    check("VK_Pool_AllocBuffers( vk.commandPool," in read(frame),
          "the frame command buffers are not allocated from the frame pool")

    # --- 7. lifetime: join, then destroy ----------------------------------
    shutdown = body(read(frame), "void VK_Frame_Shutdown( void )")
    check("VK_Pool_ShutdownAll()" in shutdown,
          "VK_Frame_Shutdown does not destroy the pools")

    re_shutdown = body(read(files["trinit"]), "void RE_Shutdown( qboolean destroyWindow )")
    i_join = re_shutdown.find("R_ShutdownCommandBuffers()")
    i_down = re_shutdown.find("VK_BringDown()")
    check(i_join >= 0 and i_down > i_join,
          "RE_Shutdown brings the device down before the render thread is joined")

    # --- 8. the pool table is private to vk_renderthread.c -----------------
    for name, path in sorted(lane.items()):
        if name != "vk_renderthread.c":
            check("s_poolOwners" not in read(path),
                  "%s reaches into the command-pool table" % name)


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0

    files = dict(DEFAULT_FILES)
    keys = ["renderthread", "vbo", "frame", "texture", "trinit"]
    for i, key in enumerate(keys):
        if len(sys.argv) > 2 + i:
            files[key] = sys.argv[2 + i]

    texts = {k: read(v) for k, v in files.items()}

    # --- planted faults ---------------------------------------------------
    if mutant == 1:
        # the pool takes the queue lock again: two locks, one pool
        texts["renderthread"] = texts["renderthread"].replace(
            "SDL_mutex\t*lock = VK_Pool_LockOf( pool );",
            "SDL_mutex\t*lock = VK_Queue_Mutex();")
    elif mutant == 2:
        # the cross-frame transfer goes to the walk's frame pool
        texts["vbo"] = texts["vbo"].replace(
            "allocInfo.commandPool = VK_Upload_Pool();",
            "allocInfo.commandPool = vk.commandPool;")
    elif mutant == 3:
        # every thread shares one pool again
        texts["renderthread"] = texts["renderthread"].replace(
            "\tfor ( i = 0; i < s_poolOwnerCount; i++ ) {\n"
            "\t\tif ( s_poolOwners[i].owner == me ) {\n"
            "\t\t\tpool = s_poolOwners[i].pool;\n"
            "\t\t\tbreak;\n"
            "\t\t}\n"
            "\t}\n",
            "\tif ( s_poolOwnerCount > 0 ) {\n"
            "\t\tpool = s_poolOwners[0].pool;\n"
            "\t}\n")
    elif mutant == 4:
        # a transfer path bypasses the owner API
        texts["texture"] = texts["texture"].replace(
            "\tVK_Upload_Begin();\n",
            "\tqvk.AllocateCommandBuffers( vk.device, &allocInfo, &cmd );\n\tVK_Upload_Begin();\n", 1)
    elif mutant == 5:
        # a pool registered without a lock
        texts["renderthread"] = texts["renderthread"].replace(
            "\tslot->lock = SDL_CreateMutex();\n", "\tslot->lock = NULL;\n")

    tmpdir = None
    if mutant:
        tmpdir = tempfile.mkdtemp(prefix="poolowner")
        for k, t in texts.items():
            if t != read(files[k]):
                path = os.path.join(tmpdir, os.path.basename(DEFAULT_FILES[k]))
                with open(path, "w", encoding="utf-8") as f:
                    f.write(t)
                files[k] = path

    try:
        census(files)
    finally:
        if tmpdir:
            shutil.rmtree(tmpdir, ignore_errors=True)

    print("poolowner control: %d checks, %d failures" % (checks, failures))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
