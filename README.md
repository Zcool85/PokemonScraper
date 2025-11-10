# PokemonScraper

Tool for downloading Pokémon card images from [TCGdex](https://tcgdex.dev).

## Features

- Download Pokémon images in all languages from [TCGdex](https://tcgdex.dev)
- Only high resolution images are downloaded
- Organize data into structured directories

## Prerequisites

Add sqlite3:

```bash
# On Ubuntu/Debian
sudo apt-get install libsqlite3-dev
# On macOS
brew install sqlite3
```

Add libcurl:

```bash
# On Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev
# On macOS
brew install curl
```

## Usage

Build and run the scraper:

```bash
cmake -S . -B build
cmake --build build
./build/PokemonScraper
```

The scraper will download Pokémon images and organize them into directories
based on language, set and Pokémon name.

## Directory Structure

The downloaded images will be stored in the `data` directory.
Then, within `data` folder, there will be one subdirectory for each languages.
Inside each language directory, there will be subdirectories for each set,
and within each set directory, the images will be named according to the
card with this pattern: `<local_id>_<pokemon_name>.jpg`

Example structure:

```
data/
├── en
│   ├── base1
│   │   ├── 01_Bulbasaur.jpg
│   │   ├── 04_Charmander.jpg
│   │   └── ...
│   ├── base2
│   │   ├── 01_Squirtle.jpg
│   │   ├── 10_Pikachu.jpg
│   │   └── ...
│   └── ...
├── fr
│   ├── base1
│   │   ├── 01_Bulbizarre.jpg
│   │   ├── 04_Salameche.jpg
│   │   └── ...
│   ├── base2
│   │   ├── 01_Carapuce.jpg
│   │   ├── 10_Pikachu.jpg
│   │   └── ...
│   └── ...
└── ...
```

## Algorithm

For each language in `en, fr, es, es-mx, it, pt, pt-br, pt-pt, de, nl, pl, ru, ja, ko, zh-tw, id, th, zh-cn`:

Call `https://api.tcgdex.net/v2/{lang}/sets` to get all sets.

Return example:

```
[
  {
    "id": "base1",
    "name": "Set de Base",
    "cardCount": {
      "total": 102,
      "official": 102
    }
  },
  {
    "id": "base2",
    "name": "Jungle",
    "logo": "https://assets.tcgdex.net/fr/base/base2/logo",
    "symbol": "https://assets.tcgdex.net/univ/base/base2/symbol",
    "cardCount": {
      "total": 64,
      "official": 64
    }
  },
  ...
]
```

Return headers :

```
< access-control-allow-headers: DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range
< access-control-allow-methods: GET,POST,OPTIONS
< access-control-allow-origin: *
< access-control-expose-headers: Content-Length,Content-Range
< alt-svc: h3=":443"; ma=2592000
< cache-control: no-cache, no-store, must-revalidate
< content-type: application/json; charset=utf-8
< date: Sat, 01 Nov 2025 13:47:32 GMT
< etag: W/"71a3-IVyvWA1Kx+tlE3+lpRmwgeyJQxA"
< strict-transport-security: max-age=31536000
< x-powered-by: Express
< content-length: 29091
```


For each set, call https://api.tcgdex.net/v2/{lang}/sets/{setId} to get all cards in the set.


Return example:

```
{
  "cardCount": {
    "firstEd": 0,
    "holo": 0,
    "normal": 0,
    "official": 102,
    "reverse": 0,
    "total": 102
  },
  "cards": [
    {
      "id": "base1-1",
      "image": "https://assets.tcgdex.net/fr/base/base1/1",
      "localId": "1",
      "name": "Alakazam"
    },
    ...
    {
      "id": "base1-102",
      "image": "https://assets.tcgdex.net/fr/base/base1/102",
      "localId": "102",
      "name": "Énergie Eau"
    }
  ],
  "id": "base1",
  "legal": {
    "expanded": false,
    "standard": false
  },
  "name": "Set de Base",
  "releaseDate": "1999-01-09",
  "serie": {
    "id": "base",
    "name": "Base"
  },
  "tcgOnline": "BS",
  "abbreviation": {
    "official": "BS",
    "localized": "BAS"
  }
}
```

Return headers :

```
< access-control-allow-headers: DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range
< access-control-allow-methods: GET,POST,OPTIONS
< access-control-allow-origin: *
< access-control-expose-headers: Content-Length,Content-Range
< alt-svc: h3=":443"; ma=2592000
< cache-control: no-cache, no-store, must-revalidate
< content-type: application/json; charset=utf-8
< date: Sat, 01 Nov 2025 13:51:10 GMT
< etag: W/"2b66-G2IzG10RvILu4yQjGpCYu4JDVrc"
< strict-transport-security: max-age=31536000
< x-powered-by: Express
< content-length: 11110
```


For each card, download the image from the `image` field appended by `high.jpg` and save it to the appropriate directory.

Example: `https://assets.tcgdex.net/fr/base/base1/1/high.jpg` for the Alakazam card in French from the Base Set.

Return headers :

```
< accept-ranges: bytes
< access-control-allow-headers: DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range
< access-control-allow-methods: GET, OPTIONS
< access-control-allow-origin: *
< access-control-expose-headers: Content-Length,Content-Range
< alt-svc: h3=":443"; ma=2592000
< cache-control: max-age=31536000
< content-type: image/jpeg
< date: Sat, 01 Nov 2025 13:53:56 GMT
< etag: "9a15b-5c062d9165240"
< expires: Sun, 01 Nov 2026 13:53:56 GMT
< last-modified: Tue, 20 Apr 2021 08:01:05 GMT
< server: Apache/2.4.65 (Unix)
< strict-transport-security: max-age=31536000; includeSubDomains; preload
< content-length: 631131
```

For each card to download, add :
- If-None-Match: etag
- If-Modified-Since: last-modified
If HTTP status code is 304 Not Modified, don't download image.

