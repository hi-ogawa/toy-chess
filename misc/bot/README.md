
Run toy-chess as Lichess bot

```
# Build
docker build --target runner -t hiogawa/toy-chess -f misc/bot/Dockerfile .

# Run
docker run --env LICHESS_TOKEN=(...secret...) --rm hiogawa/toy-chess
```

Deploy on Heroku

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
