#!/usr/bin/env python3
"""Generate deterministic diagrams for the Celestial Navigation v2 manual.

These are technical drawings, not generative images.  Every line, angle and
label is placed from explicit geometry so the figures can be audited and
regenerated.  SVG is the master format; PNG derivatives are used where an
office suite or the plugin's HTML viewer needs a raster image.
"""

from __future__ import annotations

import math
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/celnav-mpl-cache")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.path import Path as MplPath
from matplotlib.patches import Arc, Circle, FancyArrowPatch, PathPatch, Polygon


ROOT = Path(__file__).resolve().parent
SVG_DIR = ROOT / "diagrams"
PNG_DIR = ROOT / "images"

NAVY = "#17365D"
BLUE = "#2F75B5"
CYAN = "#5BC0EB"
TEAL = "#2A9D8F"
GREEN = "#5B8C5A"
GOLD = "#E9C46A"
ORANGE = "#F4A261"
RED = "#C43D3D"
GREY = "#606770"
LIGHT = "#EAF2F8"
SEA = "#D9EEF7"
LAND = "#E9D8A6"


def setup_ax(width=8.4, height=5.2, xlim=(-1.2, 1.2), ylim=(-1.1, 1.1)):
    fig, ax = plt.subplots(figsize=(width, height))
    ax.set_aspect("equal")
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.axis("off")
    fig.patch.set_facecolor("white")
    return fig, ax


def save(fig, name):
    SVG_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    fig.savefig(SVG_DIR / f"{name}.svg", bbox_inches="tight", facecolor="white")
    fig.savefig(
        PNG_DIR / f"{name}.png",
        bbox_inches="tight",
        facecolor="white",
        dpi=220,
    )
    plt.close(fig)


def point(angle_deg, radius=1.0):
    a = math.radians(angle_deg)
    return np.array([radius * math.cos(a), radius * math.sin(a)])


def arrow(ax, start, end, color=NAVY, width=1.8, head=12, style="-|>", zorder=5):
    ax.add_patch(
        FancyArrowPatch(
            start,
            end,
            arrowstyle=style,
            mutation_scale=head,
            linewidth=width,
            color=color,
            zorder=zorder,
        )
    )


def diagram_geographic_position():
    """Exact side-view relation: z = 90 degrees - Ho."""
    fig, ax = setup_ax(xlim=(-1.45, 1.75), ylim=(-1.38, 1.35))
    earth = Circle((0, 0), 1.0, facecolor=SEA, edgecolor=NAVY, linewidth=2)
    ax.add_patch(earth)
    ax.text(0, -0.25, "Earth", ha="center", va="center", color=NAVY, fontsize=13)
    ax.plot(0, 0, "o", color=NAVY, ms=4)
    ax.text(-0.06, -0.09, "O", ha="right", va="top", fontsize=10)

    gp_angle = 25.0
    z = 42.0
    obs_angle = gp_angle + z
    gp = point(gp_angle)
    obs = point(obs_angle)
    body_dir = point(gp_angle)
    zenith_dir = point(obs_angle)
    tangent = np.array([-zenith_dir[1], zenith_dir[0]])

    ax.plot(*gp, "o", color=RED, ms=8, zorder=6)
    ax.text(*(gp + np.array([0.02, -0.18])), "GP", color=RED, fontsize=12, weight="bold")
    ax.plot(*obs, "o", color=NAVY, ms=8, zorder=6)
    ax.annotate(
        "Observer",
        xy=obs,
        xytext=(-0.20, 1.18),
        color=NAVY,
        fontsize=12,
        weight="bold",
        arrowprops=dict(arrowstyle="-", color=NAVY, lw=1.2),
    )
    ax.plot([0, gp[0]], [0, gp[1]], color=RED, lw=1.6)
    ax.plot([0, obs[0]], [0, obs[1]], color=NAVY, lw=1.6)

    # Local horizon and zenith at the observer.
    h1, h2 = obs - 0.62 * tangent, obs + 0.62 * tangent
    ax.plot([h1[0], h2[0]], [h1[1], h2[1]], color=GREEN, lw=2)
    ax.text(*(h2 + np.array([-0.12, 0.08])), "local horizontal", color=GREEN, fontsize=10)
    arrow(ax, obs, obs + 0.72 * zenith_dir, color=NAVY)
    ax.text(*(obs + 0.78 * zenith_dir), "zenith", ha="center", va="bottom", color=NAVY, fontsize=10)

    # Parallel light rays point towards the body and share the GP radial direction.
    for offset in (-0.42, 0.0, 0.42):
        base = np.array([1.55, 1.05 + offset])
        end = base - 0.60 * body_dir
        arrow(ax, end, base, color=GOLD, width=1.5, head=10)
    ax.text(
        1.18,
        1.25,
        "parallel lines of sight\ntowards a distant body",
        ha="center",
        color="#8A6518",
        fontsize=10,
    )
    arrow(ax, obs, obs + 0.90 * body_dir, color=ORANGE, width=2.2)

    # Central angle z and local altitude Ho.
    ax.add_patch(Arc((0, 0), 0.72, 0.72, theta1=gp_angle, theta2=obs_angle, color=RED, lw=2))
    mid = point((gp_angle + obs_angle) / 2, 0.48)
    ax.text(*mid, "z", color=RED, fontsize=14, weight="bold", ha="center")

    horizon_angle = obs_angle - 90.0
    ax.add_patch(
        Arc(obs, 0.62, 0.62, theta1=horizon_angle, theta2=gp_angle, color=ORANGE, lw=2)
    )
    label = obs + 0.42 * point((horizon_angle + gp_angle) / 2)
    ax.text(*label, r"$H_o$", color=ORANGE, fontsize=13, weight="bold")

    ax.text(
        -1.35,
        -1.12,
        r"Zenith distance  $z = 90^{\circ} - H_o$",
        fontsize=14,
        color=NAVY,
        weight="bold",
    )
    ax.text(-1.35, -1.29, "Schematic side view — not to scale", fontsize=9, color=GREY)
    save(fig, "01_geographic_position_and_altitude")


def diagram_circle_equal_altitude():
    fig, ax = setup_ax(xlim=(-1.42, 1.55), ylim=(-1.38, 1.25))
    earth = Circle((0, 0), 1.0, facecolor=SEA, edgecolor=NAVY, linewidth=2)
    ax.add_patch(earth)
    # A view centred on GP: concentric projected small circles are exact in angular radius.
    ax.plot(0, 0, "o", color=RED, ms=9)
    ax.text(0.06, 0.05, "GP", color=RED, fontsize=12, weight="bold")
    radius = 0.67
    ax.add_patch(Circle((0, 0), radius, fill=False, edgecolor=BLUE, linewidth=3))
    obs_a = math.radians(132)
    obs = np.array([radius * math.cos(obs_a), radius * math.sin(obs_a)])
    ax.plot(*obs, "o", color=NAVY, ms=8)
    ax.text(*(obs + np.array([-0.30, 0.07])), "your possible\nposition", ha="center", color=NAVY, fontsize=10)
    arrow(ax, (0, 0), obs, color=BLUE, width=2)
    ax.text(-0.24, 0.28, r"$z=90^{\circ}-H_o$", color=BLUE, fontsize=12, rotation=-48)
    ax.text(0, -1.10, "Circle of Equal Altitude", ha="center", color=BLUE, fontsize=14, weight="bold")
    ax.text(
        1.12,
        0.45,
        "Every point on this circle\nsees the body at the same\ncorrected altitude.",
        ha="center",
        va="center",
        fontsize=11,
        color=NAVY,
        bbox=dict(boxstyle="round,pad=0.5", fc=LIGHT, ec=BLUE),
    )
    ax.text(
        1.12,
        -0.20,
        "1° of zenith distance = 60 NM\n1′ of zenith distance = 1 NM",
        ha="center",
        va="center",
        fontsize=10.5,
        color=NAVY,
        bbox=dict(boxstyle="round,pad=0.5", fc="#FFF7DF", ec=GOLD),
    )
    ax.text(-1.36, -1.29, "Orthographic view centred on the body's GP", fontsize=9, color=GREY)
    save(fig, "02_circle_of_equal_altitude")


def circle_intersections(c0, r0, c1, r1):
    x0, y0 = c0
    x1, y1 = c1
    dx, dy = x1 - x0, y1 - y0
    d = math.hypot(dx, dy)
    a = (r0 * r0 - r1 * r1 + d * d) / (2 * d)
    h = math.sqrt(max(0.0, r0 * r0 - a * a))
    xm, ym = x0 + a * dx / d, y0 + a * dy / d
    return (
        np.array([xm + h * -dy / d, ym + h * dx / d]),
        np.array([xm - h * -dy / d, ym - h * dx / d]),
    )


def diagram_intersections():
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.6))
    fig.patch.set_facecolor("white")
    for ax in axes:
        ax.set_aspect("equal")
        ax.set_xlim(-1.20, 1.20)
        ax.set_ylim(-1.32, 1.12)
        ax.axis("off")

    c1, r1 = np.array([-0.35, 0.05]), 0.72
    c2, r2 = np.array([0.38, 0.02]), 0.70
    p_top, p_bottom = circle_intersections(c1, r1, c2, r2)
    ax = axes[0]
    ax.add_patch(Circle(c1, r1, fill=False, ec=BLUE, lw=2.7))
    ax.add_patch(Circle(c2, r2, fill=False, ec=TEAL, lw=2.7))
    ax.plot(*c1, "o", color=BLUE, ms=7)
    ax.plot(*c2, "o", color=TEAL, ms=7)
    ax.text(*(c1 + np.array([-0.12, -0.12])), "GP1", color=BLUE, fontsize=10)
    ax.text(*(c2 + np.array([0.05, -0.12])), "GP2", color=TEAL, fontsize=10)
    for p in (p_top, p_bottom):
        ax.plot(*p, "o", color=RED, ms=8)
    ax.text(*(p_top + np.array([0.03, 0.08])), "candidate A", color=RED, fontsize=10)
    ax.text(*(p_bottom + np.array([0.03, -0.18])), "candidate B", color=RED, fontsize=10)
    ax.set_title("Two sights: usually two candidates", color=NAVY, fontsize=13, weight="bold")

    ax = axes[1]
    ax.add_patch(Circle(c1, r1, fill=False, ec=BLUE, lw=2.4))
    ax.add_patch(Circle(c2, r2, fill=False, ec=TEAL, lw=2.4))
    c3 = np.array([0.05, -0.25])
    r3 = float(np.linalg.norm(p_top - c3))
    ax.add_patch(Circle(c3, r3, fill=False, ec=ORANGE, lw=2.4))
    ax.plot(*c3, "o", color=ORANGE, ms=7)
    ax.text(*(c3 + np.array([0.06, -0.06])), "GP3", color="#9B5B19", fontsize=10)
    ax.plot(*p_top, marker="*", color=RED, ms=15)
    ax.text(*(p_top + np.array([0.04, 0.09])), "fix", color=RED, fontsize=12, weight="bold")
    ax.plot(*p_bottom, "o", color="#B0B0B0", ms=6)
    ax.text(
        *(p_bottom + np.array([0.08, -0.14])),
        "rejected by sight 3",
        color=GREY,
        fontsize=9,
    )
    ax.set_title("A third sight resolves and checks", color=NAVY, fontsize=13, weight="bold")
    fig.text(0.5, 0.02, "Small mismatches form a cocked hat and reveal observational scatter.", ha="center", color=GREY, fontsize=10)
    save(fig, "03_intersecting_altitude_circles")


def diagram_intercept_method():
    fig, ax = setup_ax(width=9.0, height=5.4, xlim=(-3.0, 3.2), ylim=(-2.0, 2.1))
    ap = np.array([-0.9, -0.55])
    zn_deg = 52.0
    u = point(zn_deg)
    v = np.array([-u[1], u[0]])
    intercept_nm = 0.95
    ip = ap + intercept_nm * u
    arrow(ax, ap - 0.2 * u, ap + 3.0 * u, color=GOLD, width=2.3)
    ax.text(*(ap + 2.65 * u + np.array([0.12, 0.04])), "towards GP", color="#8A6518", fontsize=11)
    ax.plot(*ap, "o", color=NAVY, ms=9)
    ax.text(*(ap + np.array([-0.45, -0.20])), "Assumed position\n(AP)", ha="center", color=NAVY, fontsize=11, weight="bold")
    ax.plot(*ip, "o", color=RED, ms=8)
    ax.plot(
        [ip[0] - 1.58 * v[0], ip[0] + 1.58 * v[0]],
        [ip[1] - 1.58 * v[1], ip[1] + 1.58 * v[1]],
        color=RED,
        lw=3,
    )
    ax.text(
        *(ip - 1.38 * v + np.array([0.08, -0.10])),
        "observed LOP",
        color=RED,
        fontsize=12,
        weight="bold",
        ha="center",
    )
    # Computed tangent at AP, shown faintly.
    ax.plot(
        [ap[0] - 1.50 * v[0], ap[0] + 1.50 * v[0]],
        [ap[1] - 1.50 * v[1], ap[1] + 1.50 * v[1]],
        color=GREY,
        lw=1.5,
        ls="--",
    )
    ax.text(*(ap + 1.62 * v), "LOP if $H_o=H_c$", color=GREY, fontsize=10, ha="center")
    arrow(ax, ap + 0.05 * v, ip + 0.05 * v, color=BLUE, width=2.5, head=11, style="<->")
    mid = (ap + ip) / 2 + 0.13 * v
    ax.text(*mid, r"intercept  $a=H_o-H_c$", color=BLUE, fontsize=12, ha="center", va="bottom", rotation=zn_deg)
    ax.text(-2.85, 1.60, "Measured higher than calculated", fontsize=13, color=NAVY, weight="bold")
    ax.text(-2.85, 1.32, r"$H_o>H_c$: move TOWARDS the body", fontsize=12, color=RED)
    ax.text(-2.85, 1.02, r"$H_o<H_c$: move AWAY from the body", fontsize=12, color=BLUE)
    ax.text(-2.85, -1.65, "The LOP is perpendicular to azimuth Zn at the intercept point.", fontsize=11, color=NAVY)
    ax.text(-2.85, -1.86, "Local chart construction — distances schematic", fontsize=9, color=GREY)
    ax.text(
        *(ap + 2.20 * u + np.array([-0.10, 0.14])),
        r"azimuth $Z_n$",
        color="#8A6518",
        fontsize=11,
        rotation=zn_deg,
    )
    save(fig, "04_intercept_method")


def diagram_altitude_corrections():
    fig, ax = setup_ax(width=10.6, height=4.1, xlim=(-0.3, 10.3), ylim=(-1.05, 2.15))
    stages = [
        (0.45, "Sextant\nreading", r"$H_s$", NAVY),
        (2.25, "Index\ncorrection", "IC", BLUE),
        (4.05, "Dip / horizon\ncorrection", "Dip", TEAL),
        (5.85, "Refraction", "R", ORANGE),
        (7.65, "Limb and\nparallax", "SD, PA", GOLD),
        (9.45, "Observed\naltitude", r"$H_o$", RED),
    ]
    for i, (x, label, symbol, color) in enumerate(stages):
        ax.add_patch(
            plt.Rectangle((x - 0.67, 0.10), 1.34, 1.16, facecolor="white", edgecolor=color, linewidth=2.2)
        )
        ax.text(x, 0.82, label, ha="center", va="center", color=NAVY, fontsize=10.5, weight="bold")
        ax.text(x, 0.32, symbol, ha="center", va="center", color=color, fontsize=12)
        if i < len(stages) - 1:
            arrow(ax, (x + 0.72, 0.68), (stages[i + 1][0] - 0.72, 0.68), color=GREY, width=1.5, head=10)
    ax.text(5.0, 1.82, "From the angle read on the instrument to the angle used for the position circle", ha="center", color=NAVY, fontsize=14, weight="bold")
    ax.text(
        5.0,
        -0.45,
        "The sign and size of each correction depend on the observation: natural or artificial horizon,\nheight of eye, pressure and temperature, selected limb, body and altitude.",
        ha="center",
        va="center",
        color=GREY,
        fontsize=10.5,
    )
    ax.text(0.0, -0.91, "Conceptual sequence; use the plugin's Calculations tab for the applied terms and signs.", fontsize=9, color=GREY)
    save(fig, "05_altitude_correction_pipeline")


def diagram_time_longitude():
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.7))
    fig.patch.set_facecolor("white")
    for ax in axes:
        ax.set_aspect("equal")
        ax.set_xlim(-1.25, 1.25)
        ax.set_ylim(-1.15, 1.20)
        ax.axis("off")
        ax.add_patch(Circle((0, 0), 1, fc=SEA, ec=NAVY, lw=2))
        ax.plot([-1, 1], [0, 0], color=GREY, lw=1)
        ax.text(0.72, -0.10, "equator", color=GREY, fontsize=9)

    for idx, (ax, meridian, title) in enumerate(
        zip(axes, (13, 18), ("Correct UTC", "Watch 20 minutes slow"))
    ):
        # Projected meridian as ellipse-like curve.
        t = np.linspace(-math.pi / 2, math.pi / 2, 200)
        x = math.sin(math.radians(meridian)) * np.cos(t)
        y = np.sin(t)
        ax.plot(x, y, color=RED, lw=2.5)
        gp = np.array([math.sin(math.radians(meridian)) * math.cos(math.radians(20)), math.sin(math.radians(20))])
        ax.plot(*gp, "o", color=RED, ms=9)
        ax.text(*(gp + np.array([0.07, 0.05])), "predicted GP", color=RED, fontsize=10, weight="bold")
        ax.set_title(title, color=NAVY, fontsize=13, weight="bold")
        if idx == 1:
            x_ref = math.sin(math.radians(13)) * np.cos(t)
            ax.plot(x_ref, y, color=GREY, lw=1.4, ls="--")
            ax.annotate("GP computed on\nthe wrong meridian", xy=gp, xytext=(0.40, -0.58), fontsize=10, color=NAVY, arrowprops=dict(arrowstyle="->", color=NAVY))

    fig.text(0.5, 0.035, "Earth turns approximately 15° per hour: a 20-minute time error corresponds to 5° of hour angle.", ha="center", color=NAVY, fontsize=11, weight="bold")
    fig.text(0.5, 0.005, "The resulting position error depends on latitude, body geometry and the combination of sights.", ha="center", color=GREY, fontsize=9.5)
    save(fig, "06_time_and_longitude")


def diagram_navigational_triangle():
    """Named elements of the spherical navigational triangle.

    The curved sides represent great-circle arcs.  This is deliberately a
    topology/notation drawing rather than a misleading planar solution.
    """
    fig, ax = setup_ax(width=8.8, height=5.8, xlim=(-1.72, 1.78), ylim=(-1.36, 1.42))
    ax.add_patch(Circle((0, 0), 1.08, fc=SEA, ec=NAVY, lw=2.0))
    ax.add_patch(Arc((0, -0.03), 2.10, 0.52, theta1=4, theta2=176, color=GREY, lw=1.1))
    ax.text(0.58, 0.18, "celestial equator", color=GREY, fontsize=9, ha="center")

    pole = np.array([0.02, 1.03])
    ap = np.array([-0.73, -0.35])
    gp = np.array([0.69, 0.00])
    for p, colour in ((pole, NAVY), (ap, BLUE), (gp, RED)):
        ax.plot(*p, "o", color=colour, ms=9, zorder=8)
    ax.text(0.02, 1.17, "P — elevated pole", ha="center", color=NAVY,
            fontsize=10.5, weight="bold")
    ax.annotate("AP — observer / assumed position", xy=ap, xytext=(-1.56, -0.55),
                ha="left", color=BLUE, fontsize=10, weight="bold",
                arrowprops=dict(arrowstyle="-", color=BLUE, lw=1.2))
    ax.annotate("GP — body's geographic position", xy=gp, xytext=(0.93, -0.46),
                ha="left", color=RED, fontsize=10, weight="bold",
                arrowprops=dict(arrowstyle="-", color=RED, lw=1.2))

    def great_circle_curve(points, colour, width=2.5):
        path = MplPath(points, [MplPath.MOVETO, MplPath.CURVE4, MplPath.CURVE4, MplPath.CURVE4])
        ax.add_patch(PathPatch(path, fill=False, color=colour, lw=width, zorder=5))

    # P–AP: observer's meridian; P–GP: body's hour circle; AP–GP: vertical circle.
    great_circle_curve([pole, (-0.45, 0.82), (-0.82, 0.20), ap], BLUE)
    great_circle_curve([pole, (0.35, 0.82), (0.69, 0.45), gp], RED)
    great_circle_curve([ap, (-0.24, -0.68), (0.34, -0.52), gp], TEAL, 3.0)

    ax.text(-0.72, 0.42, r"colatitude  $90^\circ-|\phi|$", color=BLUE,
            fontsize=10.5, rotation=62, ha="center")
    ax.text(0.61, 0.48, r"polar distance  $90^\circ-|\delta|$", color=RED,
            fontsize=10.5, rotation=-60, ha="center")
    ax.text(-0.03, -0.92, r"zenith distance  $z=90^\circ-H_c$", color=TEAL,
            fontsize=11, ha="center", weight="bold")

    ax.add_patch(Arc(pole, 0.60, 0.42, theta1=220, theta2=316, color=GOLD, lw=2.2))
    ax.text(0.03, 0.72, "LHA", color="#8A6518", fontsize=11, ha="center", weight="bold")
    ax.add_patch(Arc(ap, 0.62, 0.52, theta1=26, theta2=79, color=ORANGE, lw=2.2))
    ax.text(-0.43, -0.18, "Zn", color="#9B5B19", fontsize=11, weight="bold")

    ax.text(0, -1.17, "The sides are arcs of great circles on the celestial sphere—not straight planar distances.",
            ha="center", color=NAVY, fontsize=9.5)
    ax.text(0, -1.31, "Schematic northern-hemisphere arrangement; labels and relationships are general.",
            ha="center", color=GREY, fontsize=9)
    ax.set_title("The navigational spherical triangle", color=NAVY, fontsize=15, weight="bold", pad=8)
    save(fig, "07_navigational_spherical_triangle")


def diagram_running_fix():
    fig, ax = setup_ax(width=9.4, height=5.2, xlim=(-3.3, 3.3), ylim=(-2.1, 2.0))
    u1 = point(25)
    v1 = np.array([-u1[1], u1[0]])
    p1 = np.array([-1.8, -0.75])
    motion = np.array([2.10, 0.70])
    p1a = p1 + motion
    u2 = point(135)
    v2 = np.array([-u2[1], u2[0]])
    p2 = p1a + np.array([0.20, -0.10])

    ax.plot([p1[0] - 2.5 * v1[0], p1[0] + 2.5 * v1[0]], [p1[1] - 2.5 * v1[1], p1[1] + 2.5 * v1[1]], color=BLUE, lw=2.5)
    ax.text(*(p1 - 2.05 * v1), "LOP 1 at $t_1$", color=BLUE, fontsize=11, ha="center")
    ax.plot([p1a[0] - 2.5 * v1[0], p1a[0] + 2.5 * v1[0]], [p1a[1] - 2.5 * v1[1], p1a[1] + 2.5 * v1[1]], color=BLUE, lw=2.5, ls="--")
    ax.text(*(p1a + 1.30 * v1), "LOP 1 advanced to $t_2$", color=BLUE, fontsize=11, ha="center")
    arrow(ax, p1, p1a, color=GREEN, width=3, head=13)
    ax.text(*((p1 + p1a) / 2 + np.array([0.0, 0.18])), "vessel motion from COG/SOG", color=GREEN, fontsize=10.5, ha="center", rotation=18)
    ax.plot([p2[0] - 2.3 * v2[0], p2[0] + 2.3 * v2[0]], [p2[1] - 2.3 * v2[1], p2[1] + 2.3 * v2[1]], color=ORANGE, lw=2.8)
    ax.text(*(p2 - 1.80 * v2), "LOP 2 at $t_2$", color="#9B5B19", fontsize=11, ha="center")
    # Analytic intersection of two lines p1a+s*v1 and p2+t*v2.
    mat = np.column_stack((v1, -v2))
    s, _ = np.linalg.solve(mat, p2 - p1a)
    fix = p1a + s * v1
    ax.plot(*fix, marker="*", color=RED, ms=17)
    ax.text(*(fix + np.array([0.16, 0.08])), "running fix at $t_2$", color=RED, fontsize=12, weight="bold")
    ax.text(-3.15, 1.72, "Earlier observations must be brought to one common epoch", color=NAVY, fontsize=14, weight="bold")
    ax.text(-3.15, -1.92, "Motion uncertainty is part of the fix uncertainty; COG/SOG is not a substitute for a sound DR.", color=GREY, fontsize=9.5)
    save(fig, "08_running_fix")


def diagram_lunar_distance():
    fig, ax = setup_ax(width=9.5, height=5.4, xlim=(-3.25, 3.25), ylim=(-1.8, 2.2))
    # Horizon and local sky.
    ax.plot([-3.0, 3.0], [-0.85, -0.85], color=GREEN, lw=3)
    ax.text(2.22, -0.70, "visible horizon", color=GREEN, fontsize=10)
    moon = np.array([-1.25, 0.65])
    body = np.array([1.55, 1.35])
    observer = np.array([0.0, -0.85])
    ax.add_patch(Circle(moon, 0.20, fc="#D9D9D9", ec=NAVY, lw=1.5))
    ax.text(*(moon + np.array([-0.05, 0.37])), "Moon", ha="center", color=NAVY, fontsize=12, weight="bold")
    ax.plot(*body, marker="*", color=GOLD, ms=17)
    ax.text(
        *(body + np.array([0.46, -0.18])),
        "Sun, planet\nor star",
        ha="left",
        color=NAVY,
        fontsize=11,
        weight="bold",
    )
    ax.plot(*observer, marker="^", color=NAVY, ms=9)
    ax.plot([observer[0], moon[0]], [observer[1], moon[1]], color=BLUE, lw=2)
    ax.plot([observer[0], body[0]], [observer[1], body[1]], color=ORANGE, lw=2)
    moon_angle = math.degrees(math.atan2((moon - observer)[1], (moon - observer)[0]))
    body_angle = math.degrees(math.atan2((body - observer)[1], (body - observer)[0]))
    ax.add_patch(
        Arc(observer, 2.10, 2.10, theta1=body_angle, theta2=moon_angle, color=RED, lw=3)
    )
    ax.add_patch(
        Arc(observer, 1.35, 1.35, theta1=0, theta2=body_angle, color=ORANGE, lw=2)
    )
    ax.add_patch(
        Arc(observer, 1.55, 1.55, theta1=moon_angle, theta2=180, color=BLUE, lw=2)
    )
    ax.text(
        -0.05,
        0.40,
        "observed lunar distance",
        color=RED,
        fontsize=11,
        weight="bold",
        ha="center",
        bbox=dict(fc="white", ec="none", pad=1.0),
    )
    ax.text(-1.30, -0.12, "Moon altitude", color=BLUE, fontsize=10, rotation=50)
    ax.text(0.58, -0.04, "body altitude", color="#9B5B19", fontsize=10, rotation=55)
    ax.text(
        -3.05,
        1.85,
        "Three observations — one unknown constant watch offset",
        color=NAVY,
        fontsize=14,
        weight="bold",
    )
    ax.text(
        -2.98,
        -1.48,
        "$t_D$  lunar distance     $t_M$  Moon altitude     $t_B$  body altitude\n"
        "The readings may be seconds apart. The solver uses their individual watch times,\n"
        "a common clock correction and optional vessel motion between them.",
        fontsize=10.5,
        color=NAVY,
        bbox=dict(boxstyle="round,pad=0.55", fc=LIGHT, ec=BLUE),
    )
    ax.text(
        2.38,
        0.18,
        "Refraction and parallax\nmust be cleared before\ncomparison with the\ngeocentric ephemeris.",
        ha="center",
        fontsize=10,
        color=GREY,
    )
    save(fig, "09_lunar_distance_observations")


def diagram_coastal_methods():
    fig, axes = plt.subplots(1, 2, figsize=(10.3, 4.7))
    fig.patch.set_facecolor("white")
    for ax in axes:
        ax.set_aspect("equal")
        ax.axis("off")
    ax = axes[0]
    ax.set_xlim(-0.2, 5.2)
    ax.set_ylim(-0.7, 3.1)
    ax.plot([0, 5], [0, 0], color=BLUE, lw=3)
    ax.add_patch(plt.Rectangle((4.2, 0), 0.24, 2.2, fc=LAND, ec=NAVY, lw=1.5))
    ax.add_patch(Polygon([[4.05, 2.2], [4.56, 2.2], [4.30, 2.65]], fc=RED, ec=NAVY))
    eye = np.array([0.55, 0.65])
    foot = np.array([4.32, 0.0])
    top = np.array([4.30, 2.65])
    ax.plot(*eye, "o", color=NAVY, ms=7)
    ax.plot([eye[0], foot[0]], [eye[1], foot[1]], color=GREY, lw=1.5)
    ax.plot([eye[0], top[0]], [eye[1], top[1]], color=ORANGE, lw=2.2)
    ax.add_patch(Arc(eye, 1.25, 1.25, theta1=-10, theta2=29, color=RED, lw=2))
    ax.text(1.15, 0.78, "vertical angle", color=RED, fontsize=10)
    ax.text(4.65, 1.25, "charted height", rotation=90, color=NAVY, fontsize=10, ha="center")
    ax.set_title("Vertical angle gives range", color=NAVY, fontsize=13, weight="bold")

    ax = axes[1]
    ax.set_xlim(-2.6, 2.6)
    ax.set_ylim(-1.8, 2.3)
    objs = [np.array([-1.75, 1.45]), np.array([0.0, 1.90]), np.array([1.85, 1.28])]
    vessel = np.array([0.10, -0.75])
    labels = ["A", "B", "C"]
    for p, label in zip(objs, labels):
        ax.plot(*p, marker="^", color=NAVY, ms=9)
        ax.text(*(p + np.array([0.0, 0.18])), label, ha="center", color=NAVY, fontsize=11, weight="bold")
        ax.plot([vessel[0], p[0]], [vessel[1], p[1]], color=GREY, lw=1.3)
    ax.plot(*vessel, marker=(3, 0, 0), color=RED, ms=13)
    bearings = [math.degrees(math.atan2((p - vessel)[1], (p - vessel)[0])) for p in objs]
    ax.add_patch(
        Arc(vessel, 1.25, 1.25, theta1=bearings[1], theta2=bearings[0], color=BLUE, lw=2.2)
    )
    ax.add_patch(
        Arc(vessel, 1.55, 1.55, theta1=bearings[2], theta2=bearings[1], color=ORANGE, lw=2.2)
    )
    ax.text(-0.40, -0.02, r"$\alpha$", color=BLUE, fontsize=13, weight="bold")
    ax.text(0.48, -0.02, r"$\beta$", color="#9B5B19", fontsize=13, weight="bold")
    ax.text(0.10, -1.10, "vessel", ha="center", color=RED, fontsize=10)
    ax.set_title("Two horizontal angles give a fix", color=NAVY, fontsize=13, weight="bold")
    fig.text(
        0.5,
        0.02,
        "Schematic geometry. The calculation also accounts for eye height, curvature and refraction as applicable.\n"
        "Coastal observations use charted terrestrial objects; they are not celestial azimuth sights.",
        ha="center",
        color=GREY,
        fontsize=9.5,
    )
    save(fig, "10_coastal_sextant_methods")


def main():
    diagram_geographic_position()
    diagram_circle_equal_altitude()
    diagram_intersections()
    diagram_intercept_method()
    diagram_altitude_corrections()
    diagram_time_longitude()
    diagram_navigational_triangle()
    diagram_running_fix()
    diagram_lunar_distance()
    diagram_coastal_methods()
    print(f"Generated 10 SVG masters in {SVG_DIR}")
    print(f"Generated 10 PNG derivatives in {PNG_DIR}")


if __name__ == "__main__":
    main()
