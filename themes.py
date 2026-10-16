"""How a room looks: wall, ground, furniture and who the Clawds are (standard library only).

A theme paints the background (cached per room size), draws the "workstation" of each session
(office desk, coffee-shop table or counter, picnic table), gives roles (the barista) and, for the
park, the playground spots. Colours are muted on purpose so the Clawds stand out.
"""
import math

INK = (20, 20, 30)
WHITE = (235, 235, 245)
STEEL = (140, 140, 150)
STEEL_DARK = (96, 96, 108)
SCREEN_OFF = (14, 18, 26)
SCREEN_ON = (10, 24, 20)


def noise(x, y):
    """A cheap repeatable pseudo-random number 0..255 for textures."""
    return ((x * 73856093) ^ (y * 19349663)) & 255


class Theme:
    """The office (default look); the other themes override what differs."""
    name = "office"
    horizon = 0.42  # the wall ends and the ground starts at this fraction of the room height
    wall, wall2 = (34, 36, 46), (38, 41, 52)
    floor, floor2, base = (62, 58, 58), (57, 53, 53), (48, 45, 46)
    desk, desk_top = (112, 92, 76), (128, 106, 88)
    visit_kinds = (0, 1, 2, 3, 4, 5)  # chat, wave, hop, high-five, dance, tag
    emotes = (("stretch", 2.0), ("dance", 3.5), ("jump", 2.2), ("wave", 2.0))

    def __init__(self):
        self._cache = {}

    # ---- background -----------------------------------------------------------------
    def background(self, cv, banner):
        """Paint the whole room; the picture is cached per size and copied afterwards."""
        key = (cv.w, cv.h, banner)
        if key not in self._cache:
            self.paint(cv, banner, int(cv.h * self.horizon))
            self._cache[key] = [row[:] for row in cv.rows]
        else:
            cv.rows = [row[:] for row in self._cache[key]]

    def paint(self, cv, banner, floor_y):
        cv.rect(0, 0, cv.w, cv.h, self.wall)
        for y in range(banner + 4, floor_y, 8):  # faint wall panels
            cv.rect(0, y, cv.w, 1, self.wall2)
        cv.rect(0, floor_y, cv.w, cv.h - floor_y, self.floor)
        for y in range(floor_y + 4, cv.h, 8):
            cv.rect(0, y, cv.w, 1, self.floor2)
        cv.rect(0, floor_y - 1, cv.w, 2, self.base)

    # ---- furniture ----------------------------------------------------------------------
    def furniture(self, cv, cx, by, on, color, t, role):
        """A desk with a monitor on its right."""
        self.table(cv, cx, by)
        mx = cx + 6
        cv.rect(mx, by, 9, 9, (60, 60, 70))
        cv.rect(mx + 1, by + 1, 7, 7, SCREEN_ON if on else SCREEN_OFF)
        if on:
            for i in range(4):  # scrolling lines
                ly = (i * 2 + int(t * 8)) % 7
                cv.rect(mx + 2, by + 1 + ly, 5 - (i % 2) * 2, 1, color)
        cv.rect(mx + 3, by + 9, 3, 1, (60, 60, 70))

    def table(self, cv, cx, by):
        cv.rect(cx - 14, by + 9, 29, 2, self.desk_top)
        cv.rect(cx - 14, by + 11, 29, 4, self.desk)

    def laptop(self, cv, cx, by, on, color, t):
        """A laptop on the right of the table top."""
        mx = cx + 6
        cv.rect(mx, by + 2, 9, 7, (60, 60, 70))
        cv.rect(mx + 1, by + 3, 7, 5, SCREEN_ON if on else SCREEN_OFF)
        if on:
            for i in range(3):
                ly = (i * 2 + int(t * 8)) % 5
                cv.rect(mx + 2, by + 3 + ly, 5 - (i % 2) * 2, 1, color)
        cv.rect(mx - 1, by + 9, 11, 1, STEEL_DARK)

    # ---- people -------------------------------------------------------------------------
    def roles(self, sessions):
        """{sid: role} for the sessions that have a special role."""
        return {}

    def accessory(self, cv, x, y, role):
        """Something a Clawd with this role wears; (x, y) is the top left of its 14x8 sprite."""

    # ---- play spots (park only) -------------------------------------------------------------
    def spots(self, w, h):
        return {}


class Coffee(Theme):
    name = "coffee"
    horizon = 0.40
    wall, wall2 = (58, 48, 44), (64, 54, 49)
    floor, floor2, base = (110, 98, 88), (96, 85, 76), (44, 34, 30)
    desk, desk_top = (120, 94, 70), (146, 118, 90)
    counter, counter_top = (82, 60, 46), (104, 78, 58)
    visit_kinds = (0, 0, 1, 2, 4, 6, 6, 6)  # 6 = order a coffee from the barista
    emotes = (("stretch", 2.0), ("dance", 3.5), ("wave", 2.0))

    def paint(self, cv, banner, floor_y):
        cv.rect(0, 0, cv.w, cv.h, self.wall)
        for x in range(0, cv.w, 6):  # wainscot panelling on the lower wall
            cv.rect(x, floor_y - 9, 1, 9, self.wall2)
        cv.rect(0, floor_y - 10, cv.w, 1, (74, 62, 56))
        for ty in range(floor_y, cv.h, 4):  # chequered floor tiles
            for tx in range(0, cv.w, 4):
                cv.rect(tx, ty, 4, 4, self.floor if ((tx // 4 + ty // 4) % 2 == 0) else self.floor2)
        cv.rect(0, floor_y - 1, cv.w, 2, self.base)
        top = banner + 3
        if cv.w >= 90 and floor_y - top >= 9:
            self.menu_board(cv, 6, top, min(24, floor_y - top))
            self.shelf(cv, cv.w - 34, top + 3)
            for lx in (cv.w // 3, 2 * cv.w // 3):  # hanging lamps with a warm glow
                cv.rect(lx, top, 1, 3, (50, 44, 40))
                cv.rect(lx - 2, top + 3, 5, 2, (120, 100, 60))
                cv.px(lx, top + 5, (200, 170, 100))

    @staticmethod
    def menu_board(cv, x, y, h):
        cv.rect(x, y, 22, h, (110, 84, 60))
        cv.rect(x + 1, y + 1, 20, h - 2, (30, 40, 36))
        cv.text(x + 3, y + 2, "MENU", (220, 220, 200))
        for i in range(2):
            cv.rect(x + 3, y + 9 + i * 3, 14 - i * 4, 1, (150, 150, 140))

    @staticmethod
    def shelf(cv, x, y):
        cv.rect(x, y + 6, 28, 1, (110, 84, 60))
        for i, c in enumerate(((200, 200, 205), (170, 120, 90), (200, 200, 205), (120, 150, 130), (200, 200, 205))):
            cv.rect(x + 2 + i * 5, y + 3, 3, 3, c)
            cv.px(x + 5 + i * 5, y + 4, c)  # cup handle

    def furniture(self, cv, cx, by, on, color, t, role):
        if role == "barista":
            self.counter_with_machine(cv, cx, by, on, t)
        else:
            cv.rect(cx - 14, by + 9, 29, 2, self.desk_top)  # a small café table
            cv.rect(cx - 14, by + 11, 29, 3, self.desk)
            cv.rect(cx - 2, by + 14, 5, 1, self.desk)
            self.laptop(cv, cx, by, on, color, t)
            self.cup(cv, cx - 11, by + 7, on, t)

    def counter_with_machine(self, cv, cx, by, on, t):
        cv.rect(cx - 14, by + 9, 29, 2, self.counter_top)
        cv.rect(cx - 14, by + 11, 29, 4, self.counter)
        for x in range(cx - 12, cx + 14, 4):  # plank lines on the front
            cv.rect(x, by + 11, 1, 4, (70, 50, 38))
        mx = cx + 3
        cv.rect(mx, by + 1, 12, 8, STEEL)  # espresso machine
        cv.rect(mx, by + 1, 12, 2, STEEL_DARK)
        cv.px(mx + 2, by + 4, (220, 70, 60) if on and int(t * 4) % 2 else (110, 40, 40))
        cv.rect(mx + 4, by + 5, 4, 1, STEEL_DARK)
        cv.rect(mx + 5, by + 8, 2, 1, STEEL_DARK)
        self.cup(cv, mx + 5, by + 7, False, t)
        if on:  # steam
            for i in range(3):
                k = (t * 1.2 + i / 3.0) % 1.0
                cv.px(mx + 8 + int(math.sin(k * 6 + i) * 1.5), by - 1 - int(k * 5), (220, 220, 228))

    @staticmethod
    def cup(cv, x, y, on, t):
        cv.rect(x, y, 3, 2, WHITE)
        cv.px(x + 3, y, WHITE)
        cv.rect(x, y + 2, 3, 1, (190, 190, 198))
        if on:  # a little steam
            cv.px(x + 1, y - 1 - int(t * 2) % 3, (200, 200, 210))

    def roles(self, sessions):
        return {s.sid: ("barista" if i == 0 else "customer") for i, s in enumerate(sessions)}

    def accessory(self, cv, x, y, role):
        if role == "barista":  # white apron with straps
            cv.rect(x + 4, y + 3, 6, 3, (226, 226, 232))
            cv.px(x + 5, y + 2, (226, 226, 232))
            cv.px(x + 8, y + 2, (226, 226, 232))


class Park(Theme):
    name = "park"
    horizon = 0.40
    wall, wall2 = (88, 110, 126), (96, 120, 136)  # sky
    floor, floor2, base = (66, 92, 68), (58, 82, 60), (52, 72, 52)
    desk, desk_top = (98, 78, 58), (130, 104, 76)
    emotes = (("stretch", 2.0), ("dance", 3.5), ("wave", 2.0), ("swing", 7.0), ("swing", 7.0),
              ("slide", 7.0), ("slide", 7.0))

    def paint(self, cv, banner, floor_y):
        cv.rect(0, 0, cv.w, floor_y, self.wall)
        for y in range(banner + 2, floor_y, 6):  # a gentle sky gradient
            cv.rect(0, y, cv.w, 3, self.wall2)
        for cx, cy in ((cv.w // 5, banner + 6), (3 * cv.w // 5, banner + 9)):  # clouds
            cv.rect(cx, cy, 14, 3, (118, 138, 152))
            cv.rect(cx + 3, cy - 2, 8, 2, (118, 138, 152))
        for x in range(0, cv.w, 2):  # far hills
            h = int(4 + 3 * math.sin(x / 14.0) + 2 * math.sin(x / 5.0))
            cv.rect(x, floor_y - h, 2, h, (66, 88, 82))
        for x in range(10, cv.w - 10, 37):  # trees
            ty = floor_y - 12
            cv.rect(x + 3, ty + 6, 2, 6, (84, 64, 48))
            cv.rect(x, ty, 8, 7, (54, 84, 60))
            cv.rect(x + 2, ty - 2, 4, 2, (54, 84, 60))
        cv.rect(0, floor_y, cv.w, cv.h - floor_y, self.floor)
        for y in range(floor_y, cv.h):  # grass texture: tufts and a few flowers
            for x in range(cv.w):
                n = noise(x, y)
                if n < 14:
                    cv.px(x, y, self.floor2)
                elif n < 20:
                    cv.px(x, y, (80, 108, 76))
                elif n == 255 and (x + y) % 3 == 0:
                    cv.px(x, y, ((170, 110, 120), (170, 160, 100), (140, 120, 170))[(x * y) % 3])
        cv.rect(0, floor_y - 1, cv.w, 1, self.base)
        sp = self.spots(cv.w, cv.h)
        self.swing_set(cv, sp["swing"], floor_y)
        self.slide_set(cv, sp["slide"], floor_y)

    # ---- playground -------------------------------------------------------------------
    def spots(self, w, h):
        """Positions (body top left of the sprite centre) for the equipment, from the room size."""
        floor_y = int(h * self.horizon)
        return {
            "swing": {"x": 22, "y": floor_y - 2, "bar": max(10, floor_y - 15)},
            "slide": {"base": (w - 30, floor_y + 4), "top": (w - 30, max(10, floor_y - 17)),
                      "bottom": (w - 10, floor_y + 2)},
        }

    @staticmethod
    def swing_set(cv, sp, floor_y):
        x, bar = sp["x"], sp["bar"]
        wood = (112, 88, 64)
        cv.rect(x - 11, bar, 23, 2, wood)  # top bar
        for dx in (-11, 10):  # A-frame legs
            cv.rect(x + dx, bar, 2, floor_y - bar + 1, wood)

    @staticmethod
    def slide_set(cv, sp, floor_y):
        tx, ty = sp["top"]
        bx, by = sp["bottom"]
        wood, metal = (112, 88, 64), (150, 150, 160)
        cv.rect(tx - 5, ty + 8, 2, floor_y - ty - 4, wood)  # ladder rails
        cv.rect(tx - 1, ty + 8, 2, floor_y - ty - 4, wood)
        for ry in range(ty + 11, floor_y + 3, 3):
            cv.rect(tx - 5, ry, 6, 1, wood)
        cv.rect(tx - 5, ty + 8, 14, 2, wood)  # platform
        steps = max(1, bx - tx - 4)
        for i in range(steps):  # slanted slide
            cv.px(tx + 4 + i, ty + 10 + int((by - ty - 8) * i / steps) + 1, metal)
            cv.px(tx + 4 + i, ty + 11 + int((by - ty - 8) * i / steps) + 1, metal)

    # ---- furniture ----------------------------------------------------------------------
    def furniture(self, cv, cx, by, on, color, t, role):
        cv.rect(cx - 14, by + 9, 29, 2, self.desk_top)  # picnic table
        cv.rect(cx - 14, by + 11, 29, 3, self.desk)
        cv.rect(cx - 13, by + 14, 2, 3, self.desk)
        cv.rect(cx + 12, by + 14, 2, 3, self.desk)
        self.laptop(cv, cx, by, on, color, t)
        cv.px(cx - 11, by + 7, (60, 100, 60))  # a flower in a pot
        cv.px(cx - 11, by + 8, (60, 100, 60))
        cv.px(cx - 11, by + 6, (200, 100, 120))
        cv.rect(cx - 12, by + 8, 3, 1, (150, 90, 70))


THEMES = {"office": Theme, "coffee": Coffee, "park": Park}


def pick(room_name, theme_name=None):
    """The theme named on the command line, or one guessed from the room name."""
    name = (theme_name or "").lower()
    if name not in THEMES:
        guess = (room_name or "").lower()
        name = next((k for k, words in (("coffee", ("coffee", "cafe")), ("park", ("park", "garden")))
                     if any(w in guess for w in words)), "office")
    return THEMES[name]()
