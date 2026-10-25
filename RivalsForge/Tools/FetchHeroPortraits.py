# Downloads the hero portraits from the Marvel Rivals wiki (marvelrivals.fandom.com, the "Main Page
# Portrait" of each hero) and writes Assets\Heroes\<hero id>.png: the upper part of the card art
# (head and torso), 240 px wide, transparent background. Run it again when a hero is added (and add
# the hero to Source\Data\Heroes.cpp).
#
#   python Tools\FetchHeroPortraits.py [--cache <folder with downloaded originals>]
#
# Needs Pillow. The originals (1358 x 1920, ~2.5 MB each) are cached in Tools\.cache\heroes.
import argparse
import json
import os
import urllib.parse
import urllib.request

from PIL import Image

# Hero id (Characters/<id> in the game files) -> wiki page title
HEROES = {
    '1011': 'Hulk', '1014': 'The Punisher', '1015': 'Storm', '1016': 'Loki', '1017': 'Human Torch',
    '1018': 'Doctor Strange', '1020': 'Mantis', '1021': 'Hawkeye', '1022': 'Captain America',
    '1023': 'Rocket Raccoon', '1024': 'Hela', '1025': 'Cloak & Dagger', '1026': 'Black Panther',
    '1027': 'Groot', '1028': 'Ultron', '1029': 'Magik', '1030': 'Moon Knight', '1031': 'Luna Snow',
    '1032': 'Squirrel Girl', '1033': 'Black Widow', '1034': 'Iron Man', '1035': 'Venom',
    '1036': 'Spider-Man', '1037': 'Magneto', '1038': 'Scarlet Witch', '1039': 'Thor',
    '1040': 'Mister Fantastic', '1041': 'Winter Soldier', '1042': 'Peni Parker', '1043': 'Star-Lord',
    '1044': 'Blade', '1045': 'Namor', '1046': 'Adam Warlock', '1047': 'Jeff the Land Shark',
    '1048': 'Psylocke', '1049': 'Wolverine', '1050': 'Invisible Woman', '1051': 'The Thing',
    '1052': 'Iron Fist', '1053': 'Emma Frost', '1054': 'Phoenix', '1055': 'Angela', '1056': 'Daredevil',
    '1057': 'Deadpool', '1058': 'Gambit', '1059': 'Elsa Bloodstone', '1060': 'White Fox',
    '1061': 'Black Cat', '1062': 'Devil Dinosaur', '1063': 'Cyclops', '1064': 'Jubilee',
    '1065': 'Rogue', '1066': 'The Hood', '1067': 'Gorr',
}

API = 'https://marvelrivals.fandom.com/api.php'
HEADERS = {'User-Agent': 'Mozilla/5.0', 'Referer': 'https://marvelrivals.fandom.com/'}
WIDTH = 240          # output width (px)
TOP_SHARE = 0.74     # the part of the card kept, from the top


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=60) as r:
        return r.read()


def cache_name(title):
    return title.replace(' ', '_').replace('&', 'and') + '.png'


def portrait_urls(titles):
    query = urllib.parse.urlencode({'action': 'query', 'titles': '|'.join(titles), 'prop': 'pageimages',
                                    'piprop': 'original', 'format': 'json'})
    pages = json.loads(get(API + '?' + query))['query']['pages'].values()
    return {p['title']: p['original']['source'] for p in pages if 'original' in p}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache', default=os.path.join(root, 'Tools', '.cache', 'heroes'))
    args = parser.parse_args()
    out_dir = os.path.join(root, 'Assets', 'Heroes')
    os.makedirs(args.cache, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    missing = [t for t in HEROES.values() if not os.path.exists(os.path.join(args.cache, cache_name(t)))]
    urls = {}
    for i in range(0, len(missing), 40):   # the API takes 50 titles at most
        urls.update(portrait_urls(missing[i:i + 40]))
    for title in missing:
        if title not in urls:
            print('no portrait for', title)
            continue
        sep = '&' if '?' in urls[title] else '?'
        data = get(urls[title] + sep + 'format=original')   # PNG, not the WebP the CDN sends by default
        open(os.path.join(args.cache, cache_name(title)), 'wb').write(data)
        print('downloaded', title)

    total = 0
    for hero_id, title in HEROES.items():
        src = os.path.join(args.cache, cache_name(title))
        if not os.path.exists(src):
            continue
        im = Image.open(src).convert('RGBA')
        im = im.crop((0, 0, im.width, int(im.height * TOP_SHARE)))
        im = im.resize((WIDTH, round(im.height * WIDTH / im.width)), Image.LANCZOS)
        dest = os.path.join(out_dir, hero_id + '.png')
        im.save(dest, optimize=True)
        total += os.path.getsize(dest)
    print(f'{len(HEROES)} heroes, {total // 1024} KB in Assets\\Heroes')


if __name__ == '__main__':
    main()
