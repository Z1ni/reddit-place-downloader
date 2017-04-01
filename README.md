# reddit-place-downloader

Reddit [/r/place](https://www.reddit.com/r/place) image downloader in C.

## Requirements
- libpng
- libcurl

## Compile
```bash
$ gcc -O3 -lpng -lcurl -std=c99 -Wall -o place place.c
```

## Usage
```
Usage: ./place [options] [filename]
  options:
    -h       Help
    -s       Silent (don't print anything)
    -f file  Saves the downloaded image as 'file'
```
