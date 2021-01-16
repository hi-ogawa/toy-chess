#
# Builder
#
FROM archlinux:base-devel-20210110.0.13332 AS builder

RUN pacman --noconfirm -Syu
RUN pacman --noconfirm -S clang lld cmake git python

ARG CMAKE_ARGS

# Copy sources to /toy-chess
RUN mkdir -p /toy-chess
WORKDIR /toy-chess
COPY . /toy-chess
RUN python script.py init --clang -- $CMAKE_ARGS
RUN python script.py build --t main

# Copy engine and config.yml to lichess-bot
RUN cp build/Release/main thirdparty/lichess-bot/engines
RUN cp misc/bot/config.yml thirdparty/lichess-bot/config.yml
RUN cp misc/bot/main.sh thirdparty/lichess-bot/main.sh

#
# Runner
#
FROM archlinux:base-20210110.0.13332 AS runner

RUN pacman --noconfirm -Syu
RUN pacman --noconfirm -S python python-pip

# Copy lichess-bot from Builder
COPY --from=builder /toy-chess/thirdparty/lichess-bot /lichess-bot
WORKDIR /lichess-bot
RUN pip install -r requirements.txt

CMD cd /lichess-bot && bash main.sh
