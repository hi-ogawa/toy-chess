
Run toy-chess as Lichess bot

```
# Build
docker build --target runner -t hiogawa/toy-chess -f misc/bot/Dockerfile .
docker build --target runner -t hiogawa/toy-chess:avx --build-arg CMAKE_ARGS="-DUSE_AVX=ON" -f misc/bot/Dockerfile .

# Run locally
docker run --env LICHESS_TOKEN=(...secret...) --env LICHESS_BOT_OPTIONS="-v" --rm hiogawa/toy-chess

# Push to Docker hub
docker push hiogawa/toy-chess
docker push hiogawa/toy-chess:avx
```


Run from Github workflow dispatch

- Manually trigger from https://github.com/hi-ogawa/toy-chess/actions?query=workflow%3ABot.
- Note that the single action can run only 6 hours (cf. https://docs.github.com/en/actions/reference/usage-limits-billing-and-administration#usage-limits).


Deploy on Heroku

NOTE:
  - It seems AVX is not suppoted.
  - Sometimes there's network issues.

```
# First time setup
heroku login
heroku create
heroku rename hiro18181-toy-chess
heroku container:login

# Deployment
docker tag hiogawa/toy-chess registry.heroku.com/hiro18181-toy-chess/bot
docker push registry.heroku.com/hiro18181-toy-chess/bot
heroku config:set LICHESS_TOKEN=(...secret...)
heroku container:release bot
heroku ps:scale bot=1:Free # Hobby for running 24/7
heroku dyno:restart bot

# Check status
heroku ps
heroku logs --tail
```

References

- https://github.com/ShailChoksi/lichess-bot
- https://devcenter.heroku.com/articles/container-registry-and-runtime
