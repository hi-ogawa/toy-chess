#!/bin/bash

[ -z "$LICHESS_TOKEN" ] && echo ":: LICHESS_TOKEN is required" && exit 1

sed -i "s#xxxxxxxxxxxxxxxx#$LICHESS_TOKEN#" config.yml

exec python lichess-bot.py "$LICHESS_BOT_OPTIONS"
