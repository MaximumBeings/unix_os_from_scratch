#!/usr/bin/env python3
"""Write the book's pictures: docs/assets/art/<page>.svg (a species of Capra on another world, with that world's neighbours in the sky) and <page>.md (a short background on the animal and the world, included on the page).
The animals and worlds are stylised drawings; the goats and ibexes are shown where they could not live, for effect. Usage: python3 docs/assets/art/make_art.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import lib
HERE = os.path.dirname(os.path.abspath(__file__))
BIO = {
 "alpine": "lives in the European Alps, usually above the tree line on steep rock, and is a famously sure-footed climber. Males carry horns that can reach about a metre. Hunting left only a small remnant, around the Gran Paradiso in Italy, by the early 1800s; reintroductions have since restored it across the Alps.",
 "markhor": "lives in the mountains of Pakistan, Afghanistan and neighbouring regions. The males' horns twist in a corkscrew; the name is often explained as Persian for 'snake-eater'. It is Pakistan's national animal, and community-based conservation has helped some populations recover.",
 "nubian": "lives in the rocky, arid hills of north-east Africa and the Arabian Peninsula, from the Sinai and Egypt south to Sudan and Ethiopia. Its coat matches the desert rock, and the males' ridged horns sweep back like a sickle.",
 "siberian": "lives in the high mountains of Central Asia, from the Altai and Tian Shan to the Himalaya. It is one of the largest ibexes, and the males' ridged horns can exceed a metre in length.",
 "walia": "lives nowhere but the Simien Mountains of northern Ethiopia, on high cliffs. It is endangered, with only a few hundred animals remaining, and is protected within the Simien Mountains National Park.",
 "iberian": "lives in the mountains of Spain and Portugal. Its Pyrenean subspecies, the bucardo, died out in 2000. In 2003 scientists cloned one; the kid lived only minutes, making it the first extinct animal to be cloned.",
 "tur": "lives only in the western Caucasus, on steep slopes above the forest line. Its horns are short and heavy, curving outward and back, and it moves up and down cliffs with ease.",
 "bezoar": "lives from Turkey and the Caucasus through Iran to Pakistan. It is the wild ancestor of the domestic goat, which people began keeping some ten thousand years ago.",
 "domestic": "descends from the bezoar ibex and is among the oldest domesticated animals. There are on the order of a billion of them, kept for milk, meat, fibre and as pack animals.",
}
WORLD = {
 "earth": "Earth, with its Moon in the sky.",
 "moon": "the Moon: gravity about a sixth of Earth's, and an Earth hanging in the black sky.",
 "mars": "Mars: gravity about 38% of Earth's, a dusty red landscape and two small moons, Phobos and Deimos.",
 "europa": "Europa, the icy moon of Jupiter, which hides a salty ocean beneath its ice; Jupiter fills the sky.",
 "titan": "Titan, Saturn's largest moon: a thick orange haze, and lakes of liquid methane and ethane on its surface.",
 "io": "Io, the most volcanically active body in the solar system, under the enormous disc of Jupiter.",
 "enceladus": "Enceladus, a small icy moon of Saturn whose south pole sprays water-ice geysers; Saturn and its rings loom overhead.",
 "pluto": "Pluto, a dwarf planet with a heart-shaped plain of nitrogen ice; its largest moon, Charon, is about half its width.",
 "triton": "Triton, Neptune's largest moon: it orbits backward and has nitrogen geysers.",
 "twin": "an imaginary planet with two suns and a purple moon.",
}
# page id -> (world, cast)  (the first species is the lead)
PLAN = {
 "index": ("earth", ["alpine", "markhor", "nubian", "bezoar", "domestic"]), "getting-started": ("earth", ["domestic", "bezoar", "iberian", "tur"]), "install": ("mars", ["nubian", "domestic", "walia", "siberian"]),
 "ch-01": ("earth", ["bezoar", "domestic", "alpine", "siberian"]), "ch-02": ("mars", ["nubian", "walia", "iberian", "markhor"]), "ch-03": ("moon", ["siberian", "alpine", "tur", "domestic", "bezoar"]),
 "ch-04": ("titan", ["markhor", "siberian", "alpine", "nubian"]), "ch-05": ("europa", ["alpine", "tur", "iberian", "domestic", "walia"]), "ch-06": ("io", ["walia", "nubian", "bezoar", "markhor"]),
 "ch-07": ("mars", ["iberian", "alpine", "siberian", "tur", "domestic"]), "ch-08": ("enceladus", ["tur", "markhor", "bezoar", "alpine"]), "ch-09": ("twin", ["domestic", "domestic", "domestic", "domestic", "alpine", "markhor"]),
 "ch-10": ("triton", ["markhor", "nubian", "siberian", "walia"]), "ch-11": ("pluto", ["alpine", "siberian", "markhor", "tur", "bezoar", "iberian"]), "ch-12": ("twin", ["alpine", "markhor", "walia", "nubian", "domestic", "tur"]),
 "ch-13": ("moon", ["domestic", "bezoar", "nubian", "domestic"]), "ch-14": ("mars", ["domestic", "domestic", "domestic", "alpine", "tur"]), "ch-15": ("titan", ["tur", "alpine", "iberian", "siberian"]),
 "ch-16": ("europa", ["bezoar", "domestic", "markhor", "alpine"]), "ch-17": ("twin", ["domestic", "markhor", "nubian", "tur", "siberian", "walia"]),
 "appx-a": ("earth", ["domestic", "alpine", "bezoar", "nubian"]), "appx-b": ("moon", ["iberian", "markhor", "domestic", "tur"]), "appx-c": ("mars", ["siberian", "walia", "alpine", "domestic", "bezoar"]),
 "appx-d": ("europa", ["bezoar", "nubian", "iberian", "siberian"]), "appx-e": ("titan", ["alpine", "markhor", "tur", "domestic", "walia"]), "appx-f": ("enceladus", ["walia", "siberian", "domestic", "iberian"]),
 "appx-g": ("io", ["tur", "bezoar", "alpine", "nubian", "markhor"]), "answers": ("pluto", ["markhor", "alpine", "domestic", "tur"]),
}
for pid, (wd, cast) in PLAN.items():
    open(os.path.join(HERE, pid + ".svg"), "w").write(lib.scene(wd, pid, cast))
    seen = []; [seen.append(c) for c in cast if c not in seen]
    items = "\n".join(f'    - **{lib.SPECIES[c]["name"]}** (*{lib.SPECIES[c]["sci"]}*) {BIO[c]}' for c in seen)
    cap = f'??? info "About the animals in this picture and where they are standing"\n{items}\n\n    The scene is imaginary: it is set on {WORLD[wd]} The animals are stylised drawings, shown where they could not live, for effect.\n'
    open(os.path.join(HERE, pid + ".md"), "w").write(cap)
print(len(PLAN), "pictures and captions written")
